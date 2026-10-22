#pragma once
#include "player_health.h"
#include "client_menu.h"
#include "simple_menu.h"
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
    simpleui::event=log;
    struct Entry {uintptr_t offset;const char* bytes;void* callback;void** original;const char* label;};
    const Entry entries[]{
        {0x55acc0,"\xfd\x7b\xbe\xa9\xf4\x4f\x01\xa9\xfd\x03\x00\x91\xf3\x03\x00\xaa",reinterpret_cast<void*>(simpleui::submit),reinterpret_cast<void**>(&simpleui::originalSubmit),"CMD /alynesp"},
        {0x5b3c60,"\x08\x94\x6e\x39\x08\x0c\x00\x34\xe9\x23\xbc\x6d\xfd\x7b\x01\xa9",reinterpret_cast<void*>(simpleui::mousePosition),reinterpret_cast<void**>(&simpleui::originalPosition),"TOUCH position"},
        {0x5b3e00,"\x08\x94\x6e\x39\xe8\x12\x00\x34\xff\x43\x01\xd1\xfd\x7b\x01\xa9",reinterpret_cast<void*>(simpleui::mouseButton),reinterpret_cast<void**>(&simpleui::originalButton),"TOUCH button"}
    };
    for(const auto& entry:entries){
        void* target=reinterpret_cast<void*>(base+entry.offset);
        bool okay=!std::memcmp(target,entry.bytes,16)&&aml->Hook(target,entry.callback,entry.original);
        char message[160];std::snprintf(message,sizeof(message),"SIMPLE_UI %s hook %s",entry.label,okay?"installed":"FAILED (entry changed or hook rejected)");log(message);
    }
    log("SIMPLE_UI 0.3.1 uses GTA drawing; legacy ImGui window hook disabled");
}
}
