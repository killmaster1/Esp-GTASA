#pragma once
#include <atomic>
#include <mutex>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>
namespace clientui {
struct Vec2 {float x,y;};
struct Row {unsigned id;float hp,armour,distance;};
std::atomic<bool> boxEnabled{true},lineEnabled{true},healthEnabled{true};
std::atomic<uint64_t> frames{0};
std::mutex rowsMutex;
std::vector<Row> rows;
bool menuOpen=false,listOpen=false,installed=false;
unsigned selected=0xffff;
void (*originalEndFrame)()=nullptr;
void* (*context)()=nullptr;
void* (*io)()=nullptr;
int (*frameCount)()=nullptr;
bool (*begin)(const char*,bool*,int)=nullptr;
void (*end)()=nullptr;
bool (*button)(const char*,const Vec2&)=nullptr;
void (*setPos)(const Vec2&,int,const Vec2&)=nullptr;
void (*setSize)(const Vec2&,int)=nullptr;
void (*text)(const char*,const char*)=nullptr;
void (*color)(int,unsigned)=nullptr;
void (*popColor)(int)=nullptr;
void (*style)(int,float)=nullptr;
void (*popStyle)(int)=nullptr;
void (*fontScale)(float)=nullptr;
void (*event)(const char*)=nullptr;
int lastFrame=-1;void* lastContext=nullptr;
inline unsigned rgba(unsigned r,unsigned g,unsigned b,unsigned a=255){return r|(g<<8)|(b<<16)|(a<<24);}
void publish(std::vector<Row> value){
    std::sort(value.begin(),value.end(),[](const Row& a,const Row& b){return a.distance<b.distance;});
    std::lock_guard<std::mutex> lock(rowsMutex);rows=std::move(value);
}
void toggle(std::atomic<bool>& option,const char* label,const Vec2& size){
    char caption[96];std::snprintf(caption,sizeof(caption),"%s: %s",label,option.load()?"LIGADO":"DESLIGADO");
    if(button(caption,size)){option=!option.load();std::snprintf(caption,sizeof(caption),"%s changed to %s",label,option.load()?"ON":"OFF");if(event)event(caption);}
}
void render(){
    void* ctx=context();if(!ctx)return;
    // Verified EndFrame prefix in ImGui 1.92.8: WithinFrameScope=1, FrameCountEnded=8.
    int ended=0;std::memcpy(&ended,static_cast<const uint8_t*>(ctx)+8,sizeof(ended));
    if(!static_cast<const uint8_t*>(ctx)[1])return;
    int frame=frameCount();if(frame==ended)return;if(ctx==lastContext&&frame==lastFrame)return;
    lastContext=ctx;lastFrame=frame;++frames;
    void* data=io();if(!data)return;Vec2 display{};
    // ImGui 1.92.8 IO: ConfigFlags (4), BackendFlags (4), DisplaySize (8).
    std::memcpy(&display,static_cast<const uint8_t*>(data)+8,sizeof(display));
    if(!std::isfinite(display.x)||!std::isfinite(display.y)||display.x<200||display.y<200)return;
    float scale=std::max(.8f,std::min(2.5f,display.y/720.f));
    // Indices verified against the same ImGui 1.92.8 exported by this client.
    const int ids[]{0,2,5,22,23,24};
    const unsigned values[]{rgba(226,210,167),rgba(12,12,10,245),rgba(180,157,105),rgba(36,37,30),rgba(72,74,51),rgba(100,95,56)};
    for(int i=0;i<6;++i)color(ids[i],values[i]);
    style(3,0);style(4,2*scale);style(12,0);style(13,1.5f*scale);
    // No title, resize, move, scroll, saved settings or keyboard/gamepad focus.
    constexpr int flags=1|2|4|8|16|32|64|256|(1<<16)|(1<<17);
    setPos({display.x*.5f-58*scale,display.y*.10f},1,{0,0});
    if(begin("##AlynEspLauncher",nullptr,flags)){
        fontScale(scale);
        if(button("ESP",{100*scale,48*scale})){menuOpen=!menuOpen;if(event)event(menuOpen?"Menu opened":"Menu closed");}
    }end();
    if(menuOpen){
        float panelWidth=std::min(display.x-24*scale,380*scale);
        float panelHeight=std::min(display.y*.78f,listOpen?600*scale:390*scale);
        setPos({display.x*.5f,display.y*.56f},1,{.5f,.5f});
        setSize({panelWidth,panelHeight},1);
        // Fixed size panel: allow vertical scrolling for the nearby-player list.
        int panelFlags=(flags&~(8|16|64));
        if(begin("##AlynEspMenu",nullptr,panelFlags)){
            fontScale(scale);text("ALYN ESP - SAN ANDREAS",nullptr);
            Vec2 size{panelWidth-32*scale,48*scale};
            toggle(boxEnabled,"ESP BOX",size);
            toggle(healthEnabled,"BARRA DE VIDA",size);
            toggle(lineEnabled,"ESP LINE",size);
            if(button(listOpen?"OCULTAR JOGADORES":"JOGADORES PROXIMOS",size))listOpen=!listOpen;
            if(listOpen){
                std::vector<Row> snapshot;{std::lock_guard<std::mutex> lock(rowsMutex);snapshot=rows;}
                if(snapshot.empty())text("Aguardando jogadores sincronizados...",nullptr);
                for(const auto& row:snapshot){
                    char caption[128];std::snprintf(caption,sizeof(caption),"%sID %u | %.0f m | VIDA %.0f",selected==row.id?"> ":"",row.id,row.distance,row.hp);
                    if(button(caption,size)){selected=row.id;if(event)event(caption);}
                    if(selected==row.id){char details[96];std::snprintf(details,sizeof(details),"Vida %.0f / Colete %.0f",row.hp,row.armour);text(details,nullptr);}
                }
            }
            if(button("FECHAR",size))menuOpen=false;
        }end();
    }
    popStyle(4);popColor(6);
}
void endFrame(){render();originalEndFrame();}
}
