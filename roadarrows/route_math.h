#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
namespace road {
struct Vec { float x,y,z; };
inline Vec operator+(Vec a,Vec b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec operator-(Vec a,Vec b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec operator*(Vec a,float k){return {a.x*k,a.y*k,a.z*k};}
inline float dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline float length(Vec a){return std::sqrt(dot(a,a));}
inline float distance2D(Vec a,Vec b){a.z=b.z=0;return length(a-b);}
inline bool valid(Vec a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z)&&std::fabs(a.x)<20000&&std::fabs(a.y)<20000&&std::fabs(a.z)<4000;}
struct Progress { float along=0,distance=1e9f; };
inline Progress closest(const Vec* points,size_t n,Vec p){
    Progress best; float along=0;
    for(size_t i=1;i<n;++i){
        Vec d=points[i]-points[i-1];float len=length(d);if(len<.01f)continue;
        float t=std::max(0.f,std::min(1.f,dot(p-points[i-1],d)/(len*len)));
        float distance=length(p-(points[i-1]+d*t));
        if(distance<best.distance)best={along+len*t,distance};
        along+=len;
    }return best;
}
inline bool sample(const Vec* points,size_t n,float along,Vec& p,Vec& dir){
    if(!points||n<2||along<0||!std::isfinite(along))return false;
    for(size_t i=1;i<n;++i){
        Vec d=points[i]-points[i-1];float len=length(d);if(len<.01f)continue;
        if(along<=len){p=points[i-1]+d*(along/len);d.z=0;float flat=length(d);if(flat<.01f)return false;dir=d*(1.f/flat);return valid(p);}
        along-=len;
    }return false;
}
// A solid chevron made of two triangles. Right vector is perpendicular to travel.
inline void chevron(Vec p,Vec dir,Vec (&v)[6]){
    Vec right={dir.y,-dir.x,0};
    Vec tip=p+dir*1.8f,inner=p+dir*.6f;
    Vec left=p-dir*1.1f-right*1.25f, rightEnd=p-dir*1.1f+right*1.25f;
    v[0]=tip;v[1]=left;v[2]=inner;v[3]=tip;v[4]=inner;v[5]=rightEnd;
}
struct Bounds {float minX,maxX,minY,maxY;};
inline Bounds bounds(Vec a,Vec b){return {std::max(-2999.f,std::min(a.x,b.x)-750.f),std::min(2999.f,std::max(a.x,b.x)+750.f),std::max(-2999.f,std::min(a.y,b.y)-750.f),std::min(2999.f,std::max(a.y,b.y)+750.f)};}
inline bool same(Bounds a,Bounds b){return a.minX==b.minX&&a.maxX==b.maxX&&a.minY==b.minY&&a.maxY==b.maxY;}
inline bool inside(Bounds a,Vec p){return p.x>=a.minX+100&&p.x<=a.maxX-100&&p.y>=a.minY+100&&p.y<=a.maxY-100;}
}
