// SPDX-License-Identifier: MIT
#include "command.hpp"
#include <sstream>
#include <iostream>
int slayerMain(int,char**);
CommandResult ctlCommand(const QStringList& args,bool privileged){
    std::vector<QByteArray> storage{privileged?QByteArray("slayerctl-helper"):QByteArray("slayerctl")};
    for(const auto& arg:args)storage.push_back(arg.toUtf8());
    std::vector<char*> argv;for(auto& s:storage)argv.push_back(s.data());argv.push_back(nullptr);
    std::ostringstream out,err;auto* oldOut=std::cout.rdbuf(out.rdbuf());auto* oldErr=std::cerr.rdbuf(err.rdbuf());
    const int code=slayerMain(static_cast<int>(storage.size()),argv.data());
    std::cout.rdbuf(oldOut);std::cerr.rdbuf(oldErr);
    return {code==0,QString::fromStdString(out.str()+err.str())};
}
bool allowedCommand(const QStringList& a){
    if(a.isEmpty()||a.size()>12)return false;
    for(const auto& v:a)if(v.size()>256||v.contains(QChar::Null))return false;
    const auto c=a.value(0),sub=a.value(1);
    return (c==u"keyboard"&&QStringList{"brightness","color","key","hardware","white-point","color-profile","profile","layout","identify"}.contains(sub))
        ||(c==u"battery"&&QStringList{"limit","profile"}.contains(sub))
        ||(c==u"power"&&a.size()==3&&sub==u"set")||(c==u"profile"&&QStringList{"set","sync"}.contains(sub))||(c==u"fan"&&QStringList{"auto","boost","toggle","curve"}.contains(sub))
        ||(c==u"cpu"&&QStringList{"boost","smt","limits"}.contains(sub))
        ||(c==u"gpu"&&QStringList{"mode","power-limit"}.contains(sub))
        ||(c==u"settings"&&a.size()==3)||(c==u"usb"&&QStringList{"powersave","wake"}.contains(sub))
        ||(c==u"config"&&QStringList{"set","unset","protect","recover"}.contains(sub));
}

CommandResult applyCommand(const QStringList& command,const QStringList& remember,const CommandExecutor& execute){
    if(!remember.isEmpty()){
        QStringList validate{"--dry-run","config","set"};validate.append(remember);
        auto validated=execute(validate,true);
        if(!validated.ok)return validated;
    }
    // Preflight TPM before touching hardware; never fall back to plaintext.
    auto result=command==QStringList{"config","recover"}?CommandResult{true,{}}:execute({"config","protect"},true);
    if(result.ok)result=execute(command,true);
    if(result.ok&&command==QStringList{"cpu","limits","reset"}){
        auto saved=execute({"config","unset","cpu_min_mhz","cpu_max_mhz"},true);
        if(!saved.ok)result={false,"CPU sınırları sıfırlandı fakat kayıtlı sınırlar temizlenemedi: "+saved.output};
    }
    // Preserve the last working lighting state without overwriting named profiles.
    QStringList savedSettings=remember;
    if(result.ok&&command==QStringList{"fan","toggle"}){
        auto mode=execute({"fan","mode"},false);const auto value=mode.output.trimmed();
        if(!mode.ok||(value!="auto"&&value!="boost"))result={false,"Fan modu değişti fakat yeni durum doğrulanamadı: "+mode.output};
        else savedSettings.append({"fan_mode",value});
    }
    if(result.ok&&command.value(0)=="keyboard"&&QStringList{"color","key","hardware"}.contains(command.value(1))){
        QStringList snapshot{"keyboard","profile","snapshot","__slayer_current"};
        if(command.value(1)=="hardware"){
            QString speed="5",brightness="50",direction="right";
            for(int i=3;i+1<command.size();i+=2){if(command[i]=="--speed")speed=command[i+1];if(command[i]=="--brightness")brightness=command[i+1];if(command[i]=="--direction")direction=command[i+1];}
            snapshot={"keyboard","profile","snapshot-effect","__slayer_current",command.value(2),speed,brightness,direction};
        }
        auto saved=execute(snapshot,true);
        if(!saved.ok)result={false,"Işık uygulandı fakat son durum hatırlanamadı: "+saved.output};
        else savedSettings.append({"rgb_profile","__slayer_current"});
    }
    if(result.ok&&!savedSettings.isEmpty()){QStringList save{"config","set"};save.append(savedSettings);auto saved=execute(save,true);if(!saved.ok)result={false,"Ayar uygulandı fakat hatırlanamadı: "+saved.output};}
    return result;
}
