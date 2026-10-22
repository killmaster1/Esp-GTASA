#pragma once
#include <array>
namespace distantprobe {
struct Sample {
    bool valid=false,hasWrapper=false;int x=0,y=0,z=0;
    uint64_t received=0,lastLog=0;
};
class Cache {
    std::array<Sample,1004> slots{};
    std::mutex mutex;uint64_t window=0;unsigned lines=0;
public:
    bool update(unsigned id,int x,int y,int z,bool wrapper,uint64_t now){
        if(id>=slots.size())return false;
        std::lock_guard<std::mutex> lock(mutex);auto& s=slots[id];
        bool transition=!s.valid||s.hasWrapper!=wrapper;
        bool due=transition||!s.lastLog||now-s.lastLog>=10000;
        s.valid=true;s.hasWrapper=wrapper;s.x=x;s.y=y;s.z=z;s.received=now;
        if(!window||now-window>=1000){window=now;lines=0;}
        if(due&&lines<24){s.lastLog=now;++lines;return true;}return false;
    }
    bool invalidate(unsigned id){
        if(id>=slots.size())return false;
        std::lock_guard<std::mutex> lock(mutex);bool valid=slots[id].valid;slots[id]=Sample{};return valid;
    }
    bool get(unsigned id,uint64_t now,Sample& out){
        if(id>=slots.size())return false;
        std::lock_guard<std::mutex> lock(mutex);const auto& s=slots[id];
        if(!s.valid||now<s.received||now-s.received>5000)return false;
        out=s;return true;
    }
    unsigned freshWithoutWrapper(uint64_t now){
        std::lock_guard<std::mutex> lock(mutex);unsigned count=0;
        for(const auto& s:slots)if(s.valid&&!s.hasWrapper&&now>=s.received&&now-s.received<=5000)++count;
        return count;
    }
};
Cache cache;
void (*originalPosition)(void*,int,int,int)=nullptr;
void (*originalInactive)(void*)=nullptr;
void (*logger)(const char*)=nullptr;
unsigned installed=0;
std::atomic<uint64_t> received{0},withoutWrapper{0},inactive{0};
template<class T>T read(const void* p,size_t offset){T value;std::memcpy(&value,static_cast<const uint8_t*>(p)+offset,sizeof(value));return value;}
void position(void* remote,int rawX,int rawY,int rawZ){
    if(remote){
        unsigned id=read<uint16_t>(remote,182);
        bool wrapper=read<void*>(remote,288)!=nullptr;
        int x=static_cast<int16_t>(rawX),y=static_cast<int16_t>(rawY),z=static_cast<int16_t>(rawZ);
        if(id<1004){
            ++received;if(!wrapper)++withoutWrapper;
            if(cache.update(id,x,y,z,wrapper,diagnostics::millis())&&logger){
                char line[224];std::snprintf(line,sizeof(line),"DISTANT_SAMPLE id=%u source=packet208 x=%d y=%d z=%d remote_has_wrapper=%d precision=integer age_ms=0",id,x,y,z,int(wrapper));logger(line);
            }
        }
    }
    // Only forward the existing native call. No new radar/teleport/network calls.
    originalPosition(remote,rawX,rawY,rawZ);
}
void deactivate(void* remote){
    if(remote){unsigned id=read<uint16_t>(remote,182);if(id<1004){++inactive;
        if(cache.invalidate(id)&&logger){char line[96];std::snprintf(line,sizeof(line),"DISTANT_INACTIVE id=%u source=packet208",id);logger(line);}
    }}
    originalInactive(remote);
}
void install(IAML* aml,uintptr_t base,void (*log)(const char*)){
    logger=log;
    constexpr uintptr_t offsets[]{0x4e7724,0x4e7648};
    constexpr uint32_t instructions[]{0x940069e7,0x94006a4e};
    void* callbacks[]{reinterpret_cast<void*>(position),reinterpret_cast<void*>(deactivate)};
    void** originals[]{reinterpret_cast<void**>(&originalPosition),reinterpret_cast<void**>(&originalInactive)};
    for(int i=0;i<2;++i){
        void* target=reinterpret_cast<void*>(base+offsets[i]);uint32_t word;std::memcpy(&word,target,4);
        if(word==instructions[i]&&aml->HookBL(target,callbacks[i],originals[i]))++installed;
        else log(i==0?"DISTANT_PROBE position observer unavailable":"DISTANT_PROBE inactive observer unavailable");
    }
    log("DISTANT_PROBE diagnostic only: observe existing compact position traffic; no new markers, teleport, commands or network requests");
}
}
