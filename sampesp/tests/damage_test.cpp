#include "../main.cpp"
#include <cassert>
#include <iostream>
extern "C" int __android_log_write(int,const char*,const char*){return 0;}
namespace {
int self=1,other=2,victim=3,calls=0,last=0;bool result=true;
void* selfMock(int i){assert(i==-1);return &self;}
bool original(void* v,void* o,int weapon,int value,int piece,int direction){
    assert(v==&victim&&(o==&self||o==&other||!o)&&(weapon==34||weapon==33)&&piece==3&&direction==2);
    ++calls;last=value;return result;
}
bool hit(void* owner=&self,int weapon=34,int value=100){return damageboost::event(&victim,owner,weapon,value,3,2);}
}
int main(){
    damageboost::original=original;damageboost::localPed=selfMock;sniper::clientReady=true;
    assert(hit()&&calls==1&&last==300&&damageboost::boosted==1);
    result=false;assert(!hit()&&calls==2&&last==300);result=true;
    assert(hit(&other)&&last==100);assert(hit(nullptr)&&last==100);assert(hit(&self,33)&&last==100);
    sniper::clientReady=false;assert(hit()&&last==100);sniper::clientReady=true;
    assert(hit(&self,34,0)&&last==0);assert(hit(&self,34,-1)&&last==-1);
    assert(damageboost::triple(110)==330&&damageboost::triple(0x7fffffff)==0x7fffffff);
    assert(damageboost::triple(715827882)==2147483646);
    std::cout<<"Damage passed: 3x local sniper character damage, original event/return, remote/weapon/client guards and integer boundaries\n";
}
