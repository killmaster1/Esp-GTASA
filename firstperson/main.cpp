#include <mod/amlmod.h>
#include <android/log.h>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "camera_math.h"

MYMOD(local.headcam.sa210, Head Camera SA210 ARM64, 0.2.1, Custom)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.4.0)
END_DEPLIST()

namespace {
using headcam::Vec;
using headcam::Basis;
static_assert(sizeof(void*)==8,"ARM64 only");
static_assert(sizeof(Vec)==12,"GTA vector ABI");
// All offsets below were read from the supplied ARM64 library's DWARF.
constexpr size_t placeableMatrix=24, pedRwObject=32, pedHealth=1708, pedVehicle=1800;
constexpr size_t cameraActive=95, cameraCams=376, camSize=552;
constexpr size_t camMode=14, camFront=360, camSource=372, camUp=396, camTarget=504;
constexpr size_t gamePosition=2276, rwCamera=2352, cameraMatrix=2416, matrixPos=48;
uint8_t* camera=nullptr;
void (*originalProcess)(void*)=nullptr;
void (*originalRender)(void*)=nullptr;
void* (*findPlayer)(int)=nullptr;
void (*bonePosition)(void*,Vec&,unsigned,bool)=nullptr;
void* (*hierarchyFromClump)(void*)=nullptr;
int (*boneIndex)(void*,int)=nullptr;
void* (*matrixUpdate)(void*)=nullptr;
void (*copyToRw)(void*,bool)=nullptr;
void (*derivedValues)(void*,bool,bool)=nullptr;
void* (*nearClip)(void*,float)=nullptr;
bool active=false;
void* firstPersonPlayer=nullptr;
Basis frameView{};
// RenderWare ABI verified in the uploaded library: RwMatrixTag=64 bytes.
struct RwMatrix {
    Vec right; uint32_t flags;
    Vec up; uint32_t pad1;
    Vec at; uint32_t pad2;
    Vec pos; uint32_t pad3;
};
static_assert(sizeof(RwMatrix)==64 && offsetof(RwMatrix,pos)==48,"RwMatrix ABI");
struct Hierarchy {
    int32_t flags, count;
    RwMatrix* matrices;
};
static_assert(offsetof(Hierarchy,matrices)==8,"RpHAnimHierarchy ABI");

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
Basis basis(const void* matrix) { return {get<Vec>(matrix,0),get<Vec>(matrix,16),get<Vec>(matrix,32)}; }
Hierarchy* hierarchy(void* ped) {
    void* clump=get<void*>(ped,pedRwObject);
    if(!clump) return nullptr;
    auto* h=static_cast<Hierarchy*>(hierarchyFromClump(clump));
    if(!h || !h->matrices || h->count<1 || h->count>256) return nullptr;
    return h;
}
RwMatrix* headMatrix(Hierarchy* h) {
    if(!h) return nullptr;
    int i=boneIndex(h,5);
    return i>=0 && i<h->count ? &h->matrices[i] : nullptr;
}
struct Saved {
    bool valid=false;
    uint8_t index=0;
    void* matrix=nullptr;
    void* rw=nullptr;
    RwMatrix matrixValue{},embeddedValue{};
    Vec source{},front{},up{},game{};
    float clip=0.9f;
} saved;

void restoreNativeState() {
    if (!saved.valid) return;
    // Restore the native camera's last position before it computes the next frame.
    // This prevents the follow-camera controller feeding our eye offset into itself.
    set(camera,cameraMatrix,saved.embeddedValue);
    set(camera,gamePosition,saved.game);
    uint8_t* cam=camera+cameraCams+saved.index*camSize;
    set(cam,camSource,saved.source); set(cam,camFront,saved.front); set(cam,camUp,saved.up);
    if (saved.matrix && get<void*>(camera,placeableMatrix)==saved.matrix)
        set(saved.matrix,0,saved.matrixValue);
    if (saved.rw && get<void*>(camera,rwCamera)==saved.rw)
        nearClip(saved.rw,saved.clip);
    saved.valid=false;
}
void processCamera(void* self) {
    if (self!=camera) { originalProcess(self); return; }
    restoreNativeState();
    active=false; firstPersonPlayer=nullptr;
    originalProcess(self);
    // Leave scripted cameras, cinematics and mirror rendering under game control.
    if (!get<bool>(camera,49) || get<bool>(camera,67) || get<bool>(camera,2408)) return;
    uint8_t index=get<uint8_t>(camera,cameraActive);
    if (index>=3) return;
    uint8_t* cam=camera+cameraCams+index*camSize;
    int16_t mode=get<int16_t>(cam,camMode);
    // Walk, follow-car and ordinary aim. Scope/rocket/scripted modes stay native.
    const bool commonAim=(mode==5 || mode==53 || mode==55);
    if(mode!=4 && mode!=18 && !commonAim) return;
    void* player=findPlayer(-1);
    if (!player || !get<void*>(player,pedRwObject)) return;
    float health=get<float>(player,pedHealth);
    if (!std::isfinite(health) || health<=0.0f) return;
    void* target=get<void*>(cam,camTarget);
    void* vehicle=get<void*>(player,pedVehicle);
    // Aim cameras may store the enemy as target; the view still belongs to CJ.
    if(!commonAim && target!=player && (!vehicle || target!=vehicle)) return;
    void* matrix=get<void*>(camera,placeableMatrix);
    void* rw=get<void*>(camera,rwCamera);
    if (!matrix || !rw) return;
    // Bone tag 5 is the head. CPed::GetBonePosition is an INSTANCE method:
    // x0=player, x1=out, w2=bone tag, w3=update skin.
    Vec head{};
    bonePosition(player,head,5,true);
    Vec eye{}; Basis view{};
    // CCam gives world-space Front/Up directly. Do not reinterpret CCamera's
    // internal matrix columns as a camera basis: it uses engine-specific axes.
    if(!headcam::viewBasis(get<Vec>(cam,camFront),get<Vec>(cam,camUp),view)) return;
    if(!headcam::eyePosition(head,position(player),view,eye)) return;
    float oldClip=get<float>(rw,168);
    if (!std::isfinite(oldClip) || oldClip<=0.0f) return;
    saved.index=index; saved.matrix=matrix; saved.rw=rw; saved.clip=oldClip;
    saved.matrixValue=get<RwMatrix>(matrix,0);
    saved.embeddedValue=get<RwMatrix>(camera,cameraMatrix);
    saved.source=get<Vec>(cam,camSource);
    saved.front=get<Vec>(cam,camFront); saved.up=get<Vec>(cam,camUp);
    saved.game=get<Vec>(camera,gamePosition);
    saved.valid=true;
    // Move only the origin, as in the previously working 0.1 camera.
    // Preserve every orientation column exactly as produced by GTA.
    set(matrix,matrixPos,eye);
    set(camera,cameraMatrix+matrixPos,eye);
    set(camera,gamePosition,eye);
    set(cam,camSource,eye);
    nearClip(rw,0.053333335f);
    copyToRw(camera,true);
    derivedValues(camera,false,false);
    active=true; firstPersonPlayer=player; frameView=view;
}
void renderPed(void* self) {
    if(!active || self!=firstPersonPlayer || get<bool>(camera,2408)) {
        originalRender(self); return;
    }
    // Align only the head and its standard facial bones to the look direction.
    // No alpha changes, zero scaling or suppression of the player render call.
    Hierarchy* h=hierarchy(self);
    RwMatrix* head=headMatrix(h);
    if(!head || !headcam::orthogonal(basis(head)) || !headcam::finite(head->pos)) {
        originalRender(self); return;
    }
    Basis oldHead=basis(head), desiredHead=headcam::headBasis(frameView);
    Vec pivot=head->pos;
    RwMatrix* changed[4]{};
    RwMatrix backups[4]{};
    int count=0;
    for(int tag : {5,6,7,8}) { // head, left/right brow, jaw
        int i=boneIndex(h,tag);
        if(i<0 || i>=h->count) continue;
        RwMatrix* m=&h->matrices[i];
        bool duplicate=false;
        for(int j=0;j<count;++j) if(changed[j]==m) duplicate=true;
        if(duplicate || !headcam::orthogonal(basis(m)) || !headcam::finite(m->pos)
           || headcam::norm2(headcam::sub(m->pos,pivot))>1.0f) continue;
        changed[count]=m; backups[count]=*m; ++count;
    }
    for(int i=0;i<count;++i) {
        RwMatrix* m=changed[i];
        const auto& old=backups[i];
        m->right=headcam::rotate(old.right,oldHead,desiredHead);
        m->up=headcam::rotate(old.up,oldHead,desiredHead);
        m->at=headcam::rotate(old.at,oldHead,desiredHead);
        m->pos=headcam::add(pivot,headcam::rotate(headcam::sub(old.pos,pivot),oldHead,desiredHead));
        matrixUpdate(m);
    }
    originalRender(self); // torso, arms and legs keep the game's regular rendering
    for(int i=0;i<count;++i) *changed[i]=backups[i];
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
        "_Z24RwCameraSetNearClipPlaneP8RwCameraf",
        "_Z29GetAnimHierarchyFromSkinClumpP7RpClump",
        "_Z17RpHAnimIDGetIndexP16RpHAnimHierarchyi",
        "_Z14RwMatrixUpdateP11RwMatrixTag"};
    constexpr uintptr_t offsets[]{0xbba8d0,0x4bab78,0x59d3b8,0x4efae0,0x59aee4,0x4bca24,0x4bc93c,0x26cf8c,0x6f54cc,0x255be8,0x27e218};
    uintptr_t symbols[11]{};
    for (int i=0;i<11;++i) {
        symbols[i]=aml->GetSym(handle,names[i]);
        if (!symbols[i]) { log("Required symbol missing; disabled"); return; }
    }
    uintptr_t base=symbols[0]-offsets[0];
    for (int i=1;i<11;++i) {
        if (symbols[i]-base!=offsets[i]) { log("Unsupported library layout; disabled"); return; }
    }
    camera=reinterpret_cast<uint8_t*>(symbols[0]);
    findPlayer=reinterpret_cast<decltype(findPlayer)>(symbols[3]);
    bonePosition=reinterpret_cast<decltype(bonePosition)>(symbols[4]);
    copyToRw=reinterpret_cast<decltype(copyToRw)>(symbols[5]);
    derivedValues=reinterpret_cast<decltype(derivedValues)>(symbols[6]);
    nearClip=reinterpret_cast<decltype(nearClip)>(symbols[7]);
    hierarchyFromClump=reinterpret_cast<decltype(hierarchyFromClump)>(symbols[8]);
    boneIndex=reinterpret_cast<decltype(boneIndex)>(symbols[9]);
    matrixUpdate=reinterpret_cast<decltype(matrixUpdate)>(symbols[10]);
    // Install rendering first. On failure it remains a transparent pass-through.
    if (!aml->Hook(reinterpret_cast<void*>(symbols[2]),reinterpret_cast<void*>(renderPed),reinterpret_cast<void**>(&originalRender))) {
        log("Ped render hook failed; disabled"); return;
    }
    if (!aml->Hook(reinterpret_cast<void*>(symbols[1]),reinterpret_cast<void*>(processCamera),reinterpret_cast<void**>(&originalProcess))) {
        log("Camera hook failed; disabled"); return;
    }
    log("HeadCam 0.2.1 enabled: native camera axes, visible body, head alignment, ordinary aim");
}
