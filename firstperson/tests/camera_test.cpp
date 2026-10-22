// Host-only tests: the Android/JNI substitutes never enter the Android build.
#include "../main.cpp"
#include <cassert>
#include <limits>
extern "C" int __android_log_write(int,const char*,const char*) { return 0; }
namespace {
alignas(8) uint8_t fakeCamera[3504]{}, fakeMatrix[80]{}, fakeRw[432]{}, fakePlayer[2456]{}, fakePedMatrix[80]{};
RwMatrix bones[6]{};
Hierarchy fakeHierarchy{0,6,bones};
int mode=4, renders=0, copies=0, frame=0;
bool cinematic=false, badBone=false, missingHierarchy=false, expectAlignment=false;
Vec nativePos{2,3,4}, nativeSource{1,2,3}, nativeGame{4,5,6}, nativeFront{0,1,0};
Basis nativeView{{1,0,0},{0,1,0},{0,0,1}};
Basis initialHead{{0,0,1},{0,1,0},{-1,0,0}};
bool near(float a,float b) { return std::fabs(a-b)<0.0001f; }
bool same(Vec a,Vec b) { return near(a.x,b.x)&&near(a.y,b.y)&&near(a.z,b.z); }
void* playerMock(int) { return fakePlayer; }
void boneMock(void* p,Vec& out,unsigned id,bool update) {
    assert(p==fakePlayer && id==5 && update);
    out=badBone ? Vec{999,999,999} : bones[1].pos;
}
void* hierarchyMock(void* clump) { assert(clump==fakePedMatrix); return missingHierarchy ? nullptr : &fakeHierarchy; }
int boneIndexMock(void* h,int tag) { assert(h==&fakeHierarchy); return tag>=5&&tag<=8 ? tag-4 : -1; }
void* matrixMock(void* m) { set(m,12,uint32_t{0}); return m; }
void* clipMock(void* rw,float n) { assert(rw==fakeRw); set(rw,168,n); return rw; }
void copyMock(void* c,bool update) {
    assert(c==fakeCamera && update); ++copies;
    Vec eye{};
    assert(headcam::eyePosition(bones[1].pos,position(fakePlayer),basis(fakeMatrix),eye));
    assert(same(get<Vec>(fakeMatrix,48),eye));
    assert(same(get<Vec>(fakeCamera,2464),eye));
}
void derivedMock(void* c,bool mirror,bool oriented) { assert(c==fakeCamera && !mirror && !oriented); }
void renderMock(void* p) {
    ++renders;
    if(expectAlignment && p==fakePlayer) {
        Basis expected=headcam::headBasis(frameView);
        assert(same(bones[1].right,expected.right));
        assert(same(bones[1].up,expected.forward));
        assert(same(bones[1].at,expected.up));
        assert(headcam::orthogonal(basis(&bones[1])));
        // Facial bone offsets rotate rigidly instead of collapsing geometry.
        assert(near(headcam::norm2(headcam::sub(bones[2].pos,bones[1].pos)),0.01f));
        assert(bones[1].flags==0);
        assert(bones[0].flags==0x1234); // body/neck matrix untouched
    }
}
void processMock(void* p) {
    assert(p==fakeCamera);
    if(frame++) {
        assert(same(get<Vec>(fakeMatrix,48),nativePos));
        assert(same(get<Vec>(fakeCamera,2464),nativePos));
        assert(same(get<Vec>(fakeCamera,376+372),nativeSource));
        assert(same(get<Vec>(fakeCamera,2276),nativeGame));
        assert(get<float>(fakeRw,168)==0.9f);
        assert(same(get<Vec>(fakeMatrix,0),nativeView.right));
    }
    set(fakeCamera,49,true); set(fakeCamera,67,cinematic);
    set(fakeCamera,95,uint8_t{0}); set(fakeCamera,24,static_cast<void*>(fakeMatrix));
    set(fakeCamera,2352,static_cast<void*>(fakeRw));
    for(void* m : {static_cast<void*>(fakeMatrix),static_cast<void*>(fakeCamera+2416)}) {
        set(m,0,nativeView.right); set(m,16,nativeView.forward); set(m,32,nativeView.up);
        set(m,48,nativePos);
    }
    set(fakeCamera,376+372,nativeSource); set(fakeCamera,2276,nativeGame);
    set(fakeCamera,376+14,static_cast<int16_t>(mode));
    set(fakeCamera,376+360,nativeFront); set(fakeCamera,376+396,nativeView.up);
    set(fakeCamera,376+504,static_cast<void*>(fakePlayer));
    set(fakeRw,168,0.9f);
}
}
int main() {
    camera=fakeCamera; originalProcess=processMock; originalRender=renderMock;
    findPlayer=playerMock; bonePosition=boneMock; hierarchyFromClump=hierarchyMock;
    boneIndex=boneIndexMock; matrixUpdate=matrixMock;
    copyToRw=copyMock; derivedValues=derivedMock; nearClip=clipMock;
    set(fakePlayer,24,static_cast<void*>(fakePedMatrix));
    set(fakePlayer,32,static_cast<void*>(fakePedMatrix));
    set(fakePlayer,1708,100.0f); set(fakePedMatrix,48,Vec{10,20,30});
    for(auto& b:bones) {
        b.right=initialHead.right; b.up=initialHead.forward; b.at=initialHead.up;
        b.flags=0x1234; b.pos={10,20,31.7f};
    }
    bones[2].pos.x+=0.1f;
    RwMatrix before[6]{}; std::memcpy(before,bones,sizeof(bones));
    processCamera(fakeCamera);
    assert(active && copies==1 && near(get<float>(fakeRw,168),0.053333335f));
    expectAlignment=true; renderPed(fakePlayer); assert(renders==1);
    assert(std::memcmp(before,bones,sizeof(bones))==0); // restores every matrix byte
    renderPed(fakePedMatrix); assert(renders==2);
    // A side look rotates the head but leaves all body bones as they were.
    nativeFront={1,0,0}; mode=18; processCamera(fakeCamera);
    assert(active && copies==2);
    renderPed(fakePlayer); assert(std::memcmp(before,bones,sizeof(bones))==0);
    // Mirrors use the unmodified head, and must not inherit a prior draw's changes.
    set(fakeCamera,2408,true); expectAlignment=false; renderPed(fakePlayer);
    assert(std::memcmp(before,bones,sizeof(bones))==0); set(fakeCamera,2408,false);
    mode=7; processCamera(fakeCamera); assert(!active && copies==2);
    renderPed(fakePlayer);
    mode=4; cinematic=true; processCamera(fakeCamera); assert(!active);
    cinematic=false; badBone=true; processCamera(fakeCamera); assert(!active);
    badBone=false; missingHierarchy=true; processCamera(fakeCamera); assert(!active);
    missingHierarchy=false; set(fakePlayer,1708,0.0f); processCamera(fakeCamera); assert(!active);
    Basis view{};
    assert(!headcam::viewBasis({0,0,0},{0,0,1},{0,0,1},view));
    Vec out{};
    assert(!headcam::eyePosition({0,0,std::numeric_limits<float>::quiet_NaN()},{0,0,0},nativeView,out));
    assert(headcam::viewBasis({0,1,0},{0,0,1},{1,0,0},view));
    // Roll is bounded to 15 degrees even when animation leans 90 degrees.
    assert(near(std::acos(headcam::dot(view.up,Vec{0,0,1})),0.2617994f));
    assert(headcam::orthogonal(headcam::headBasis(view)));
}
