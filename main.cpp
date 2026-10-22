#include <mod/amlmod.h>
#include <android/log.h>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstddef>

MYMOD(local.npclines.sa210, NPC Lines SA210 ARM64, 0.1.1, Custom)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.4.0)
END_DEPLIST()

namespace {
struct Vec { float x, y, z; };
struct Color { uint8_t r, g, b, a; };
// Layouts verified in the uploaded library's DWARF. Do not reuse on other builds.
struct Pool { void* storage; uint8_t* flags; int32_t size, freeIndex; bool own, noMemory; };
static_assert(offsetof(Pool, flags) == 8 && offsetof(Pool, size) == 16);
static_assert(sizeof(void*) == 8, "ARM64 only");
void (*originalHud)() = nullptr;
void* (*findPlayer)(int) = nullptr;
void* (*getPed)(int) = nullptr;
bool (*project)(const Vec&, Vec*, float*, float*, bool, bool) = nullptr;
// Static Draw2DPolygon: eight float arguments in FP registers, color in x0.
// DrawTriangle is an instance method and cannot be called with this ABI.
void (*polygon)(float, float, float, float, float, float, float, float, const Color&) = nullptr;
Pool** poolPtr = nullptr;
uint8_t* screen = nullptr;
constexpr float maxDistance = 100.0f;
constexpr float thickness = 1.5f;
constexpr Color color{50, 255, 90, 220};

template<class T> T read(const void* obj, size_t offset) {
    T value; std::memcpy(&value, static_cast<const uint8_t*>(obj) + offset, sizeof(value));
    return value;
}
bool finite(Vec v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
Vec position(const void* ped) {
    const void* matrix = read<void*>(ped, 24);
    return matrix ? read<Vec>(matrix, 48) : read<Vec>(ped, 8);
}
void line(float x0, float y0, float x1, float y1) {
    float dx=x1-x0, dy=y1-y0;
    float len=std::sqrt(dx*dx+dy*dy);
    if (!std::isfinite(len) || len < 1.0f) return;
    float nx=-dy/len*thickness*0.5f, ny=dx/len*thickness*0.5f;
    // SetVertices emits corners in order 3,4,2,1 for the triangle fan.
    // Supply the two ends as pairs to form a rectangle without crossed edges.
    polygon(x0+nx,y0+ny, x0-nx,y0-ny,
            x1+nx,y1+ny, x1-nx,y1-ny, color);
}
void drawHud() {
    originalHud();
    void* player=findPlayer(-1);
    Pool* pool=*poolPtr;
    if (!player || !pool || !pool->storage || !pool->flags || pool->size<=0 || pool->size>100000) return;
    int w=read<int>(screen,8), h=read<int>(screen,12);
    if (w<1 || h<1 || w>32768 || h>32768) return;
    Vec origin=position(player);
    if (!finite(origin)) return;
    int drawn=0;
    for (int i=0;i<pool->size && drawn<128;++i) {
        uint8_t flag=pool->flags[i];
        if (flag & 0x80) continue;
        // Use the game's accessor so FLA can maintain its own allocation stride.
        void* ped=getPed((i<<8) | flag);
        if (!ped || ped==player || read<float>(ped,1708)<=0.0f) continue;
        Vec p=position(ped);
        if (!finite(p)) continue;
        float dx=p.x-origin.x,dy=p.y-origin.y,dz=p.z-origin.z;
        if (dx*dx+dy*dy+dz*dz>maxDistance*maxDistance) continue;
        p.z+=0.8f;
        Vec out{}; float sx{},sy{};
        if (!project(p,&out,&sx,&sy,true,true) || !finite(out) || out.z<=0.0f) continue;
        if (out.x<0 || out.x>w || out.y<0 || out.y>h) continue;
        line(w*0.5f,h-2.0f,out.x,out.y); ++drawn;
    }
}
void log(const char* s) { __android_log_write(ANDROID_LOG_INFO,"NPCLines",s); }
}
extern "C" JNIEXPORT void OnAllModsLoaded() {
    if (!aml) return;
    void* handle=aml->GetLibHandle("libGTASA.so");
    if (!handle) { log("libGTASA not loaded; disabled"); return; }
    const char* names[]{"_ZN4CHud4DrawEv","_Z13FindPlayerPedi","_ZN6CPools6GetPedEi",
      "_ZN7CSprite15CalcScreenCoorsERK5RwV3dPS0_PfS4_bb",
      "_ZN9CSprite2d13Draw2DPolygonEffffffffRK5CRGBA","_ZN6CPools11ms_pPedPoolE","RsGlobal"};
    constexpr uintptr_t offsets[]{0x51f6f0,0x4efae0,0x4f2378,0x6e9df8,0x6ee4a4,0xbc3bb8,0xc9b320};
    uintptr_t symbols[7]{};
    for (int i=0;i<7;++i) {
        symbols[i]=aml->GetSym(handle,names[i]);
        if (!symbols[i]) { log("Required symbol missing; disabled"); return; }
    }
    uintptr_t base=symbols[0]-offsets[0];
    for (int i=1;i<7;++i) {
        if (symbols[i]-base!=offsets[i]) { log("Library layout differs from supplied build; disabled"); return; }
    }
    findPlayer=reinterpret_cast<decltype(findPlayer)>(symbols[1]);
    getPed=reinterpret_cast<decltype(getPed)>(symbols[2]);
    project=reinterpret_cast<decltype(project)>(symbols[3]);
    polygon=reinterpret_cast<decltype(polygon)>(symbols[4]);
    poolPtr=reinterpret_cast<Pool**>(symbols[5]);
    screen=reinterpret_cast<uint8_t*>(symbols[6]);
    if (!aml->Hook(reinterpret_cast<void*>(symbols[0]),reinterpret_cast<void*>(drawHud),reinterpret_cast<void**>(&originalHud))) {
        log("HUD hook failed; disabled"); return;
    }
    log("NPC Lines 0.1.1 enabled: ARM64, 100m, max 128 lines");
}
