// SPDX-License-Identifier: MIT
#include "../gui/keyboard_map.hpp"
#include <QGuiApplication>
#include <QTimer>
#include <iostream>
int main(int argc,char** argv){
    QGuiApplication app(argc,argv);DesktopKeyboard mapper;
    QObject::connect(&mapper,&DesktopKeyboard::changed,&app,[&]{
        std::cout<<mapper.origin().toStdString()<<": "<<mapper.name().toStdString()<<'\n';
        QVariantMap key{{QStringLiteral("id"),QStringLiteral("Gbreve")},{QStringLiteral("label"),QStringLiteral("Gbreve")}};
        std::cout<<mapper.decorate(key).value(QStringLiteral("label")).toString().toStdString()<<'\n';app.exit(0);
    });
    QTimer::singleShot(2500,&app,[&]{std::cerr<<"No native keymap received\n";app.exit(4);});
    return app.exec();
}
