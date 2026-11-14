// SPDX-License-Identifier: MIT
#include "power.hpp"
#include <QCoreApplication>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusObjectPath>
#include <QProcess>
#include <QTextStream>
#include <systemd/sd-login.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
static int reloadSession(bool restart=false){
    if(geteuid()==0){
        uid_t uid=0;char* session=nullptr;const int result=sd_seat_get_active("seat0",&session,&uid);free(session);
        if(result<0||uid==0)return 0;
        auto* user=getpwuid(uid);if(!user)return 3;
        // Exec a fresh unprivileged process before touching the user bus.
        const QByteArray address="unix:path=/run/user/"+QByteArray::number(uid)+"/bus";
        const QByteArray runtime="/run/user/"+QByteArray::number(uid);
        const QByteArray home=user->pw_dir,name=user->pw_name;
        if(initgroups(name.constData(),user->pw_gid)||setgid(user->pw_gid)||setuid(uid))return 3;
        clearenv();setenv("DBUS_SESSION_BUS_ADDRESS",address.constData(),1);setenv("XDG_RUNTIME_DIR",runtime.constData(),1);setenv("HOME",home.constData(),1);setenv("USER",name.constData(),1);
        execl("/usr/libexec/slayerctl-desktop","slayerctl-desktop",restart?"--restart-session-user":"--reload-session-user",static_cast<char*>(nullptr));return 4;
    }
    if(restart){
        QDBusInterface systemd("org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager",QDBusConnection::sessionBus());systemd.setTimeout(5000);
        if(!QDBusReply<void>(systemd.call("Reload")).isValid())return 4;
        return QDBusReply<QDBusObjectPath>(systemd.call("RestartUnit","slayerctl-session.service","replace")).isValid()?0:4;
    }
    QDBusInterface iface("org.gamegaraj.Slayer.Session1","/org/gamegaraj/Slayer/Session1","org.gamegaraj.Slayer.Session1",QDBusConnection::sessionBus());iface.setTimeout(5000);
    return QDBusReply<void>(iface.call("Reload")).isValid()?0:4;
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);auto args=app.arguments();args.removeFirst();
    if(args==QStringList{"--reload-session"}||args==QStringList{"--reload-session-user"})return reloadSession();
    if(args==QStringList{"--restart-session"}||args==QStringList{"--restart-session-user"})return reloadSession(true);
    if(args.size()==2&&args[0]=="--accent"&&QStringList{"status","on","off"}.contains(args[1])&&geteuid()!=0){
        QDBusInterface accent("org.kde.kded6","/modules/kameleon","org.kde.kameleon",QDBusConnection::sessionBus());accent.setTimeout(2000);
        if(args[1]=="status"){
            QDBusReply<bool> result=accent.call("isEnabled");if(!result.isValid())return 4;
            QTextStream(stdout)<<(result.value()?"true":"false")<<'\n';return 0;
        }
        return QDBusReply<void>(accent.call("setEnabled",args[1]=="on")).isValid()?0:4;
    }
    if(args.value(0)=="--idle"){
        QDBusInterface core("org.gamegaraj.Slayer1","/org/gamegaraj/Slayer1","org.gamegaraj.Slayer1",QDBusConnection::systemBus());core.setTimeout(90000);
        if(args.size()==1||args==QStringList{"--idle","status"}){
            QDBusReply<QVariantMap> result=core.call("ReadSessionSettings");if(!result.isValid())return 4;
            auto values=result.value();
            if(geteuid()!=0){
                QDBusInterface session("org.gamegaraj.Slayer.Session1","/org/gamegaraj/Slayer/Session1","org.gamegaraj.Slayer.Session1",QDBusConnection::sessionBus());session.setTimeout(2000);
                QDBusReply<QVariantMap> desktop=session.call("Status");
                if(desktop.isValid()){const auto state=desktop.value();for(auto it=state.cbegin();it!=state.cend();++it)values.insert("session_"+it.key(),it.value());}
            }
            QTextStream out(stdout);for(auto it=values.cbegin();it!=values.cend();++it)out<<it.key()<<": "<<it.value().toString()<<'\n';return 0;
        }
        QStringList command{"config","set"};
        if(args.size()==2&&QStringList{"on","off"}.contains(args[1]))command.append({"keyboard_idle_dim",args[1]});
        else if(args.size()==3&&args[1]=="timeout")command.append({"keyboard_idle_timeout",args[2]});
        else return 2;
        QDBusReply<QString> result=core.call("Apply",command,QStringList{});
        if(!result.isValid()){QTextStream(stderr)<<result.error().message()<<'\n';return 4;}
        QTextStream(stdout)<<result.value();return 0;
    }
    if(args.value(0)=="--settings"&&args.size()==2&&QStringList{"display","touchpad","power"}.contains(args[1])&&geteuid()!=0){
        const auto module=args[1];const bool gnome=qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("GNOME",Qt::CaseInsensitive);
        const auto panel=gnome?(module=="touchpad"?QString("mouse"):module):(module=="display"?QString("kcm_kscreen"):module=="touchpad"?QString("kcm_touchpad"):QString("kcm_powerdevilprofilesconfig"));
        return QProcess::startDetached(gnome?"/usr/bin/gnome-control-center":"/usr/bin/kcmshell6",{panel})?0:4;
    }
    if(args==QStringList{"--reboot"}&&geteuid()!=0){
        QDBusInterface login("org.freedesktop.login1","/org/freedesktop/login1","org.freedesktop.login1.Manager",QDBusConnection::systemBus());login.setTimeout(60000);
        return QDBusReply<void>(login.call("Reboot",true)).isValid()?0:4;
    }
    return desktopPower(args);
}
