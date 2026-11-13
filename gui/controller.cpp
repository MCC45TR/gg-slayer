// SPDX-License-Identifier: MIT
#include "controller.hpp"
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusArgument>
#include <QSettings>
#include <QStandardPaths>
#include <QColor>
namespace {
constexpr auto service="org.gamegaraj.Slayer1";
constexpr auto path="/org/gamegaraj/Slayer1";
QDBusMessage call(const QString& method){return QDBusMessage::createMethodCall(service,path,service,method);}
}
Controller::Controller(SlayerBackend* backend,bool fixtureMode,QObject* parent):QObject(parent),device(backend),fixture(fixtureMode){
    QSettings preferences;reduce=preferences.value("reducedMotion",false).toBool();
    timer.setInterval(3000);connect(&timer,&QTimer::timeout,this,&Controller::telemetry);
    if(!fixture){
        QDBusConnection::systemBus().connect(service,path,service,"StateChanged",this,SLOT(stateChanged()));

    }
    QTimer::singleShot(0,this,&Controller::refresh);
}
void Controller::setActive(bool value){
    if(foreground==value)return;
    foreground=value;
    if(!fixture){QDBusConnection::systemBus().asyncCall(call(value?"Subscribe":"Unsubscribe"));if(value)refresh();}
    Q_EMIT changed();
}
void Controller::setMonitoring(bool value){if(visible==value)return;visible=value;if(value){timer.start();telemetry();}else timer.stop();Q_EMIT changed();}
void Controller::setReducedMotion(bool value){reduce=value;if(!fixture)QSettings().setValue("reducedMotion",value);Q_EMIT changed();}
void Controller::refresh(){
    if(fixture){values={{"cpu_usage",23},{"gpu_usage",12},{"cpu_fan",1850},{"gpu_fan",1720},{"cpu_temp",48},{"gpu_temp",43}};power="balanced";ppChoices={"power-saver","balanced","performance"};ppAvailable=true;Q_EMIT metricsChanged();Q_EMIT changed();return;}
    auto* watcher=new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call("ReadState"),20000),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher]{QDBusPendingReply<QString> reply=*watcher;watcher->deleteLater();if(reply.isError()){lastError=tr("Cihaz servisine ulaşılamadı: %1").arg(reply.error().message());device->acceptData({});}else{device->acceptData(reply.value());readPower();const auto problem=device->info().value("config_error").toString();if(!problem.isEmpty())lastError=problem;}Q_EMIT changed();});
}
void Controller::apply(const QStringList& command,const QStringList& remember){
    if(command.isEmpty())return;
    if(working&&inFlight.command==command&&inFlight.remember==remember)return;
    if(fixture){lastError=tr("Önizlemede donanım ve dosya yazmaları kapalıdır.");Q_EMIT changed();return;}
    // Coalesce pending edits of the same control; preserve ordering across controls.
    for(auto& pending:queue)if(pending.command.mid(0,2)==command.mid(0,2)){pending={command,remember};return;}
    if(queue.size()<16)queue.enqueue({command,remember});
    drain();
}
void Controller::drain(){
    if(working||queue.isEmpty())return;
    working=true;lastError.clear();Q_EMIT changed();const auto pending=queue.dequeue();inFlight=pending;
    auto message=call("Apply");message<<pending.command<<pending.remember;
    auto* watcher=new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message,90000),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher]{QDBusPendingReply<QString> reply=*watcher;watcher->deleteLater();working=false;if(reply.isError()){lastError=reply.error().message();queue.clear();}else Q_EMIT succeeded();refresh();Q_EMIT changed();drain();});
}
void Controller::telemetry(){
    if(!visible||fixture||sampling)return;
    sampling=true;
    auto* watcher=new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call("ReadTelemetry"),5000),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher]{QDBusPendingReply<QString> reply=*watcher;watcher->deleteLater();sampling=false;if(!visible||reply.isError())return;
        QVariantMap next;quint64 total=0,idle=0;
        for(const auto& line:reply.value().split(u'\n')){const auto p=line.split(u'\t');if(p.size()==3&&p[0]==u"value"){
            if(p[1]==u"cpu_ticks"){auto ticks=p[2].split(u' ',Qt::SkipEmptyParts);for(int i=0;i<qMin(8,ticks.size());++i){auto n=ticks[i].toULongLong();total+=n;if(i==3||i==4)idle+=n;}}
            else if(p[1]==u"gpu_usage"||p[1]==u"amd_usage"||p[1]==u"nvidia_usage")next.insert(p[1],p[2].toInt());
        }else if(p.size()==5&&p[0]==u"sensor"){
            const auto label=p[2].toLower();bool ok=false;const double n=p[4].toDouble(&ok);if(!ok)continue;
            if(p[3]==u"rpm"){if(label.contains("cpu"))next["cpu_fan"]=n;else if(label.contains("gpu"))next["gpu_fan"]=n;}
            else if(label.contains("cpu")||(p[1].contains("k10temp")&&label.contains("tctl")))next["cpu_temp"]=n/1000.;else if(label.contains("gpu")||(p[1].contains("amdgpu")&&!next.contains("gpu_temp")))next["gpu_temp"]=n/1000.;
        }}
        if(previousTotal&&total>previousTotal&&idle>=previousIdle)next["cpu_usage"]=qBound(0.,100.*(1.-double(idle-previousIdle)/double(total-previousTotal)),100.);
        if(next.contains("nvidia_usage"))next["gpu_usage"]=next.value("nvidia_usage");
        else if(next.contains("amd_usage"))next["gpu_usage"]=next.value("amd_usage");
        previousTotal=total;previousIdle=idle;if(values!=next){values=next;Q_EMIT metricsChanged();}
    });
}
void Controller::diagnose(const QString& topic){
    if(fixture){diagnostic=tr("Önizleme: %1\nDonanıma erişilmedi.").arg(topic);Q_EMIT changed();return;}
    auto msg=call("Diagnose");msg<<topic;auto* watcher=new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(msg,10000),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher]{QDBusPendingReply<QString> reply=*watcher;watcher->deleteLater();diagnostic=reply.isError()?reply.error().message():reply.value();Q_EMIT changed();});
}
void Controller::readPower(){
    const auto info=device->info();power=info.value("desktop_profile").toString();
    ppAvailable=info.value("desktop_available").toString()=="1";
    ppChoices=info.value("desktop_choices").toString().split(u' ',Qt::SkipEmptyParts);
}
void Controller::setPower(const QString& value){
    if(fixture||!ppChoices.contains(value))return;
    apply({"power","set",value},{"desktop_profile",value});
}
void Controller::reboot(){
    if(fixture){lastError=tr("Önizlemede yeniden başlatma kapalıdır.");Q_EMIT changed();return;}
    device->run({"desktop","reboot"},false);
}
