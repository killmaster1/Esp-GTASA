#include "../main.cpp"
#include <cassert>
#include <iostream>
extern "C" int __android_log_write(int,const char*,const char*){return 0;}
namespace {
alignas(8) uint8_t fakeCamera[3504]{},fakeRw[432]{},peds[3][2456]{},display[80]{};
uint8_t flags[3]{};sniper::Pool fakePool{peds,flags,3,0,false,false};sniper::Pool* poolValue=&fakePool;
int bones[2]{0,64},calls=0;float step=1,fieldFov=70;bool in=false,out=false,blocked=false,missing=false;
Vec points[3]{{0,0,0},{.1f,100,0},{1,10,0}};
template<class T>void put(void* p,size_t offset,T value){std::memcpy(static_cast<uint8_t*>(p)+offset,&value,sizeof(value));}
void* selfMock(int){return peds[0];}
void* pedMock(int handle){return peds[handle>>8];}
bool isPlayerMock(const void* p){return sniper::read<int>(p,1824)<2;}
void* hierarchyMock(void*){return bones;}int boneIndexMock(void*,int tag){assert(tag==3);return missing?-1:3;}
void boneMock(void* p,Vec& result,unsigned tag,bool update){assert(tag==3&&!update);int index=(static_cast<uint8_t*>(p)-&peds[0][0])/2456;result=points[index];}
bool sightMock(const Vec&,const Vec&,bool buildings,bool vehicles,bool ped,bool objects,bool dummies,bool see,bool ignore){assert(buildings&&vehicles&&!ped&&objects&&dummies&&!see&&!ignore);return !blocked;}
void* padMock(int){return peds[0];}bool inMock(void*){return in;}bool outMock(void*){return out;}
void originalMock(void* cam){++calls;put(cam,140,sniper::read<int16_t>(cam,14)==4?70.f:12.f);put(cam,360,Vec{0,1,0});put(cam,396,Vec{0,0,1});put(cam,132,0.f);put(cam,148,1.f);}
void fovMock(float f,bool){fieldFov=f+11;}
}
int main(){
    sniper::camera=fakeCamera;sniper::screen=display;sniper::poolPtr=&poolValue;sniper::timeStep=&step;sniper::globalFov=&fieldFov;
    sniper::localPed=selfMock;sniper::getPed=pedMock;sniper::isPlayer=isPlayerMock;sniper::hierarchy=hierarchyMock;sniper::boneIndex=boneIndexMock;sniper::bone=boneMock;
    sniper::clearSight=sightMock;sniper::pad=padMock;sniper::zoomIn=inMock;sniper::zoomOut=outMock;sniper::originalProcess=originalMock;sniper::originalFov=fovMock;
    put(sniper::camera,49,true);put(sniper::camera,2352,static_cast<void*>(fakeRw));put(fakeRw,172,1000.f);put(sniper::screen,8,1280);put(sniper::screen,12,720);
    for(int i=0;i<3;++i){put(peds[i],8,points[i]);put(peds[i],32,static_cast<void*>(peds[i]));put(peds[i],1708,1000.f);put(peds[i],1824,0);}
    put(peds[0],2268,uint8_t(6));put(peds[0],1840+6*32,34);
    auto* cam=sniper::activeCam();put(cam,14,int16_t(7));put(cam,504,static_cast<void*>(peds[0]));
    sniper::clientReady=true;options::aimEnabled=true;options::zoomEnabled=true;
    sniper::process(cam);assert(calls==1&&sniper::lockedPed==peds[1]); // Farther in the world, closer to the red dot.
    Vec front=sniper::read<Vec>(cam,360);assert(front.x>0&&front.x<.002f&&front.y>.999f);
    float alpha=sniper::read<float>(cam,132),beta=sniper::read<float>(cam,148);
    assert(std::fabs(-std::cos(beta)*std::cos(alpha)-front.x)<1e-5f&&std::fabs(-std::sin(beta)*std::cos(alpha)-front.y)<1e-5f);
    in=true;for(int i=0;i<500;++i)sniper::process(cam);assert(sniper::customFov==1&&sniper::read<float>(cam,140)==1);
    sniper::setFov(1,false);assert(fieldFov==1);sniper::setFov(60,false);assert(fieldFov==71); // Other FOV requests retain native behavior.
    in=false;out=true;for(int i=0;i<500;++i)sniper::process(cam);assert(sniper::customFov==70);out=false;
    blocked=true;sniper::process(cam);assert(!sniper::lockedPed);blocked=false;
    sniper::reset();put(fakeRw,172,50.f);sniper::process(cam);assert(sniper::lockedPed==peds[2]); // Current far plane limits acquisition.
    sniper::reset();put(fakeRw,172,1000.f);put(peds[1],1824,4);put(peds[2],1824,4);sniper::process(cam);assert(!sniper::lockedPed);
    put(peds[1],1824,0);missing=true;sniper::process(cam);assert(!sniper::lockedPed);missing=false;
    health::Sample entry{peds[1],peds[1],42,0,0,diagnostics::millis()};features::healthCache.update(entry);sniper::process(cam);assert(!sniper::lockedPed); // Synthetic GTA HP must not override synced death.
    features::healthCache.remove(peds[1]);sniper::process(cam);assert(sniper::lockedPed==peds[1]);
    options::aimEnabled=false;sniper::process(cam);assert(!sniper::lockedPed&&sniper::read<Vec>(cam,360).x==0);
    options::aimEnabled=true;put(peds[0],1840+6*32,33);sniper::process(cam);assert(!sniper::scoped&&!sniper::lockedPed);
    put(peds[0],1840+6*32,34);put(cam,14,int16_t(4));sniper::process(cam);assert(!sniper::scoped&&sniper::read<float>(cam,140)==70);sniper::setFov(70,false);assert(fieldFov==81);
    put(cam,14,int16_t(7));sniper::clientReady=false;sniper::process(cam);assert(!sniper::scoped&&!sniper::lockedPed);
    sniper::clientReady=true;options::zoomEnabled=false;sniper::process(cam);sniper::setFov(12,false);assert(!sniper::zoomActive&&fieldFov==23);
    options::zoomEnabled=true;put(peds[0],1708,0.f);sniper::process(cam);assert(!sniper::scoped);
    put(peds[0],1708,100.f);put(sniper::camera,2408,true);sniper::process(cam);assert(!sniper::scoped);put(sniper::camera,2408,false);
    std::cout<<"Sniper tests passed: closest crosshair target, visibility, scope/weapon gating, zoom limits and native FOV restoration\n";
}
