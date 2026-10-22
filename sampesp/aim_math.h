#pragma once
#include "overlay_math.h"
namespace sniper_math {
using esp::Vec;
constexpr float pi=3.14159265358979323846f;
inline Vec sub(Vec a,Vec b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec scale(Vec v,float n){return {v.x*n,v.y*n,v.z*n};}
inline float dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec cross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline bool normal(Vec value,Vec& out){float n=dot(value,value);if(!esp::finite(value)||!std::isfinite(n)||n<.000001f)return false;out=scale(value,1/std::sqrt(n));return true;}
inline bool basis(Vec forward,Vec& front,Vec& right,Vec& up){return normal(forward,front)&&normal(cross(front,{0,0,1}),right)&&normal(cross(right,front),up);}
// FOV is horizontal. Pixel-space distance to the center is proportional to this
// angular score; no previous-frame world-to-screen camera matrix is required.
inline bool score(Vec point,Vec source,Vec front,Vec right,Vec up,float fov,float aspect,bool requireOnScreen,float& result){
    Vec d=sub(point,source);float depth=dot(d,front);if(!esp::finite(d)||depth<=.1f||!std::isfinite(fov)||fov<1||fov>100||aspect<=0)return false;
    float x=dot(d,right)/depth,y=dot(d,up)/depth;
    float tanHalf=std::tan(fov*pi/360.f);
    if(requireOnScreen&&(std::fabs(x)>tanHalf||std::fabs(y)>tanHalf/aspect))return false;
    result=x*x+y*y;return std::isfinite(result);
}
inline float zoom(float current,bool in,bool out,float seconds){
    if(!std::isfinite(current))current=70;
    float step=std::max(0.f,std::min(.1f,seconds));
    if(in!=out)current*=std::exp((in?-1.f:1.f)*step);
    return std::max(1.f,std::min(70.f,current));
}
inline void angles(Vec front,float& alpha,float& beta){
    alpha=std::atan2(front.z,std::hypot(front.x,front.y));beta=std::atan2(-front.y,-front.x);if(beta<0)beta+=2*pi;
}
}
