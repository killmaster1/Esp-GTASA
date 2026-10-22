#include <mod/amlmod.h>
#include <android/log.h>
#include <cstdint>
#include <cstring>
#include "route_math.h"

MYMOD(local.roadarrows.sa210, Road Arrows SA210 ARM64, 0.1.0, Custom)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.4.0)
END_DEPLIST()

namespace {
using road::Vec;
using road::Bounds;
// Layouts from the supplied GTA SA 2.10 ARM64 DWARF, not PC structures.
struct NodeAddress {uint16_t region,index;};
struct Vertex {Vec position,normal;uint8_t r,g,b,a;float u,v;};
static_assert(sizeof(void*)==8 && sizeof(Vec)==12 && sizeof(NodeAddress)==4,"ARM64 GTA ABI");
static_assert(sizeof(Vertex)==36 && offsetof(Vertex,r)==24,"RxObjSpace3DVertex ABI");
constexpr int capacity=4096;
constexpr NodeAddress empty{0xffff,0xffff};
uint8_t* paths=nullptr;
uint8_t* radar=nullptr;
uint32_t* clockMs=nullptr;
void (*originalEffects)()=nullptr;
void* (*findVehicle)(int,bool)=nullptr;
void (*search)(void*,uint8_t,Vec,NodeAddress,Vec,NodeAddress*,int16_t*,int,float*,float,NodeAddress*,float,bool,NodeAddress,bool,bool)=nullptr;
void (*requestNodes)(void*,float,float,float,float)=nullptr;
bool (*nodesReady)(void*)=nullptr;
void (*releaseNodes)(void*)=nullptr;
float (*groundZ)(float,float,float,bool*,void**)=nullptr;
void* (*transform)(Vertex*,unsigned,void*,unsigned)=nullptr;
int (*renderPrimitive)(int)=nullptr;
int (*end3D)()=nullptr;
int (*stateGet)(int,void*)=nullptr;
int (*stateSet)(int,void*)=nullptr;
Vec route[capacity]{};
NodeAddress addresses[capacity]{};
Vertex vertices[96]{};
int routeCount=0,vertexCount=0;
uint32_t routeTime=0,meshTime=0;
Vec destination{},routeOrigin{};
int marker=-1;
bool haveRouteTime=false,haveMeshTime=false,ownsRequest=false,arrived=false;
Bounds owned{};

template<class T>T read(const void* p,size_t off){T v;std::memcpy(&v,static_cast<const uint8_t*>(p)+off,sizeof(v));return v;}
void log(const char* s){__android_log_write(ANDROID_LOG_INFO,"RoadArrows",s);}
Vec position(void* p){void* m=read<void*>(p,24);return m?read<Vec>(m,48):read<Vec>(p,8);}
Bounds currentRequest(){return read<Bounds>(paths,19604);}
bool ownsCurrentRequest(){return ownsRequest&&read<bool>(paths,19600)&&road::same(owned,currentRequest());}
void relinquish(){if(ownsCurrentRequest())releaseNodes(paths);ownsRequest=false;}
void reset(){relinquish();routeCount=vertexCount=0;marker=-1;haveRouteTime=haveMeshTime=false;arrived=false;}
int waypoint(Vec& target){
    for(int i=0;i<250;++i){
        const uint8_t* trace=radar+i*48;
        // sRadarTrace: sprite 40, tracking bit 1 of byte 41, display bits 0..1 of byte 42.
        if(trace[40]!=41 || !(trace[41]&2) || !(trace[42]&3))continue;
        target=read<Vec>(trace,8);
        if(road::valid(target))return i;
    }return -1;
}
bool nodePosition(NodeAddress a,Vec& out){
    if(a.region>=64)return false;
    uint32_t count=read<uint32_t>(paths,8136+a.region*4);
    auto* nodes=read<uint8_t*>(paths,4104+a.region*8);
    if(!nodes || a.index>=count || count>65535)return false;
    const uint8_t* node=nodes+a.index*40;
    if(read<uint16_t>(node,26)!=a.region||read<uint16_t>(node,28)!=a.index)return false;
    // Coordinates are signed fixed point (1/8 world unit), not full floats.
    out={read<int16_t>(node,16)/8.f,read<int16_t>(node,18)/8.f,read<int16_t>(node,20)/8.f};
    return road::valid(out);
}
void ensureNodes(Vec origin){
    if(ownsRequest && !ownsCurrentRequest())ownsRequest=false;
    if(ownsCurrentRequest()&&road::inside(owned,origin)&&road::inside(owned,destination))return;
    relinquish();
    // There is one shared streaming request slot. Missions retain priority.
    if(read<bool>(paths,19600))return;
    owned=road::bounds(origin,destination);
    requestNodes(paths,owned.minX,owned.maxX,owned.minY,owned.maxY);
    ownsRequest=true;
}
void calculate(Vec origin,uint32_t now){
    routeTime=now;haveRouteTime=true;routeOrigin=origin;routeCount=0;vertexCount=0;
    ensureNodes(origin);
    if(ownsCurrentRequest()&&!nodesReady(paths))return;
    int16_t count=0;float distance=0;
    search(paths,0,origin,empty,destination,addresses,&count,capacity,&distance,300.f,nullptr,30000.f,true,empty,false,false);
    if(count<2||count>capacity){return;}
    for(int i=0;i<count;++i){
        Vec p{};if(!nodePosition(addresses[i],p)){routeCount=0;return;}
        if(routeCount&&road::length(p-route[routeCount-1])<.05f)continue;
        route[routeCount++]=p;
    }
    if(routeCount<2){routeCount=0;return;}
    if(road::length(route[routeCount-1]-origin)<road::length(route[0]-origin))std::reverse(route,route+routeCount);
    // Do not draw a disconnected path if the nearest usable road is too far away.
    if(road::length(route[0]-origin)>300.f){routeCount=0;return;}
    log("Vehicle route updated");
}
void makeMesh(Vec origin,uint32_t now){
    meshTime=now;haveMeshTime=true;vertexCount=0;
    auto progress=road::closest(route,routeCount,origin);
    if(progress.distance>35.f)return;
    // Geometry stays anchored to fixed intervals along the road, avoiding sliding arrows.
    float start=std::ceil((progress.along+7.f)/8.f)*8.f;
    for(int arrow=0;arrow<16;++arrow){
        Vec p{},dir{};if(!road::sample(route,routeCount,start+arrow*8.f,p,dir))break;
        if(road::length(p-origin)>140.f)break;
        Vec corners[6];road::chevron(p,dir,corners);bool good=true;
        float lo=1e9f,hi=-1e9f;
        for(auto& v:corners){
            bool found=false;void* entity=nullptr;
            float z=groundZ(v.x,v.y,p.z+3.f,&found,&entity);
            // Path-node height selects the correct road deck on bridges and tunnels.
            if(!found||!std::isfinite(z)||std::fabs(z-p.z)>3.5f){good=false;break;}
            v.z=z+.09f;lo=std::min(lo,z);hi=std::max(hi,z);
        }
        if(!good||hi-lo>1.5f)continue;
        uint8_t alpha=static_cast<uint8_t>(220-arrow*5);
        for(auto v:corners)vertices[vertexCount++]={v,{0,0,1},40,220,255,alpha,0,0};
    }
}
void update(){
    uint32_t now=*clockMs;
    void* vehicle=findVehicle(-1,false);Vec target{};int index=waypoint(target);
    if(!vehicle||index<0||read<uint8_t>(vehicle,75)!=0){reset();return;}
    Vec origin=position(vehicle);
    if(!road::valid(origin)){reset();return;}
    bool changed=index!=marker||road::distance2D(target,destination)>1.f;
    if(changed){reset();marker=index;destination=target;}
    if(road::distance2D(origin,destination)<15.f){arrived=true;relinquish();routeCount=vertexCount=0;return;}
    if(arrived)return;
    if(!haveRouteTime || (uint32_t(now-routeTime)>=1500 && (routeCount<2||road::length(origin-routeOrigin)>12.f))){
        calculate(origin,now);haveMeshTime=false;
    }
    if(!haveMeshTime||uint32_t(now-meshTime)>=150)makeMesh(origin,now);
}
// Preserve every state we change, including pointer-sized texture raster values.
struct RenderStates {
    const int ids[9]{1,6,8,10,11,12,14,20,30};
    uintptr_t values[9]{};bool valid=true;
    RenderStates(){for(int i=0;i<9;++i)if(!stateGet(ids[i],&values[i]))valid=false;}
    ~RenderStates(){if(valid)for(int i=0;i<9;++i)stateSet(ids[i],reinterpret_cast<void*>(values[i]));}
};
void effects(){
    originalEffects();update();
    if(vertexCount<3)return;
    RenderStates saved;if(!saved.valid)return;
    auto state=[](int id,uintptr_t v){stateSet(id,reinterpret_cast<void*>(v));};
    state(1,0);state(6,1);state(8,0);state(10,5);state(11,6);
    state(12,1);state(14,0);state(20,1);state(30,0);
    // Native enum: XYZ=8, RGBA=16. Leave NOCLIP unset for camera clipping.
    if(transform(vertices,static_cast<unsigned>(vertexCount),nullptr,8|16)){
        renderPrimitive(3);end3D();
    }
}
}
extern "C" JNIEXPORT void OnAllModsLoaded(){
    if(!aml)return;
    void* handle=aml->GetLibHandle("libGTASA.so");
    if(!handle){log("GTA library missing; disabled");return;}
    const char* names[]{"ThePaths","_ZN6CRadar13ms_RadarTraceE","_ZN6CTimer22m_snTimeInMillisecondsE","_Z13RenderEffectsv",
        "_Z17FindPlayerVehicleib","_ZN9CPathFind12DoPathSearchEh7CVector12CNodeAddressS0_PS1_PsiPffS2_fbS1_bb",
        "_ZN9CPathFind29MakeRequestForNodesToBeLoadedEffff","_ZN9CPathFind28HaveRequestedNodesBeenLoadedEv",
        "_ZN9CPathFind21ReleaseRequestedNodesEv","_ZN6CWorld21FindGroundZFor3DCoordEfffPbPP7CEntity",
        "_Z15RwIm3DTransformP18RxObjSpace3DVertexjP11RwMatrixTagj","_Z21RwIm3DRenderPrimitive15RwPrimitiveType","_Z9RwIm3DEndv",
        "_Z16RwRenderStateGet13RwRenderStatePv","_Z16RwRenderStateSet13RwRenderStatePv"};
    constexpr uintptr_t offsets[]{0x9869c0,0xc21728,0xbdc558,0x4d88a8,0x4efdec,0x3dbc30,0x3e0858,0x3e088c,0x3db464,0x50f408,0x275d54,0x275fd8,0x275e20,0x27d6b8,0x27d668};
    uintptr_t symbols[15]{};
    for(int i=0;i<15;++i){symbols[i]=aml->GetSym(handle,names[i]);if(!symbols[i]){log("Required symbol missing; disabled");return;}}
    uintptr_t base=symbols[0]-offsets[0];
    for(int i=1;i<15;++i)if(symbols[i]-base!=offsets[i]){log("Unsupported GTA library layout; disabled");return;}
    paths=reinterpret_cast<uint8_t*>(symbols[0]);radar=reinterpret_cast<uint8_t*>(symbols[1]);clockMs=reinterpret_cast<uint32_t*>(symbols[2]);
    findVehicle=reinterpret_cast<decltype(findVehicle)>(symbols[4]);search=reinterpret_cast<decltype(search)>(symbols[5]);
    requestNodes=reinterpret_cast<decltype(requestNodes)>(symbols[6]);nodesReady=reinterpret_cast<decltype(nodesReady)>(symbols[7]);releaseNodes=reinterpret_cast<decltype(releaseNodes)>(symbols[8]);
    groundZ=reinterpret_cast<decltype(groundZ)>(symbols[9]);transform=reinterpret_cast<decltype(transform)>(symbols[10]);
    renderPrimitive=reinterpret_cast<decltype(renderPrimitive)>(symbols[11]);end3D=reinterpret_cast<decltype(end3D)>(symbols[12]);
    stateGet=reinterpret_cast<decltype(stateGet)>(symbols[13]);stateSet=reinterpret_cast<decltype(stateSet)>(symbols[14]);
    if(!aml->Hook(reinterpret_cast<void*>(symbols[3]),reinterpret_cast<void*>(effects),reinterpret_cast<void**>(&originalEffects))){log("Render hook failed; disabled");return;}
    log("Road Arrows 0.1.0 enabled: place a map waypoint, then drive");
}
