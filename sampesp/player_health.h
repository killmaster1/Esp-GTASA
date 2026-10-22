#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <algorithm>
namespace health {
struct Color {uint8_t r,g,b,a;};
inline Color color(float value){
    if(value>75)return {55,220,90,245};
    if(value>50)return {245,220,40,245};
    if(value>25)return {255,140,35,245};
    return {240,45,45,245};
}
inline float fraction(float value){return std::isfinite(value)?std::max(0.f,std::min(1.f,value/100.f)):0.f;}
struct Sample {const void* remote=nullptr;const void* ped=nullptr;uint16_t id=0;float hp=0,armour=0;uint64_t time=0;};
class Cache {
    std::array<Sample,512> samples{};
    std::mutex mutex;
public:
    void remove(const void* remote){std::lock_guard<std::mutex> lock(mutex);for(auto& s:samples)if(s.remote==remote)s=Sample{};}
    void update(Sample value){
        if(!value.remote||!value.ped||value.id>=1004||!std::isfinite(value.hp)||value.hp<0||value.hp>255||!std::isfinite(value.armour))return;
        std::lock_guard<std::mutex> lock(mutex);Sample* target=&samples[0];
        bool found=false;
        for(auto& s:samples)if(s.remote==value.remote||s.ped==value.ped){target=&s;found=true;break;}
        if(!found)for(auto& s:samples){
            if(!s.ped){target=&s;break;}
            if(s.time<target->time)target=&s;
        }*target=value;
    }
    bool get(const void* ped,uint64_t now,Sample& out){
        std::lock_guard<std::mutex> lock(mutex);
        for(const auto& s:samples)if(s.ped==ped&&now>=s.time&&now-s.time<=2000){out=s;return true;}
        return false;
    }
};
}
