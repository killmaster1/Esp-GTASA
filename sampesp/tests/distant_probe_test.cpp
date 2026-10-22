#include "../main.cpp"
#include <cassert>
#include <iostream>
extern "C" int __android_log_write(int,const char*,const char*){return 0;}
namespace {
uint8_t remote[320]{};int positionCalls=0,inactiveCalls=0;
void pos(void* r,int x,int y,int z){assert(r==remote&&x==65535&&y==32768&&z==3000);++positionCalls;}
void inactiveMock(void* r){assert(r==remote);++inactiveCalls;}
template<class T>void put(size_t offset,T value){std::memcpy(remote+offset,&value,sizeof(value));}
}
int main(){
    distantprobe::Cache c;distantprobe::Sample out;
    assert(c.update(42,100,-200,5,false,100));assert(c.get(42,101,out)&&out.x==100&&out.y==-200&&!out.hasWrapper);
    assert(c.freshWithoutWrapper(100)==1);assert(!c.get(42,99,out)&&!c.get(42,5101,out));
    assert(!c.update(42,110,-210,6,false,200));assert(c.get(42,201,out)&&out.x==110);
    assert(c.update(42,110,-210,6,true,300));assert(c.freshWithoutWrapper(300)==0);
    assert(c.invalidate(42)&&!c.get(42,301,out));assert(!c.update(1004,0,0,0,false,100));
    distantprobe::Cache bounded;for(unsigned i=0;i<1004;++i)bounded.update(i,int(i),0,0,false,100);
    assert(bounded.get(1003,100,out)&&out.x==1003&&bounded.freshWithoutWrapper(100)==1004);
    distantprobe::originalPosition=pos;distantprobe::originalInactive=inactiveMock;
    put(182,uint16_t(42));put(288,static_cast<void*>(nullptr));distantprobe::position(remote,65535,32768,3000);
    assert(positionCalls==1&&distantprobe::cache.get(42,diagnostics::millis(),out));
    assert(out.x==-1&&out.y==-32768&&out.z==3000&&!out.hasWrapper&&distantprobe::withoutWrapper==1);
    distantprobe::deactivate(remote);assert(inactiveCalls==1&&!distantprobe::cache.get(42,diagnostics::millis(),out));
    std::cout<<"Distant probe passed: signed coordinates, no-wrapper observations, expiry, inactive invalidation, full ID bounds, throttling and unchanged native calls\n";
}
