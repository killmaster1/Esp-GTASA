#include "../main.cpp"
#include <cassert>
#include <iostream>
extern "C" int __android_log_write(int,const char*,const char*){return 0;}
namespace {
uint8_t weapon[32]{};int self=1,other=2;uint32_t now=1000;bool success=true,consume=true;int state=1,calls=0;
void* selfMock(int n){assert(n==-1);return &self;}
template<class T>void put(size_t off,T value){std::memcpy(weapon+off,&value,sizeof(value));}
bool original(void* w,void*,void* a,void* b,void* t,void* c,void* d){
    assert(w==weapon&&a==&self&&b==&other&&t==nullptr&&c==nullptr&&d==nullptr);++calls;
    if(success){put(4,state);put(16,now+1000);if(consume){put(8,cadence::read<uint32_t>(w,8)-1);put(12,cadence::read<uint32_t>(w,12)-1);}}
    return success;
}
bool shoot(void* owner=&self){return cadence::fire(original,weapon,owner,&self,&other,nullptr,nullptr,nullptr);}
void setup(){put(0,34);put(4,0);put(8,uint32_t(10));put(12,uint32_t(30));put(16,now);}
}
int main(){
    cadence::clockMs=&now;cadence::localPed=selfMock;sniper::clientReady=true;setup();
    assert(shoot()&&cadence::read<uint32_t>(weapon,16)==1500&&cadence::adjusted==1);
    assert(cadence::read<uint32_t>(weapon,8)==9&&cadence::read<uint32_t>(weapon,12)==29);
    now=1500;assert(shoot()&&cadence::read<uint32_t>(weapon,16)==2000); // fixed half, not exponential halving
    state=2;setup();put(8,uint32_t(1));assert(shoot()&&cadence::read<uint32_t>(weapon,16)==now+500&&cadence::read<int>(weapon,4)==2);
    state=1;setup();put(0,33);assert(shoot()&&cadence::read<uint32_t>(weapon,16)==now+1000);
    setup();assert(shoot(&other)&&cadence::read<uint32_t>(weapon,16)==now+1000);
    setup();sniper::clientReady=false;assert(shoot()&&cadence::read<uint32_t>(weapon,16)==now+1000);sniper::clientReady=true;
    setup();success=false;assert(!shoot()&&cadence::read<uint32_t>(weapon,16)==now);success=true;
    setup();consume=false;assert(shoot()&&cadence::read<uint32_t>(weapon,16)==now+1000);consume=true;
    assert(cadence::reduced(1000,2000)==1500&&cadence::reduced(1000,2001)==1501);
    assert(cadence::reduced(1000,900)==900&&cadence::reduced(1000,1000)==1000);
    uint32_t wrap=0xffffff00;assert(cadence::reduced(wrap,wrap+1000)==uint32_t(wrap+500));
    std::cout<<"Cadence passed: 2x post-shot timing, no exponential reduction, ammo/return forwarding, reload, remote/weapon guards and clock wrap\n";
}
