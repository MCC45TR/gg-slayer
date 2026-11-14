// SPDX-License-Identifier: MIT
#include "../gui/keyboard_map.hpp"
#include <QCoreApplication>
#include <iostream>
using namespace Qt::Literals::StringLiterals;
static QVariantMap key(const QString& id){return {{u"id"_s,id},{u"label"_s,id},{u"row"_s,2},{u"column"_s,3},{u"width"_s,1}};}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);DesktopKeyboard mapper;mapper.setFormat(u"iso"_s,false);
    if(!mapper.loadNames("tr"))return 1;
    if(mapper.decorate(key(u"Gbreve"_s))[u"label"_s].toString()!=u"Ğ"_s)return 2;
    if(mapper.decorate(key(u"Idot"_s))[u"label"_s].toString()!=u"İ"_s)return 3;
    if(!mapper.loadNames("tr","f"))return 4;
    if(mapper.decorate(key(u"Q"_s))[u"label"_s].toString()!=u"F"_s)return 5;
    if(!mapper.loadNames("us"))return 6;
    if(mapper.decorate(key(u"Gbreve"_s))[u"label"_s].toString()!=u"["_s)return 7;
    if(!mapper.loadNames("us",{},"caps:swapescape"))return 8;
    if(mapper.decorate(key(u"CapsLock"_s))[u"label"_s].toString()!=u"Esc"_s)return 9;
    if(!mapper.loadNames("us,tr"))return 10;mapper.setGroup(1);
    if(mapper.decorate(key(u"Gbreve"_s))[u"label"_s].toString()!=u"Ğ"_s)return 11;
    auto old=mapper.decorate(key(u"Gbreve"_s));mapper.setGroup(0);
    if(mapper.decorate(old)[u"label"_s].toString()!=u"["_s)return 12;
    if(mapper.loadText(nullptr,0))return 13;
    auto tall=key(u"NumAdd"_s);if(mapper.decorate(tall)[u"h"_s].toDouble()!=2)return 14;
    if(mapper.decorate(key(u"Enter"_s))[u"shape"_s].toString()!=u"iso-enter")return 15;
    mapper.setFormat(u"ansi"_s,false);
    if(mapper.decorate(key(u"Enter"_s))[u"h"_s].toDouble()!=1)return 16;
    if(mapper.decorate(key(u"ISO"_s))[u"present"_s].toBool())return 17;
    mapper.setFormat(u"iso"_s,false);
    if(mapper.decorate(key(u"Menu"_s))[u"label"_s].toString()!=u"Copilot"_s)return 18;
    std::cout<<"TR-Q, TR-F, US, remapped Caps/Escape, group switch and geometry passed; no input device opened.\n";
}
