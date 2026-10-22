#pragma once
#include "player_health.h"
#include "client_menu.h"
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
    void* handle=aml->GetLibHandle("libAlyn_SAMPMOBILE.so");
    if(!handle){log("UI: client symbol handle unavailable");return;}
    const char* names[]{"_ZN5ImGui10GetVersionEv","_ZN5ImGui8EndFrameEv","_ZN5ImGui17GetCurrentContextEv","_ZN5ImGui5GetIOEv","_ZN5ImGui13GetFrameCountEv",
        "_ZN5ImGui5BeginEPKcPbi","_ZN5ImGui3EndEv","_ZN5ImGui6ButtonEPKcRK6ImVec2","_ZN5ImGui16SetNextWindowPosERK6ImVec2iS2_",
        "_ZN5ImGui17SetNextWindowSizeERK6ImVec2i","_ZN5ImGui15TextUnformattedEPKcS1_","_ZN5ImGui14PushStyleColorEij","_ZN5ImGui13PopStyleColorEi",
        "_ZN5ImGui12PushStyleVarEif","_ZN5ImGui11PopStyleVarEi","_ZN5ImGui18SetWindowFontScaleEf","_ZN5ImGui18SetNextWindowFocusEv"};
    constexpr uintptr_t offsets[]{0x5c5e50,0x5d1140,0x5b7ec0,0x5c5e60,0x5c5f10,0x5ccf40,0x5d1f40,0x639e50,0x5d6d60,0x5ccf00,0x638900,0x5b8d40,0x5b8f40,0x5b91a0,0x5b9560,0x5d6f60,0x5dea70};
    uintptr_t symbols[17]{};
    for(int i=0;i<17;++i){symbols[i]=aml->GetSym(handle,names[i]);if(!symbols[i]||symbols[i]-base!=offsets[i]){log("UI symbols/layout differ; menu disabled, overlay remains available");return;}}
    auto version=reinterpret_cast<const char*(*)()>(symbols[0]);
    if(std::strcmp(version(),"1.92.8")){log("UI: unsupported ImGui version");return;}
    clientui::context=reinterpret_cast<decltype(clientui::context)>(symbols[2]);clientui::io=reinterpret_cast<decltype(clientui::io)>(symbols[3]);
    clientui::frameCount=reinterpret_cast<decltype(clientui::frameCount)>(symbols[4]);clientui::begin=reinterpret_cast<decltype(clientui::begin)>(symbols[5]);
    clientui::end=reinterpret_cast<decltype(clientui::end)>(symbols[6]);clientui::button=reinterpret_cast<decltype(clientui::button)>(symbols[7]);
    clientui::setPos=reinterpret_cast<decltype(clientui::setPos)>(symbols[8]);clientui::setSize=reinterpret_cast<decltype(clientui::setSize)>(symbols[9]);
    clientui::text=reinterpret_cast<decltype(clientui::text)>(symbols[10]);clientui::color=reinterpret_cast<decltype(clientui::color)>(symbols[11]);
    clientui::popColor=reinterpret_cast<decltype(clientui::popColor)>(symbols[12]);clientui::style=reinterpret_cast<decltype(clientui::style)>(symbols[13]);
    clientui::popStyle=reinterpret_cast<decltype(clientui::popStyle)>(symbols[14]);clientui::fontScale=reinterpret_cast<decltype(clientui::fontScale)>(symbols[15]);clientui::focusNext=reinterpret_cast<decltype(clientui::focusNext)>(symbols[16]);clientui::event=log;
    if(!aml->Hook(reinterpret_cast<void*>(symbols[1]),reinterpret_cast<void*>(clientui::endFrame),reinterpret_cast<void**>(&clientui::originalEndFrame))){log("UI EndFrame hook failed");return;}
    clientui::installed=true;log("SA-style menu 0.2.1 installed: single-window MENU ESP launcher, box/line/health toggles and nearby player inspector");
}
}
