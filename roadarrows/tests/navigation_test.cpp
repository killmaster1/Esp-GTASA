#include "../main.cpp"
#include <cassert>
#include <map>
#include <limits>
#include <iostream>
extern "C" int __android_log_write(int,const char*,const char*){return 0;}
namespace {
alignas(8) uint8_t fakePaths[19624]{},fakeRadar[12000]{},fakeVehicle[128]{},fakeNodes[120]{};
uint32_t timeValue=1000;
int requests=0,releases=0,searches=0,draws=0,ends=0;
bool riding=true,ready=true,transformOk=true;
std::map<int,uintptr_t> states;
template<class T> void put(void* p,size_t off,T v){std::memcpy(static_cast<uint8_t*>(p)+off,&v,sizeof(v));}
void* vehicleMock(int p,bool remote){assert(p==-1&&!remote);return riding?fakeVehicle:nullptr;}
void requestMock(void* self,float a,float b,float c,float d){assert(self==paths);++requests;put(paths,19600,true);put(paths,19604,Bounds{a,b,c,d});}
void releaseMock(void* self){assert(self==paths);++releases;put(paths,19600,false);}
bool readyMock(void*){return ready;}
void searchMock(void* self,uint8_t type,Vec origin,NodeAddress hint,Vec target,NodeAddress* out,int16_t* n,int max,float* dist,float maxDist,NodeAddress* targetHint,float depth,bool lanes,NodeAddress forbidden,bool water,bool boats){
    assert(self==paths&&type==0&&hint.region==65535&&hint.index==65535&&max==4096);
    assert(maxDist==300&&targetHint==nullptr&&depth==30000&&lanes&&!water&&!boats&&forbidden.region==65535);
    ++searches;*n=3;*dist=100;out[0]={0,0};out[1]={0,1};out[2]={0,2};
}
float groundMock(float x,float y,float z,bool* found,void** entity){*found=true;*entity=nullptr;return 0;}
int getMock(int id,void* out){std::memcpy(out,&states[id],sizeof(uintptr_t));return 1;}
int setMock(int id,void* value){states[id]=reinterpret_cast<uintptr_t>(value);return 1;}
void originalMock(){}
void* transformMock(Vertex* v,unsigned n,void* matrix,unsigned flags){assert(n>0&&n<=96&&n%6==0&&matrix==nullptr&&flags==24);return transformOk?v:nullptr;}
int drawMock(int prim){assert(prim==3);++draws;return 1;}
int endMock(){++ends;return 1;}
void mark(Vec p){put(radar,8,p);radar[40]=41;radar[41]=2;radar[42]=2|(4<<2);}
void node(int index,Vec p){auto* n=fakeNodes+index*40;put(n,16,int16_t(p.x*8));put(n,18,int16_t(p.y*8));put(n,20,int16_t(p.z*8));put(n,26,uint16_t(0));put(n,28,uint16_t(index));}
}
int main(){
    paths=fakePaths;radar=fakeRadar;clockMs=&timeValue;
    findVehicle=vehicleMock;requestNodes=requestMock;releaseNodes=releaseMock;nodesReady=readyMock;search=searchMock;groundZ=groundMock;
    stateGet=getMock;stateSet=setMock;originalEffects=originalMock;transform=transformMock;renderPrimitive=drawMock;end3D=endMock;
    put(paths,4104,static_cast<uint8_t*>(fakeNodes));put(paths,8136,uint32_t(3));
    node(0,{0,0,0});node(1,{80,0,0});node(2,{80,80,0});
    mark({80,80,0});put(fakeVehicle,8,Vec{0,0,0});
    // Loading is asynchronous; no path search before the requested regions are ready.
    ready=false;update();assert(requests==1&&searches==0&&vertexCount==0);
    ready=true;timeValue+=1600;update();assert(searches==1&&routeCount==3&&vertexCount>0);
    // Samples go around the L-shaped road, rather than cutting diagonally across blocks.
    Vec p{},dir{};assert(road::sample(route,3,100,p,dir));assert(p.x==80&&p.y==20&&dir.y==1);
    auto progress=road::closest(route,3,{80,40,0});assert(std::fabs(progress.along-120)<.001f);
    assert(!road::sample(route,3,1000,p,dir));assert(!road::sample(route,1,0,p,dir));
    assert(!road::sample(route,3,std::numeric_limits<float>::quiet_NaN(),p,dir));
    // Terrain probing skips geometry that would jump onto a different deck.
    route[0].z=route[1].z=route[2].z=12;makeMesh({0,0,12},timeValue);assert(vertexCount==0);
    route[0].z=route[1].z=route[2].z=0;makeMesh({0,0,0},timeValue);
    for(int id:{1,6,8,10,11,12,14,20,30})states[id]=id==1?uintptr_t(0x123456789ab):uintptr_t(7+id);
    auto saved=states;effects();assert(draws==1&&ends==1&&states==saved);
    transformOk=false;effects();assert(draws==1&&ends==1&&states==saved);
    // Removal releases only the mod's own streaming request and clears the mesh immediately.
    radar[41]=0;update();assert(releases==1&&routeCount==0&&vertexCount==0);
    mark({80,80,0});put(paths,19600,true);put(paths,19604,Bounds{-10,10,-10,10});update();assert(requests==1&&!ownsRequest);
    radar[41]=0;update();assert(releases==1&&read<bool>(paths,19600));
    // Another script replacing our request must not have it cleared when navigation ends.
    put(paths,19600,false);mark({80,80,0});update();assert(requests==2&&ownsRequest);
    put(paths,19604,Bounds{-20,20,-20,20});radar[41]=0;update();assert(releases==1&&read<bool>(paths,19600));
    // Destination changes rebuild the route; arriving stops arrows even if the marker remains.
    put(paths,19600,false);mark({80,80,0});update();assert(routeCount==3);
    mark({81,81,0});update();assert(destination.x==81&&routeCount==3);
    put(fakeVehicle,8,Vec{81,81,0});update();assert(arrived&&vertexCount==0&&!ownsRequest);
    riding=false;update();assert(marker==-1&&!arrived);
    // Corrupt/unloaded node addresses are rejected before indexing the array.
    assert(!nodePosition({64,0},p));assert(!nodePosition({0,3},p));
    std::cout<<"Navigation tests passed: route, loading ownership, terrain, render state, waypoint lifecycle\n";
}
