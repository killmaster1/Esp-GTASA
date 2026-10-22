#pragma once
#include <cmath>
#include <algorithm>
namespace headcam {
struct Vec { float x, y, z; };
struct Basis { Vec right, forward, up; };
inline bool finite(Vec v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
inline Vec add(Vec a, Vec b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Vec sub(Vec a, Vec b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Vec mul(Vec a, float n) { return {a.x*n,a.y*n,a.z*n}; }
inline float dot(Vec a,Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline float norm2(Vec a) { return dot(a,a); }
inline Vec cross(Vec a,Vec b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline bool normalize(Vec in,Vec& out) {
    float n=norm2(in);
    if(!finite(in) || !std::isfinite(n) || n<0.0001f || n>100.0f) return false;
    out=mul(in,1.0f/std::sqrt(n)); return true;
}
inline bool orthogonal(const Basis& b) {
    return finite(b.right) && finite(b.forward) && finite(b.up)
        && std::fabs(norm2(b.right)-1)<0.05f && std::fabs(norm2(b.forward)-1)<0.05f
        && std::fabs(norm2(b.up)-1)<0.05f && std::fabs(dot(b.right,b.forward))<0.05f
        && std::fabs(dot(b.right,b.up))<0.05f && std::fabs(dot(b.forward,b.up))<0.05f
        && dot(cross(b.right,b.forward),b.up)>0.9f;
}
// Native touch look sets yaw/pitch. Add only a bounded amount of animation roll.
inline bool viewBasis(Vec front,Vec nativeUp,Vec animatedHeadUp,Basis& out) {
    Vec f{},r{},u{};
    if(!normalize(front,f) || !normalize(cross(f,nativeUp),r)) return false;
    u=cross(r,f);
    Vec hint{};
    if(!normalize(sub(animatedHeadUp,mul(f,dot(animatedHeadUp,f))),hint)) return false;
    float roll=std::atan2(dot(hint,r),dot(hint,u));
    // Avoid a sudden upside-down view during leaning/falling/getting into cars.
    roll=std::max(-0.5235988f,std::min(0.5235988f,roll))*0.5f;
    Vec up=add(mul(u,std::cos(roll)),mul(r,std::sin(roll)));
    out={cross(f,up),f,up};
    return orthogonal(out);
}
inline Vec rotate(Vec v,const Basis& oldBasis,const Basis& newBasis) {
    return add(add(mul(newBasis.right,dot(v,oldBasis.right)),mul(newBasis.forward,dot(v,oldBasis.forward))),
               mul(newBasis.up,dot(v,oldBasis.up)));
}
// PC mod's axis conversion: cameraRight=-headAt, cameraFront=headUp,
// cameraUp=headRight. 'Basis' names are just columns for skeletal matrices too.
inline Basis headBasis(const Basis& view) { return {view.up,view.forward,mul(view.right,-1)}; }
inline bool eyePosition(Vec head,Vec player,const Basis& view,Vec& out) {
    if(!finite(head) || !finite(player) || !orthogonal(view) || norm2(sub(head,player))>16.0f) return false;
    // Original CJ cfg: local eye=(0, 0.09+0.02962963, 0.09+0.00370371).
    out=add(add(head,mul(view.forward,0.11962963f)),mul(view.up,0.09370371f));
    return finite(out);
}
}
