#include "../main.cpp"
#include <cassert>
#include <iostream>
#include <limits>
#include <string>
extern "C" int __android_log_write(int,const char*,const char*){return 0;}
namespace {
alignas(8) uint8_t remote[320]{},wrapper[352]{},ped[2456]{},ctx[24]{},fakeIo[24]{};
int endCalls=0,beginDepth=0,colors=0,styles=0,guiFrame=1,rootCalls=0,panelHeaders=0,focusCalls=0,optionCalls=0;
bool allButtons=false;
std::vector<std::string> events;
const char* click=nullptr;bool closed=false;
template<class T>void put(void* p,size_t o,T v){std::memcpy(static_cast<uint8_t*>(p)+o,&v,sizeof(v));}
void processMock(void* p){assert(p==remote);put(ped,1708,1000.f);}
void* contextMock(){return ctx;}void* ioMock(){return fakeIo;}int frameMock(){return guiFrame;}
void originalMock(){++endCalls;}
bool beginMock(const char* name,bool*,int){assert(std::strcmp(name,"##AlynEspControlPanel021")==0);++rootCalls;++beginDepth;return !closed;}
void endMock(){assert(beginDepth>0);--beginDepth;}
bool buttonMock(const char* label,const clientui::Vec2& size){assert(size.x>0&&size.y>=38);if(std::strncmp(label,"ESP BOX",7)==0||std::strncmp(label,"BARRA DE VIDA",13)==0||std::strncmp(label,"ESP LINE",8)==0)++optionCalls;if(allButtons)return true;if(click&&std::strcmp(label,click)==0){click=nullptr;return true;}return false;}
void posMock(const clientui::Vec2&,int condition,const clientui::Vec2&){assert(condition==1);}
void sizeMock(const clientui::Vec2&,int condition){assert(condition==1);}
void textMock(const char* value,const char*){if(std::strcmp(value,"ALYN ESP 0.2.1")==0)++panelHeaders;}
void focusMock(){++focusCalls;}
void eventMock(const char* value){events.emplace_back(value);}
void colorMock(int,unsigned){++colors;}void popColorMock(int n){colors-=n;assert(colors==0);}
void styleMock(int,float){++styles;}void popStyleMock(int n){styles-=n;assert(styles==0);}
void fontMock(float f){assert(f>0);}
}
int main(){
    using health::fraction;using health::color;
    assert(color(100).g==220&&color(76).g==220);assert(color(75).r==245&&color(51).r==245);
    assert(color(50).r==255&&color(26).r==255);assert(color(25).r==240&&color(1).r==240);
    assert(fraction(1000)==1&&fraction(-10)==0&&fraction(25)==.25f&&fraction(std::numeric_limits<float>::quiet_NaN())==0);
    health::Cache cache;health::Sample out{};
    cache.update({remote,ped,42,65,20,100});assert(cache.get(ped,101,out)&&out.hp==65);
    assert(!cache.get(ped,2201,out)&&!cache.get(ped,99,out));
    cache.update({remote,ped,42,std::numeric_limits<float>::quiet_NaN(),20,100});assert(cache.get(ped,101,out)&&out.hp==65);
    int otherRemote=0,otherPed=0;cache.update({&otherRemote,&otherPed,43,40,0,100});cache.remove(remote);
    cache.update({&otherRemote,&otherPed,43,20,0,200});assert(cache.get(&otherPed,201,out)&&out.hp==20);
    // The health source comes from CRemotePlayer sync fields, even when CPed is set to 1000.
    put(remote,288,static_cast<void*>(wrapper));put(wrapper,312,static_cast<void*>(ped));put(remote,182,uint16_t(42));
    put(remote,16,65.f);put(remote,20,20.f);features::originalRemoteProcess=processMock;features::processRemote(remote);
    assert(read<float>(ped,1708)==1000);assert(features::healthCache.get(ped,diagnostics::millis(),out)&&out.hp==65&&out.id==42);
    put(remote,288,static_cast<void*>(nullptr));features::processRemote(remote);assert(!features::healthCache.get(ped,diagnostics::millis(),out));
    // Exercise menu state changes through the client UI calls, with stack restoration.
    using namespace clientui;
    context=contextMock;io=ioMock;frameCount=frameMock;begin=beginMock;end=endMock;button=buttonMock;setPos=posMock;setSize=sizeMock;
    text=textMock;clientui::color=colorMock;popColor=popColorMock;style=styleMock;popStyle=popStyleMock;fontScale=fontMock;originalEndFrame=originalMock;focusNext=focusMock;event=eventMock;
    put(fakeIo,8,Vec2{1280,720});ctx[1]=1;put(ctx,8,0);
    click="MENU ESP";endFrame();assert(menuOpen&&endCalls==1&&beginDepth==0&&colors==0&&styles==0);
    assert(rootCalls==1&&panelHeaders==0&&optionCalls==0&&boxEnabled&&lineEnabled&&healthEnabled);
    assert(openRequests==1&&events.back().find("UI_OPEN_REQUEST")!=std::string::npos);
    click="ESP BOX: LIGADO";++guiFrame;endFrame();assert(!boxEnabled.load()&&lineEnabled.load()&&healthEnabled.load());
    assert(rootCalls==2&&panelHeaders==1&&focusCalls==1&&panelFrames==1);
    endFrame();assert(!boxEnabled.load()&&click==nullptr); // Same frame must not apply the toggle again.
    click="ESP BOX: DESLIGADO";++guiFrame;endFrame();assert(boxEnabled.load());
    publish({{43,50,0,30},{42,65,20,10}});assert(rows[0].id==42);
    click="JOGADORES PROXIMOS";++guiFrame;endFrame();assert(listOpen);
    click="ID 42 | 10 m | VIDA 65";++guiFrame;endFrame();assert(selected==42);
    closed=true;++guiFrame;endFrame();assert(beginDepth==0&&colors==0&&styles==0);closed=false;
    // Closing the panel never changes an overlay toggle. Reopening is isolated
    // from options even if the input backend incorrectly activates every widget.
    bool boxBefore=boxEnabled,lineBefore=lineEnabled,healthBefore=healthEnabled;
    click="FECHAR MENU";++guiFrame;endFrame();assert(!menuOpen&&boxEnabled==boxBefore&&lineEnabled==lineBefore&&healthEnabled==healthBefore);
    ++guiFrame;endFrame();assert(!menuOpen); // Submit the collapsed launcher.
    int oldOptions=optionCalls;allButtons=true;++guiFrame;endFrame();
    assert(menuOpen&&optionCalls==oldOptions&&boxEnabled==boxBefore&&lineEnabled==lineBefore&&healthEnabled==healthBefore);
    ++guiFrame;endFrame();assert(menuOpen&&boxEnabled!=boxBefore&&lineEnabled==lineBefore&&healthEnabled==healthBefore);
    allButtons=false;
    int oldFrame=lastFrame;ctx[1]=0;++guiFrame;endFrame();assert(lastFrame==oldFrame);ctx[1]=1;
    put(ctx,8,guiFrame);endFrame();assert(lastFrame==oldFrame); // Already-ended frame is also skipped.
    std::cout<<"Feature tests passed: synchronized HP, thresholds, expiry, selection and menu toggles\n";
}
