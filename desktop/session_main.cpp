// SPDX-License-Identifier: MIT
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
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
    QTimer exitTimer;
    QDBusServiceWatcher root;
    QDBusServiceWatcher powerDevil;
    uint keyboardPolicyCookie=0;
    quint64 keyboardPolicyGeneration=0;
    bool keyboardPolicyWanted=false,keyboardPolicyPending=false;
    const bool kde=qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(u':').contains("KDE",Qt::CaseInsensitive);
    QString keyboardPolicyOwner,keyboardPolicyError;
    static QDBusMessage policyCall(const QString& destination,const QString& method){
        return QDBusMessage::createMethodCall(destination,"/org/kde/Solid/PowerManagement/PolicyAgent","org.kde.Solid.PowerManagement.PolicyAgent",method);
    }
    static void releasePolicy(const QString& destination,uint cookie){
        auto msg=policyCall(destination,"ReleaseInhibition");msg<<cookie;
        QDBusConnection::sessionBus().asyncCall(msg,2000);
    }
    void updateKeyboardPolicy(bool wanted){
        keyboardPolicyWanted=kde&&wanted;
        if(!keyboardPolicyWanted){
            ++keyboardPolicyGeneration;keyboardPolicyPending=false;keyboardPolicyError.clear();
            if(keyboardPolicyCookie)releasePolicy(keyboardPolicyOwner,std::exchange(keyboardPolicyCookie,0u));
            return;
        }
        if(keyboardPolicyCookie||keyboardPolicyPending)return;
        keyboardPolicyPending=true;const auto generation=++keyboardPolicyGeneration;
        auto get=QDBusMessage::createMethodCall("org.kde.Solid.PowerManagement","/org/kde/Solid/PowerManagement/PolicyAgent","org.freedesktop.DBus.Properties","Get");
        get<<QStringLiteral("org.kde.Solid.PowerManagement.PolicyAgent")<<QStringLiteral("SupportedInhibitionTypes");
        auto* pending=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(get,2000),this);
        connect(pending,&QDBusPendingCallWatcher::finished,this,[this,pending,generation]{
            QDBusPendingReply<QDBusVariant> result=*pending;
            const auto destination=pending->reply().service();pending->deleteLater();
            if(generation!=keyboardPolicyGeneration)return;
            if(result.isError()||!(result.value().variant().toUInt()&8u)){
                keyboardPolicyPending=false;
                keyboardPolicyError=tr("KDE klavye bekleme devri kullanılamıyor. COPR PowerDevil güncellemesi gerekiyor.");return;
            }
            auto add=policyCall(destination,"AddInhibition");
            add<<8u<<QStringLiteral("slayerctl")<<tr("Yalnız dahili klavyenin bekleme aydınlatması yönetiliyor");
            auto* acquired=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(add,2000),this);
            connect(acquired,&QDBusPendingCallWatcher::finished,this,[this,acquired,generation,destination]{
                QDBusPendingReply<uint> cookie=*acquired;acquired->deleteLater();
                if(generation!=keyboardPolicyGeneration){if(!cookie.isError()&&cookie.value())releasePolicy(destination,cookie.value());return;}
                keyboardPolicyPending=false;
                if(cookie.isError()||!cookie.value()){keyboardPolicyError=tr("KDE klavye bekleme devri alınamadı.");return;}
                keyboardPolicyOwner=destination;keyboardPolicyCookie=cookie.value();keyboardPolicyError.clear();
            });
        });
    }
    QDBusServiceWatcher calibrationClients;
    QSet<QString> calibrations;
    QMap<QString,QDBusMessage> calibrationReplies;
    bool calibrationPrepared=false;
    quint64 calibrationGeneration=0;
    unsigned seconds=30;
    bool enabled=false,armed=false,isIdle=false,reloading=false,reloadPending=false;
    QString lightingOwner,error,backend;
    void releaseCalibration(const QString& sender,bool cancelled){
        if(!calibrations.remove(sender))return;
        calibrationClients.removeWatchedService(sender);
        if(calibrationReplies.contains(sender)){
            if(cancelled)QDBusConnection::sessionBus().send(calibrationReplies.value(sender).createErrorReply("org.gamegaraj.Slayer.Session1.Cancelled",tr("Kalibrasyon kapatıldı.")));
            calibrationReplies.remove(sender);
        }
        if(calibrations.isEmpty()){
            ++calibrationGeneration;calibrationPrepared=false;
            QDBusConnection::systemBus().asyncCall(QDBusMessage::createMethodCall(service,path,service,"FinishCalibration"));
            Reload();
        }
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
        powerDevil.setConnection(QDBusConnection::sessionBus());
        powerDevil.setWatchedServices({QStringLiteral("org.kde.Solid.PowerManagement")});
        powerDevil.setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
        connect(&powerDevil,&QDBusServiceWatcher::serviceOwnerChanged,this,[this](const QString&,const QString&,const QString& newOwner){
            ++keyboardPolicyGeneration;keyboardPolicyCookie=0;keyboardPolicyPending=false;keyboardPolicyOwner.clear();
            if(!newOwner.isEmpty())updateKeyboardPolicy(keyboardPolicyWanted);
        });
        exitTimer.setSingleShot(true);connect(&exitTimer,&QTimer::timeout,qApp,&QCoreApplication::quit);
        calibrationClients.setConnection(QDBusConnection::sessionBus());calibrationClients.setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
        connect(&calibrationClients,&QDBusServiceWatcher::serviceUnregistered,this,[this](const QString& sender){releaseCalibration(sender,false);});
        QDBusConnection::systemBus().connect(service,path,service,"SessionSettingsChanged",this,SLOT(Reload()));
        root.setConnection(QDBusConnection::systemBus());root.setWatchedServices({service});root.setWatchMode(QDBusServiceWatcher::WatchForRegistration);
        connect(&root,&QDBusServiceWatcher::serviceRegistered,this,[this]{Reload();});
        QTimer::singleShot(0,this,&Session::Reload);
    }
    ~Session() override {updateKeyboardPolicy(false);}
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
        // Subscribe before querying: a caller may vanish while its activation
        // request is queued. Do not retain an already dead unique bus name.
        const auto alive=QDBusConnection::sessionBus().interface()->isServiceRegistered(sender);
        if(!alive.isValid()||!alive.value()){releaseCalibration(sender,false);return false;}
        if(calibrationPrepared)return true;
        const auto request=message();setDelayedReply(true);calibrationReplies.insert(sender,request);
        if(calibrations.size()==1){
            armed=false;isIdle=false;
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
        if(reloading){reloadPending=true;return;}reloading=true;
        auto msg=QDBusMessage::createMethodCall(service,path,service,"ReadSessionSettings");
        auto* pending=new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(msg,20000),this);
        connect(pending,&QDBusPendingCallWatcher::finished,this,[this,pending]{
            QDBusPendingReply<QVariantMap> result=*pending;pending->deleteLater();reloading=false;
            if(reloadPending){reloadPending=false;QTimer::singleShot(0,this,&Session::Reload);}
            if(result.isError()){updateKeyboardPolicy(false);error=result.error().message();armed=false;isIdle=false;if(calibrations.isEmpty())exitTimer.start(5200);return;}
            const auto cfg=result.value();const bool wasEnabled=enabled;
            enabled=cfg.value("enabled").toBool();const unsigned timeout=cfg.value("timeout",30).toUInt();
            owner(cfg.value("lightingOwner").toString());
            armed=calibrations.isEmpty()&&cfg.value("armed").toBool();
            isIdle=armed&&cfg.value("idle").toBool();
            backend=cfg.value("backend").toString();
            error=cfg.value("idleError").toString();
            seconds=timeout;
            // This policy affects only automatic keyboard dimming. Hold it while
            // asleep or explicitly off too, so KDE cannot restore a stale level.
            updateKeyboardPolicy(enabled&&cfg.value("powerSaver").toBool()&&cfg.value("available").toBool());
            if(enabled||!calibrations.isEmpty()){
                exitTimer.stop();QDBusConnection::systemBus().asyncCall(QDBusMessage::createMethodCall(service,path,service,"Subscribe"));
            }else exitTimer.start(wasEnabled?5200:2500);
        });
    }
    QVariantMap Status(){return {{"enabled",enabled},{"armed",armed},{"idle",isIdle},{"timeout",seconds},{"calibrating",!calibrations.isEmpty()},{"backend",backend},{"keyboardIdleDelegated",keyboardPolicyCookie!=0},{"error",error.isEmpty()?keyboardPolicyError:error}};}
};
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);if(geteuid()==0)return 3;
    Session session;auto bus=QDBusConnection::sessionBus();
    if(!bus.registerService(sessionService))return 0;
    if(!bus.registerObject("/org/gamegaraj/Slayer/Session1",&session,QDBusConnection::ExportAllSlots))return 1;
    return app.exec();
}
#include "session_main.moc"
