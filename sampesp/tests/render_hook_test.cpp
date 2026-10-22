#include "../render_hook.h"
#include <cassert>
#include <vector>
#include <iostream>
struct Loader {
    uintptr_t base;int branches=0;bool accept=true;
    bool HookBL(void* address,void* callback,void** original){
        assert(reinterpret_cast<uintptr_t>(address)==base+renderhook::callOffset);
        assert(callback);++branches;if(!accept)return false;
        *original=reinterpret_cast<void*>(base+renderhook::entryOffset);
        uint32_t replacement=0x94000000;std::memcpy(address,&replacement,4);return true;
    }
    // A function-entry hook would reproduce the original conflict. It is forbidden here.
    bool Hook(void*,void*,void**){assert(false);return false;}
};
int main(){
    std::vector<uint8_t> memory(renderhook::callOffset+8,0xaa);
    uintptr_t base=reinterpret_cast<uintptr_t>(memory.data());Loader loader{base};
    void* callback=reinterpret_cast<void*>(base+16);void* original=nullptr;
    auto call=memory.data()+renderhook::callOffset;auto entry=memory.data()+renderhook::entryOffset;
    // Both original bytes and an already patched Alyn entry must remain untouched.
    for(uint8_t value:{uint8_t(0xfd),uint8_t(0x00)}){
        std::memset(entry,value,32);uint8_t before[32];std::memcpy(before,entry,32);
        uint32_t instruction=renderhook::expectedCall;std::memcpy(call,&instruction,4);
        assert(renderhook::install(&loader,base,callback,&original));
        assert(original==entry&&!std::memcmp(before,entry,32));
        assert(!renderhook::install(&loader,base,callback,&original)); // already replaced call: reject
    }
    assert(loader.branches==2);
    uint32_t wrongBranch=0x17fffe23;std::memcpy(call,&wrongBranch,4); // B rather than BL
    assert(!renderhook::install(&loader,base,callback,&original)&&loader.branches==2);
    uint32_t instruction=renderhook::expectedCall;std::memcpy(call,&instruction,4);loader.accept=false;
    assert(!renderhook::install(&loader,base,callback,&original)&&loader.branches==3);
    assert(!renderhook::install(&loader,base,nullptr,&original));
    std::cout<<"Render hook passed: exact BL validation, patched entry preservation, original target, failure without inline fallback\n";
}
