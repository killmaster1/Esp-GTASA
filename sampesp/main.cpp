#include <mod/amlmod.h>
#include <android/log.h>
#include <link.h>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "overlay_math.h"

MYMOD(local.sampesp.alyn.arm64, Alyn Player ESP ARM64, 0.1.0, Custom)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.4.0)
END_DEPLIST()
namespace {
using esp::Vec;using esp::Point;using esp::Box;
struct Color{uint8_t r,g,b,a;};
struct Pool{void* storage;uint8_t* flags;int32_t size,freeIndex;bool own,noMemory;};
static_assert(sizeof(void*)==8&&sizeof(Vec)==12&&offsetof(Pool,size)==16,"ARM64 GTA ABI");
constexpr float range=250.f;
constexpr int maxDrawn=256;
constexpr Color green{50,255,90,230},outline{0,0,0,190};
void (*original2D)()=nullptr;
void* (*localPed)(int)=nullptr;
void* (*getPed)(int)=nullptr;
bool (*isPlayer)(const void*)=nullptr;
void (*bonePosition)(void*,Vec&,unsigned,bool)=nullptr;
void* (*hierarchyFromClump)(void*)=nullptr;
int (*boneIndex)(void*,int)=nullptr;
bool (*project)(const Vec&,Vec*,float*,float*,bool,bool)=nullptr;
void (*polygon)(float,float,float,float,float,float,float,float,const Color&)=nullptr;
Pool** poolPtr=nullptr;
uint8_t* screen=nullptr;
bool sampReady=false;
unsigned retryFrames=0;
float width=0,height=0;
template<class T>T read(const void* p,size_t off){T v;std::memcpy(&v,static_cast<const uint8_t*>(p)+off,sizeof(v));return v;}
void log(const char* s){__android_log_write(ANDROID_LOG_INFO,"AlynESP",s);}
Vec position(const void* ped){void* m=read<void*>(ped,24);return m?read<Vec>(m,48):read<Vec>(ped,8);}
// Only inspect loaded modules. Do not dlopen the multiplayer client as a side effect.
int findSamp(dl_phdr_info* info,size_t,void* result){
    const char* path=info->dlpi_name;if(!path)return 0;
    const char* name=std::strrchr(path,'/');name=name?name+1:path;
    if(std::strcmp(name,"libAlyn_SAMPMOBILE.so"))return 0;
    constexpr uint8_t expected[]{0xa8,0x61,0x94,0x42,0x04,0xc6,0x30,0x9b,0xb4,0x33,0xb7,0xe2,0xbe,0x8a,0x2f,0x13,0xc1,0x37,0xae,0x66};
    for(int i=0;i<info->dlpi_phnum;++i){
        const auto& segment=info->dlpi_phdr[i];if(segment.p_type!=PT_NOTE||segment.p_memsz>65536)continue;
        auto* data=reinterpret_cast<const uint8_t*>(info->dlpi_addr+segment.p_vaddr);
        size_t off=0;
        while(off+sizeof(ElfW(Nhdr))<=segment.p_memsz){
            auto note=read<ElfW(Nhdr)>(data,off);off+=sizeof(note);
            size_t nameSize=(size_t(note.n_namesz)+3)&~size_t(3),descSize=(size_t(note.n_descsz)+3)&~size_t(3);
            if(nameSize>segment.p_memsz-off||descSize>segment.p_memsz-off-nameSize)break;
            if(note.n_type==NT_GNU_BUILD_ID&&note.n_namesz==4&&note.n_descsz==sizeof(expected)&&!std::memcmp(data+off,"GNU\0",4)&&!std::memcmp(data+off+nameSize,expected,sizeof(expected))){
                *static_cast<bool*>(result)=true;return 1;
            }
            off+=nameSize+descSize;
        }
    }return 0;
}
void segment(Point a,Point b,float thickness,Color color){
    // Clip before constructing a quad so partially visible targets stay bounded.
    if(!esp::clip(a,b,width-1,height-1))return;
    float dx=b.x-a.x,dy=b.y-a.y,len=std::sqrt(dx*dx+dy*dy);
    if(!std::isfinite(len)||len<.5f)return;
    float nx=-dy/len*thickness*.5f,ny=dx/len*thickness*.5f;
    // Static Draw2DPolygon, not the instance DrawTriangle method.
    polygon(a.x+nx,a.y+ny,a.x-nx,a.y-ny,b.x+nx,b.y+ny,b.x-nx,b.y-ny,color);
}
void boxEdges(Box b,float thickness,Color color){
    segment({b.left,b.top},{b.right,b.top},thickness,color);
    segment({b.right,b.top},{b.right,b.bottom},thickness,color);
    segment({b.right,b.bottom},{b.left,b.bottom},thickness,color);
    segment({b.left,b.bottom},{b.left,b.top},thickness,color);
}
bool playerBox(void* ped,Vec center,Box& box){
    // Head, pelvis, hands and feet. Native bone tag IDs, not skin matrix indices.
    constexpr unsigned bones[]{5,1,24,34,44,54};Point points[6]{};
    void* hierarchy=hierarchyFromClump(read<void*>(ped,32));
    if(!hierarchy)return false;
    int count=read<int>(hierarchy,4);if(count<1||count>256)return false;
    for(int i=0;i<6;++i){
        int index=boneIndex(hierarchy,int(bones[i]));if(index<0||index>=count)return false;
        Vec world{};bonePosition(ped,world,bones[i],false);
        if(!esp::finite(world))return false;
        float dx=world.x-center.x,dy=world.y-center.y,dz=world.z-center.z;
        // Reject an unloaded/stale skeleton instead of drawing a huge box at map origin.
        if(dx*dx+dy*dy+dz*dz>25.f)return false;
        if(i==0)world.z+=.20f;
        if(i>=4)world.z-=.07f;
        Vec out{};float sx=0,sy=0;
        // Check the near plane; allow distant streamed players, up to our own range.
        if(!project(world,&out,&sx,&sy,false,true)||!esp::finite(out)||out.z<=.05f)return false;
        points[i]={out.x,out.y};
    }
    if(!esp::makeBox(points,6,box))return false;
    return box.right>=0&&box.left<width&&box.bottom>=0&&box.top<height;
}
void draw(){
    original2D();
    if(!sampReady){
        if(retryFrames++%120==0){dl_iterate_phdr(findSamp,&sampReady);if(sampReady)log("Alyn client build verified; player boxes and lines enabled");}
        if(!sampReady)return;
    }
    void* self=localPed(-1);Pool* pool=*poolPtr;
    if(!self||!pool||!pool->storage||!pool->flags||pool->size<1||pool->size>100000)return;
    width=read<int>(screen,8);height=read<int>(screen,12);
    if(width<1||height<1||width>32768||height>32768)return;
    Vec origin=position(self);if(!esp::finite(origin))return;
    int drawn=0;
    for(int i=0;i<pool->size&&drawn<maxDrawn;++i){
        uint8_t flag=pool->flags[i];if(flag&0x80)continue;
        // Access via CPools, retaining the allocation handling provided by FLA.
        void* ped=getPed((i<<8)|flag);
        if(!ped||ped==self||!isPlayer(ped)||!read<void*>(ped,32))continue;
        float health=read<float>(ped,1708);
        if(!std::isfinite(health)||health<=0)continue;
        if(read<uint8_t>(ped,75)!=read<uint8_t>(self,75))continue;
        Vec center=position(ped);if(!esp::finite(center))continue;
        float dx=center.x-origin.x,dy=center.y-origin.y,dz=center.z-origin.z;
        if(dx*dx+dy*dy+dz*dz>range*range)continue;
        Box box{};if(!playerBox(ped,center,box))continue;
        Point a{width*.5f,height-3.f},b{(box.left+box.right)*.5f,box.bottom};
        float scale=std::max(.8f,std::min(2.5f,height/720.f));
        segment(a,b,3.5f*scale,outline);boxEdges(box,3.8f*scale,outline);
        segment(a,b,1.5f*scale,green);boxEdges(box,1.8f*scale,green);++drawn;
    }
}
}
extern "C" JNIEXPORT void OnAllModsLoaded(){
    if(!aml)return;
    void* handle=aml->GetLibHandle("libGTASA.so");
    if(!handle){log("GTA library missing; disabled");return;}
    const char* names[]{"_Z13Render2dStuffv","_Z13FindPlayerPedi","_ZN6CPools6GetPedEi","_ZNK4CPed8IsPlayerEv",
        "_ZN4CPed15GetBonePositionER5RwV3djb","_ZN7CSprite15CalcScreenCoorsERK5RwV3dPS0_PfS4_bb",
        "_ZN9CSprite2d13Draw2DPolygonEffffffffRK5CRGBA","_ZN6CPools11ms_pPedPoolE","RsGlobal",
        "_Z29GetAnimHierarchyFromSkinClumpP7RpClump","_Z17RpHAnimIDGetIndexP16RpHAnimHierarchyi"};
    constexpr uintptr_t offsets[]{0x4d8994,0x4efae0,0x4f2378,0x595400,0x59aee4,0x6e9df8,0x6ee4a4,0xbc3bb8,0xc9b320,0x6f54cc,0x255be8};
    uintptr_t symbols[11]{};
    for(int i=0;i<11;++i){symbols[i]=aml->GetSym(handle,names[i]);if(!symbols[i]){log("Required GTA symbol missing; disabled");return;}}
    uintptr_t base=symbols[0]-offsets[0];
    for(int i=1;i<11;++i)if(symbols[i]-base!=offsets[i]){log("Unsupported GTA library layout; disabled");return;}
    localPed=reinterpret_cast<decltype(localPed)>(symbols[1]);getPed=reinterpret_cast<decltype(getPed)>(symbols[2]);
    isPlayer=reinterpret_cast<decltype(isPlayer)>(symbols[3]);bonePosition=reinterpret_cast<decltype(bonePosition)>(symbols[4]);
    project=reinterpret_cast<decltype(project)>(symbols[5]);polygon=reinterpret_cast<decltype(polygon)>(symbols[6]);
    poolPtr=reinterpret_cast<Pool**>(symbols[7]);screen=reinterpret_cast<uint8_t*>(symbols[8]);
    hierarchyFromClump=reinterpret_cast<decltype(hierarchyFromClump)>(symbols[9]);boneIndex=reinterpret_cast<decltype(boneIndex)>(symbols[10]);
    if(!aml->Hook(reinterpret_cast<void*>(symbols[0]),reinterpret_cast<void*>(draw),reinterpret_cast<void**>(&original2D))){log("2D render hook failed; disabled");return;}
    log("Alyn ESP 0.1.0 ready; awaiting matching loaded multiplayer client");
}
