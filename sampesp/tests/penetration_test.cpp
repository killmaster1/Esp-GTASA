#include "../main.cpp"
#include <cassert>
#include <iostream>
#include <limits>
extern "C" int __android_log_write(int,const char*,const char*){return 0;}
namespace {
penetration::Bullet entries[8]{};
int self=1,other=2,wall=3,ped=4,vehicle=5,prop=6;
void* ignored=&self;void* expectedPoint=nullptr;void** expectedEntity=nullptr;
bool hasWall=true,hasVehicle=false,hasProp=false;int calls=0;
bool flags[8]{};
void* selfMock(int index){assert(index==-1);return &self;}
bool worldMock(const esp::Vec& a,const esp::Vec& b,void* point,void** entity,
    bool buildings,bool vehicles,bool peds,bool objects,bool dummies,bool see,bool camera,bool shoot){
    ++calls;assert(a.x==0&&b.x==20&&point==expectedPoint&&entity==expectedEntity);
    bool current[]{buildings,vehicles,peds,objects,dummies,see,camera,shoot};std::memcpy(flags,current,sizeof(flags));
    *entity=nullptr;
    // A wall/prop is between the muzzle and the target. Vehicles remain solid.
    if(buildings&&hasWall)*entity=&wall;
    else if(objects&&hasProp)*entity=&prop;
    else if(vehicles&&hasVehicle)*entity=&vehicle;
    else if(peds)*entity=&ped;
    if(*entity)static_cast<uint8_t*>(point)[0]=0x42;
    return *entity!=nullptr;
}
bool fire(bool peds=true){
    uint8_t point[64]{};void* hit=nullptr;expectedPoint=point;expectedEntity=&hit;
    bool result=penetration::ray({0,0,0},{20,0,0},point,&hit,true,true,peds,true,true,false,true,false);
    assert(result==(hit!=nullptr));if(result)assert(point[0]==0x42);
    return hit==&ped;
}
struct Loader {
    uintptr_t base;bool okay=true;int count=0;
    bool HookBL(void* address,void* callback,void** original){
        assert(address==reinterpret_cast<void*>(base+penetration::callOffset));assert(callback==reinterpret_cast<void*>(penetration::ray));++count;
        if(!okay)return false;*original=reinterpret_cast<void*>(worldMock);return true;
    }
};
}
int main(){
    using namespace penetration;
    originalRay=worldMock;bullets=entries;ignoredEntity=&ignored;penetration::localPed=selfMock;
    sniper::clientReady=true;options::penetrationEnabled=true;
    auto& b=entries[7];b.active=1;b.type=34;b.owner=&self;b.position={0,0,0};b.damage=125;b.expires=12345;
    Bullet saved=b;
    assert(fire());assert(!flags[0]&&flags[1]&&flags[2]&&!flags[3]&&!flags[4]&&!flags[5]&&flags[6]&&!flags[7]);
    assert(filtered==1&&hits==1&&!std::memcmp(&saved,&b,sizeof(b))); // no ammo, damage, position or expiry edits
    hasWall=false;hasProp=true;assert(fire());hasVehicle=true;assert(!fire());assert(flags[1]);
    hasVehicle=false;hasWall=true;
    entries[0]=b;entries[0].type=33;assert(!fire()&&flags[0]);entries[0].active=0; // ambiguous same-owner source
    b.type=33;assert(!fire()&&flags[0]&&flags[3]&&flags[4]);b.type=34;
    b.owner=&other;assert(!fire()&&flags[0]);b.owner=&self;
    ignored=&other;assert(!fire()&&flags[0]);ignored=&self;
    b.active=0;assert(!fire());b.active=1;
    b.position.x=1;assert(!fire());b.position.x=0;
    options::penetrationEnabled=false;assert(!fire());options::penetrationEnabled=true;
    sniper::clientReady=false;assert(!fire());sniper::clientReady=true;
    assert(!fire(false)&&!flags[2]); // don't enable ped collisions when the caller disabled them
    assert(!eligible({NAN,0,0},{20,0,0},&self,&self,entries));
    assert(!eligible({0,0,0},{20,0,0},nullptr,&self,entries));
    std::vector<uint8_t> code(callOffset+4,0xaa);uintptr_t base=reinterpret_cast<uintptr_t>(code.data());Loader loader{base};
    uint32_t instruction=expectedCall;std::memcpy(code.data()+callOffset,&instruction,4);assert(hookCall(&loader,base)&&loader.count==1);
    instruction=0;std::memcpy(code.data()+callOffset,&instruction,4);assert(!hookCall(&loader,base)&&loader.count==1);
    instruction=expectedCall;std::memcpy(code.data()+callOffset,&instruction,4);loader.okay=false;assert(!hookCall(&loader,base));
    std::cout<<"Penetration passed: wall/prop bypass, local sniper scope, other bullets preserved, vehicle collisions, native outputs/damage and guarded call-site hook\n";
}
