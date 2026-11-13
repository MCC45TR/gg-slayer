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
        ||(c==u"power"&&a.size()==3&&sub==u"set")||(c==u"profile"&&QStringList{"set","sync"}.contains(sub))||(c==u"fan"&&QStringList{"auto","boost","curve"}.contains(sub))
        ||(c==u"cpu"&&QStringList{"boost","smt","limits"}.contains(sub))
        ||(c==u"gpu"&&QStringList{"mode","power-limit"}.contains(sub))
        ||(c==u"settings"&&a.size()==3)||(c==u"usb"&&QStringList{"powersave","wake"}.contains(sub))
        ||(c==u"config"&&QStringList{"set","unset","protect","recover"}.contains(sub));
}
