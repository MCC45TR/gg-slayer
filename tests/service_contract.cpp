// SPDX-License-Identifier: MIT
#include "../service/command.hpp"
#include <iostream>
int main(){
    for(const auto& command:{QStringList{"--root","/tmp","keyboard","color","ffffff"},QStringList{"shell","id"},QStringList{"gui"},QStringList{"keyboard","effect","wave"},QStringList{"config","clear"},QStringList{"service","restart"}})
        if(allowedCommand(command)){std::cerr<<"Unsafe broker command accepted\n";return 1;}
    for(const auto& command:{QStringList{"keyboard","white-point","160","255","255"},QStringList{"keyboard","color","ffffff"},QStringList{"keyboard","hardware","wave","--speed","5"},QStringList{"config","recover"},QStringList{"settings","camera","off"}})
        if(!allowedCommand(command))return 2;
    if(!allowedCommand({"keyboard","color-profile","set","panel",QString(256,u'a'),QString(256,u'b')}))return 4;
    if(allowedCommand({"keyboard","color-profile","set","panel",QString(257,u'a')}))return 5;
    auto info=ctlCommand({"--version"});if(!info.ok||info.output.isEmpty())return 3;
    QList<QStringList> calls;
    auto execute=[&](const QStringList& args,bool privileged){
        if(!privileged)return CommandResult{false,"Unprivileged mutation"};
        calls.append(args);return CommandResult{true,{}};
    };
    auto reset=applyCommand({"cpu","limits","reset"},{},execute);
    if(!reset.ok||calls!=QList<QStringList>{{"config","protect"},{"cpu","limits","reset"},{"config","unset","cpu_min_mhz","cpu_max_mhz"}})return 6;
    calls.clear();
    auto failing=[&](const QStringList& args,bool privileged){
        auto result=execute(args,privileged);
        if(args.value(0)=="cpu")return CommandResult{false,"fixture hardware failure"};
        return result;
    };
    if(applyCommand({"cpu","limits","reset"},{},failing).ok||calls.contains({"config","unset","cpu_min_mhz","cpu_max_mhz"}))return 7;
    calls.clear();
    auto diskFailure=[&](const QStringList& args,bool privileged){
        auto result=execute(args,privileged);
        if(args.value(1)=="unset")return CommandResult{false,"fixture disk failure"};
        return result;
    };
    const auto failedSave=applyCommand({"cpu","limits","reset"},{},diskFailure);
    if(failedSave.ok||!failedSave.output.contains("temizlenemedi"))return 8;
    calls.clear();
    auto tpmFailure=[&](const QStringList& args,bool privileged){
        execute(args,privileged);return CommandResult{false,"fixture TPM failure"};
    };
    if(applyCommand({"keyboard","color","ffffff"},{},tpmFailure).ok||calls!=QList<QStringList>{{"config","protect"}})return 9;
    calls.clear();
    auto toggle=[&](const QStringList& args,bool){calls.append(args);return CommandResult{true,args==QStringList{"fan","mode"}?QStringLiteral("boost\n"):QString{}};};
    if(!allowedCommand({"fan","toggle"})||!applyCommand({"fan","toggle"},{},toggle).ok||calls.last()!=QStringList{"config","set","fan_mode","boost"})return 10;
    return 0;
}
