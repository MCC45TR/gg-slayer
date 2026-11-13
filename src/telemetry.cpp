// SPDX-License-Identifier: MIT
#include "core.hpp"
#include <iostream>
#include <dlfcn.h>
namespace slayer {
namespace {
std::optional<unsigned> nvidiaUsage(const std::string& busId){
    // Read-only NVML, loaded only for a GPU already active in runtime PM.
    // Release the handle immediately so monitoring cannot pin the GPU awake.
    void* library=dlopen("libnvidia-ml.so.1",RTLD_LAZY|RTLD_LOCAL);if(!library)return {};
    using Init=int(*)();using Lookup=int(*)(const char*,void**);
    struct Rates {unsigned gpu,memory;};using Usage=int(*)(void*,Rates*);
    auto init=reinterpret_cast<Init>(dlsym(library,"nvmlInit_v2"));
    auto shutdown=reinterpret_cast<Init>(dlsym(library,"nvmlShutdown"));
    auto lookup=reinterpret_cast<Lookup>(dlsym(library,"nvmlDeviceGetHandleByPciBusId_v2"));
    auto usage=reinterpret_cast<Usage>(dlsym(library,"nvmlDeviceGetUtilizationRates"));
    std::optional<unsigned> result;
    if(init&&shutdown&&lookup&&usage&&init()==0){void* device=nullptr;Rates rates{};
        if(lookup(busId.c_str(),&device)==0&&usage(device,&rates)==0&&rates.gpu<=100)result=rates.gpu;
        shutdown();
    }
    dlclose(library);return result;
}
}

void telemetry(const Io& io){
    // Cheap, read-only snapshot. No LED enumeration, NVML process or GPU wake-up.
    auto value=[&](const std::string& key,const std::string& path){auto v=trim(io.read(path,256));if(!v.empty())std::cout<<"value\t"<<key<<'\t'<<displayText(v)<<'\n';};
    auto stat=io.read("/proc/stat",1024);auto end=stat.find('\n');
    if(stat.starts_with("cpu "))std::cout<<"value\tcpu_ticks\t"<<displayText(stat.substr(4,end-4))<<'\n';
    for(const auto& node:io.list("/sys/class/hwmon")){
        auto base="/sys/class/hwmon/"+node;auto name=trim(io.read(base+"/name",128));
        if(trim(io.read(base+"/device/power/runtime_status",64))=="suspended")continue;
        for(const auto& attr:io.list(base)){
            if(!(attr.starts_with("fan")||attr.starts_with("temp"))||!attr.ends_with("_input"))continue;
            auto label=trim(io.read(base+"/"+attr.substr(0,attr.size()-6)+"_label",128));
            auto v=trim(io.read(base+"/"+attr,64));if(v.empty())continue;
            std::cout<<"sensor\t"<<displayText(name+"/"+attr)<<'\t'<<displayText(label.empty()?name+" "+attr:label)<<'\t'<<(attr.starts_with("fan")?"rpm":"temperature")<<'\t'<<displayText(v)<<'\n';
        }
    }
    for(const auto& node:io.list("/sys/class/drm")){
        if(!node.starts_with("card")||node.find('-')!=std::string::npos)continue;
        auto base="/sys/class/drm/"+node+"/device";
        if(trim(io.read(base+"/power/runtime_status",64))=="suspended")continue;
        auto vendor=trim(io.read(base+"/vendor",64));
        if(vendor=="0x1002")value("amd_usage",base+"/gpu_busy_percent");
        if(vendor=="0x10de"&&!io.fixture()){
            std::error_code error;auto pci=fs::canonical(io.path(base),error).filename().string();
            if(!error)if(auto usage=nvidiaUsage(pci))std::cout<<"value\tnvidia_usage\t"<<*usage<<'\n';
        }
    }
}
}
