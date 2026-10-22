#include "../diagnostics.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <unistd.h>
extern "C" int __android_log_write(int,const char*,const char*){return 0;}
std::string contents(const std::string& path){std::ifstream f(path);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
    char directory[]="/tmp/alyn-esp-log-XXXXXX";assert(mkdtemp(directory));
    std::string path=std::string(directory)+"/alyn_esp.log";
    {
        diagnostics::Log log;
        std::string missing=std::string(directory)+"/does-not-exist";
        assert(!log.openDirectory(missing.c_str()));
        assert(log.openDirectory(directory));assert(log.filename()==path);
        log.write("SESSION test event %d",42);
        // Must be readable without closing the logger, as after an unrelated game crash.
        assert(contents(path).find("SESSION test event 42")!=std::string::npos);
        std::thread first([&]{for(int i=0;i<20;++i)log.write("writer-A %d",i);});
        std::thread second([&]{for(int i=0;i<20;++i)log.write("writer-B %d",i);});
        first.join();second.join();
        auto data=contents(path);assert(data.find("writer-A 19")!=std::string::npos&&data.find("writer-B 19")!=std::string::npos);
        std::string large(1900,'x');for(int i=0;i<300;++i)log.write("rotation %s",large.c_str());
        log.write("after rotation");assert(contents(path).find("after rotation")!=std::string::npos);
        struct stat st{};assert(!stat((path+".previous").c_str(),&st));assert(st.st_size>=512*1024);
        assert(!stat(path.c_str(),&st));assert(st.st_size<512*1024);
    }
    unlink(path.c_str());unlink((path+".previous").c_str());rmdir(directory);
    std::cout<<"File log tests passed: fallback, flush, concurrent writers and rotation\n";
}
