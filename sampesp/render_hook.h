#pragma once
#include <cstdint>
#include <cstring>
namespace renderhook {
constexpr uintptr_t entryOffset=0x4d8994,callOffset=0x4d9108;
constexpr uint32_t expectedCall=0x97fffe23; // Idle: BL Render2dStuff; returns to +0x4d910c.
// Hook the caller, never relocate a prologue also patched by the multiplayer client.
// Loader is templated so the installer can be checked with an isolated fake loader.
template<class Loader>
bool install(Loader* loader,uintptr_t base,void* callback,void** original){
    if(!loader||!base||!callback||!original)return false;
    uint32_t instruction;std::memcpy(&instruction,reinterpret_cast<const void*>(base+callOffset),4);
    if(instruction!=expectedCall)return false; // No unsafe entry-hook fallback.
    return loader->HookBL(reinterpret_cast<void*>(base+callOffset),callback,original);
}
}
