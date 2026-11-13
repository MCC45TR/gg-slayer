// SPDX-License-Identifier: MIT
#include "idle_watch.hpp"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QTimer>
#include <QVariantMap>
#include <unistd.h>

namespace {
constexpr auto service="org.gamegaraj.Slayer1";
constexpr auto path="/org/gamegaraj/Slayer1";
constexpr auto sessionService="org.gamegaraj.Slayer.Session1";
}
class Session final:public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.gamegaraj.Slayer.Session1")
    IdleWatch watch;
    QTimer exitTimer;
    QDBusServiceWatcher root;
    unsigned seconds=30;
    bool enabled=false,armed=false,isIdle=false,reloading=false;
    QString lightingOwner,error;
    void notify(bool idle){
        auto call=QDBusMessage::createMethodCall(service,path,service,"IdleChanged");call<<idle;
        auto* pending=new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call,3000),this);
        connect(pending,&QDBusPendingCallWatcher::finished,this,[this,pending]{QDBusPendingReply<bool> reply=*pending;pending->deleteLater();if(reply.isError()||!reply.value())error=tr("Etkin oturumun klavye kontrolü alınamadı.");});
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
        connect(&watch,&IdleWatch::idleChanged,this,[this](bool idle){isIdle=idle;notify(idle);});
        connect(&watch,&IdleWatch::unavailable,this,[this](const QString& reason){error=reason;armed=false;isIdle=false;notify(false);});
        QDBusConnection::systemBus().connect(service,path,service,"SessionSettingsChanged",this,SLOT(Reload()));
        root.setConnection(QDBusConnection::systemBus());root.setWatchedServices({service});root.setWatchMode(QDBusServiceWatcher::WatchForRegistration);
        connect(&root,&QDBusServiceWatcher::serviceRegistered,this,[this]{Reload();});
        QTimer::singleShot(0,this,&Session::Reload);
    }
public Q_SLOTS:
    void Reload(){
        if(reloading)return;reloading=true;
        auto msg=QDBusMessage::createMethodCall(service,path,service,"ReadSessionSettings");
        auto* pending=new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(msg,20000),this);
        connect(pending,&QDBusPendingCallWatcher::finished,this,[this,pending]{
            QDBusPendingReply<QVariantMap> result=*pending;pending->deleteLater();reloading=false;
            if(result.isError()){error=result.error().message();watch.stop();armed=false;notify(false);exitTimer.start(5200);return;}
            const auto cfg=result.value();const bool wasEnabled=enabled;
            enabled=cfg.value("enabled").toBool();const unsigned timeout=cfg.value("timeout",30).toUInt();
            owner(cfg.value("lightingOwner").toString());
            const bool wanted=enabled&&!cfg.value("sleeping").toBool()&&cfg.value("powerSaver").toBool()&&cfg.value("available").toBool();
            if(armed&&(!wanted||seconds!=timeout)){watch.stop();armed=false;isIdle=false;notify(false);}
            seconds=timeout;
            if(wanted&&!armed){armed=watch.start(seconds);if(!armed)error=tr("Bu oturum etkinlik bildirimi sunmuyor.");}
            if(enabled){
                exitTimer.stop();QDBusConnection::systemBus().asyncCall(QDBusMessage::createMethodCall(service,path,service,"Subscribe"));
            }else exitTimer.start(wasEnabled?5200:2500);
        });
    }
    QVariantMap Status(){return {{"enabled",enabled},{"armed",armed},{"idle",isIdle},{"timeout",seconds},{"backend",watch.backend()},{"error",error}};}
};
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);if(geteuid()==0)return 3;
    Session session;auto bus=QDBusConnection::sessionBus();
    if(!bus.registerService(sessionService))return 0;
    if(!bus.registerObject("/org/gamegaraj/Slayer/Session1",&session,QDBusConnection::ExportAllSlots))return 1;
    return app.exec();
}
#include "session_main.moc"
