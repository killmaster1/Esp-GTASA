#pragma once
#include <android/log.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <mutex>
#include <string>
#include <thread>
#include <sys/stat.h>
namespace diagnostics {
inline uint64_t millis(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
class Log {
    std::mutex mutex;
    FILE* file=nullptr;
    std::string path;
    size_t bytes=0;
    uint64_t epoch=millis();
    static constexpr size_t limit=512*1024;
public:
    ~Log(){if(file)std::fclose(file);}
    bool openDirectory(const char* directory){
        std::lock_guard<std::mutex> lock(mutex);
        if(file)return true;
        if(!directory||!*directory)return false;
        std::string candidate=directory;
        if(candidate.back()!='/')candidate+='/';
        candidate+="alyn_esp.log";
        FILE* opened=std::fopen(candidate.c_str(),"a");
        if(!opened){
            int error=errno;char message[1200];
            std::snprintf(message,sizeof(message),"Cannot open log %s: errno=%d (%s)",candidate.c_str(),error,std::strerror(error));
            __android_log_write(ANDROID_LOG_INFO,"AlynESP",message);return false;
        }
        file=opened;path=candidate;struct stat st{};
        if(!fstat(fileno(file),&st)&&st.st_size>0)bytes=size_t(st.st_size);
        return true;
    }
    std::string filename(){std::lock_guard<std::mutex> lock(mutex);return path;}
    void write(const char* format,...){
        char message[2048];va_list args;va_start(args,format);std::vsnprintf(message,sizeof(message),format,args);va_end(args);
        __android_log_write(ANDROID_LOG_INFO,"AlynESP",message);
        std::lock_guard<std::mutex> lock(mutex);
        if(!file)return;
        if(bytes>=limit){
            std::fclose(file);file=nullptr;
            if(std::rename(path.c_str(),(path+".previous").c_str())!=0){
                // Keep appending to the existing file if rotation cannot be performed.
                file=std::fopen(path.c_str(),"a");
            }else{file=std::fopen(path.c_str(),"a");bytes=0;}
            if(!file)return;
        }
        int n=std::fprintf(file,"[%llu ms] %s\n",static_cast<unsigned long long>(millis()-epoch),message);
        if(n>0)bytes+=size_t(n);
        // Events and periodic summaries survive a later game crash; no per-frame file writes.
        if(std::fflush(file)!=0)__android_log_write(ANDROID_LOG_INFO,"AlynESP","File log flush failed; consult logcat");
    }
};
}
