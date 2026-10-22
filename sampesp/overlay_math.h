#pragma once
#include <algorithm>
#include <cmath>
namespace esp {
struct Vec {float x,y,z;};
struct Point {float x,y;};
struct Box {float left,top,right,bottom;};
inline bool finite(Vec v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
inline bool finite(Point v){return std::isfinite(v.x)&&std::isfinite(v.y);}
inline bool clip(Point& a,Point& b,float w,float h){
    if(!finite(a)||!finite(b)||w<=0||h<=0)return false;
    float dx=b.x-a.x,dy=b.y-a.y,lo=0,hi=1;
    const float p[]{-dx,dx,-dy,dy},q[]{a.x,w-a.x,a.y,h-a.y};
    for(int i=0;i<4;++i){
        if(std::fabs(p[i])<1e-6f){if(q[i]<0)return false;continue;}
        float t=q[i]/p[i];if(p[i]<0)lo=std::max(lo,t);else hi=std::min(hi,t);
        if(lo>hi)return false;
    }
    b={a.x+hi*dx,a.y+hi*dy};a={a.x+lo*dx,a.y+lo*dy};return true;
}
inline bool makeBox(const Point* p,int n,Box& box){
    if(n<2)return false;
    box={p[0].x,p[0].y,p[0].x,p[0].y};
    for(int i=0;i<n;++i){if(!finite(p[i]))return false;
        box.left=std::min(box.left,p[i].x);box.right=std::max(box.right,p[i].x);
        box.top=std::min(box.top,p[i].y);box.bottom=std::max(box.bottom,p[i].y);
    }
    float height=box.bottom-box.top;if(height<3||height>32000)return false;
    float mid=(box.left+box.right)*.5f;
    float half=std::max((box.right-box.left)*.5f,height*.20f)+height*.04f;
    box.left=mid-half;box.right=mid+half;box.top-=height*.03f;box.bottom+=height*.02f;
    return true;
}
}
