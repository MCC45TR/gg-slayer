// SPDX-License-Identifier: MIT
#include "../gui/controller.hpp"
#include <QCoreApplication>
#include <QDBusContext>
#include <QDBusMessage>
#include <QElapsedTimer>
#include <QThread>
#include <QTemporaryDir>
#include <functional>
#include <iostream>

class Broker final:public QObject,protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.gamegaraj.Slayer1")
public:
    QList<QStringList> applied;
    QList<QDBusMessage> waiting,reads;
    int readCount=0;
    int samples=0;
    bool holdReads=true,failRead=false;
    void finish(){const auto request=waiting.takeFirst();QDBusConnection::sessionBus().send(request.createReply(QVariantList{QString{}}));}
    void finishRead(){const auto request=reads.takeFirst();QDBusConnection::sessionBus().send(request.createReply(QVariantList{QStringLiteral("value\tdesktop_available\t1\nvalue\tdesktop_choices\tbalanced\n")}));}
public Q_SLOTS:
    QString ReadState(){
        ++readCount;
        if(failRead){sendErrorReply("org.gamegaraj.Slayer1.Failed","fixture read failure");return {};}
        if(holdReads){setDelayedReply(true);reads.append(message());return {};}
        return "value\tdesktop_available\t1\nvalue\tdesktop_choices\tbalanced\n";
    }
    QString Apply(const QStringList& args,const QStringList&){applied.append(args);waiting.append(message());setDelayedReply(true);return {};}
    QString ReadTelemetry(){++samples;return QString("value\tcpu_ticks\t%1 0 0 100 0 0 0 0\n").arg(100+samples*10);}
};
static void require(bool ok,const char* reason){if(!ok){std::cerr<<reason<<'\n';std::exit(1);}}
static bool until(const std::function<bool()>& predicate){QElapsedTimer clock;clock.start();while(!predicate()&&clock.elapsed()<5000){QCoreApplication::processEvents(QEventLoop::AllEvents,10);QThread::msleep(1);}return predicate();}
int main(int argc,char** argv){
    QTemporaryDir config;qputenv("XDG_CONFIG_HOME",config.path().toUtf8());
    QCoreApplication app(argc,argv);
    qputenv("DBUS_SYSTEM_BUS_ADDRESS",qgetenv("DBUS_SESSION_BUS_ADDRESS"));
    auto bus=QDBusConnection::sessionBus();Broker broker;
    require(bus.registerService("org.gamegaraj.Slayer1")&&bus.registerObject("/org/gamegaraj/Slayer1",&broker,QDBusConnection::ExportAllSlots),"Private broker registration");
    SlayerBackend backend(nullptr,false);Controller control(&backend,false);
    require(until([&]{return broker.readCount==1;}),"Initial state request");
    for(int i=0;i<100;++i)control.refresh();
    QCoreApplication::processEvents();require(broker.readCount==1,"Refresh flooded the bus");
    broker.finishRead();require(until([&]{return broker.readCount==2;}),"Latest refresh was dropped");
    broker.holdReads=false;broker.finishRead();
    require(until([&]{return backend.info().value("desktop_available")=="1";}),"State reply missing");
    const QList<QStringList> expected{
        {"keyboard","identify","1"},
        {"config","set","desktop_profile_ac","balanced"},
        {"config","set","desktop_profile_battery","power-saver"},
        {"keyboard","key","W","ff0000"},{"keyboard","key","A","00ff00"},
        {"keyboard","profile","create","one"},{"keyboard","profile","create","two"},
        {"keyboard","brightness","10"}
    };
    control.apply(expected[0]);require(until([&]{return broker.applied.size()==1;}),"First command missing");
    for(int i=1;i<expected.size()-1;++i)control.apply(expected[i]);
    control.apply({"keyboard","brightness","15"});control.apply(expected.last());
    while(control.busy()){
        require(until([&]{return !broker.waiting.isEmpty();}),"Command stalled");broker.finish();
        require(until([&]{return !control.busy()||!broker.waiting.isEmpty();}),"Command reply stalled");
    }
    require(broker.applied==expected,"Independent edits or command ordering lost");
    broker.applied.clear();
    control.apply({"keyboard","brightness","10"});require(until([&]{return broker.applied.size()==1;}),"Brightness command missing");
    control.apply({"keyboard","brightness","20"});control.apply({"keyboard","brightness","10"});
    broker.finish();require(until([&]{return broker.applied.size()==2;}),"Return to in-flight value was discarded");
    require(broker.applied.last().last()=="10","Obsolete queued value applied");
    broker.finish();require(until([&]{return !control.busy();}),"Final command stalled");
    broker.failRead=true;control.refresh();
    require(until([&]{return control.error().contains("fixture read failure")&&!control.powerAvailable();}),"Read failure retained stale power controls");
    broker.failRead=false;control.refresh();
    require(until([&]{return control.error().isEmpty()&&control.powerAvailable();}),"Recovered service retained a stale read error");
    require(!control.liveTelemetry()&&control.reducedMotion(),"Default settings consume background resources");
    control.setMonitoring(true);require(until([&]{return broker.samples==2&&control.metrics().contains("cpu_usage");}),"On-demand CPU sample did not complete");
    QElapsedTimer quiet;quiet.start();require(until([&]{return quiet.elapsed()>3300;}),"Idle interval");
    require(broker.samples==2,"Default page keeps polling sensors");
    control.setLiveTelemetry(true);require(until([&]{return broker.samples==4;}),"Opt-in live telemetry did not start");
    control.setMonitoring(false);control.refreshTelemetry();QCoreApplication::processEvents();
    require(broker.samples==4,"Hidden page sampled sensors");
    std::cout<<"Independent edits survive; adjacent setters coalesce; refresh requests stay bounded.\n";
}
#include "controller.moc"
