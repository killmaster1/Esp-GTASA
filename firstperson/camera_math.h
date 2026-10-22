#pragma once
#include <cmath>
namespace headcam {
struct Vec { float x, y, z; };
inline bool finite(Vec v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
inline Vec add(Vec a, Vec b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Vec sub(Vec a, Vec b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Vec mul(Vec a, float n) { return {a.x*n,a.y*n,a.z*n}; }
inline float norm2(Vec a) { return a.x*a.x+a.y*a.y+a.z*a.z; }
inline bool eyePosition(Vec head, Vec player, Vec front, Vec& out) {
    const float n=norm2(front);
    if (!finite(head) || !finite(player) || !finite(front) || n<0.0001f || n>100.0f) return false;
    // A corrupt or not-yet-updated bone must not move the view across the map.
    if (norm2(sub(head,player))>16.0f) return false;
    front=mul(front,1.0f/std::sqrt(n));
    out=add(head,mul(front,0.10f));
    out.z+=0.03f;
    return finite(out);
}
}
