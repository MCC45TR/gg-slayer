// SPDX-License-Identifier: MIT
#include "shortcut.hpp"
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QTextStream>
#include <QVariantMap>

namespace {
int failed(const QString& text){
    QTextStream(stderr)<<text<<'\n';
    auto message=QDBusMessage::createMethodCall("org.freedesktop.Notifications","/org/freedesktop/Notifications","org.freedesktop.Notifications","Notify");
    message.setAutoStartService(false);
    message<<QStringLiteral("Slayer Control")<<0u<<QStringLiteral("input-keyboard")<<QStringLiteral("Copilot işlemi uygulanamadı")<<text.left(512).toHtmlEscaped()<<QStringList{}<<QVariantMap{}<<5000;
    QDBusConnection::sessionBus().call(message,QDBus::NoBlock);
    return 4;
}
}
int desktopShortcut(const QString& action){
    QStringList command,remember;
    if(action=="fan-boost")command={"fan","toggle"};
    else if(action=="fan-auto"){command={"fan","auto"};remember={"fan_mode","auto"};}
    else if(QStringList{"power-cycle","power-saver","balanced","performance"}.contains(action)){
        QString profile=action;
        if(action=="power-cycle"){
            constexpr auto provider="org.freedesktop.UPower.PowerProfiles";
            auto request=QDBusMessage::createMethodCall(provider,"/org/freedesktop/UPower/PowerProfiles","org.freedesktop.DBus.Properties","GetAll");request<<QString::fromLatin1(provider);
            QDBusReply<QVariantMap> reply=QDBusConnection::systemBus().call(request,QDBus::Block,3000);
            if(!reply.isValid())return failed(reply.error().message());
            const auto values=reply.value();const auto active=values.value("ActiveProfile").toString();QStringList available;
            const auto array=qvariant_cast<QDBusArgument>(values.value("Profiles"));
            array.beginArray();while(!array.atEnd()){QVariantMap item;array>>item;available.append(item.value("Profile").toString());}array.endArray();
            QStringList choices;
            for(const auto& candidate:QStringList{"power-saver","balanced","performance"})if(available.contains(candidate))choices.append(candidate);
            if(choices.isEmpty()||!choices.contains(active))return failed(QStringLiteral("Güç profili seçenekleri doğrulanamadı."));
            if(choices.size()==1)return 0;
            profile=choices[(choices.indexOf(active)+1)%choices.size()];
        }
        command={"power","set",profile};remember={"desktop_profile",profile};
    }else return failed(QStringLiteral("Desteklenmeyen Copilot işlemi."));
    auto request=QDBusMessage::createMethodCall("org.gamegaraj.Slayer1","/org/gamegaraj/Slayer1","org.gamegaraj.Slayer1","Apply");
    request<<command<<remember;
    QDBusReply<QString> result=QDBusConnection::systemBus().call(request,QDBus::Block,90000);
    if(!result.isValid())return failed(result.error().message());
    QTextStream(stdout)<<result.value();return 0;
}
