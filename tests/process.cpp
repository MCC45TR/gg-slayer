// SPDX-License-Identifier: MIT
#include "../src/core.hpp"
#include <chrono>
#include <iostream>
#include <fstream>
#include <thread>
#include <filesystem>
using namespace slayer;
static bool fails(const std::string&program,int ms,const std::string&message){
    try{capture({"/usr/bin/python3","-c",program},ms);}
    catch(const Error&e){return std::string(e.what()).find(message)!=std::string::npos;}
    return false;
}
int main(){
    // Child tools cannot deadlock the parent on stderr or lose trailing data.
    if(capture({"/usr/bin/python3","-c","import sys;sys.stdout.write('x'*60000+'TAIL');sys.stderr.write('e'*4000)"})!=std::string(60000,'x')+"TAIL")return 1;
    if(!fails("import sys;sys.stderr.write('diagnostic');sys.exit(7)",2000,"diagnostic"))return 2;
    if(!fails("import sys;sys.stdout.write('x'*100000)",2000,"sınırını"))return 3;
    if(!fails("import sys;sys.stderr.write('e'*20000)",2000,"sınırını"))return 4;
    if(!fails("import time;time.sleep(5)",100,"zaman aşımına"))return 5;
    if(capture({"/usr/bin/python3","-c","import os,time;os.close(1);os.close(2);time.sleep(.2)"})!="")return 6;
    // A completed or timed-out helper cannot leave its own descendants alive.
    auto pid=capture({"/usr/bin/python3","-c","import os,time; p=os.fork(); (time.sleep(5)) if p==0 else print(p,flush=True)"});
    int child=integer(pid,1,2000000000,"test child");
    bool alive=true;
    for(unsigned tries=0;tries<20;++tries){
        std::ifstream file("/proc/"+std::to_string(child)+"/stat");std::string text;std::getline(file,text);
        auto end=text.rfind(')');alive=file&&end!=std::string::npos&&text.substr(end+2,1)!="Z";
        if(!alive)break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if(alive)return 7;
    return 0;
}
