#include <mod/amlmod.h>
#include <android/log.h>
#include <cstdint>
#include <cstring>
#include "camera_math.h"

MYMOD(local.headcam.sa210, Head Camera SA210 ARM64, 0.1.0, Custom)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.4.0)
END_DEPLIST()

namespace {
using headcam::Vec;
static_assert(sizeof(void*)==8,"ARM64 only");
static_assert(sizeof(Vec)==12,"GTA vector ABI");
// All offsets below were read from the supplied ARM64 library's DWARF.
constexpr size_t placeableMatrix=24, pedRwObject=32, pedHealth=1708, pedVehicle=1800;
constexpr size_t cameraActive=95, cameraCams=376, camSize=552;
constexpr size_t camMode=14, camFront=360, camSource=372, camTarget=504;
constexpr size_t gamePosition=2276, rwCamera=2352, cameraMatrix=2416, matrixPos=48;
uint8_t* camera=nullptr;
void (*originalProcess)(void*)=nullptr;
void (*originalRender)(void*)=nullptr;
void* (*findPlayer)(int)=nullptr;
void (*bonePosition)(void*,Vec&,unsigned,bool)=nullptr;
void (*copyToRw)(void*,bool)=nullptr;
void (*derivedValues)(void*,bool,bool)=nullptr;
void* (*nearClip)(void*,float)=nullptr;
bool active=false;
void* hiddenPlayer=nullptr;

template<class T> T get(const void* p,size_t off) {
    T v; std::memcpy(&v,static_cast<const uint8_t*>(p)+off,sizeof(v)); return v;
}
template<class T> void set(void* p,size_t off,const T& v) {
    std::memcpy(static_cast<uint8_t*>(p)+off,&v,sizeof(v));
}
Vec position(const void* p) {
    void* matrix=get<void*>(p,placeableMatrix);
    return matrix ? get<Vec>(matrix,matrixPos) : get<Vec>(p,8);
}
struct Saved {
    bool valid=false;
    uint8_t index=0;
    void* matrix=nullptr;
    void* rw=nullptr;
    Vec matrixPosition{},embeddedPosition{},source{},game{};
    float clip=0.9f;
} saved;

void restoreNativeState() {
    if (!saved.valid) return;
    // Restore the native camera's last position before it computes the next frame.
    // This prevents the follow-camera controller feeding our eye offset into itself.
    set(camera,cameraMatrix+matrixPos,saved.embeddedPosition);
    set(camera,gamePosition,saved.game);
    set(camera,cameraCams+saved.index*camSize+camSource,saved.source);
    if (saved.matrix && get<void*>(camera,placeableMatrix)==saved.matrix)
        set(saved.matrix,matrixPos,saved.matrixPosition);
    if (saved.rw && get<void*>(camera,rwCamera)==saved.rw)
        nearClip(saved.rw,saved.clip);
    saved.valid=false;
}
void processCamera(void* self) {
    if (self!=camera) { originalProcess(self); return; }
    restoreNativeState();
    active=false; hiddenPlayer=nullptr;
    originalProcess(self);
    // Leave scripted cameras, cinematics and mirror rendering under game control.
    if (!get<bool>(camera,49) || get<bool>(camera,67) || get<bool>(camera,2408)) return;
    uint8_t index=get<uint8_t>(camera,cameraActive);
    if (index>=3) return;
    uint8_t* cam=camera+cameraCams+index*camSize;
    int16_t mode=get<int16_t>(cam,camMode);
    if (mode!=4 && mode!=18) return; // MODE_FOLLOWPED / MODE_CAM_ON_A_STRING
    void* player=findPlayer(-1);
    if (!player || !get<void*>(player,pedRwObject)) return;
    float health=get<float>(player,pedHealth);
    if (!std::isfinite(health) || health<=0.0f) return;
    void* target=get<void*>(cam,camTarget);
    void* vehicle=get<void*>(player,pedVehicle);
    if (target!=player && (!vehicle || target!=vehicle)) return;
    void* matrix=get<void*>(camera,placeableMatrix);
    void* rw=get<void*>(camera,rwCamera);
    if (!matrix || !rw) return;
    // Bone tag 5 is the head. CPed::GetBonePosition is an INSTANCE method:
    // x0=player, x1=out, w2=bone tag, w3=update skin.
    Vec head{};
    bonePosition(player,head,5,true);
    Vec front=get<Vec>(cam,camFront), eye{};
    if (!headcam::eyePosition(head,position(player),front,eye)) return;
    float oldClip=get<float>(rw,168);
    if (!std::isfinite(oldClip) || oldClip<=0.0f) return;
    saved.index=index; saved.matrix=matrix; saved.rw=rw; saved.clip=oldClip;
    saved.matrixPosition=get<Vec>(matrix,matrixPos);
    saved.embeddedPosition=get<Vec>(camera,cameraMatrix+matrixPos);
    saved.source=get<Vec>(cam,camSource);
    saved.game=get<Vec>(camera,gamePosition);
    saved.valid=true;
    // Keep native orientation/touch look and move only the view's origin.
    set(matrix,matrixPos,eye);
    set(camera,cameraMatrix+matrixPos,eye);
    set(camera,gamePosition,eye);
    set(cam,camSource,eye);
    nearClip(rw,0.06f);
    copyToRw(camera,true);
    derivedValues(camera,false,false);
    active=true; hiddenPlayer=player;
}
void renderPed(void* self) {
    // Hide the local player model only while the head camera is active.
    // Skin animation/PreRender still runs. NPCs remain visible.
    if (active && self==hiddenPlayer && !get<bool>(camera,2408)) return;
    originalRender(self);
}
void log(const char* s) { __android_log_write(ANDROID_LOG_INFO,"HeadCam",s); }
}
extern "C" JNIEXPORT void OnAllModsLoaded() {
    if (!aml) return;
    void* handle=aml->GetLibHandle("libGTASA.so");
    if (!handle) { log("No GTA library; disabled"); return; }
    const char* names[]{"TheCamera","_ZN7CCamera7ProcessEv","_ZN4CPed6RenderEv",
        "_Z13FindPlayerPedi","_ZN4CPed15GetBonePositionER5RwV3djb",
        "_ZN7CCamera23CopyCameraMatrixToRWCamEb","_ZN7CCamera22CalculateDerivedValuesEbb",
        "_Z24RwCameraSetNearClipPlaneP8RwCameraf"};
    constexpr uintptr_t offsets[]{0xbba8d0,0x4bab78,0x59d3b8,0x4efae0,0x59aee4,0x4bca24,0x4bc93c,0x26cf8c};
    uintptr_t symbols[8]{};
    for (int i=0;i<8;++i) {
        symbols[i]=aml->GetSym(handle,names[i]);
        if (!symbols[i]) { log("Required symbol missing; disabled"); return; }
    }
    uintptr_t base=symbols[0]-offsets[0];
    for (int i=1;i<8;++i) {
        if (symbols[i]-base!=offsets[i]) { log("Unsupported library layout; disabled"); return; }
    }
    camera=reinterpret_cast<uint8_t*>(symbols[0]);
    findPlayer=reinterpret_cast<decltype(findPlayer)>(symbols[3]);
    bonePosition=reinterpret_cast<decltype(bonePosition)>(symbols[4]);
    copyToRw=reinterpret_cast<decltype(copyToRw)>(symbols[5]);
    derivedValues=reinterpret_cast<decltype(derivedValues)>(symbols[6]);
    nearClip=reinterpret_cast<decltype(nearClip)>(symbols[7]);
    // Install rendering first. On failure it remains a transparent pass-through.
    if (!aml->Hook(reinterpret_cast<void*>(symbols[2]),reinterpret_cast<void*>(renderPed),reinterpret_cast<void**>(&originalRender))) {
        log("Ped render hook failed; disabled"); return;
    }
    if (!aml->Hook(reinterpret_cast<void*>(symbols[1]),reinterpret_cast<void*>(processCamera),reinterpret_cast<void**>(&originalProcess))) {
        log("Camera hook failed; disabled"); return;
    }
    log("HeadCam 0.1.0 enabled: walking, car entry/exit, driving; no weapon camera changes");
}
