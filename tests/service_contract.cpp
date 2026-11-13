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
    return 0;
}
