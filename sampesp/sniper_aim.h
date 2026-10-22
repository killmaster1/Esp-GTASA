#pragma once
#include "aim_math.h"
namespace sniper {
using esp::Vec;
struct Pool {void* storage;uint8_t* flags;int32_t size,freeIndex;bool own,noMemory;};
uint8_t* camera=nullptr;uint8_t* screen=nullptr;Pool** poolPtr=nullptr;
float* timeStep=nullptr;float* globalFov=nullptr;
void (*originalProcess)(void*)=nullptr;
void (*originalFov)(float,bool)=nullptr;
void* (*localPed)(int)=nullptr;void* (*getPed)(int)=nullptr;
bool (*isPlayer)(const void*)=nullptr;
void (*bone)(void*,Vec&,unsigned,bool)=nullptr;
void* (*hierarchy)(void*)=nullptr;int (*boneIndex)(void*,int)=nullptr;
bool (*clearSight)(const Vec&,const Vec&,bool,bool,bool,bool,bool,bool,bool)=nullptr;
void* (*pad)(int)=nullptr;bool (*zoomIn)(void*)=nullptr;bool (*zoomOut)(void*)=nullptr;
void (*logger)(const char*)=nullptr;
std::atomic<bool> clientReady{false},scoped{false};
std::atomic<int> lockedId{-1},reportedMode{-1},reportedWeapon{-1};std::atomic<float> reportedFov{70};
bool installed=false,zoomActive=false;
float customFov=70;void* lockedPed=nullptr;
template<class T>T read(const void* p,size_t off){T v;std::memcpy(&v,static_cast<const uint8_t*>(p)+off,sizeof(v));return v;}
template<class T>void write(void* p,size_t off,T value){std::memcpy(static_cast<uint8_t*>(p)+off,&value,sizeof(value));}
void reset(){scoped=false;zoomActive=false;lockedPed=nullptr;lockedId=-1;}
uint8_t* activeCam(){unsigned index=read<uint8_t>(camera,95);return index<3?camera+376+index*552:nullptr;}
Vec position(void* p){void* matrix=read<void*>(p,24);return matrix?read<Vec>(matrix,48):read<Vec>(p,8);}
bool validScope(void* cam,void* player){
    if(!clientReady||!player||cam!=activeCam())return false;
    int mode=read<int16_t>(cam,14);if(mode!=7&&mode!=46)return false;
    if(read<void*>(cam,504)!=player)return false;
    if(!read<bool>(camera,49)||read<bool>(camera,67)||read<bool>(camera,2408))return false;
    float hp=read<float>(player,1708);if(!std::isfinite(hp)||hp<=0)return false;
    unsigned slot=read<uint8_t>(player,2268);
    return slot<13&&read<int32_t>(player,1840+slot*32)==34;
}
bool chest(void* ped,Vec center,Vec& point){
    void* object=read<void*>(ped,32);if(!object)return false;
    void* h=hierarchy(object);if(!h)return false;
    int n=read<int>(h,4);if(n<1||n>256)return false;
    int index=boneIndex(h,3);if(index<0||index>=n)return false;
    bone(ped,point,3,false);Vec delta=sniper_math::sub(point,center);
    return esp::finite(point)&&sniper_math::dot(delta,delta)<25;
}
void* choose(void* self,Vec source,Vec front,Vec right,Vec up,float fov,float aspect,float distanceLimit,Vec& aimPoint){
    Pool* pool=poolPtr?*poolPtr:nullptr;if(!pool||!pool->storage||!pool->flags||pool->size<1||pool->size>100000)return nullptr;
    void* best=nullptr;float bestScore=1e30f;uint64_t now=diagnostics::millis();
    for(int i=0;i<pool->size;++i){
        unsigned flag=pool->flags[i];if(flag&128)continue;
        void* ped=getPed((i<<8)|flag);
        if(!ped||ped==self||!isPlayer(ped)||read<uint8_t>(ped,75)!=read<uint8_t>(self,75))continue;
        float hp=read<float>(ped,1708);if(!std::isfinite(hp)||hp<=0)continue;
        health::Sample synced{};if(features::healthCache.get(ped,now,synced)&&synced.hp<=0)continue;
        Vec center=position(ped),point{};if(!esp::finite(center)||!chest(ped,center,point))continue;
        Vec delta=sniper_math::sub(point,source);float distance2=sniper_math::dot(delta,delta);
        if(distance2>distanceLimit*distanceLimit||distance2<.25f)continue;
        float value=0;bool existing=ped==lockedPed;
        if(!sniper_math::score(point,source,front,right,up,fov,aspect,!existing,value))continue;
        if(!existing&&value>=bestScore)continue;
        // Ignore peds in the ray so the target does not occlude itself. World
        // buildings, vehicles and objects can still prevent target acquisition.
        if(!clearSight(source,point,true,true,false,true,true,false,false))continue;
        if(existing){aimPoint=point;return ped;}
        best=ped;bestScore=value;aimPoint=point;
    }return best;
}
void process(void* cam){
    originalProcess(cam);
    if(cam!=activeCam())return;
    void* self=localPed(-1);reportedMode=read<int16_t>(cam,14);reportedWeapon=-1;
    if(self){unsigned slot=read<uint8_t>(self,2268);if(slot<13)reportedWeapon=read<int32_t>(self,1840+slot*32);}
    if(!validScope(cam,self)){reset();return;}
    scoped=true;
    float nativeFov=read<float>(cam,140);
    if(options::zoomEnabled){
        if(!zoomActive){customFov=sniper_math::zoom(nativeFov,false,false,0);zoomActive=true;}
        void* controls=pad(0);bool in=controls&&zoomIn(controls),out=controls&&zoomOut(controls);
        float step=timeStep?*timeStep:1;
        if(!std::isfinite(step)||step<0)step=0;
        customFov=sniper_math::zoom(customFov,in,out,step*.02f);
        write(cam,140,customFov);write(cam,144,0.f);reportedFov=customFov;
    }else{zoomActive=false;reportedFov=nativeFov;}
    if(!options::aimEnabled){lockedPed=nullptr;lockedId=-1;return;}
    Vec source=read<Vec>(cam,372),front{},right{},up{};
    if(!esp::finite(source)||!sniper_math::basis(read<Vec>(cam,360),front,right,up)){lockedPed=nullptr;lockedId=-1;return;}
    float w=read<int>(screen,8),h=read<int>(screen,12);if(w<1||h<1)return;
    float distanceLimit=500;void* rw=read<void*>(camera,2352);
    if(rw){float far=read<float>(rw,172);if(std::isfinite(far)&&far>1)distanceLimit=std::min(far,10000.f);}
    float fov=zoomActive?customFov:(globalFov?*globalFov:nativeFov);
    if(!std::isfinite(fov)||fov<1||fov>100)fov=70;
    Vec point{};void* target=choose(self,source,front,right,up,fov,w/h,distanceLimit,point);
    if(!target){lockedPed=nullptr;lockedId=-1;return;}
    Vec direction=sniper_math::sub(point,source);
    if(!sniper_math::basis(direction,front,right,up)){lockedPed=nullptr;lockedId=-1;return;}
    float alpha=0,beta=0;sniper_math::angles(front,alpha,beta);
    if(std::fabs(alpha)>1.50f){lockedPed=nullptr;lockedId=-1;return;}
    // Native sniper convention: Front=(-cos(beta)*cos(alpha),
    // -sin(beta)*cos(alpha),sin(alpha)). The outer CCamera::Process builds
    // its regular matrices from these vectors after CCam::Process returns.
    write(cam,360,front);write(cam,396,up);write(cam,132,alpha);write(cam,148,beta);
    write(cam,120,beta);write(cam,124,alpha);write(cam,136,0.f);write(cam,152,0.f);
    lockedPed=target;health::Sample sample{};
    lockedId=features::healthCache.get(target,diagnostics::millis(),sample)?int(sample.id):-1;
}
void setFov(float value,bool special){
    originalFov(value,special);
    if(!scoped||!options::zoomEnabled||!zoomActive)return;
    uint8_t* cam=activeCam();void* self=localPed(-1);
    if(!cam||!validScope(cam,self))return;
    // The stock SetFOV adds a widescreen correction. Keep the extended scope
    // lens positive and independent of that addition, only for its own request.
    if(std::isfinite(value)&&std::fabs(value-read<float>(cam,140))<.01f)*globalFov=customFov;
}
void install(IAML* aml,void* handle,uintptr_t base,void (*log)(const char*)){
    logger=log;
    const char* names[]{"TheCamera","_ZN4CCam7ProcessEv","_Z13FindPlayerPedi","_ZN6CPools6GetPedEi","_ZNK4CPed8IsPlayerEv","_ZN4CPed15GetBonePositionER5RwV3djb",
        "_Z29GetAnimHierarchyFromSkinClumpP7RpClump","_Z17RpHAnimIDGetIndexP16RpHAnimHierarchyi","_ZN6CWorld21GetIsLineOfSightClearERK7CVectorS2_bbbbbbb",
        "_ZN4CPad6GetPadEi","_ZN4CPad12SniperZoomInEv","_ZN4CPad13SniperZoomOutEv","_ZN6CPools11ms_pPedPoolE","RsGlobal","_ZN6CTimer12ms_fTimeStepE",
        "_ZN5CDraw6SetFOVEfb","_ZN5CDraw7ms_fFOVE"};
    constexpr uintptr_t offsets[]{0xbba8d0,0x49eebc,0x4efae0,0x4f2378,0x595400,0x59aee4,0x6f54cc,0x255be8,0x5075a4,0x4db628,0x4df55c,0x4df668,0xbc3bb8,0xc9b320,0xbdc580,0x6c9c88,0x88e6bc};
    uintptr_t symbols[17]{};
    for(int i=0;i<17;++i){symbols[i]=aml->GetSym(handle,names[i]);if(!symbols[i]||symbols[i]-base!=offsets[i]){log("Sniper: unsupported symbols/layout; new features disabled");return;}}
    camera=reinterpret_cast<uint8_t*>(symbols[0]);localPed=reinterpret_cast<decltype(localPed)>(symbols[2]);getPed=reinterpret_cast<decltype(getPed)>(symbols[3]);
    isPlayer=reinterpret_cast<decltype(isPlayer)>(symbols[4]);bone=reinterpret_cast<decltype(bone)>(symbols[5]);hierarchy=reinterpret_cast<decltype(hierarchy)>(symbols[6]);
    boneIndex=reinterpret_cast<decltype(boneIndex)>(symbols[7]);clearSight=reinterpret_cast<decltype(clearSight)>(symbols[8]);pad=reinterpret_cast<decltype(pad)>(symbols[9]);
    zoomIn=reinterpret_cast<decltype(zoomIn)>(symbols[10]);zoomOut=reinterpret_cast<decltype(zoomOut)>(symbols[11]);poolPtr=reinterpret_cast<Pool**>(symbols[12]);
    screen=reinterpret_cast<uint8_t*>(symbols[13]);timeStep=reinterpret_cast<float*>(symbols[14]);globalFov=reinterpret_cast<float*>(symbols[16]);
    if(!aml->Hook(reinterpret_cast<void*>(symbols[15]),reinterpret_cast<void*>(setFov),reinterpret_cast<void**>(&originalFov))){log("Sniper: FOV hook failed; new features disabled");return;}
    if(!aml->Hook(reinterpret_cast<void*>(symbols[1]),reinterpret_cast<void*>(process),reinterpret_cast<void**>(&originalProcess))){log("Sniper: camera hook failed; new features disabled");return;}
    installed=true;log("Sniper: scoped aimlock and extended zoom installed; waiting for verified Alyn client");
}
}
