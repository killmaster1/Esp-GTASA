// Host-only tests: Android/JNI declarations are substitutes, never shipped in the .so.
#include "../main.cpp"
#include <cassert>
#include <limits>
extern "C" int __android_log_write(int,const char*,const char*) { return 0; }
namespace {
alignas(8) uint8_t fakeCamera[3504]{}, fakeMatrix[80]{}, fakeRw[432]{}, fakePlayer[2456]{}, fakePedMatrix[80]{};
int mode=4, renders=0, copies=0, frame=0;
bool cinematic=false, badBone=false;
Vec nativePos{2,3,4}, nativeSource{1,2,3}, nativeGame{4,5,6};
Vec eyeExpected{10,20.1f,31.73f};
void* playerMock(int) { return fakePlayer; }
void boneMock(void* p,Vec& out,unsigned id,bool update) {
    assert(p==fakePlayer && id==5 && update);
    out=badBone ? Vec{999,999,999} : Vec{10,20,31.7f};
}
void* clipMock(void* rw,float n) { assert(rw==fakeRw); set(rw,168,n); return rw; }
void copyMock(void* c,bool update) {
    assert(c==fakeCamera && update); ++copies;
    auto pos=get<Vec>(fakeMatrix,48);
    assert(std::fabs(pos.y-eyeExpected.y)<0.0001f);
}
void derivedMock(void* c,bool mirror,bool oriented) { assert(c==fakeCamera && !mirror && !oriented); }
void renderMock(void*) { ++renders; }
void processMock(void* p) {
    assert(p==fakeCamera);
    // Every frame must start with the engine's state, not our prior eye offset.
    if(frame++) {
        assert(get<Vec>(fakeMatrix,48).x==nativePos.x);
        assert(get<Vec>(fakeCamera,2464).z==nativePos.z);
        assert(get<Vec>(fakeCamera,376+372).z==nativeSource.z);
        assert(get<Vec>(fakeCamera,2276).z==nativeGame.z);
        assert(get<float>(fakeRw,168)==0.9f);
    }
    set(fakeCamera,49,true); set(fakeCamera,67,cinematic);
    set(fakeCamera,95,uint8_t{0}); set(fakeCamera,24,static_cast<void*>(fakeMatrix));
    set(fakeCamera,2352,static_cast<void*>(fakeRw));
    set(fakeMatrix,48,nativePos); set(fakeCamera,2464,nativePos);
    set(fakeCamera,376+372,nativeSource); set(fakeCamera,2276,nativeGame);
    set(fakeCamera,376+14,static_cast<int16_t>(mode));
    set(fakeCamera,376+360,Vec{0,1,0});
    set(fakeCamera,376+504,static_cast<void*>(fakePlayer));
    set(fakeRw,168,0.9f);
}
}
int main() {
    camera=fakeCamera;
    originalProcess=processMock; originalRender=renderMock;
    findPlayer=playerMock; bonePosition=boneMock;
    copyToRw=copyMock; derivedValues=derivedMock; nearClip=clipMock;
    set(fakePlayer,24,static_cast<void*>(fakePedMatrix));
    set(fakePlayer,32,static_cast<void*>(fakePedMatrix));
    set(fakePlayer,1708,100.0f); set(fakePedMatrix,48,Vec{10,20,30});
    processCamera(fakeCamera);
    assert(active && copies==1 && std::fabs(get<float>(fakeRw,168)-0.06f)<0.0001f);
    renderPed(fakePlayer); assert(renders==0);
    renderPed(fakePedMatrix); assert(renders==1);
    mode=18; processCamera(fakeCamera); assert(active && copies==2);
    mode=7; processCamera(fakeCamera); assert(!active && copies==2);
    renderPed(fakePlayer); assert(renders==2);
    mode=4; cinematic=true; processCamera(fakeCamera); assert(!active);
    cinematic=false; badBone=true; processCamera(fakeCamera); assert(!active);
    badBone=false; set(fakePlayer,1708,0.0f); processCamera(fakeCamera); assert(!active);
    Vec out{};
    assert(!headcam::eyePosition({0,0,0},{0,0,0},{0,0,0},out));
    assert(!headcam::eyePosition({0,0,std::numeric_limits<float>::quiet_NaN()},{0,0,0},{0,1,0},out));
}
