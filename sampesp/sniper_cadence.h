#pragma once
namespace cadence {
using Fire=bool(*)(void*,void*,void*,void*,void*,void*,void*);
Fire originals[2]{};
uint32_t* clockMs=nullptr;
void* (*localPed)(int)=nullptr;
unsigned installed=0;
std::atomic<uint64_t> adjusted{0};
std::atomic<uint32_t> lastOriginalMs{0},lastReducedMs{0};
template<class T>T read(const void* p,size_t off){T v;std::memcpy(&v,static_cast<const uint8_t*>(p)+off,sizeof(v));return v;}
// Unsigned subtraction supports the game's millisecond counter wrapping.
uint32_t reduced(uint32_t now,uint32_t deadline){
    uint32_t remaining=deadline-now;
    if(!remaining||remaining>60000)return deadline;
    return now+(remaining+1)/2;
}
bool fire(Fire original,void* weapon,void* owner,void* a,void* b,void* target,void* c,void* d){
    bool local=sniper::clientReady.load()&&clockMs&&localPed&&owner&&owner==localPed(-1)&&weapon&&read<int32_t>(weapon,0)==34;
    uint32_t beforeClip=local?read<uint32_t>(weapon,8):0,beforeTotal=local?read<uint32_t>(weapon,12):0;
    bool result=original(weapon,owner,a,b,target,c,d);
    if(local&&result&&read<int32_t>(weapon,0)==34&&
       (read<uint32_t>(weapon,8)<beforeClip||read<uint32_t>(weapon,12)<beforeTotal)){
        int state=read<int32_t>(weapon,4);
        // Last-shot reload/bolt timers are also part of the sniper's cadence.
        // A separate manual reload with no shot does not pass this check.
        if(state==1||state==2){
            uint32_t now=*clockMs,deadline=read<uint32_t>(weapon,16),next=reduced(now,deadline);
            if(next!=deadline){std::memcpy(static_cast<uint8_t*>(weapon)+16,&next,4);lastOriginalMs=deadline-now;lastReducedMs=next-now;++adjusted;}
        }
    }
    return result;
}
bool fire0(void* w,void* o,void* a,void* b,void* t,void* c,void* d){return fire(originals[0],w,o,a,b,t,c,d);}
bool fire1(void* w,void* o,void* a,void* b,void* t,void* c,void* d){return fire(originals[1],w,o,a,b,t,c,d);}
void install(IAML* aml,void* handle,uintptr_t base,void (*log)(const char*)){
    const char* names[]{"_ZN7CWeapon4FireEP7CEntityP7CVectorS3_S1_S3_S3_","_ZN6CTimer22m_snTimeInMillisecondsE","_Z13FindPlayerPedi"};
    constexpr uintptr_t offsets[]{0x700c64,0xbdc558,0x4efae0};uintptr_t symbols[3]{};
    for(int i=0;i<3;++i){symbols[i]=aml->GetSym(handle,names[i]);if(symbols[i]!=base+offsets[i]){log("CADENCE disabled: GTA symbols/layout differ");return;}}
    clockMs=reinterpret_cast<uint32_t*>(symbols[1]);localPed=reinterpret_cast<decltype(localPed)>(symbols[2]);
    // PlayerOnFoot and UseGun callers. Never patch Fire's entry.
    constexpr uintptr_t calls[]{0x6536ec,0x5dff14};constexpr uint32_t instructions[]{0x97ef54a9,0x97f1229f};
    void* callbacks[]{reinterpret_cast<void*>(fire0),reinterpret_cast<void*>(fire1)};
    for(int i=0;i<2;++i){
        auto* address=reinterpret_cast<void*>(base+calls[i]);uint32_t word;std::memcpy(&word,address,4);
        if(word==instructions[i]&&aml->HookBL(address,callbacks[i],reinterpret_cast<void**>(&originals[i])))++installed;
        else log(i==0?"CADENCE PlayerOnFoot fire call unavailable":"CADENCE regular fire call unavailable");
    }
    if(installed)log("CADENCE installed: local sniper post-shot timer x0.5 (2x nominal rate); ammo/damage unchanged");
}
}
