// SPDX-License-Identifier: MIT
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QElapsedTimer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QThread>
#include <QVariantMap>
#include <functional>
#include <iostream>
using Profiles=QList<QVariantMap>;
static void require(bool ok,const char* text){if(!ok){std::cerr<<text<<'\n';std::exit(1);}}
static bool until(const std::function<bool()>& predicate){QElapsedTimer clock;clock.start();while(!predicate()&&clock.elapsed()<4000){QCoreApplication::processEvents(QEventLoop::AllEvents,10);QThread::msleep(1);}return predicate();}
class Provider final:public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.freedesktop.UPower.PowerProfiles")
    Q_PROPERTY(QString ActiveProfile READ active)
    Q_PROPERTY(Profiles Profiles READ profiles)
public:
    QString current="balanced";
    QStringList choices{"performance","power-saver","balanced"};
    QString active()const{return current;}
    Profiles profiles()const{Profiles list;for(const auto& name:choices)list.append({{"Profile",name}});return list;}
};
class Core final:public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.gamegaraj.Slayer1")
public:
    QList<QStringList> commands,remembered;
public Q_SLOTS:
    QString Apply(const QStringList& command,const QStringList& remember){commands.append(command);remembered.append(remember);return "fixture accepted";}
};
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);require(argc==2,"Desktop binary required");qDBusRegisterMetaType<Profiles>();
    auto bus=QDBusConnection::sessionBus();Provider provider;Core core;
    require(bus.registerService("org.freedesktop.UPower.PowerProfiles")&&bus.registerObject("/org/freedesktop/UPower/PowerProfiles",&provider,QDBusConnection::ExportAllProperties),"Private power provider");
    require(bus.registerService("org.gamegaraj.Slayer1")&&bus.registerObject("/org/gamegaraj/Slayer1",&core,QDBusConnection::ExportAllSlots),"Private ctl broker");
    auto run=[&](const QString& action){
        QProcess process;auto env=QProcessEnvironment::systemEnvironment();env.insert("DBUS_SYSTEM_BUS_ADDRESS",qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"));process.setProcessEnvironment(env);
        process.start(QString::fromLocal8Bit(argv[1]),{"--shortcut",action});require(process.waitForStarted(),"Shortcut process startup");require(until([&]{return process.state()==QProcess::NotRunning;}),"Shortcut process did not exit");
        return process.exitCode();
    };
    require(run("fan-boost")==0&&core.commands.last()==QStringList{"fan","toggle"}&&core.remembered.last().isEmpty(),"Fan Boost did not use authenticated toggle");
    require(run("fan-auto")==0&&core.commands.last()==QStringList{"fan","auto"}&&core.remembered.last()==QStringList{"fan_mode","auto"},"Automatic fan mode persistence");
    require(run("power-cycle")==0&&core.commands.last()==QStringList{"power","set","performance"},"Cycle order depends on provider order");
    provider.choices={"balanced","power-saver"};
    require(run("power-cycle")==0&&core.commands.last()==QStringList{"power","set","power-saver"},"Cycle requested unsupported performance");
    provider.choices={"balanced"};const auto count=core.commands.size();
    require(run("power-cycle")==0&&core.commands.size()==count,"Single-profile cycle performed an unnecessary operation");
    for(const auto& name:QStringList{"power-saver","balanced","performance"})
        require(run(name)==0&&core.commands.last()==QStringList{"power","set",name}&&core.remembered.last()==QStringList{"desktop_profile",name},"Explicit power profile persistence");
    const auto before=core.commands.size();require(run("shell")==4&&core.commands.size()==before,"Unknown shortcut reached broker");
    std::cout<<"On-demand shortcuts use the broker, honor provider capabilities, preserve preferences and exit.\n";
}
#include "shortcut.moc"
