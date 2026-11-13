// SPDX-License-Identifier: MIT
#include "session.hpp"
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QTextStream>
int desktopPower(const QStringList&args){
    QTextStream out(stdout),err(stderr);
    const QString iface=QStringLiteral("org.freedesktop.UPower.PowerProfiles");
    QDBusInterface properties(iface,QStringLiteral("/org/freedesktop/UPower/PowerProfiles"),QStringLiteral("org.freedesktop.DBus.Properties"),QDBusConnection::systemBus());
    properties.setTimeout(3000);
    if(!properties.isValid()){err<<"Masaüstü güç profili servisi kullanılamıyor.\n";return 4;}
    QDBusReply<QVariant> reply=properties.call(QStringLiteral("Get"),iface,QStringLiteral("Profiles"));
    if(!reply.isValid()){err<<reply.error().message()<<'\n';return 4;}
    QStringList names;
    const QDBusArgument array=qvariant_cast<QDBusArgument>(reply.value());
    array.beginArray();while(!array.atEnd()){QVariantMap item;array>>item;const auto profile=item.value(QStringLiteral("Profile")).toString();if(!profile.isEmpty())names.append(profile);}array.endArray();
    if(args.size()==1&&args[0]==u"--power-list"){
        QDBusReply<QVariant> active=properties.call(QStringLiteral("Get"),iface,QStringLiteral("ActiveProfile"));
        if(!active.isValid()){err<<active.error().message()<<'\n';return 4;}
        out<<"profile: "<<active.value().toString()<<"\nchoices: "<<names.join(u' ')<<"\nprovider: org.freedesktop.UPower.PowerProfiles\n";return 0;
    }
    if(args.size()!=2||args[0]!=u"--power-set"||!names.contains(args[1])){err<<"Servisin bildirdiği bir güç profili gerekli.\n";return 2;}
    QDBusReply<void> set=properties.call(QStringLiteral("Set"),iface,QStringLiteral("ActiveProfile"),QVariant::fromValue(QDBusVariant(args[1])));
    if(!set.isValid()){err<<set.error().message()<<'\n';return 4;}
    out<<"Masaüstü güç profili: "<<args[1]<<'\n';return 0;
}
