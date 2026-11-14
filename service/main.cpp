// SPDX-License-Identifier: MIT
#include "command.hpp"
#include "command_queue.hpp"
#include "idle_dimmer.hpp"
#include "internal_keyboard.hpp"
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
#include <QDBusInterface>
#include <QDBusArgument>
#include <QDBusReply>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFileSystemWatcher>
#include <QProcess>
#include <QSocketNotifier>
#include <QTimer>
#include <QFile>
#include <QElapsedTimer>
#include <linux/netlink.h>
#include <sys/socket.h>
#include <unistd.h>
#include <systemd/sd-login.h>
#include <memory>

class Service final:public QObject,protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.gamegaraj.Slayer1")
    CommandQueue commands;
    QTimer idle,debounce,settingsDebounce;
    QFileSystemWatcher files;
    QString cached;
    quint64 stateVersion=0;
    void invalidateState(){++stateVersion;cached.clear();}
    QSet<QString> subscribers;
    QDBusServiceWatcher clients;
    int socketFd=-1;
    bool authorizing=false;
    bool settingsLoading=false,settingsPending=false,syncRunning=false;
    bool powerLoading=false,powerPending=false;
    quint64 powerVersion=0;
    bool syncEnabled=false;
    IdleDimmer dimmer;
    InternalKeyboard keyboardActivity;
    QString idleOwner,desktopProfile;
    QStringList desktopChoices;
    bool powerAvailable=false,idleEnabled=false,asleep=false;
    QString settingsError;
    unsigned idleTimeout=30;
    std::map<std::string,std::string> settings;
    struct SettingsResult {std::map<std::string,std::string> values;QString error;};
    static SettingsResult loadSettings(){
        try{return {slayer::configurationValues(slayer::Io({})),{}};}
        catch(const std::exception& e){return {{},QString::fromStdString(slayer::displayText(e.what()))};}
    }
    bool background(std::function<CommandResult()> work,std::function<void(CommandResult)> done){
        const bool queued=commands.run(std::move(work),[this,done=std::move(done)](CommandResult result){done(std::move(result));touch();if(settingsPending&&!settingsLoading){settingsPending=false;reloadSync();}reconcile();});
        if(queued)idle.stop();
        return queued;
    }
    void finishAuthorization(){
        authorizing=false;touch();
        if(settingsPending&&!settingsLoading){settingsPending=false;reloadSync();}
        reconcile();
    }
    static constexpr auto powerService="org.freedesktop.UPower.PowerProfiles";
    void readPower(){
        if(powerLoading){powerPending=true;return;}
        powerLoading=true;
        const auto generation=powerVersion;
        auto request=QDBusMessage::createMethodCall(powerService,"/org/freedesktop/UPower/PowerProfiles","org.freedesktop.DBus.Properties","GetAll");
        request<<QString::fromLatin1(powerService);
        auto* pending=new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(request,2000),this);
        connect(pending,&QDBusPendingCallWatcher::finished,this,[this,pending,generation]{
            QDBusPendingReply<QVariantMap> result=*pending;pending->deleteLater();powerLoading=false;
            if(generation!=powerVersion){if(powerPending){powerPending=false;readPower();}return;}
            powerAvailable=!result.isError();desktopChoices.clear();desktopProfile.clear();
            if(powerAvailable){
                desktopProfile=result.value().value("ActiveProfile").toString();
                const auto array=qvariant_cast<QDBusArgument>(result.value().value("Profiles"));
                array.beginArray();while(!array.atEnd()){QVariantMap profile;array>>profile;desktopChoices.append(profile.value("Profile").toString());}array.endArray();
            }
            refreshActivity();
            if(desktopProfile=="power-saver"&&idleEnabled)activateSession();
            Q_EMIT SessionSettingsChanged();changed();
            if(powerPending){powerPending=false;readPower();}
        });
    }
    bool activeUser(const QString& sender){
        const auto uid=QDBusConnection::systemBus().interface()->serviceUid(sender);
        uid_t active=0;char* session=nullptr;
        const bool match=uid.isValid()&&sd_seat_get_active("seat0",&session,&active)>=0&&active!=0&&active==uid.value();
        free(session);return match;
    }
    void refreshActivity(){
        uid_t uid=0;char* session=nullptr;
        const bool active=sd_seat_get_active("seat0",&session,&uid)>=0&&uid!=0;
        free(session);
        QFile brightness("/sys/class/leds/rgb:kbd_backlight/brightness");
        const bool lit=brightness.open(QIODevice::ReadOnly)&&brightness.read(16).trimmed().toUInt()>0;
        keyboardActivity.configure(idleEnabled&&!asleep&&desktopProfile=="power-saver"&&
            idleOwner.isEmpty()&&active&&lit&&dimmer.available(),idleTimeout);
        if(!lit)dimmer.reset();
        touch();
    }
    void releaseIdle(){
        if(!idleOwner.isEmpty()&&!subscribers.contains(idleOwner))clients.removeWatchedService(idleOwner);
        idleOwner.clear();
        if(asleep)dimmer.reset();else dimmer.fade(false);
        refreshActivity();
    }
    void activateSession(){
        const auto owner=settings.find("lighting_owner");
        const bool palette=owner!=settings.end()&&(owner->second=="accent"||owner->second=="custom");
        if(palette||(idleEnabled&&desktopProfile=="power-saver"))QProcess::startDetached("/usr/libexec/slayerctl-desktop",{"--reload-session"});
    }
    QString syncOrigin,syncError,expectedDesktop,expectedHardware;
    void useSettings(SettingsResult loaded){
        const auto before=settings.find("lighting_owner");
        const auto after=loaded.values.find("lighting_owner");
        const bool ownerChanged=(before==settings.end())!=(after==loaded.values.end())||
            (before!=settings.end()&&after!=loaded.values.end()&&before->second!=after->second);
        const bool wasIdleEnabled=idleEnabled;
        settings=std::move(loaded.values);settingsError=std::move(loaded.error);invalidateState();
        const bool enabled=settings.contains("performance_sync")&&settings.at("performance_sync")=="on";
        idleEnabled=settings.contains("keyboard_idle_dim")&&settings.at("keyboard_idle_dim")=="on";
        idleTimeout=settings.contains("keyboard_idle_timeout")?unsigned(std::stoi(settings.at("keyboard_idle_timeout"))):30u;
        if(!idleEnabled)dimmer.fade(false);
        refreshActivity();
        const bool newlyEnabled=enabled&&!syncEnabled;
        syncEnabled=enabled;
        if(!enabled){syncError.clear();expectedDesktop.clear();expectedHardware.clear();syncOrigin.clear();}
        if(newlyEnabled)syncOrigin="hardware";
        touch();
        Q_EMIT SessionSettingsChanged();
        if(ownerChanged||(!wasIdleEnabled&&idleEnabled))activateSession();
    }
    void reloadSync(){
        if(settingsLoading){settingsPending=true;return;}
        settingsLoading=true;
        auto loaded=std::make_shared<SettingsResult>();
        if(!background([loaded]{*loaded=loadSettings();return CommandResult{true,{}};},[this,loaded](CommandResult){
            settingsLoading=false;useSettings(std::move(*loaded));reconcile();
            if(settingsPending){settingsPending=false;reloadSync();}
        })) {settingsLoading=false;settingsPending=true;}
    }
    void reconcile(){
        if(!syncEnabled||syncOrigin.isEmpty()||syncRunning||authorizing)return;
        const bool fromHardware=syncOrigin=="hardware";syncOrigin.clear();
        slayer::Io io({});
        if(fromHardware){
            try{expectedDesktop=QString::fromStdString(slayer::desktopForHardware(slayer::trim(io.read(slayer::hardwareProfilePath(io)))));}
            catch(const slayer::Error&){expectedDesktop.clear();}
        }
        syncRunning=true;
        if(!background([fromHardware]{return ctlCommand({"profile","sync",fromHardware?"from-hardware":"from-desktop"},true);},[this,fromHardware](CommandResult result){
            syncRunning=false;
            if(result.ok){
                syncError.clear();
                if(!fromHardware){slayer::Io live({});expectedHardware=QString::fromStdString(slayer::trim(live.read(slayer::hardwareProfilePath(live))));}
            }else{syncError=result.output.trimmed();expectedDesktop.clear();expectedHardware.clear();}
            invalidateState();Q_EMIT StateChanged();reconcile();
        })){syncRunning=false;syncOrigin=fromHardware?"hardware":"desktop";}
    }
    void hardwareChanged(){
        slayer::Io io({});
        const auto actual=QString::fromStdString(slayer::trim(io.read(slayer::hardwareProfilePath(io))));
        if(!expectedHardware.isEmpty()&&actual==expectedHardware)expectedHardware.clear();
        else {expectedHardware.clear();syncOrigin="hardware";}
        changed();
    }
    void touch(){if(subscribers.isEmpty()&&!authorizing&&!commands.pending()&&!syncEnabled&&!idleEnabled&&idleOwner.isEmpty())idle.start(5000);else idle.stop();}
    QString state(CommandResult r){
        if(!r.ok)return {};
        for(const auto& [key,value]:settings)r.output+="value\tsaved_"+QString::fromStdString(key)+"\t"+QString::fromStdString(slayer::displayText(value))+"\n";
        if(!settingsError.isEmpty())r.output+="value\tconfig_error\t"+settingsError+"\n";
        r.output+="value\tdesktop_profile\t"+desktopProfile+"\nvalue\tdesktop_choices\t"+desktopChoices.join(u' ')+"\nvalue\tdesktop_available\t"+(powerAvailable?"1":"0")+"\n";
        r.output+="value\tkeyboard_idle_available\t"+(dimmer.available()&&keyboardActivity.available()?QString("1"):QString("0"))+"\n";
        if(!keyboardActivity.error().isEmpty())r.output+="value\tkeyboard_idle_error\t"+keyboardActivity.error()+"\n";
        if(!syncError.isEmpty())r.output+="value\tperformance_sync_error\t"+QString::fromStdString(slayer::displayText(syncError.toStdString()))+"\n";
        return r.output;
    }
    void changed(){invalidateState();debounce.start(150);}
public:
    Service(){
        connect(&keyboardActivity,&InternalKeyboard::idleChanged,this,[this](bool value){dimmer.fade(value);});
        connect(&keyboardActivity,&InternalKeyboard::statusChanged,this,[this]{invalidateState();Q_EMIT SessionSettingsChanged();});
        clients.setConnection(QDBusConnection::systemBus());clients.setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
        connect(&clients,&QDBusServiceWatcher::serviceUnregistered,this,[this](const QString& name){if(name==idleOwner)releaseIdle();subscribers.remove(name);clients.removeWatchedService(name);touch();});
        idle.setSingleShot(true);connect(&idle,&QTimer::timeout,qApp,&QCoreApplication::quit);touch();
        debounce.setSingleShot(true);connect(&debounce,&QTimer::timeout,this,[this]{reconcile();Q_EMIT StateChanged();});
        for(const auto& p:{"/etc/slayerctl","/etc/slayerctl/rgb-profiles"})if(QFile::exists(p))files.addPath(p);
        settingsDebounce.setSingleShot(true);settingsDebounce.setInterval(150);
        connect(&settingsDebounce,&QTimer::timeout,this,[this]{
            if(authorizing){settingsPending=true;return;}
            reloadSync();changed();
        });
        connect(&files,&QFileSystemWatcher::directoryChanged,this,[this]{settingsDebounce.start();});
        // Kernel uevents only. Accept no userspace-injected packets as hardware truth.
        socketFd=socket(AF_NETLINK,SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC,NETLINK_KOBJECT_UEVENT);
        sockaddr_nl address{};address.nl_family=AF_NETLINK;address.nl_groups=1;
        if(socketFd>=0&&bind(socketFd,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0){
            auto* notifier=new QSocketNotifier(socketFd,QSocketNotifier::Read,this);
            connect(notifier,&QSocketNotifier::activated,this,[this]{char buffer[8192];sockaddr_nl sender{};socklen_t len=sizeof(sender);ssize_t n;
                while((n=recvfrom(socketFd,buffer,sizeof(buffer),0,reinterpret_cast<sockaddr*>(&sender),&len))>0){
                    if(sender.nl_pid!=0)continue;
                    QByteArray data(buffer,static_cast<qsizetype>(n));
                    if(data.contains("SUBSYSTEM=platform-profile")||data.contains("/devices/platform/slayerctl")){refreshActivity();hardwareChanged();continue;}
                    if(data.contains("SUBSYSTEM=power_supply")||data.contains("SUBSYSTEM=platform-profile")||data.contains("SUBSYSTEM=leds")||data.contains("/devices/platform/slayerctl"))changed();
                    else if(data.contains("SUBSYSTEM=hid")&&data.contains("HID_ID=0003:0000048D:0000600B"))changed();
                }
            });
        }
        QDBusConnection::systemBus().connect("org.freedesktop.UPower.PowerProfiles","/org/freedesktop/UPower/PowerProfiles","org.freedesktop.DBus.Properties","PropertiesChanged",this,SLOT(powerChanged(QString,QVariantMap,QStringList)));
        QDBusConnection::systemBus().connect("org.freedesktop.UPower","/org/freedesktop/UPower/KbdBacklight","org.freedesktop.UPower.KbdBacklight","BrightnessChanged",this,SLOT(keyboardBrightnessChanged(int)));
        QDBusConnection::systemBus().connect("org.freedesktop.login1","/org/freedesktop/login1/seat/seat0","org.freedesktop.DBus.Properties","PropertiesChanged",this,SLOT(seatChanged(QString,QVariantMap,QStringList)));
        QDBusConnection::systemBus().connect("org.freedesktop.login1","/org/freedesktop/login1","org.freedesktop.login1.Manager","PrepareForSleep",this,SLOT(prepareForSleep(bool)));
        QTimer::singleShot(0,this,[this]{readPower();reloadSync();});
    }
    ~Service()override{commands.stop();keyboardActivity.stop();dimmer.reset();if(socketFd>=0)close(socketFd);}
private Q_SLOTS:
    void seatChanged(const QString&,const QVariantMap& values,const QStringList&){if(values.contains("ActiveSession")){if(!idleOwner.isEmpty()&&!activeUser(idleOwner))releaseIdle();else refreshActivity();}}
    void prepareForSleep(bool sleeping){asleep=sleeping;if(sleeping)releaseIdle();else refreshActivity();Q_EMIT SessionSettingsChanged();}
    void keyboardBrightnessChanged(int){refreshActivity();changed();}
    void powerChanged(const QString& interface,const QVariantMap& values,const QStringList& invalidated){
        if(interface!="org.freedesktop.UPower.PowerProfiles"||(!values.contains("ActiveProfile")&&!invalidated.contains("ActiveProfile")))return;
        ++powerVersion;
        const auto value=values.value("ActiveProfile").toString();
        desktopProfile=value;powerAvailable=true;
        if(value.isEmpty()||invalidated.contains("ActiveProfile"))readPower();
        if(desktopProfile!="power-saver")dimmer.fade(false);
        refreshActivity();
        if(desktopProfile=="power-saver"&&idleEnabled)activateSession();
        Q_EMIT SessionSettingsChanged();
        if(!expectedDesktop.isEmpty()&&value==expectedDesktop)expectedDesktop.clear();
        else {expectedDesktop.clear();syncOrigin="desktop";}
        changed();
    }
public Q_SLOTS:
    QVariantMap ReadSessionSettings(){
        touch();
        return {{"loading",settingsLoading},{"enabled",idleEnabled},{"timeout",idleTimeout},{"powerSaver",desktopProfile=="power-saver"},{"sleeping",asleep},
            {"available",dimmer.available()&&keyboardActivity.available()},{"armed",keyboardActivity.active()},
            {"idle",keyboardActivity.idle()},{"backend",QStringLiteral("internal-keyboard")},{"idleError",keyboardActivity.error()},
            {"lightingOwner",QString::fromStdString(settings.contains("lighting_owner")?settings.at("lighting_owner"):"")}};
    }
    bool PrepareCalibration(){
        if(!calledFromDBus())return false;
        const auto sender=message().service();
        if(!idleOwner.isEmpty()&&idleOwner!=sender)return false;
        clients.addWatchedService(sender);
        if(!activeUser(sender)){
            if(!subscribers.contains(sender)&&sender!=idleOwner)clients.removeWatchedService(sender);
            return false;
        }
        // Older drivers have no transient scale and need no fade barrier.
        if(!dimmer.available()){if(!subscribers.contains(sender))clients.removeWatchedService(sender);return true;}
        idleOwner=sender;clients.addWatchedService(sender);
        keyboardActivity.configure(false,idleTimeout);
        const auto request=message();setDelayedReply(true);
        dimmer.restore([this,request](bool ok){
            QDBusConnection::systemBus().send(request.createReply(QVariantList{ok&&activeUser(request.service())}));
        });
        return false;
    }
    bool FinishCalibration(){
        if(!calledFromDBus()||message().service()!=idleOwner)return false;
        releaseIdle();Q_EMIT SessionSettingsChanged();return true;
    }
    void Subscribe(){
        if(!calledFromDBus()||subscribers.size()>=16)return;
        const auto sender=message().service();clients.addWatchedService(sender);
        const auto alive=QDBusConnection::systemBus().interface()->isServiceRegistered(sender);
        if(!alive.isValid()||!alive.value()){if(sender!=idleOwner)clients.removeWatchedService(sender);return;}
        subscribers.insert(sender);touch();
    }
    void Unsubscribe(){if(!calledFromDBus())return;const auto sender=message().service();subscribers.remove(sender);if(sender!=idleOwner)clients.removeWatchedService(sender);touch();}
    QString ReadState(){
        touch();if(!cached.isEmpty())return cached;
        if(!calledFromDBus())return {};
        const auto request=message();const auto generation=stateVersion;setDelayedReply(true);
        if(!background([]{return ctlCommand({"ui-data"});},[this,request,generation](CommandResult result){
            if(!result.ok){QDBusConnection::systemBus().send(request.createErrorReply("org.gamegaraj.Slayer1.Failed",result.output));return;}
            // Events may arrive while the worker reads sysfs; do not cache a
            // result that could precede an already processed hardware event.
            const auto output=state(std::move(result));
            if(generation==stateVersion)cached=output;
            QDBusConnection::systemBus().send(request.createReply(QVariantList{output}));
        }))QDBusConnection::systemBus().send(request.createErrorReply("org.gamegaraj.Slayer1.Busy","İşlem kuyruğu dolu."));
        return {};
    }
    QString ReadTelemetry(){
        touch();if(!calledFromDBus())return {};
        const auto request=message();setDelayedReply(true);
        if(!background([]{return ctlCommand({"telemetry"});},[request](CommandResult result){
            QDBusConnection::systemBus().send(result.ok?request.createReply(QVariantList{result.output}):request.createErrorReply("org.gamegaraj.Slayer1.Failed",result.output));
        }))
            QDBusConnection::systemBus().send(request.createErrorReply("org.gamegaraj.Slayer1.Busy","İşlem kuyruğu dolu."));
        return {};
    }
    QString Diagnose(const QString& topic){
        touch();
        if(!calledFromDBus()||topic.size()>64)return "Geçersiz tanılama konusu.";
        QStringList args;
        if(topic.startsWith("profile:"))args={"keyboard","profile","show",topic.mid(8)};
        else if(topic=="gpu-limits")args={"gpu","limits"};
        else if(topic=="cpu-topology")args={"cpu","topology"};
        else if(topic=="fan-curves")args={"fan","curve","list"};
        else if(QStringList{"doctor","sensors","capabilities","info"}.contains(topic))args={topic};
        else return "Geçersiz tanılama konusu.";
        const auto request=message();setDelayedReply(true);
        if(!background([args]{return ctlCommand(args);},[request](CommandResult result){QDBusConnection::systemBus().send(request.createReply(QVariantList{result.output}));}))
            QDBusConnection::systemBus().send(request.createErrorReply("org.gamegaraj.Slayer1.Busy","İşlem kuyruğu dolu."));
        return {};
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
            check->deleteLater();
            if(code!=0||status!=QProcess::NormalExit){finishAuthorization();QDBusConnection::systemBus().send(request.createErrorReply("org.gamegaraj.Slayer1.NotAuthorized","Yönetici doğrulaması tamamlanmadı."));return;}
            auto loaded=std::make_shared<SettingsResult>();
            if(!background([command,remember,loaded]{
                auto result=applyCommand(command,remember);
                *loaded=loadSettings();
                const auto& config=loaded->values;
                if(result.ok&&config.contains("performance_sync")&&config.at("performance_sync")=="on"&&
                   (command.value(0)=="power"||command.value(0)=="profile")){
                    auto synced=ctlCommand({"profile","sync",command.value(0)=="power"?"from-desktop":"from-hardware"},true);
                    if(!synced.ok)result={false,"Ayar değişti fakat eşitleme tamamlanmadı: "+synced.output};
                }
                return result;
            },[this,request,command,loaded](CommandResult result){
                authorizing=false;settingsPending=false;useSettings(std::move(*loaded));
                reconcile();
                invalidateState();Q_EMIT StateChanged();
                if(result.ok)QDBusConnection::systemBus().send(request.createReply(QVariantList{result.output}));
                else QDBusConnection::systemBus().send(request.createErrorReply("org.gamegaraj.Slayer1.Failed",result.output));
            })){
                finishAuthorization();
                QDBusConnection::systemBus().send(request.createErrorReply("org.gamegaraj.Slayer1.Busy","İşlem kuyruğu dolu."));
            }
        });
        connect(check,&QProcess::errorOccurred,this,[this,check,request](QProcess::ProcessError error){
            if(error!=QProcess::FailedToStart)return;
            finishAuthorization();check->deleteLater();
            QDBusConnection::systemBus().send(request.createErrorReply("org.gamegaraj.Slayer1.AuthUnavailable","Kimlik doğrulama aracı başlatılamadı."));
        });
        const bool recover=command==QStringList{"config","recover"};
        check->start("/usr/bin/pkcheck",{"--action-id",recover?"org.gamegaraj.slayerctl.recover":"org.gamegaraj.slayerctl.control","--system-bus-name",request.service(),"--allow-user-interaction"});
        return {};
    }
Q_SIGNALS:
    void StateChanged();
    void SessionSettingsChanged();
};
int main(int argc,char** argv){QCoreApplication app(argc,argv);if(geteuid()!=0)return 3;Service service;auto bus=QDBusConnection::systemBus();if(!bus.registerObject("/org/gamegaraj/Slayer1",&service,QDBusConnection::ExportAllSlots|QDBusConnection::ExportAllSignals)||!bus.registerService("org.gamegaraj.Slayer1"))return 1;return app.exec();}
#include "main.moc"
