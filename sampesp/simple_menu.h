#pragma once
#include "client_menu.h"
#include <deque>
#include <string>
namespace simpleui {
struct Color {uint8_t r,g,b,a;};
struct Touch {float x,y;};
std::mutex inputMutex;
std::deque<std::string> commands;
std::deque<Touch> touches;
void* positionIO=nullptr;
Touch pointer{-1,-1};bool held=false;
std::atomic<uint64_t> commandCount{0},touchCount{0};
void (*originalPosition)(void*,float,float)=nullptr;
void (*originalButton)(void*,int,bool)=nullptr;
void (*originalSubmit)(void*,const void*)=nullptr;
void (*event)(const char*)=nullptr;
void (*rect)(float,float,float,float,Color)=nullptr;
void (*print)(float,float,uint16_t*)=nullptr;
void (*flush)()=nullptr;
void* details=nullptr;
void (*fontScale)(float)=nullptr;
void (*fontColor)(Color)=nullptr;
void (*dropColor)(Color)=nullptr;
void (*orientation)(uint8_t)=nullptr;
void (*fontStyle)(uint8_t)=nullptr;
void (*edge)(int8_t)=nullptr;
void (*shadow)(int8_t)=nullptr;
void (*background)(uint8_t,uint8_t)=nullptr;
void (*proportional)(uint8_t)=nullptr;
void (*justify)(uint8_t)=nullptr;
void (*centre)(float)=nullptr;
void (*alpha)(float)=nullptr;
void (*slant)(float)=nullptr;
bool fontReady=false;unsigned page=0;
std::string banner;
uint64_t bannerUntil=0;
void notify(const char* message){banner=message;bannerUntil=diagnostics::millis()+5000;if(event)event(message);}
// Consume our namespace only. Other server commands and chat remain untouched.
bool queueCommand(const char* data,size_t length){
    if(!data||length>256)return false;
    std::string value(data,length);
    for(char& c:value)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
    if(value!="/alynesp"&&value.compare(0,9,"/alynesp ")!=0)return false;
    std::lock_guard<std::mutex> lock(inputMutex);
    if(commands.size()<32)commands.push_back(std::move(value));
    ++commandCount;return true;
}
void submit(void* self,const void* string){
    // Android libc++ std::string, normal 24-byte layout, verified at 0x55acc0.
    const auto* p=static_cast<const uint8_t*>(string);size_t length=0;const char* data=nullptr;
    if(p[0]&1){std::memcpy(&length,p+8,8);std::memcpy(&data,p+16,8);}
    else {length=p[0]>>1;if(length<=22)data=reinterpret_cast<const char*>(p+1);}
    if(queueCommand(data,length)){
        // Keep the client's normal submit/close cycle, with no mod command sent.
        alignas(8) const uint8_t empty[24]{};originalSubmit(self,empty);
    }else originalSubmit(self,string);
}
void mousePosition(void* io,float x,float y){
    originalPosition(io,x,y);
    float size[2];std::memcpy(size,static_cast<uint8_t*>(io)+8,sizeof(size));
    std::lock_guard<std::mutex> lock(inputMutex);positionIO=io;pointer={-1,-1};
    if(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(size[0])&&std::isfinite(size[1])&&size[0]>0&&size[1]>0&&x>=0&&y>=0&&x<size[0]&&y<size[1])pointer={x/size[0],y/size[1]};
}
void mouseButton(void* io,int button,bool down){
    originalButton(io,button,down);if(button!=0)return;
    std::lock_guard<std::mutex> lock(inputMutex);
    if(down&&!held&&io==positionIO&&pointer.x>=0&&touches.size()<32){touches.push_back(pointer);++touchCount;}
    held=down;
}
void runCommand(const std::string& value){
    if(value=="/alynesp"){clientui::menuOpen=true;++clientui::openRequests;notify("CMD RECEBIDO - MENU ABERTO");return;}
    if(value=="/alynesp fechar"){clientui::menuOpen=false;notify("CMD RECEBIDO - MENU FECHADO");return;}
    if(value=="/alynesp teste"){notify("ALYN 0.3.1 - COMANDO FUNCIONANDO");return;}
    const char* names[]{"box","linha","vida","aim","zoom"};
    std::atomic<bool>* options[]{&clientui::boxEnabled,&clientui::lineEnabled,&clientui::healthEnabled,&clientui::aimEnabled,&clientui::zoomEnabled};
    for(int i=0;i<5;++i)for(int on=0;on<2;++on){
        if(value==std::string("/alynesp ")+names[i]+(on?" on":" off")){*options[i]=bool(on);notify(value.c_str());return;}
    }
    notify("USO: /alynesp | box/linha/vida/aim/zoom on/off");
}
bool hit(Touch t,float l,float top,float r,float bottom){return t.x>=l&&t.x<=r&&t.y>=top&&t.y<=bottom;}
void label(float x,float y,const char* text){
    if(!fontReady)return;uint16_t buffer[160]{};
    size_t i=0;for(;text[i]&&i<159;++i)buffer[i]=static_cast<uint8_t>(text[i]);
    print(x,y,buffer);
}
// All geometry is in a fixed 800x600 reference canvas. Input is normalized.
void frame(float width,float height){
    if(!rect||width<200||height<200)return;
    ++clientui::frames;
    std::deque<std::string> pending;std::deque<Touch> clicks;
    {std::lock_guard<std::mutex> lock(inputMutex);pending.swap(commands);clicks.swap(touches);}
    for(auto& command:pending)runCommand(command);
    bool expanded=clientui::menuOpen.load();
    // Apply one press at most; the opening press cannot also toggle ESP.
    for(Touch t:clicks){
        t.x*=800;t.y*=600;
        if(hit(t,310,34,490,77)){
            clientui::menuOpen=!expanded;if(!expanded)++clientui::openRequests;
            notify(expanded?"TOQUE - MENU FECHADO":"TOQUE - MENU ABERTO");break;
        }
        if(!expanded)continue;
        if(hit(t,230,468,570,508)){clientui::menuOpen=false;notify("TOQUE - FECHAR MENU");break;}
        if(!clientui::listOpen){
            std::atomic<bool>* options[]{&clientui::boxEnabled,&clientui::lineEnabled,&clientui::healthEnabled,&clientui::aimEnabled,&clientui::zoomEnabled};
            bool changed=false;
            for(int i=0;i<5;++i)if(hit(t,230,150+i*48,570,190+i*48)){*options[i]=!options[i]->load();notify("TOQUE - OPCAO ALTERADA");changed=true;break;}
            if(changed)break;
            if(hit(t,230,400,570,440)){clientui::listOpen=true;page=0;break;}
        }else{
            if(hit(t,230,400,390,440)){if(page)--page;break;}
            if(hit(t,410,400,570,440)){std::lock_guard<std::mutex> lock(clientui::rowsMutex);if((page+1)*5<clientui::rows.size())++page;break;}
            if(hit(t,230,102,570,140)){clientui::listOpen=false;break;}
            for(int i=0;i<5;++i)if(hit(t,230,150+i*48,570,190+i*48)){
                std::lock_guard<std::mutex> lock(clientui::rowsMutex);size_t index=page*5+i;
                if(index<clientui::rows.size()){clientui::selected=clientui::rows[index].id;notify("JOGADOR SELECIONADO PARA CONSULTA");}break;
            }
        }
    }
    uint8_t saved[64]{};
    if(fontReady){
        std::memcpy(saved,details,64);fontStyle(1);fontScale(.47f*height/600.f);alpha(255);fontColor({240,230,205,255});
        dropColor({0,0,0,255});edge(1);shadow(0);background(0,0);proportional(1);justify(0);orientation(0);centre(width);slant(0);
    }
    auto box=[&](float l,float top,float r,float bottom,Color c){rect(l*width/800,top*height/600,r*width/800,bottom*height/600,c);};
    auto text=[&](float x,float y,const char* s){label(x*width/800,y*height/600,s);};
    auto button=[&](float top,const char* s){box(230,top,570,top+40,{45,52,65,245});text(400,top+6,s);};
    box(310,34,490,77,{30,95,150,255});text(400,42,"MENU TESTE");
    if(clientui::menuOpen){
        ++clientui::panelFrames;
        box(215,90,585,520,{12,14,18,245});text(400,103,clientui::listOpen?"JOGADORES / VOLTAR":"ALYN ESP 0.3.1");
        if(!clientui::listOpen){
            const char* names[]{"ESP BOX","ESP LINE","BARRA DE VIDA","TRAVA SNIPER","ZOOM ESTENDIDO"};
            std::atomic<bool>* options[]{&clientui::boxEnabled,&clientui::lineEnabled,&clientui::healthEnabled,&clientui::aimEnabled,&clientui::zoomEnabled};
            for(int i=0;i<5;++i){char s[80];std::snprintf(s,sizeof(s),"%s: %s",names[i],options[i]->load()?"ON":"OFF");button(150+i*48,s);}
            button(400,"JOGADORES PROXIMOS");
        }else{
            std::lock_guard<std::mutex> lock(clientui::rowsMutex);
            for(int i=0;i<5;++i){size_t index=page*5+i;if(index>=clientui::rows.size())break;
                auto row=clientui::rows[index];char s[100];std::snprintf(s,sizeof(s),"%sID %u | %.0fm | HP %.0f",row.id==clientui::selected?"> ":"",row.id,row.distance,row.hp);button(150+i*48,s);}
            text(310,408,"< ANTERIOR");text(490,408,"PROXIMA >");
            for(const auto& row:clientui::rows)if(row.id==clientui::selected){char s[90];std::snprintf(s,sizeof(s),"ID %u | HP %.0f | COLETE %.0f",row.id,row.hp,row.armour);text(400,443,s);break;}
        }
        button(468,"FECHAR MENU");
    }
    text(400,551,"ALYN 0.3.1 - /alynesp");
    if(diagnostics::millis()<bannerUntil){box(100,577,700,599,{20,65,40,245});text(400,577,banner.c_str());}
    if(fontReady){flush();std::memcpy(details,saved,64);}
}
void installFont(IAML* aml,void* handle,uintptr_t base){
    const char* names[]{"_ZN5CFont11PrintStringEffPt","_ZN5CFont16RenderFontBufferEv","_ZN5CFont7DetailsE","_ZN5CFont8SetScaleEf","_ZN5CFont8SetColorE5CRGBA","_ZN5CFont12SetDropColorE5CRGBA","_ZN5CFont14SetOrientationEh","_ZN5CFont12SetFontStyleEh","_ZN5CFont7SetEdgeEa","_ZN5CFont21SetDropShadowPositionEa","_ZN5CFont13SetBackgroundEhh","_ZN5CFont15SetProportionalEh","_ZN5CFont10SetJustifyEh","_ZN5CFont13SetCentreSizeEf","_ZN5CFont12SetAlphaFadeEf","_ZN5CFont8SetSlantEf"};
    constexpr uintptr_t offsets[]{0x6cdeb0,0x6ccea0,0xccc324,0x6cefbc,0x6cee0c,0x6cf0f0,0x6cf1dc,0x6ceff8,0x6cf148,0x6cc8e8,0x6cf18c,0x6cf17c,0x6cf1cc,0x6cf0c0,0x6cf0e0,0x6cefe8};
    uintptr_t symbols[16]{};for(int i=0;i<16;++i){symbols[i]=aml->GetSym(handle,names[i]);if(symbols[i]!=base+offsets[i]){if(event)event("SIMPLE_UI CFont layout mismatch; text disabled");return;}}
    print=reinterpret_cast<decltype(print)>(symbols[0]);flush=reinterpret_cast<decltype(flush)>(symbols[1]);details=reinterpret_cast<void*>(symbols[2]);
    fontScale=reinterpret_cast<decltype(fontScale)>(symbols[3]);fontColor=reinterpret_cast<decltype(fontColor)>(symbols[4]);dropColor=reinterpret_cast<decltype(dropColor)>(symbols[5]);orientation=reinterpret_cast<decltype(orientation)>(symbols[6]);fontStyle=reinterpret_cast<decltype(fontStyle)>(symbols[7]);edge=reinterpret_cast<decltype(edge)>(symbols[8]);shadow=reinterpret_cast<decltype(shadow)>(symbols[9]);background=reinterpret_cast<decltype(background)>(symbols[10]);proportional=reinterpret_cast<decltype(proportional)>(symbols[11]);justify=reinterpret_cast<decltype(justify)>(symbols[12]);centre=reinterpret_cast<decltype(centre)>(symbols[13]);alpha=reinterpret_cast<decltype(alpha)>(symbols[14]);slant=reinterpret_cast<decltype(slant)>(symbols[15]);fontReady=true;
    if(event)event("SIMPLE_UI native GTA font ready");
}
}
