// SPDX-License-Identifier: MIT
#include "command.hpp"
#include "../src/controls.hpp"
#include "../src/profile_sync.hpp"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusServiceWatcher>
#include <QSet>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QFileSystemWatcher>
#include <QProcess>
#include <QSocketNotifier>
#include <QTimer>
#include <QFile>
#include <QElapsedTimer>
#include <linux/netlink.h>
#include <sys/socket.h>
#include <unistd.h>

class Service final:public QObject,protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.gamegaraj.Slayer1")
    QTimer idle,debounce;
    QFileSystemWatcher files;
    QString cached;
    QSet<QString> subscribers;
    QDBusServiceWatcher clients;
    int socketFd=-1;
    bool authorizing=false;
    bool syncEnabled=false;
    QString syncOrigin,syncError,expectedDesktop,expectedHardware;
    void reloadSync(){
        auto cfg=ctlCommand({"config","show"});
        const bool enabled=cfg.ok&&cfg.output.split(u'\n').contains("performance_sync=on");
        const bool newlyEnabled=enabled&&!syncEnabled;
        syncEnabled=enabled;
        if(!enabled){syncError.clear();expectedDesktop.clear();expectedHardware.clear();syncOrigin.clear();}
        if(newlyEnabled)syncOrigin="hardware";
        touch();
    }
    void reconcile(){
        if(!syncEnabled||syncOrigin.isEmpty())return;
        const bool fromHardware=syncOrigin=="hardware";syncOrigin.clear();
        slayer::Io io({});
        if(fromHardware){
            try{expectedDesktop=QString::fromStdString(slayer::desktopForHardware(slayer::trim(io.read(slayer::hardwareProfilePath(io)))));}
            catch(const slayer::Error&){expectedDesktop.clear();}
        }
        auto result=ctlCommand({"profile","sync",fromHardware?"from-hardware":"from-desktop"},true);
        if(result.ok){
            syncError.clear();
            if(!fromHardware)expectedHardware=QString::fromStdString(slayer::trim(io.read(slayer::hardwareProfilePath(io))));
        }else{syncError=result.output.trimmed();expectedDesktop.clear();expectedHardware.clear();}
        cached.clear();
    }
    void hardwareChanged(){
        slayer::Io io({});
        const auto actual=QString::fromStdString(slayer::trim(io.read(slayer::hardwareProfilePath(io))));
        if(!expectedHardware.isEmpty()&&actual==expectedHardware)expectedHardware.clear();
        else {expectedHardware.clear();syncOrigin="hardware";}
        changed();
    }
    void touch(){if(subscribers.isEmpty()&&!authorizing&&!syncEnabled)idle.start(30000);else idle.stop();}
    QString state(){
        auto r=ctlCommand({"ui-data"});if(!r.ok)return {};
        auto config=ctlCommand({"config","show"});
        if(config.ok){for(const auto& line:config.output.split(u'\n')){auto at=line.indexOf(u'=');if(at>0)r.output+="value\tsaved_"+line.left(at)+"\t"+line.mid(at+1)+"\n";}}
        else r.output+="value\tconfig_error\tŞifreli ayarlar açılamadı. Yönetici doğrulamasıyla TPM kurtarma gerekir.\n";
        if(!syncError.isEmpty())r.output+="value\tperformance_sync_error\t"+QString::fromStdString(slayer::displayText(syncError.toStdString()))+"\n";
        cached=r.output;return cached;
    }
    void changed(){cached.clear();debounce.start(150);}
public:
    Service(){
        clients.setConnection(QDBusConnection::systemBus());clients.setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
        connect(&clients,&QDBusServiceWatcher::serviceUnregistered,this,[this](const QString& name){subscribers.remove(name);clients.removeWatchedService(name);touch();});
        idle.setSingleShot(true);connect(&idle,&QTimer::timeout,qApp,&QCoreApplication::quit);touch();
        debounce.setSingleShot(true);connect(&debounce,&QTimer::timeout,this,[this]{reconcile();Q_EMIT StateChanged();});
        for(const auto& p:{"/etc/slayerctl","/etc/slayerctl/rgb-profiles"})if(QFile::exists(p))files.addPath(p);
        connect(&files,&QFileSystemWatcher::directoryChanged,this,[this]{reloadSync();changed();});
        // Kernel uevents only. Accept no userspace-injected packets as hardware truth.
        socketFd=socket(AF_NETLINK,SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC,NETLINK_KOBJECT_UEVENT);
        sockaddr_nl address{};address.nl_family=AF_NETLINK;address.nl_groups=1;
        if(socketFd>=0&&bind(socketFd,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0){
            auto* notifier=new QSocketNotifier(socketFd,QSocketNotifier::Read,this);
            connect(notifier,&QSocketNotifier::activated,this,[this]{char buffer[8192];sockaddr_nl sender{};socklen_t len=sizeof(sender);ssize_t n;
                while((n=recvfrom(socketFd,buffer,sizeof(buffer),0,reinterpret_cast<sockaddr*>(&sender),&len))>0){
                    if(sender.nl_pid!=0)continue;
                    QByteArray data(buffer,static_cast<qsizetype>(n));
                    if(data.contains("SUBSYSTEM=platform-profile")||data.contains("/devices/platform/slayerctl")){hardwareChanged();continue;}
                    if(data.contains("SUBSYSTEM=power_supply")||data.contains("SUBSYSTEM=platform-profile")||data.contains("SUBSYSTEM=leds")||data.contains("/devices/platform/slayerctl"))changed();
                    else if(data.contains("SUBSYSTEM=hid")&&data.contains("HID_ID=0003:0000048D:0000600B"))changed();
                }
            });
        }
        QDBusConnection::systemBus().connect("org.freedesktop.UPower.PowerProfiles","/org/freedesktop/UPower/PowerProfiles","org.freedesktop.DBus.Properties","PropertiesChanged",this,SLOT(powerChanged(QString,QVariantMap,QStringList)));
        QDBusConnection::systemBus().connect("org.freedesktop.UPower","/org/freedesktop/UPower/KbdBacklight","org.freedesktop.UPower.KbdBacklight","BrightnessChanged",this,SLOT(keyboardBrightnessChanged(int)));
        QTimer::singleShot(0,this,[this]{reloadSync();reconcile();});
    }
    ~Service()override{if(socketFd>=0)close(socketFd);}
private Q_SLOTS:
    void keyboardBrightnessChanged(int){changed();}
    void powerChanged(const QString& interface,const QVariantMap& values,const QStringList& invalidated){
        if(interface!="org.freedesktop.UPower.PowerProfiles"||(!values.contains("ActiveProfile")&&!invalidated.contains("ActiveProfile")))return;
        const auto value=values.value("ActiveProfile").toString();
        if(!expectedDesktop.isEmpty()&&value==expectedDesktop)expectedDesktop.clear();
        else {expectedDesktop.clear();syncOrigin="desktop";}
        changed();
    }
public Q_SLOTS:
    void Subscribe(){if(!calledFromDBus()||subscribers.size()>=16)return;const auto sender=message().service();subscribers.insert(sender);clients.addWatchedService(sender);touch();}
    void Unsubscribe(){if(!calledFromDBus())return;const auto sender=message().service();subscribers.remove(sender);clients.removeWatchedService(sender);touch();}
    QString ReadState(){touch();return cached.isEmpty()?state():cached;}
    QString ReadTelemetry(){touch();return ctlCommand({"telemetry"}).output;}
    QString Diagnose(const QString& topic){
        touch();
        if(topic.startsWith("profile:"))return ctlCommand({"keyboard","profile","show",topic.mid(8)}).output;
        if(topic=="gpu-limits")return ctlCommand({"gpu","limits"}).output;
        if(topic=="cpu-topology")return ctlCommand({"cpu","topology"}).output;
        if(topic=="fan-curves")return ctlCommand({"fan","curve","list"}).output;
        if(!QStringList{"doctor","sensors","capabilities","info"}.contains(topic))return "Geçersiz tanılama konusu.";
        return ctlCommand({topic}).output;
    }
    QString Apply(const QStringList& command,const QStringList& remember){
        touch();if(!calledFromDBus())return {};
        if(!allowedCommand(command)||remember.size()>10||remember.size()%2){sendErrorReply(QDBusError::InvalidArgs,"İşlem reddedildi.");return {};}
        for(const auto& v:remember)if(v.size()>64||v.contains(QChar::Null)){sendErrorReply(QDBusError::InvalidArgs,"Ayar reddedildi.");return {};}
        if(authorizing){sendErrorReply("org.gamegaraj.Slayer1.Busy","Başka bir işlem doğrulanıyor.");return {};}
        auto request=message();setDelayedReply(true);authorizing=true;idle.stop();
        auto* check=new QProcess(this);
        auto* deadline=new QTimer(check);deadline->setSingleShot(true);connect(deadline,&QTimer::timeout,check,&QProcess::kill);deadline->start(60000);
        // Identity is the bus sender, never a client-supplied PID/user/group.
        connect(check,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,check,request,command,remember](int code,QProcess::ExitStatus status){
            authorizing=false;touch();check->deleteLater();
            if(code!=0||status!=QProcess::NormalExit){QDBusConnection::systemBus().send(request.createErrorReply("org.gamegaraj.Slayer1.NotAuthorized","Yönetici doğrulaması tamamlanmadı."));return;}
            if(!remember.isEmpty()){
                QStringList validate{"--dry-run","config","set"};validate.append(remember);
                auto validated=ctlCommand(validate,true);
                if(!validated.ok){QDBusConnection::systemBus().send(request.createErrorReply("org.gamegaraj.Slayer1.InvalidSetting",validated.output));return;}
            }
            // Preflight TPM before touching hardware; never fall back to plaintext.
            auto result=command==QStringList{"config","recover"}?CommandResult{true,{}}:ctlCommand({"config","protect"},true);
            if(result.ok)result=ctlCommand(command,true);
            // Preserve the last working lighting state without overwriting named profiles.
            QStringList savedSettings=remember;
            if(result.ok&&command.value(0)=="keyboard"&&QStringList{"color","key","hardware"}.contains(command.value(1))){
                QStringList snapshot{"keyboard","profile","snapshot","__slayer_current"};
                if(command.value(1)=="hardware"){
                    QString speed="5",brightness="50",direction="right";
                    for(int i=3;i+1<command.size();i+=2){if(command[i]=="--speed")speed=command[i+1];if(command[i]=="--brightness")brightness=command[i+1];if(command[i]=="--direction")direction=command[i+1];}
                    snapshot={"keyboard","profile","snapshot-effect","__slayer_current",command.value(2),speed,brightness,direction};
                }
                auto saved=ctlCommand(snapshot,true);
                if(!saved.ok)result={false,"Işık uygulandı fakat son durum hatırlanamadı: "+saved.output};
                else savedSettings.append({"rgb_profile","__slayer_current"});
            }
            if(result.ok&&!savedSettings.isEmpty()){QStringList save{"config","set"};save.append(savedSettings);auto saved=ctlCommand(save,true);if(!saved.ok)result={false,"Ayar uygulandı fakat hatırlanamadı: "+saved.output};}
            reloadSync();
            if(result.ok&&syncEnabled){
                if(command.value(0)=="power")syncOrigin="desktop";
                else if(command.value(0)=="profile")syncOrigin="hardware";
                reconcile();
                if(!syncError.isEmpty())result={false,"Ayar değişti fakat eşitleme tamamlanmadı: "+syncError};
            }
            cached.clear();Q_EMIT StateChanged();
            if(result.ok)QDBusConnection::systemBus().send(request.createReply(QVariantList{result.output}));
            else QDBusConnection::systemBus().send(request.createErrorReply("org.gamegaraj.Slayer1.Failed",result.output));
        });
        connect(check,&QProcess::errorOccurred,this,[this,check,request](QProcess::ProcessError error){
            if(error!=QProcess::FailedToStart)return;
            authorizing=false;touch();check->deleteLater();
            QDBusConnection::systemBus().send(request.createErrorReply("org.gamegaraj.Slayer1.AuthUnavailable","Kimlik doğrulama aracı başlatılamadı."));
        });
        const bool recover=command==QStringList{"config","recover"};
        check->start("/usr/bin/pkcheck",{"--action-id",recover?"org.gamegaraj.slayerctl.recover":"org.gamegaraj.slayerctl.control","--system-bus-name",request.service(),"--allow-user-interaction"});
        return {};
    }
Q_SIGNALS:
    void StateChanged();
};
int main(int argc,char** argv){QCoreApplication app(argc,argv);if(geteuid()!=0)return 3;Service service;auto bus=QDBusConnection::systemBus();if(!bus.registerObject("/org/gamegaraj/Slayer1",&service,QDBusConnection::ExportAllSlots|QDBusConnection::ExportAllSignals)||!bus.registerService("org.gamegaraj.Slayer1"))return 1;return app.exec();}
#include "main.moc"
