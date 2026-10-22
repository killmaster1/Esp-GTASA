#include "../main.cpp"
#include <cassert>
#include <iostream>
#include <limits>
extern "C" int __android_log_write(int,const char*,const char*){return 0;}
namespace {
alignas(8) uint8_t remote[320]{},wrapper[352]{},ped[2456]{};
template<class T>void put(void* p,size_t o,T v){std::memcpy(static_cast<uint8_t*>(p)+o,&v,sizeof(v));}
void processMock(void* p){assert(p==remote);put(ped,1708,1000.f);}
}
int main(){
    using health::fraction;using health::color;
    assert(color(100).g==220&&color(76).g==220);assert(color(75).r==245&&color(51).r==245);
    assert(color(50).r==255&&color(26).r==255);assert(color(25).r==240&&color(1).r==240);
    assert(fraction(1000)==1&&fraction(-10)==0&&fraction(25)==.25f&&fraction(std::numeric_limits<float>::quiet_NaN())==0);
    health::Cache cache;health::Sample out{};
    cache.update({remote,ped,42,65,20,100});assert(cache.get(ped,101,out)&&out.hp==65);
    assert(!cache.get(ped,2201,out)&&!cache.get(ped,99,out));
    cache.update({remote,ped,42,std::numeric_limits<float>::quiet_NaN(),20,100});assert(cache.get(ped,101,out)&&out.hp==65);
    int otherRemote=0,otherPed=0;cache.update({&otherRemote,&otherPed,43,40,0,100});cache.remove(remote);
    cache.update({&otherRemote,&otherPed,43,20,0,200});assert(cache.get(&otherPed,201,out)&&out.hp==20);
    // The health source comes from CRemotePlayer sync fields, even when CPed is set to 1000.
    put(remote,288,static_cast<void*>(wrapper));put(wrapper,312,static_cast<void*>(ped));put(remote,182,uint16_t(42));
    put(remote,16,65.f);put(remote,20,20.f);features::originalRemoteProcess=processMock;features::processRemote(remote);
    assert(read<float>(ped,1708)==1000);assert(features::healthCache.get(ped,diagnostics::millis(),out)&&out.hp==65&&out.id==42);
    put(remote,288,static_cast<void*>(nullptr));features::processRemote(remote);assert(!features::healthCache.get(ped,diagnostics::millis(),out));
    std::cout<<"Health tests passed: synchronized values, thresholds, expiry and removal\n";
}
