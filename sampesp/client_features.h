#pragma once
#include "player_health.h"
#include "feature_options.h"
#include "distant_probe.h"
namespace features {
health::Cache healthCache;
void (*originalRemoteProcess)(void*)=nullptr;
void (*logger)(const char*)=nullptr;
bool attempted=false,healthInstalled=false;
template<class T>T read(const void* p,size_t off){T value;std::memcpy(&value,static_cast<const uint8_t*>(p)+off,sizeof(value));return value;}
void processRemote(void* remote){
    originalRemoteProcess(remote);
    // CRemotePlayer fields verified in this client's spawn and sync handlers.
    void* wrapper=read<void*>(remote,288);
    if(!wrapper){healthCache.remove(remote);return;}
    void* ped=read<void*>(wrapper,312);
    if(!ped){healthCache.remove(remote);return;}
    healthCache.update({remote,ped,read<uint16_t>(remote,182),read<float>(remote,16),read<float>(remote,20),diagnostics::millis()});
}
void install(IAML* aml,uintptr_t base,void (*log)(const char*)){
    if(attempted||!aml||!base)return;
    attempted=true;logger=log;
    // This private function's entry was checked against the uploaded ARM64 ELF.
    const uint8_t prologue[]{0xff,0xc3,0x02,0xd1,0xfd,0x7b,0x08,0xa9,0xf5,0x4b,0x00,0xf9,0xf4,0x4f,0x0a,0xa9};
    void* process=reinterpret_cast<void*>(base+0x502060);
    if(!std::memcmp(process,prologue,sizeof(prologue))&&aml->Hook(process,reinterpret_cast<void*>(processRemote),reinterpret_cast<void**>(&originalRemoteProcess))){
        healthInstalled=true;log("Synchronized remote health hook installed (CPed's synthetic 1000 HP is not used for the bar)");
    }else log("Remote health hook unavailable or entry changed; unsynchronized health bars stay gray");
    distantprobe::install(aml,base,log);
    log("Features 0.3.8: automatic operation; menu, font, touch and chat hooks removed");
}
}
