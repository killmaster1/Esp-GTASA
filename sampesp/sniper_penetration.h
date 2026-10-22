#pragma once
#include <cstddef>
namespace penetration {
using esp::Vec;
// CBulletInfo: GTA ARM64 DWARF, eight slots at +0xd28a08, stride 56.
struct Bullet {
    int32_t type;uint32_t padding;void* owner;float expires;
    uint8_t active;uint8_t unused[3];Vec position,velocity;
    uint16_t damage;uint8_t tail[6];
};
static_assert(sizeof(Bullet)==56&&offsetof(Bullet,owner)==8&&offsetof(Bullet,active)==20&&offsetof(Bullet,position)==24&&offsetof(Bullet,damage)==48,"Native CBulletInfo layout");
using Ray=bool(*)(const Vec&,const Vec&,void*,void**,bool,bool,bool,bool,bool,bool,bool,bool);
Ray originalRay=nullptr;
Bullet* bullets=nullptr;void** ignoredEntity=nullptr;
void* (*localPed)(int)=nullptr;
bool installed=false;
std::atomic<uint64_t> rays{0},filtered{0},hits{0};
constexpr uintptr_t callOffset=0x6fc7f8;
constexpr uint32_t expectedCall=0x97eca0d2;
bool eligible(const Vec& start,const Vec& end,void* self,void* ignored,const Bullet* entries){
    if(!self||ignored!=self||!entries||!esp::finite(start)||!esp::finite(end))return false;
    bool matched=false;
    for(int i=0;i<8;++i){const auto& b=entries[i];
        // The native update copied this slot's position into the ray's start.
        // Require an exact match, not the player's currently equipped weapon.
        if(b.active&&b.owner==self&&!std::memcmp(&b.position,&start,sizeof(Vec))){
            // Ambiguous collocated bullets of different weapons: keep native collisions.
            if(b.type!=34)return false;
            matched=true;
        }
    }
    return matched;
}
bool ray(const Vec& from,const Vec& to,void* point,void** entity,
         bool buildings,bool vehicles,bool peds,bool objects,bool dummies,bool seeThrough,bool cameraIgnore,bool shootThrough){
    ++rays;
    bool apply=options::penetrationEnabled.load()&&sniper::clientReady.load()&&localPed&&ignoredEntity&&
        eligible(from,to,localPed(-1),*ignoredEntity,bullets);
    if(apply){buildings=false;objects=false;dummies=false;++filtered;}
    // Forward original output buffers, endpoints and all other flags. Native
    // damage, lifetime, ammo, hit effects and client processing stay in place.
    bool result=originalRay(from,to,point,entity,buildings,vehicles,peds,objects,dummies,seeThrough,cameraIgnore,shootThrough);
    if(apply&&result)++hits;
    return result;
}
template<class Loader>
bool hookCall(Loader* loader,uintptr_t base){
    uint32_t instruction;std::memcpy(&instruction,reinterpret_cast<void*>(base+callOffset),4);
    if(instruction!=expectedCall)return false;
    return loader->HookBL(reinterpret_cast<void*>(base+callOffset),reinterpret_cast<void*>(ray),reinterpret_cast<void**>(&originalRay));
}
void install(IAML* aml,void* handle,uintptr_t base,void (*log)(const char*)){
    const char* names[]{"_ZN11CBulletInfo6UpdateEv","_ZN6CWorld18ProcessLineOfSightERK7CVectorS2_R9CColPointRP7CEntitybbbbbbbb","_ZN6CWorld13pIgnoreEntityE","_Z13FindPlayerPedi"};
    constexpr uintptr_t offsets[]{0x6fc638,0x508c7c,0xbdcaf8,0x4efae0};
    uintptr_t symbols[4]{};
    for(int i=0;i<4;++i){symbols[i]=aml->GetSym(handle,names[i]);if(symbols[i]!=base+offsets[i]){log("PENETRATION disabled: GTA symbols/layout differ");return;}}
    // Verify the native array address construction before using a private global.
    const uint8_t arrayCode[]{0x7c,0x31,0x00,0x90,0xfb,0x03,0x1f,0xaa,0x9c,0x23,0x28,0x91};
    if(std::memcmp(reinterpret_cast<void*>(base+0x6fc6e0),arrayCode,sizeof(arrayCode))){log("PENETRATION disabled: bullet array reference changed");return;}
    bullets=reinterpret_cast<Bullet*>(base+0xd28a08);
    ignoredEntity=reinterpret_cast<void**>(symbols[2]);localPed=reinterpret_cast<decltype(localPed)>(symbols[3]);
    if(!hookCall(aml,base)){log("PENETRATION disabled: bullet ray BL changed or hook failed; no inline fallback");return;}
    installed=true;
    log("PENETRATION installed: local sniper bullets ignore buildings/objects/dummies; peds/vehicles and native damage preserved");
}
}
