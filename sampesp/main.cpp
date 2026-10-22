#include <mod/amlmod.h>
#include <android/log.h>
#include <link.h>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "overlay_math.h"
#include "diagnostics.h"
#include "client_features.h"
#include "sniper_aim.h"
#include "render_hook.h"
#include "sniper_penetration.h"
#include "sniper_cadence.h"

MYMOD(local.sampesp.alyn.arm64, Alyn Player ESP ARM64, 0.3.6, Custom)
// AML calls below exist since 1.0.1 or earlier; allow the reported AML 1.3.1.
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.3.1)
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
uintptr_t sampBase=0;
unsigned retryFrames=0;
float width=0,height=0;
template<class T>T read(const void* p,size_t off){T v;std::memcpy(&v,static_cast<const uint8_t*>(p)+off,sizeof(v));return v;}
diagnostics::Log fileLog;
std::atomic<uint64_t> renderFrames{0};
std::atomic<const char*> phase{"not_initialized"};
class Watchdog {
    std::mutex mutex;std::condition_variable cv;bool stopping=false;std::thread worker;
public:
    void start(){
        if(worker.joinable())return;
        worker=std::thread([this]{
            uint64_t previous=renderFrames.load();
            std::unique_lock<std::mutex> lock(mutex);
            while(!cv.wait_for(lock,std::chrono::seconds(5),[this]{return stopping;})){
                uint64_t frames=renderFrames.load();const char* status=phase.load();
                lock.unlock();
                fileLog.write("WATCHDOG phase=%s render_frames=%llu frames_last_5s=%llu%s",status,
                    (unsigned long long)frames,(unsigned long long)(frames-previous),
                    frames==previous?" (no render callback: game/menu state or hook must be checked)":"");
                previous=frames;lock.lock();
            }
        });
    }
    void stop(){
        {std::lock_guard<std::mutex> lock(mutex);stopping=true;}cv.notify_all();
        if(worker.joinable())worker.join();
    }
    ~Watchdog(){stop();}
} watchdog;
void log(const char* s){fileLog.write("%s",s);}
void initializeLog(){
    if(!fileLog.filename().empty())return;
    if(aml){
        const char* external=aml->GetAndroidDataPath();
        if(!fileLog.openDirectory(external)){
            const char* config=aml->GetConfigPath();
            if(!fileLog.openDirectory(config))fileLog.openDirectory(aml->GetDataPath());
        }
    }
    auto path=fileLog.filename();
    fileLog.write("===== Alyn ESP 0.3.6 diagnostic session; ABI=arm64-v8a log=%s =====",path.empty()?"LOGCAT ONLY (no writable AML path)":path.c_str());
}
struct FrameStats {
    const char* reason="scanned";
    int slots=0,free=0,nullPed=0,self=0,notPlayer=0,players=0,noRw=0,dead=0,area=0,range=0,badPos=0;
    int noHierarchy=0,badHierarchy=0,missingBone=0,badBonePos=0,staleBone=0,projection=0,badBox=0,offscreen=0,drawn=0,quads=0;
    int missingTag=-1,missingIndex=-1,nodeCount=0,types[8]{};
    float failedDepth=0;int syncedHealth=0,unknownHealth=0;
};
FrameStats* currentStats=nullptr;
uint64_t lastReport=0;
struct FrameReport {
    FrameStats stats;
    FrameReport(){currentStats=&stats;}
    ~FrameReport(){
        currentStats=nullptr;
        uint64_t now=diagnostics::millis();if(lastReport&&now-lastReport<5000)return;lastReport=now;
        fileLog.write("FRAME reason=%s screen=%.0fx%.0f slots=%d free=%d null=%d self=%d not_player=%d players=%d no_rw=%d dead=%d other_area=%d too_far=%d bad_position=%d drawn=%d polygon_calls=%d",
            stats.reason,width,height,stats.slots,stats.free,stats.nullPed,stats.self,stats.notPlayer,stats.players,stats.noRw,stats.dead,stats.area,stats.range,stats.badPos,stats.drawn,stats.quads);
        fileLog.write("FEATURES box=%d line=%d health=%d synced_health=%d unknown_health=%d ui=removed",int(options::boxEnabled.load()),int(options::lineEnabled.load()),int(options::healthEnabled.load()),stats.syncedHealth,stats.unknownHealth);
        fileLog.write("CADENCE hooks=%u shots_adjusted=%llu original_ms=%u reduced_ms=%u",cadence::installed,(unsigned long long)cadence::adjusted.load(),cadence::lastOriginalMs.load(),cadence::lastReducedMs.load());
        fileLog.write("PENETRATION installed=%d enabled=%d bullet_rays=%llu filtered_local_sniper=%llu filtered_hits=%llu",int(penetration::installed),int(options::penetrationEnabled.load()),(unsigned long long)penetration::rays.load(),(unsigned long long)penetration::filtered.load(),(unsigned long long)penetration::hits.load());
        fileLog.write("SNIPER installed=%d enabled=%d zoom=%d scoped=%d mode=%d weapon=%d target_id=%d fov=%.3f",int(sniper::installed),int(options::aimEnabled.load()),int(options::zoomEnabled.load()),int(sniper::scoped.load()),sniper::reportedMode.load(),sniper::reportedWeapon.load(),sniper::lockedId.load(),sniper::reportedFov.load());
        fileLog.write("FILTER no_hierarchy=%d bad_hierarchy=%d missing_bone=%d sample_tag=%d sample_index=%d sample_node_count=%d invalid_bone_position=%d stale_bone=%d projection_failed=%d sample_depth=%.3f invalid_box=%d offscreen=%d ped_types_0_to_6=%d,%d,%d,%d,%d,%d,%d other_types=%d",
            stats.noHierarchy,stats.badHierarchy,stats.missingBone,stats.missingTag,stats.missingIndex,stats.nodeCount,stats.badBonePos,stats.staleBone,stats.projection,stats.failedDepth,stats.badBox,stats.offscreen,
            stats.types[0],stats.types[1],stats.types[2],stats.types[3],stats.types[4],stats.types[5],stats.types[6],stats.types[7]);
    }
};
int inventory(dl_phdr_info* info,size_t,void*){
    const char* p=info->dlpi_name;
    if(p&&(std::strstr(p,"GTASA")||std::strstr(p,"SAMP")||std::strstr(p,"samp")||std::strstr(p,"Alyn")||std::strstr(p,"aml")||std::strstr(p,"AML")))
        fileLog.write("MODULE path=%s base=0x%llx",p,(unsigned long long)info->dlpi_addr);
    return 0;
}
Vec position(const void* ped){void* m=read<void*>(ped,24);return m?read<Vec>(m,48):read<Vec>(ped,8);}
// Only inspect loaded modules. Do not dlopen the multiplayer client as a side effect.
int findSamp(dl_phdr_info* info,size_t,void* result){
    const char* path=info->dlpi_name;if(!path)return 0;
    const char* name=std::strrchr(path,'/');name=name?name+1:path;
    if(std::strcmp(name,"libAlyn_SAMPMOBILE.so"))return 0;
    static bool reported=false;
    if(!reported)fileLog.write("CLIENT found path=%s base=0x%llx",path,(unsigned long long)info->dlpi_addr);
    constexpr uint8_t expected[]{0xa8,0x61,0x94,0x42,0x04,0xc6,0x30,0x9b,0xb4,0x33,0xb7,0xe2,0xbe,0x8a,0x2f,0x13,0xc1,0x37,0xae,0x66};
    for(int i=0;i<info->dlpi_phnum;++i){
        const auto& segment=info->dlpi_phdr[i];if(segment.p_type!=PT_NOTE||segment.p_memsz>65536)continue;
        auto* data=reinterpret_cast<const uint8_t*>(info->dlpi_addr+segment.p_vaddr);
        size_t off=0;
        while(off+sizeof(ElfW(Nhdr))<=segment.p_memsz){
            auto note=read<ElfW(Nhdr)>(data,off);off+=sizeof(note);
            size_t nameSize=(size_t(note.n_namesz)+3)&~size_t(3),descSize=(size_t(note.n_descsz)+3)&~size_t(3);
            if(nameSize>segment.p_memsz-off||descSize>segment.p_memsz-off-nameSize)break;
            if(!reported&&note.n_type==NT_GNU_BUILD_ID&&note.n_namesz==4&&note.n_descsz<=64&& !std::memcmp(data+off,"GNU\0",4)){
                char actual[129]{};for(unsigned j=0;j<note.n_descsz;++j)std::snprintf(actual+j*2,3,"%02x",data[off+nameSize+j]);
                fileLog.write("CLIENT build_id=%s expected=a861944204c6309bb433b7e2be8a2f13c137ae66",actual);
            }
            if(note.n_type==NT_GNU_BUILD_ID&&note.n_namesz==4&&note.n_descsz==sizeof(expected)&&!std::memcmp(data+off,"GNU\0",4)&&!std::memcmp(data+off+nameSize,expected,sizeof(expected))){
                *static_cast<bool*>(result)=true;sniper::clientReady=true;sampBase=info->dlpi_addr;reported=true;return 1;
            }
            off+=nameSize+descSize;
        }
    }
    if(!reported)log("CLIENT rejected: build ID missing, malformed or different; drawing remains disabled");
    reported=true;return 0;
}
void segment(Point a,Point b,float thickness,Color color){
    // Clip before constructing a quad so partially visible targets stay bounded.
    if(!esp::clip(a,b,width-1,height-1))return;
    float dx=b.x-a.x,dy=b.y-a.y,len=std::sqrt(dx*dx+dy*dy);
    if(!std::isfinite(len)||len<.5f)return;
    float nx=-dy/len*thickness*.5f,ny=dx/len*thickness*.5f;
    // Static Draw2DPolygon, not the instance DrawTriangle method.
    if(currentStats)++currentStats->quads;
    polygon(a.x+nx,a.y+ny,a.x-nx,a.y-ny,b.x+nx,b.y+ny,b.x-nx,b.y-ny,color);
}
void boxEdges(Box b,float thickness,Color color){
    segment({b.left,b.top},{b.right,b.top},thickness,color);
    segment({b.right,b.top},{b.right,b.bottom},thickness,color);
    segment({b.right,b.bottom},{b.left,b.bottom},thickness,color);
    segment({b.left,b.bottom},{b.left,b.top},thickness,color);
}
void rectangle(float left,float top,float right,float bottom,Color color){
    left=std::max(0.f,left);top=std::max(0.f,top);right=std::min(width-1,right);bottom=std::min(height-1,bottom);
    if(right<=left||bottom<=top)return;
    if(currentStats)++currentStats->quads;
    polygon(left,top,right,top,left,bottom,right,bottom,color);
}
void drawHealth(Box box,bool known,float value,float scale){
    float right=box.left-4*scale,left=right-4*scale;
    rectangle(left-scale,box.top-scale,right+scale,box.bottom+scale,{0,0,0,220});
    rectangle(left,box.top,right,box.bottom,{75,75,75,220});
    if(known){
        float fill=health::fraction(value);auto color=health::color(value);
        rectangle(left,box.bottom-(box.bottom-box.top)*fill,right,box.bottom,{color.r,color.g,color.b,color.a});
    }
}
bool playerBox(void* ped,Vec center,Box& box){
    // Head, pelvis, hands and feet. Native bone tag IDs, not skin matrix indices.
    constexpr unsigned bones[]{5,1,24,34,44,54};Point points[6]{};
    void* hierarchy=hierarchyFromClump(read<void*>(ped,32));
    if(!hierarchy){if(currentStats)++currentStats->noHierarchy;return false;}
    int count=read<int>(hierarchy,4);if(count<1||count>256){if(currentStats){++currentStats->badHierarchy;currentStats->nodeCount=count;}return false;}
    for(int i=0;i<6;++i){
        int index=boneIndex(hierarchy,int(bones[i]));if(index<0||index>=count){if(currentStats){++currentStats->missingBone;currentStats->missingTag=int(bones[i]);currentStats->missingIndex=index;currentStats->nodeCount=count;}return false;}
        Vec world{};bonePosition(ped,world,bones[i],false);
        if(!esp::finite(world)){if(currentStats)++currentStats->badBonePos;return false;}
        float dx=world.x-center.x,dy=world.y-center.y,dz=world.z-center.z;
        // Reject an unloaded/stale skeleton instead of drawing a huge box at map origin.
        if(dx*dx+dy*dy+dz*dz>25.f){if(currentStats)++currentStats->staleBone;return false;}
        if(i==0)world.z+=.20f;
        if(i>=4)world.z-=.07f;
        Vec out{};float sx=0,sy=0;
        // Check the near plane; allow distant streamed players, up to our own range.
        if(!project(world,&out,&sx,&sy,false,true)||!esp::finite(out)||out.z<=.05f){if(currentStats){++currentStats->projection;currentStats->failedDepth=out.z;}return false;}
        points[i]={out.x,out.y};
    }
    if(!esp::makeBox(points,6,box)){if(currentStats)++currentStats->badBox;return false;}
    bool visible=box.right>=0&&box.left<width&&box.bottom>=0&&box.top<height;
    if(!visible&&currentStats)++currentStats->offscreen;
    return visible;
}
void draw(){
    ++renderFrames;
    phase="calling_original_2d";
    if(!original2D){log("RENDER original callback missing; drawing skipped");return;}
    original2D();
    phase="original_2d_returned";
    FrameReport report;auto& stats=report.stats;
    if(!sampReady){
        phase="waiting_for_client";
        if(retryFrames++%120==0){
            dl_iterate_phdr(findSamp,&sampReady);
            if(sampReady){phase="active";log("Alyn client build verified; player boxes and lines enabled");}
        }
        if(!sampReady){
            static uint64_t lastInventory=0;uint64_t now=diagnostics::millis();
            if(!lastInventory||now-lastInventory>=15000){lastInventory=now;dl_iterate_phdr(inventory,nullptr);}
            stats.reason="client_not_verified";return;
        }
    }
    phase="active";
    if(!features::attempted&&sampBase)features::install(aml,sampBase,log);
    void* self=localPed(-1);Pool* pool=*poolPtr;
    if(!self){stats.reason="local_player_missing";return;}
    if(!pool||!pool->storage||!pool->flags||pool->size<1||pool->size>100000){stats.reason="ped_pool_missing_or_invalid";return;}
    stats.slots=pool->size;
    width=read<int>(screen,8);height=read<int>(screen,12);
    if(width<1||height<1||width>32768||height>32768){stats.reason="invalid_screen_size";return;}
    Vec origin=position(self);if(!esp::finite(origin)){stats.reason="invalid_local_position";return;}
    int drawn=0;
    for(int i=0;i<pool->size&&drawn<maxDrawn;++i){
        uint8_t flag=pool->flags[i];if(flag&0x80){++stats.free;continue;}
        void* ped=getPed((i<<8)|flag);
        if(!ped){++stats.nullPed;continue;}
        if(ped==self){++stats.self;continue;}
        uint32_t type=read<uint32_t>(ped,1824);++stats.types[type<7?type:7];
        if(!isPlayer(ped)){++stats.notPlayer;continue;}++stats.players;
        if(!read<void*>(ped,32)){++stats.noRw;continue;}
        float health=read<float>(ped,1708);
        if(!std::isfinite(health)||health<=0){++stats.dead;continue;}
        if(read<uint8_t>(ped,75)!=read<uint8_t>(self,75)){++stats.area;continue;}
        Vec center=position(ped);if(!esp::finite(center)){++stats.badPos;continue;}
        float dx=center.x-origin.x,dy=center.y-origin.y,dz=center.z-origin.z;
        if(dx*dx+dy*dy+dz*dz>range*range){++stats.range;continue;}
        health::Sample hp{};bool known=features::healthCache.get(ped,diagnostics::millis(),hp);
        if(known)++stats.syncedHealth;else ++stats.unknownHealth;
        bool boxes=options::boxEnabled.load(),lines=options::lineEnabled.load();
        if(!boxes&&!lines){stats.reason="overlay_disabled";continue;}
        Box box{};if(!playerBox(ped,center,box))continue;
        Point a{width*.5f,height-3.f},b{(box.left+box.right)*.5f,box.bottom};
        float scale=std::max(.8f,std::min(2.5f,height/720.f));
        if(lines)segment(a,b,3.5f*scale,outline);
        if(boxes)boxEdges(box,3.8f*scale,outline);
        if(lines)segment(a,b,1.5f*scale,green);
        if(boxes){boxEdges(box,1.8f*scale,green);if(options::healthEnabled.load())drawHealth(box,known,hp.hp,scale);}
        ++drawn;++stats.drawn;
    }
}
}
extern "C" JNIEXPORT void OnModLoad(){initializeLog();log("CALLBACK OnModLoad reached");}
extern "C" JNIEXPORT void OnAllModsLoaded(){
    initializeLog();phase="initializing";watchdog.start();log("CALLBACK OnAllModsLoaded reached");
    dl_iterate_phdr(inventory,nullptr);
    if(!aml){phase="aml_interface_missing";log("AML interface missing; disabled");return;}
    void* handle=aml->GetLibHandle("libGTASA.so");
    if(!handle){phase="gta_library_missing";log("GTA library missing; disabled");return;}
    const char* names[]{"_Z13Render2dStuffv","_Z13FindPlayerPedi","_ZN6CPools6GetPedEi","_ZNK4CPed8IsPlayerEv",
        "_ZN4CPed15GetBonePositionER5RwV3djb","_ZN7CSprite15CalcScreenCoorsERK5RwV3dPS0_PfS4_bb",
        "_ZN9CSprite2d13Draw2DPolygonEffffffffRK5CRGBA","_ZN6CPools11ms_pPedPoolE","RsGlobal",
        "_Z29GetAnimHierarchyFromSkinClumpP7RpClump","_Z17RpHAnimIDGetIndexP16RpHAnimHierarchyi"};
    constexpr uintptr_t offsets[]{0x4d8994,0x4efae0,0x4f2378,0x595400,0x59aee4,0x6e9df8,0x6ee4a4,0xbc3bb8,0xc9b320,0x6f54cc,0x255be8};
    uintptr_t symbols[11]{};
    for(int i=0;i<11;++i){symbols[i]=aml->GetSym(handle,names[i]);if(!symbols[i]){phase="required_symbol_missing";fileLog.write("Required GTA symbol missing: %s; disabled",names[i]);return;}}
    uintptr_t base=symbols[0]-offsets[0];
    for(int i=0;i<11;++i)fileLog.write("SYMBOL %s address=0x%llx offset=0x%llx expected=0x%llx",names[i],(unsigned long long)symbols[i],(unsigned long long)(symbols[i]-base),(unsigned long long)offsets[i]);
    for(int i=1;i<11;++i)if(symbols[i]-base!=offsets[i]){phase="gta_layout_mismatch";fileLog.write("Unsupported GTA library layout at %s; disabled",names[i]);return;}
    localPed=reinterpret_cast<decltype(localPed)>(symbols[1]);getPed=reinterpret_cast<decltype(getPed)>(symbols[2]);
    isPlayer=reinterpret_cast<decltype(isPlayer)>(symbols[3]);bonePosition=reinterpret_cast<decltype(bonePosition)>(symbols[4]);
    project=reinterpret_cast<decltype(project)>(symbols[5]);polygon=reinterpret_cast<decltype(polygon)>(symbols[6]);
    poolPtr=reinterpret_cast<Pool**>(symbols[7]);screen=reinterpret_cast<uint8_t*>(symbols[8]);
    hierarchyFromClump=reinterpret_cast<decltype(hierarchyFromClump)>(symbols[9]);boneIndex=reinterpret_cast<decltype(boneIndex)>(symbols[10]);
    uint32_t call=read<uint32_t>(reinterpret_cast<void*>(base+renderhook::callOffset),0);
    uint64_t entry0=read<uint64_t>(reinterpret_cast<void*>(symbols[0]),0),entry8=read<uint64_t>(reinterpret_cast<void*>(symbols[0]),8),entry16=read<uint64_t>(reinterpret_cast<void*>(symbols[0]),16);
    fileLog.write("RENDER install=Idle_BL call_offset=0x4d9108 instruction=0x%08x entry_words=%016llx,%016llx,%016llx",call,(unsigned long long)entry0,(unsigned long long)entry8,(unsigned long long)entry16);
    if(!renderhook::install(aml,base,reinterpret_cast<void*>(draw),reinterpret_cast<void**>(&original2D))){phase="render_hook_failed";log("RENDER Idle BL hook failed or call changed; no entry-hook fallback; disabled");return;}
    fileLog.write("RENDER Idle BL installed; original=0x%llx; Render2dStuff entry left untouched",(unsigned long long)reinterpret_cast<uintptr_t>(original2D));
    sniper::install(aml,handle,base,log);
    penetration::install(aml,handle,base,log);
    cadence::install(aml,handle,base,log);
    phase="hook_installed";
    log("Alyn ESP 0.3.6 ready; hook installed; waiting for first render callback and matching client");
}

extern "C" JNIEXPORT void OnModUnload(){watchdog.stop();log("CALLBACK OnModUnload reached");}
