// SPDX-License-Identifier: MIT
#include "idle_watch.hpp"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QTimer>
#include <QVariantMap>
#include <QSet>
#include <QMap>
#include <utility>
#include <unistd.h>

namespace {
constexpr auto service="org.gamegaraj.Slayer1";
constexpr auto path="/org/gamegaraj/Slayer1";
constexpr auto sessionService="org.gamegaraj.Slayer.Session1";
}
class Session final:public QObject,protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.gamegaraj.Slayer.Session1")
    IdleWatch watch;
    QTimer exitTimer;
    QDBusServiceWatcher root;
    QDBusServiceWatcher calibrationClients;
    QSet<QString> calibrations;
    QMap<QString,QDBusMessage> calibrationReplies;
    bool calibrationPrepared=false;
    quint64 calibrationGeneration=0;
    unsigned seconds=30;
    bool enabled=false,armed=false,isIdle=false,reloading=false;
    QString lightingOwner,error;
    void notify(bool idle){
        auto call=QDBusMessage::createMethodCall(service,path,service,"IdleChanged");call<<idle;
        auto* pending=new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call,3000),this);
        connect(pending,&QDBusPendingCallWatcher::finished,this,[this,pending]{QDBusPendingReply<bool> reply=*pending;pending->deleteLater();if(reply.isError()||!reply.value())error=tr("Etkin oturumun klavye kontrolü alınamadı.");});
    }
    void releaseCalibration(const QString& sender,bool cancelled){
        if(!calibrations.remove(sender))return;
        calibrationClients.removeWatchedService(sender);
        if(calibrationReplies.contains(sender)){
            if(cancelled)QDBusConnection::sessionBus().send(calibrationReplies.value(sender).createErrorReply("org.gamegaraj.Slayer.Session1.Cancelled",tr("Kalibrasyon kapatıldı.")));
            calibrationReplies.remove(sender);
        }
        if(calibrations.isEmpty()){++calibrationGeneration;calibrationPrepared=false;Reload();}
    }
    void owner(const QString& value){
        if(value==lightingOwner)return;lightingOwner=value;
        if(value!="accent"&&value!="custom")return;
        // Session-specific KDE integration belongs to ctl, never the frontend.
        auto get=QDBusMessage::createMethodCall("org.kde.kded6","/modules/kameleon","org.kde.kameleon","isEnabled");
        auto* pending=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(get,2000),this);
        connect(pending,&QDBusPendingCallWatcher::finished,this,[this,pending,value]{
            QDBusPendingReply<bool> result=*pending;pending->deleteLater();
            if(result.isError()){if(value=="accent")error=tr("KDE vurgu rengi servisi kullanılamıyor.");return;}
            if(value!=lightingOwner||result.value()==(value=="accent"))return;
            auto set=QDBusMessage::createMethodCall("org.kde.kded6","/modules/kameleon","org.kde.kameleon","setEnabled");set<<(value=="accent");
            QDBusConnection::sessionBus().asyncCall(set,2000);
        });
    }
public:
    Session(){
        exitTimer.setSingleShot(true);connect(&exitTimer,&QTimer::timeout,qApp,&QCoreApplication::quit);
        calibrationClients.setConnection(QDBusConnection::sessionBus());calibrationClients.setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
        connect(&calibrationClients,&QDBusServiceWatcher::serviceUnregistered,this,[this](const QString& sender){releaseCalibration(sender,false);});
        connect(&watch,&IdleWatch::idleChanged,this,[this](bool idle){isIdle=idle;notify(idle);});
        connect(&watch,&IdleWatch::unavailable,this,[this](const QString& reason){error=reason;armed=false;isIdle=false;notify(false);});
        QDBusConnection::systemBus().connect(service,path,service,"SessionSettingsChanged",this,SLOT(Reload()));
        root.setConnection(QDBusConnection::systemBus());root.setWatchedServices({service});root.setWatchMode(QDBusServiceWatcher::WatchForRegistration);
        connect(&root,&QDBusServiceWatcher::serviceRegistered,this,[this]{Reload();});
        QTimer::singleShot(0,this,&Session::Reload);
    }
public Q_SLOTS:
    bool BeginCalibration(){
        if(!calledFromDBus())return false;
        const auto sender=message().service();
        if(calibrations.contains(sender)){
            if(calibrationPrepared)return true;
            sendErrorReply("org.gamegaraj.Slayer.Session1.Busy",tr("Kalibrasyon parlaklığı hazırlanıyor."));return false;
        }
        if(calibrations.size()>=8){sendErrorReply("org.gamegaraj.Slayer.Session1.Busy",tr("Çok fazla kalibrasyon aracı açık."));return false;}
        calibrations.insert(sender);calibrationClients.addWatchedService(sender);exitTimer.stop();
        if(calibrationPrepared)return true;
        const auto request=message();setDelayedReply(true);calibrationReplies.insert(sender,request);
        if(calibrations.size()==1){
            if(armed){watch.stop();armed=false;}isIdle=false;
            const auto generation=++calibrationGeneration;
            const auto prepare=QDBusMessage::createMethodCall(service,path,service,"PrepareCalibration");
            auto* pending=new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(prepare,20000),this);
            connect(pending,&QDBusPendingCallWatcher::finished,this,[this,pending,generation]{
                QDBusPendingReply<bool> result=*pending;pending->deleteLater();
                if(calibrations.isEmpty()||generation!=calibrationGeneration)return;
                if(!result.isError()&&result.value()){
                    calibrationPrepared=true;
                    for(const auto& reply:std::as_const(calibrationReplies))QDBusConnection::sessionBus().send(reply.createReply(QVariantList{true}));
                    calibrationReplies.clear();
                }
                else{
                    error=tr("Kalibrasyon parlaklığı hazırlanamadı.");
                    for(const auto& reply:std::as_const(calibrationReplies))QDBusConnection::sessionBus().send(reply.createErrorReply("org.gamegaraj.Slayer.Session1.Unavailable",error));
                    calibrationReplies.clear();
                    const auto holders=calibrations;for(const auto& holder:holders)releaseCalibration(holder,false);
                }
            });
        }
        return false;
    }
    bool EndCalibration(){
        if(!calledFromDBus())return false;
        const auto sender=message().service();const bool held=calibrations.contains(sender);releaseCalibration(sender,true);return held;
    }
    void Reload(){
        if(reloading)return;reloading=true;
        auto msg=QDBusMessage::createMethodCall(service,path,service,"ReadSessionSettings");
        auto* pending=new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(msg,20000),this);
        connect(pending,&QDBusPendingCallWatcher::finished,this,[this,pending]{
            QDBusPendingReply<QVariantMap> result=*pending;pending->deleteLater();reloading=false;
            if(result.isError()){error=result.error().message();watch.stop();armed=false;notify(false);if(calibrations.isEmpty())exitTimer.start(5200);return;}
            const auto cfg=result.value();const bool wasEnabled=enabled;
            enabled=cfg.value("enabled").toBool();const unsigned timeout=cfg.value("timeout",30).toUInt();
            owner(cfg.value("lightingOwner").toString());
            const bool wanted=calibrations.isEmpty()&&enabled&&!cfg.value("sleeping").toBool()&&cfg.value("powerSaver").toBool()&&cfg.value("available").toBool();
            if(armed&&(!wanted||seconds!=timeout)){watch.stop();armed=false;isIdle=false;notify(false);}
            seconds=timeout;
            if(wanted&&!armed){armed=watch.start(seconds);if(!armed)error=tr("Bu oturum etkinlik bildirimi sunmuyor.");}
            if(enabled||!calibrations.isEmpty()){
                exitTimer.stop();QDBusConnection::systemBus().asyncCall(QDBusMessage::createMethodCall(service,path,service,"Subscribe"));
            }else exitTimer.start(wasEnabled?5200:2500);
        });
    }
    QVariantMap Status(){return {{"enabled",enabled},{"armed",armed},{"idle",isIdle},{"timeout",seconds},{"calibrating",!calibrations.isEmpty()},{"backend",watch.backend()},{"error",error}};}
};
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);if(geteuid()==0)return 3;
    Session session;auto bus=QDBusConnection::sessionBus();
    if(!bus.registerService(sessionService))return 0;
    if(!bus.registerObject("/org/gamegaraj/Slayer/Session1",&session,QDBusConnection::ExportAllSlots))return 1;
    return app.exec();
}
#include "session_main.moc"
