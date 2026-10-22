#include "../main.cpp"
#include <cassert>
#include <iostream>
extern "C" int __android_log_write(int,const char*,const char*){return 0;}
namespace {
uint8_t fontDetails[64]{};
unsigned fontColors=0,dropColors=0,printed=0,flushed=0;
void printMock(float x,float y,uint16_t* value){assert(x>=0&&y>=0&&value&&value[0]);++printed;}
void flushMock(){assert(printed>0);++flushed;}
void colorMock(const simpleui::Color* value){
    assert(value&&value->r==240&&value->g==230&&value->b==205&&value->a==255);
    assert(fontDetails[32]==255);std::memcpy(fontDetails,value,4);++fontColors;
}
void dropMock(const simpleui::Color* value){
    assert(value&&value->r==0&&value->g==0&&value->b==0&&value->a==255);
    std::memcpy(fontDetails+55,value,4);++dropColors;
}
void floatMock(float value){assert(std::isfinite(value));fontDetails[4]=1;}
void alphaMock(float value){assert(value==255);fontDetails[32]=uint8_t(value);}
void byteMock(uint8_t){fontDetails[5]=2;}
void signedMock(int8_t){fontDetails[6]=3;}
void backgroundMock(uint8_t,uint8_t){fontDetails[7]=4;}
unsigned polygons=0,submits=0,positions=0,buttons=0;bool submittedEmpty=false;
void rectMock(float l,float t,float r,float b,simpleui::Color){assert(l>=0&&t>=0&&r>l&&b>t);++polygons;}
void submitMock(void*,const void* text){++submits;submittedEmpty=static_cast<const uint8_t*>(text)[0]==0;}
void posMock(void*,float,float){++positions;}
void buttonMock(void*,int,bool){++buttons;}
void frame(){simpleui::frame(1280,720);}
void tap(float x,float y){
    alignas(8) static uint8_t io[24]{};float size[]{1280,720};std::memcpy(io+8,size,8);
    simpleui::mousePosition(io,x*1280/800,y*720/600);
    simpleui::mouseButton(io,0,true);simpleui::mouseButton(io,0,true); // duplicate down must not toggle twice
    simpleui::mouseButton(io,0,false);frame();
}
}
int main(){
    using namespace simpleui;
    rect=rectMock;originalSubmit=submitMock;originalPosition=posMock;originalButton=buttonMock;
    assert(!queueCommand("/alynespoutro",13));assert(!queueCommand("/outro",6));assert(!queueCommand("chat",4));
    alignas(8) uint8_t sso[24]{};const char* cmd="/alynesp";sso[0]=16;std::memcpy(sso+1,cmd,8);
    submit(nullptr,sso);assert(submits==1&&submittedEmpty&&!clientui::menuOpen);
    frame();assert(clientui::menuOpen&&clientui::boxEnabled&&clientui::panelFrames==1);
    assert(queueCommand("/alynesp",8));frame();assert(clientui::menuOpen); // command opens, never toggles closed
    assert(queueCommand("/alynesp box off",16));frame();assert(!clientui::boxEnabled&&clientui::lineEnabled);
    const char* longCommand="/alynesp zoom off";
    alignas(8) uint8_t longString[24]{};longString[0]=33;size_t length=std::strlen(longCommand);
    std::memcpy(longString+8,&length,8);std::memcpy(longString+16,&longCommand,8);
    submit(nullptr,longString);frame();assert(!clientui::zoomEnabled&&submittedEmpty);
    sso[0]=8;std::memcpy(sso+1,"chat",4);submit(nullptr,sso);assert(!submittedEmpty);
    assert(queueCommand("/alynesp fechar",15));frame();assert(!clientui::menuOpen);
    auto presses=touchCount.load();tap(400,55);assert(clientui::menuOpen&&touchCount==presses+1&&!clientui::boxEnabled);
    tap(400,170);assert(clientui::boxEnabled&&clientui::menuOpen);
    tap(400,490);assert(!clientui::menuOpen&&clientui::boxEnabled);
    tap(400,55);assert(clientui::menuOpen&&clientui::boxEnabled);
    clientui::publish({{12,75,20,10}});tap(400,420);assert(clientui::listOpen);tap(400,170);assert(clientui::selected==12);
    tap(400,120);assert(!clientui::listOpen);assert(polygons>0&&positions>0&&buttons>0);
    // Commands remain independent of the GUI input path.
    assert(queueCommand("/ALYNESP",8));frame();assert(clientui::menuOpen);
    // The native text path was previously skipped because fontReady was false.
    // Native callees dereference the color argument; cover both and restore game state.
    static_assert(std::is_same<decltype(fontColor),void(*)(const simpleui::Color*)>::value,"Indirect native color ABI");
    static_assert(std::is_same<decltype(dropColor),void(*)(const simpleui::Color*)>::value,"Indirect native shadow ABI");
    details=fontDetails;print=printMock;flush=flushMock;fontColor=colorMock;dropColor=dropMock;
    fontScale=floatMock;centre=floatMock;slant=floatMock;alpha=alphaMock;
    fontStyle=byteMock;orientation=byteMock;proportional=byteMock;justify=byteMock;
    edge=signedMock;shadow=signedMock;background=backgroundMock;fontReady=true;
    uint8_t expected[64];for(size_t i=0;i<64;++i)fontDetails[i]=expected[i]=uint8_t(i+17);
    clientui::menuOpen=false;frame();assert(fontColors==1&&dropColors==1&&printed>0&&flushed==1);
    assert(!std::memcmp(expected,fontDetails,64));
    unsigned before=printed;assert(queueCommand("/alynesp",8));frame();
    assert(clientui::menuOpen&&fontColors==2&&dropColors==2&&flushed==2&&printed>before+5);
    assert(!std::memcmp(expected,fontDetails,64));
    std::cout<<"Simple menu: native geometry, both string layouts, chat pass-through, command queue, touch edges, toggles, players, indirect native colors and font state restoration passed\n";
}
