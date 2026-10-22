#include <cstdint>
#include <cassert>
#include <map>
#include <string>
#include <iostream>
#include "imgui.h"
#include "imgui_internal.h"
#include "../client_menu.h"
namespace {
struct Rect{ImVec2 min,max;};std::map<std::string,Rect> controls;
int roots=0;bool visiblePanel=false;
void* current(){return ImGui::GetCurrentContext();}void* io(){return &ImGui::GetIO();}
bool begin(const char* title,bool* open,int flags){++roots;return ImGui::Begin(title,open,flags);}
bool button(const char* title,const clientui::Vec2& size){bool clicked=ImGui::Button(title,{size.x,size.y});controls[title]={ImGui::GetItemRectMin(),ImGui::GetItemRectMax()};return clicked;}
void pos(const clientui::Vec2& p,int condition,const clientui::Vec2& pivot){ImGui::SetNextWindowPos({p.x,p.y},condition,{pivot.x,pivot.y});}
void size(const clientui::Vec2& s,int condition){ImGui::SetNextWindowSize({s.x,s.y},condition);}
void text(const char* value,const char* end){if(std::string(value)=="ALYN ESP 0.3.0")visiblePanel=true;ImGui::TextUnformatted(value,end);}
void frame(){
    controls.clear();roots=0;visiblePanel=false;ImGui::NewFrame();clientui::endFrame();ImGui::Render();
    assert(roots==1);assert(ImGui::GetCurrentContext()->ColorStack.Size==0);assert(ImGui::GetCurrentContext()->StyleVarStack.Size==0);
}
void tap(const char* title){
    assert(controls.count(title));auto r=controls.at(title);auto& input=ImGui::GetIO();
    input.AddMousePosEvent((r.min.x+r.max.x)*.5f,(r.min.y+r.max.y)*.5f);frame();
    input.AddMouseButtonEvent(0,true);frame();input.AddMouseButtonEvent(0,false);frame();
}
}
int main(){
    static_assert(offsetof(ImGuiContext,WithinFrameScope)==1&&offsetof(ImGuiContext,FrameCountEnded)==8,"Matched client context prefix");
    IMGUI_CHECKVERSION();ImGui::CreateContext();auto& input=ImGui::GetIO();input.DisplaySize={1280,720};input.DeltaTime=1.f/60;input.IniFilename=nullptr;input.LogFilename=nullptr;
    unsigned char* pixels=nullptr;int w=0,h=0;input.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);input.Fonts->SetTexID(1);
    clientui::context=current;clientui::io=io;clientui::frameCount=ImGui::GetFrameCount;clientui::begin=begin;clientui::end=ImGui::End;clientui::button=button;
    clientui::setPos=pos;clientui::setSize=size;clientui::focusNext=ImGui::SetNextWindowFocus;clientui::text=text;
    clientui::color=static_cast<void(*)(ImGuiCol,ImU32)>(ImGui::PushStyleColor);clientui::popColor=ImGui::PopStyleColor;
    clientui::style=static_cast<void(*)(ImGuiStyleVar,float)>(ImGui::PushStyleVar);clientui::popStyle=ImGui::PopStyleVar;clientui::fontScale=ImGui::SetWindowFontScale;
    clientui::originalEndFrame=ImGui::EndFrame;
    for(int i=0;i<4;++i)frame();assert(!clientui::menuOpen&&controls.count("MENU ESP"));
    tap("MENU ESP");assert(clientui::menuOpen&&clientui::boxEnabled&&clientui::lineEnabled&&clientui::healthEnabled);
    for(int i=0;i<3;++i)frame();assert(visiblePanel&&controls.count("ESP BOX: LIGADO")&&!controls.count("MENU ESP"));
    assert(ImGui::GetDrawData()->CmdListsCount>0&&ImGui::GetDrawData()->TotalVtxCount>0);
    tap("ESP BOX: LIGADO");assert(!clientui::boxEnabled&&clientui::lineEnabled&&clientui::healthEnabled&&clientui::menuOpen);
    tap("TRAVA SNIPER: LIGADO");assert(!clientui::aimEnabled&&!clientui::boxEnabled&&clientui::lineEnabled);
    tap("ZOOM ESTENDIDO: LIGADO");assert(!clientui::zoomEnabled&&!clientui::aimEnabled);
    tap("FECHAR MENU");assert(!clientui::menuOpen&&!clientui::boxEnabled);for(int i=0;i<3;++i)frame();assert(controls.count("MENU ESP"));
    tap("MENU ESP");for(int i=0;i<3;++i)frame();assert(clientui::menuOpen&&visiblePanel&&!clientui::boxEnabled&&controls.count("ESP BOX: DESLIGADO"));
    ImGui::DestroyContext();std::cout<<"Real ImGui touch test passed: open, draw panel, toggle box, close, reopen\n";
}
