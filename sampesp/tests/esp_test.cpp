#include "../main.cpp"
#include <cassert>
#include <iostream>
#include <limits>
#include <vector>
extern "C" int __android_log_write(int,const char*,const char*){return 0;}
namespace {
alignas(8) uint8_t peds[8][2456]{},fakeScreen[80]{};
uint8_t flags[8]{};Pool pool{peds,flags,8,0,false,false};Pool* poolValue=&pool;
int renderCalls=0,quads=0;
int hierarchyData[2]{0,64};bool missingBone=false;
void* hierarchyMock(void*){return hierarchyData;}
int indexMock(void*,int tag){return missingBone?-1:tag;}
std::vector<Color> colors;
template<class T>void put(void* p,size_t off,T v){std::memcpy(static_cast<uint8_t*>(p)+off,&v,sizeof(v));}
void originalMock(){++renderCalls;}
void* selfMock(int n){assert(n==-1);return peds[0];}
void* pedMock(int handle){int i=handle>>8;assert(i>=0&&i<8);assert((handle&255)==flags[i]);return peds[i];}
bool playerMock(const void* p){auto type=read<uint32_t>(p,1824);return type==0||type==1;}
void boneMock(void* ped,Vec& out,unsigned tag,bool update){
    assert(!update);out=position(ped);
    switch(tag){case 5:out.z+=.8f;break;case 1:break;
        case 24:out.x+=.3f;break;case 34:out.x-=.3f;break;
        case 44:out.x-=.15f;out.z-=.9f;break;case 54:out.x+=.15f;out.z-=.9f;break;
        default:assert(false);}
}
bool projectMock(const Vec& v,Vec* out,float* sx,float* sy,bool far,bool near){
    assert(!far&&near);if(v.y<1)return false;
    *out={640+v.x*50,360-v.z*50,v.y};*sx=*sy=1;return true;
}
void polygonMock(float x0,float y0,float x1,float y1,float x2,float y2,float x3,float y3,const Color& c){
    for(float x:{x0,x1,x2,x3})assert(std::isfinite(x)&&x>=-10&&x<=width+10);
    for(float y:{y0,y1,y2,y3})assert(std::isfinite(y)&&y>=-10&&y<=height+10);
    ++quads;colors.push_back(c);
}
void setup(){
    for(int i=0;i<8;++i){
        put(peds[i],8,Vec{float(i),10,0});put(peds[i],32,static_cast<void*>(peds[i]));
        put(peds[i],1708,100.f);put(peds[i],1824,uint32_t(4));flags[i]=0;
    }
    put(peds[0],1824,uint32_t(0)); // local player, always excluded
    put(peds[1],1824,uint32_t(0)); // streamed remote player
    put(peds[2],1824,uint32_t(1));put(peds[2],1708,0.f); // dead remote
    put(peds[3],1824,uint32_t(1));put(peds[3],8,Vec{300,10,0}); // distant
    put(peds[4],1824,uint32_t(0));flags[4]=0x80; // freed slot
    put(peds[5],1824,uint32_t(0));put(peds[5],75,uint8_t(1)); // other interior
    // index 6 is an ordinary NPC
    put(peds[7],1824,uint32_t(0));put(peds[7],8,Vec{0,-10,0}); // behind camera
}
}
int main(){
    hierarchyFromClump=hierarchyMock;boneIndex=indexMock;original2D=originalMock;localPed=selfMock;getPed=pedMock;isPlayer=playerMock;bonePosition=boneMock;project=projectMock;polygon=polygonMock;
    poolPtr=&poolValue;screen=fakeScreen;put(screen,8,1280);put(screen,12,720);
    setup();sampReady=true;draw();assert(renderCalls==1&&quads==12);
    assert(colors.size()==12&&colors.front().r==0&&colors[9].g==255);
    FrameStats rejected;currentStats=&rejected;Box rejectedBox{};
    missingBone=true;assert(!playerBox(peds[1],position(peds[1]),rejectedBox));
    assert(rejected.missingBone==1&&rejected.missingTag==5&&rejected.missingIndex==-1);currentStats=nullptr;
    quads=0;draw();assert(quads==0);missingBone=false;
    // No client: render stays transparent and no ped pool access is required.
    sampReady=false;retryFrames=1;quads=0;draw();assert(quads==0);sampReady=true;
    // NPCs, dead players, the local player, freed slots and other interiors stay excluded.
    put(peds[1],1824,uint32_t(4));quads=0;draw();assert(quads==0);
    put(peds[1],1824,uint32_t(1));put(peds[1],1708,std::numeric_limits<float>::quiet_NaN());draw();assert(quads==0);
    // Remote in a vehicle/animation is still eligible; skeleton determines the box.
    setup();put(peds[1],1800,static_cast<void*>(peds[2]));draw();assert(quads==12);
    // Clip partially visible boxes and lines without sending huge coordinates to RenderWare.
    Point a{-10000,100},b{10000,100};assert(esp::clip(a,b,1280,720));assert(std::fabs(a.x)<.01f&&std::fabs(b.x-1280)<.01f);
    a={-20,-10};b={-10,300};assert(!esp::clip(a,b,1280,720));
    a={std::numeric_limits<float>::quiet_NaN(),0};b={1,2};assert(!esp::clip(a,b,1280,720));
    quads=0;put(peds[1],8,Vec{-12.7f,10,0});draw();assert(quads>0&&quads<=12);
    // Exact client build ID is checked through the loaded ELF note, without dlopen.
    alignas(8) uint8_t notes[36]{};put(notes,0,ElfW(Nhdr){4,20,NT_GNU_BUILD_ID});std::memcpy(notes+12,"GNU\0",4);
    const uint8_t build[]{0xa8,0x61,0x94,0x42,0x04,0xc6,0x30,0x9b,0xb4,0x33,0xb7,0xe2,0xbe,0x8a,0x2f,0x13,0xc1,0x37,0xae,0x66};std::memcpy(notes+16,build,20);
    ElfW(Phdr) phdr{};phdr.p_type=PT_NOTE;phdr.p_memsz=36;
    dl_phdr_info info{};info.dlpi_name="/test/libAlyn_SAMPMOBILE.so";info.dlpi_addr=reinterpret_cast<uintptr_t>(notes);info.dlpi_phnum=1;info.dlpi_phdr=&phdr;
    bool match=false;assert(findSamp(&info,sizeof(info),&match)==1&&match);
    notes[16]^=1;match=false;assert(findSamp(&info,sizeof(info),&match)==0&&!match);
    notes[16]^=1;put(notes,4,uint32_t(0xffffffff));assert(findSamp(&info,sizeof(info),&match)==0&&!match);
    // Health stays separate from synthetic CPed HP and toggles control real draw calls.
    setup();int remoteMarker=0;
    features::healthCache.update({&remoteMarker,peds[1],42,25,10,diagnostics::millis()});
    quads=0;colors.clear();draw();assert(quads==13&&colors.back().r==240&&colors.back().g==45);
    options::boxEnabled=false;options::lineEnabled=false;quads=0;draw();assert(quads==0);
    options::lineEnabled=true;draw();assert(quads==2);
    options::boxEnabled=true;options::lineEnabled=false;quads=0;draw();assert(quads==11);
    std::cout<<"ESP tests passed: player filter, skeleton boxes, lines, clipping and client build check\n";
}
