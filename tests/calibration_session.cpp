// SPDX-License-Identifier: MIT
// Exercise the real unprivileged ctl process on a private bus, without sysfs,
// a physical input stream, a display server or a privileged broker.
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusPendingReply>
#include <QElapsedTimer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include <QThread>
#include <QVariantMap>
#include <functional>
#include <utility>
#include <iostream>

class Core final:public QObject,protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.gamegaraj.Slayer1")
public:
    int preparations=0;
    bool reject=false,monitoring=true,loading=true,powerSaver=true;
    int subscriptions=0,unsubscriptions=0;
    QList<bool> notifications;
    QList<QDBusMessage> waiting;
    void finish(){for(const auto& request:std::as_const(waiting))QDBusConnection::sessionBus().send(request.createReply(QVariantList{true}));waiting.clear();}
public Q_SLOTS:
    QVariantMap ReadSessionSettings(){return {{"loading",loading},{"enabled",!loading},{"timeout",5u},{"powerSaver",powerSaver},{"available",true},{"lightingOwner",QStringLiteral("kde")},{"sleeping",false},{"armed",monitoring&&powerSaver},{"idle",false},{"backend",QStringLiteral("internal-keyboard")}};}
    void Subscribe(){++subscriptions;}
    void Unsubscribe(){++unsubscriptions;}
    bool IdleChanged(bool value){notifications.append(value);return true;}
    bool PrepareCalibration(){++preparations;monitoring=false;Q_EMIT SessionSettingsChanged();if(reject)return false;setDelayedReply(true);waiting.append(message());return false;}
    bool FinishCalibration(){monitoring=true;Q_EMIT SessionSettingsChanged();return true;}
Q_SIGNALS:
    void SessionSettingsChanged();
};
class KeyboardPolicy final:public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.kde.Solid.PowerManagement.PolicyAgent")
    Q_PROPERTY(uint SupportedInhibitionTypes READ supported CONSTANT)
public:
    uint supported() const {return 13u;}
    uint calls=0,releases=0,lastTypes=0;
public Q_SLOTS:
    uint AddInhibition(uint types,const QString&,const QString&){++calls;lastTypes=types;return calls;}
    void ReleaseInhibition(uint){++releases;}
};
class Mutter final:public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.gnome.Mutter.IdleMonitor")
public:
    uint next=1,lastIdle=0;
    QSet<uint> watches;
public Q_SLOTS:
    uint AddIdleWatch(qulonglong){lastIdle=next++;watches.insert(lastIdle);return lastIdle;}
    uint AddUserActiveWatch(){const auto id=next++;watches.insert(id);return id;}
    void RemoveWatch(uint id){watches.remove(id);}
Q_SIGNALS:
    void WatchFired(uint id);
};
static void require(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
static bool until(const std::function<bool()>& predicate,int limit=4000){QElapsedTimer elapsed;elapsed.start();while(!predicate()&&elapsed.elapsed()<limit){QCoreApplication::processEvents(QEventLoop::AllEvents,10);QThread::msleep(2);}return predicate();}
static QDBusPendingCall send(const QDBusConnection& bus,const QString& method){return bus.asyncCall(QDBusMessage::createMethodCall("org.gamegaraj.Slayer.Session1","/org/gamegaraj/Slayer/Session1","org.gamegaraj.Slayer.Session1",method),4000);}
static QVariantMap status(const QDBusConnection& bus){auto pending=send(bus,"Status");require(until([&]{return pending.isFinished();}),"Status timeout");QDBusPendingReply<QVariantMap> reply=pending;require(!reply.isError(),"Status failed");return reply.value();}
static bool boolean(QDBusPendingCall pending){require(until([&]{return pending.isFinished();}),"Method timeout");QDBusPendingReply<bool> reply=pending;require(!reply.isError(),"Unexpected method error");return reply.value();}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);require(argc==2,"Session binary required");
    auto bus=QDBusConnection::sessionBus();Core core;Mutter mutter;KeyboardPolicy keyboardPolicy;
    const auto flags=QDBusConnection::ExportAllSlots|QDBusConnection::ExportAllSignals;
    require(bus.registerService("org.gamegaraj.Slayer1")&&bus.registerObject("/org/gamegaraj/Slayer1",&core,flags),"Private core registration");
    require(bus.registerService("org.gnome.Mutter.IdleMonitor")&&bus.registerObject("/org/gnome/Mutter/IdleMonitor/Core",&mutter,flags),"Private idle provider registration");
    require(bus.registerService("org.kde.Solid.PowerManagement")&&bus.registerObject("/org/kde/Solid/PowerManagement/PolicyAgent",&keyboardPolicy,flags|QDBusConnection::ExportAllProperties),"Private keyboard policy registration");
    QProcess child;auto env=QProcessEnvironment::systemEnvironment();env.insert("DBUS_SYSTEM_BUS_ADDRESS",qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"));env.insert("XDG_CURRENT_DESKTOP","KDE");env.remove("WAYLAND_DISPLAY");env.remove("DISPLAY");child.setProcessEnvironment(env);child.start(QString::fromLocal8Bit(argv[1]));require(child.waitForStarted(),"Session process startup");
    require(until([&]{return bus.interface()->isServiceRegistered("org.gamegaraj.Slayer.Session1").value();}),"Session bus name");
    QElapsedTimer startup;startup.start();
    require(until([&]{return startup.elapsed()>=2800;}),"Slow settings fixture");
    require(child.state()==QProcess::Running&&!status(bus).value("armed").toBool(),"Session used defaults or exited during slow TPM load");
    core.loading=false;Q_EMIT core.SessionSettingsChanged();
    require(until([&]{return status(bus).value("armed").toBool();}),"Idle watch armed");
    require(status(bus).value("backend").toString()==QStringLiteral("internal-keyboard"),"Internal keyboard backend selected");
    require(until([&]{return status(bus).value("keyboardIdleDelegated").toBool();}),"Keyboard policy delegated");
    require(keyboardPolicy.calls==1&&keyboardPolicy.lastTypes==8u,"Keyboard delegation inhibited screens or sleep");
    bus.unregisterService("org.kde.Solid.PowerManagement");
    require(until([&]{return !status(bus).value("keyboardIdleDelegated").toBool();}),"Stale policy retained after PowerDevil exit");
    require(bus.registerService("org.kde.Solid.PowerManagement"),"PowerDevil restart registration");
    require(until([&]{return status(bus).value("keyboardIdleDelegated").toBool();}),"Keyboard delegation not reacquired after restart");
    require(keyboardPolicy.calls==2&&keyboardPolicy.lastTypes==8u,"Restart duplicated or broadened keyboard policy");
    require(mutter.watches.isEmpty(),"Global idle input must not be monitored");
    Q_EMIT mutter.WatchFired(mutter.lastIdle);
    QCoreApplication::processEvents(QEventLoop::AllEvents,10);
    require(core.notifications.isEmpty(),"Global desktop input affected keyboard lighting");
    core.powerSaver=false;Q_EMIT core.SessionSettingsChanged();
    require(until([&]{return core.unsubscriptions>0&&!status(bus).value("keyboardIdleDelegated").toBool();}),"Inactive session retained its policy/subscription");
    require(until([&]{return child.state()==QProcess::NotRunning;}),"Session stayed resident outside power saving");
    core.powerSaver=true;child.start(QString::fromLocal8Bit(argv[1]));require(child.waitForStarted(),"Session restart");
    require(until([&]{return bus.interface()->isServiceRegistered("org.gamegaraj.Slayer.Session1").value();}),"Restart session bus name");
    require(until([&]{return status(bus).value("keyboardIdleDelegated").toBool();}),"Active session did not reacquire policy");
    {
        auto a=QDBusConnection::connectToBus(QDBusConnection::SessionBus,"calibration-a");
        auto requestA=send(a,"BeginCalibration");
        require(until([&]{return core.preparations==1;}),"Full-brightness barrier requested");
        auto state=status(bus);require(state.value("calibrating").toBool()&&!state.value("armed").toBool(),"Calibration suspends idle watch");
        require(!requestA.isFinished(),"Measurement started before brightness settled");
        {
            auto b=QDBusConnection::connectToBus(QDBusConnection::SessionBus,"calibration-b");
            auto requestB=send(b,"BeginCalibration");
            auto duplicate=send(b,"BeginCalibration");require(until([&]{return duplicate.isFinished();}),"Duplicate preparation reply");
            QDBusPendingReply<bool> busy=duplicate;require(busy.isError(),"Pending preparation incorrectly became ready");
            require(!boolean(send(bus,"EndCalibration")),"Unrelated peer released another calibration");
            require(boolean(send(a,"EndCalibration")),"Original peer release");
            require(until([&]{return requestA.isFinished();}),"Cancelled preparation reply");
            QDBusPendingReply<bool> cancelled=requestA;require(cancelled.isError(),"Cancelled preparation incorrectly succeeded");
            require(core.preparations==1,"Parallel tools duplicated the hardware barrier");
            core.finish();require(boolean(requestB),"Remaining tool did not become ready");
            require(status(bus).value("calibrating").toBool(),"Remaining tool lost its lease");
        }
        QDBusConnection::disconnectFromBus("calibration-b");
        require(until([&]{const auto state=status(bus);return !state.value("calibrating").toBool()&&state.value("armed").toBool();}),"Disconnected tool did not release/rearm idle watch");
        {
            auto early=QDBusConnection::connectToBus(QDBusConnection::SessionBus,"calibration-early-exit");
            auto abandoned=send(early,"BeginCalibration");
            require(until([&]{return core.preparations==2;}),"Early-exit tool preparation");
            require(!abandoned.isFinished(),"Early-exit measurement prematurely ready");
        }
        QDBusConnection::disconnectFromBus("calibration-early-exit");
        require(until([&]{return !status(bus).value("calibrating").toBool();}),"Disconnected preparation retained its lease");
        core.finish();
        core.reject=true;
        auto rejected=send(a,"BeginCalibration");require(until([&]{return rejected.isFinished();}),"Rejected brightness barrier reply");
        QDBusPendingReply<bool> failure=rejected;require(failure.isError(),"Failed hardware preparation incorrectly succeeded");
        require(until([&]{return !status(bus).value("calibrating").toBool();}),"Failed preparation retained its lease");
    }
    QDBusConnection::disconnectFromBus("calibration-a");
    core.reject=false;
    auto restartClient=QDBusConnection::connectToBus(QDBusConnection::SessionBus,"calibration-restart");
    const int preparations=core.preparations;
    auto restartLease=send(restartClient,"BeginCalibration");
    require(until([&]{return core.preparations==preparations+1;}),"Restart test preparation");
    core.finish();require(boolean(restartLease),"Restart test lease");
    require(bus.unregisterService("org.gamegaraj.Slayer1"),"Broker disappearance");
    require(until([&]{return child.state()==QProcess::NotRunning;}),"Stale calibration survived broker loss");
    require(until([&]{return !bus.interface()->isServiceRegistered("org.gamegaraj.Slayer.Session1").value();}),"Measurement clients did not lose the invalid lease");
    QDBusConnection::disconnectFromBus("calibration-restart");
    child.terminate();if(!child.waitForFinished(2000)){child.kill();child.waitForFinished(2000);}
    std::cout<<"Calibration waits for brightness completion, shares restoration, cleans up cancellation/disconnect/failure, and invalidates measurements on broker loss.\n";
}
#include "calibration_session.moc"
