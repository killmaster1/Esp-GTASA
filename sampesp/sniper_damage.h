#pragma once
namespace damageboost {
using Event=bool(*)(void*,void*,int,int,int,int);
Event original=nullptr;
void* (*localPed)(int)=nullptr;
bool installed=false;
std::atomic<uint64_t> boosted{0};
std::atomic<int> lastBase{0},lastValue{0};
constexpr uintptr_t callOffset=0x6fc8a0;
constexpr uint32_t expectedCall=0x97ec753c;
int triple(int value){return value<=0?value:(value>0x7fffffff/3?0x7fffffff:value*3);}
bool event(void* victim,void* owner,int weapon,int value,int piece,int direction){
    if(sniper::clientReady.load()&&localPed&&owner&&owner==localPed(-1)&&weapon==34&&value>0){
        lastBase=value;value=triple(value);lastValue=value;++boosted;
    }
    // This wraps a confirmed native bullet impact and forwards exactly one
    // native event, preserving target, shooter, body part, direction and return.
    return original(victim,owner,weapon,value,piece,direction);
}
void install(IAML* aml,void* handle,uintptr_t base,void (*log)(const char*)){
    const char* names[]{"_ZN7CWeapon19GenerateDamageEventEP4CPedP7CEntity11eWeaponTypei14ePedPieceTypesi","_Z13FindPlayerPedi"};
    constexpr uintptr_t offsets[]{0x706888,0x4efae0};uintptr_t symbols[2]{};
    for(int i=0;i<2;++i){symbols[i]=aml->GetSym(handle,names[i]);if(symbols[i]!=base+offsets[i]){log("DAMAGE disabled: GTA symbols/layout differ");return;}}
    auto* address=reinterpret_cast<void*>(base+callOffset);uint32_t word;std::memcpy(&word,address,4);
    localPed=reinterpret_cast<decltype(localPed)>(symbols[1]);
    if(word!=expectedCall||!aml->HookBL(address,reinterpret_cast<void*>(event),reinterpret_cast<void**>(&original))){log("DAMAGE disabled: bullet damage call changed or hook failed");return;}
    installed=true;log("DAMAGE installed: local sniper character impact damage x3; native event forwarding only");
}
}
