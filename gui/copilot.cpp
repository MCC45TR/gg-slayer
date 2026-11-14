// SPDX-License-Identifier: MIT
#include "copilot.hpp"
#include <KApplicationTrader>
#include <KConfigGroup>
#include <KDesktopFile>
#include <KService>
#include <KShell>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QKeySequence>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <algorithm>
#include <memory>

namespace SlayerShortcut {
QDBusArgument& operator<<(QDBusArgument& out,const Sequence& value){out.beginStructure();out<<value.keys;out.endStructure();return out;}
const QDBusArgument& operator>>(const QDBusArgument& in,Sequence& value){in.beginStructure();in>>value.keys;in.endStructure();return in;}
QString desktopPath(){return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/kglobalaccel/org.gamegaraj.slayerctl.copilot.desktop";}
}
namespace {
using SlayerShortcut::Sequence;
using SlayerShortcut::Sequences;
const QString component=QStringLiteral("org.gamegaraj.slayerctl.copilot.desktop");
QStringList actionId(const QString& action="_launch"){return {component,action,QStringLiteral("Slayer · Copilot"),QStringLiteral("Copilot tuşu")};}
QString entry(QString text){return text.replace('\\',"\\\\").replace('\n',"\\n").replace('\r',"\\r").replace('\t',"\\t");}
QString quotedArgument(QString text){
    text.replace('%',"%%");text.replace('\\',"\\\\");text.replace('"',"\\\"");text.replace('`',"\\`");text.replace('$',"\\$");
    return '"'+text+'"';
}
bool writeDesktop(const QByteArray& bytes){
    const auto path=SlayerShortcut::desktopPath();
    if(QFileInfo(path).isSymLink()||!QDir().mkpath(QFileInfo(path).path()))return false;
    QSaveFile file(path);file.setDirectWriteFallback(false);
    return file.open(QIODevice::WriteOnly)&&file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)&&file.write(bytes)==bytes.size()&&file.commit();
}
}
CopilotIntegration::CopilotIntegration(bool fixture,QObject* parent):QObject(parent),preview(fixture){
    qDBusRegisterMetaType<Sequence>();qDBusRegisterMetaType<Sequences>();
}
void CopilotIntegration::call(const QString& method,const QVariantList& args,std::function<void(QDBusMessage)> ready){
    auto request=QDBusMessage::createMethodCall("org.kde.kglobalaccel","/kglobalaccel","org.kde.KGlobalAccel",method);
    request.setArguments(args);request.setAutoStartService(false);
    auto* pending=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request,3000),this);
    connect(pending,&QDBusPendingCallWatcher::finished,this,[pending,ready=std::move(ready)]{const auto reply=pending->reply();pending->deleteLater();ready(reply);});
}
void CopilotIntegration::finish(const QString& error){working=false;problem=error;Q_EMIT changed();}
void CopilotIntegration::refresh(){
    if(working)return;
    saved={{"action","none"},{"target",""},{"shortcut","Meta+Shift+F23"},{"assigned",false},{"available",false}};
    if(preview){saved["available"]=true;Q_EMIT changed();return;}
    const auto path=SlayerShortcut::desktopPath();
    if(QFileInfo(path).isFile()&&!QFileInfo(path).isSymLink()&&QFileInfo(path).size()<16384){
        KDesktopFile file(path);const auto group=file.desktopGroup();
        saved["action"]=group.readEntry("X-Slayerctl-Action","none");saved["target"]=group.readEntry("X-Slayerctl-Target",QString{});
    }
    const auto bus=QDBusConnection::sessionBus();
    const auto present=bus.interface()->isServiceRegistered("org.kde.kglobalaccel");
    saved["available"]=present.isValid()&&present.value();
    if(!saved.value("available").toBool()){Q_EMIT changed();return;}
    working=true;Q_EMIT changed();
    call("shortcutKeys",{actionId()},[this](const QDBusMessage& message){
        QDBusReply<Sequences> reply(message);
        if(!reply.isValid()){finish(tr("Kısayol durumu okunamadı: %1").arg(reply.error().message()));return;}
        const auto keys=reply.value();
        if(!keys.isEmpty()&&!keys.first().keys.isEmpty()){
            saved["shortcut"]=QKeySequence(keys.first().keys.first()).toString(QKeySequence::PortableText);saved["assigned"]=true;
        }
        finish();
    });
}
void CopilotIntegration::loadApplications(){
    if(!apps.isEmpty())return;
    if(preview){apps={QVariantMap{{"id","org.kde.dolphin.desktop"},{"name",tr("Dosya yöneticisi")}}};Q_EMIT applicationsChanged();return;}
    const auto list=KApplicationTrader::query([](const KService::Ptr& service){return !service->noDisplay()&&!service->exec().isEmpty();});
    for(const auto& service:list)apps.append(QVariantMap{{"id",service->storageId()},{"name",service->name()}});
    std::sort(apps.begin(),apps.end(),[](const QVariant& a,const QVariant& b){return QString::localeAwareCompare(a.toMap().value("name").toString(),b.toMap().value("name").toString())<0;});
    Q_EMIT applicationsChanged();
}
void CopilotIntegration::save(const QString& action,const QString& target,const QKeySequence& sequence){
    if(working)return;
    if(preview){finish(tr("Önizlemede kısayol ataması kapalıdır."));return;}
    if(!saved.value("available").toBool()){finish(tr("KDE genel kısayol hizmeti kullanılamıyor."));return;}
    if(action!="none"&&(sequence.count()!=1||sequence[0].key()==Qt::Key_unknown||sequence[0].key()==0)){
        finish(tr("Copilot tuşuna basarak tek bir kısayol kaydedin."));return;
    }
    QString exec;
    if(action=="control")exec="/usr/bin/slayer-control-qt";
    else if(QStringList{"fan-boost","fan-auto","power-cycle","power-saver","balanced","performance"}.contains(action))
        exec="/usr/libexec/slayerctl-desktop --shortcut "+action;
    else if(action=="command"){
        if(target.size()>2048||target.contains('\n')||target.contains('\r')||target.contains(QChar::Null)){finish(tr("Tek satırlık bir komut girin."));return;}
        KShell::Errors error=KShell::NoError;
        const auto arguments=KShell::splitArgs(target,KShell::AbortOnMeta,&error);
        if(error!=KShell::NoError||arguments.isEmpty()){finish(tr("Geçerli bir program ve argümanlarını girin. Birden fazla işlem için kendi betiğinizi seçebilirsiniz."));return;}
        const auto program=QStandardPaths::findExecutable(arguments.first());
        if(program.isEmpty()){finish(tr("Komuttaki program bulunamadı."));return;}
        QStringList encoded{quotedArgument(program)};
        for(qsizetype i=1;i<arguments.size();++i)encoded.append(quotedArgument(arguments[i]));
        exec=encoded.join(' ');
    }
    else if(action=="application"){
        if(target.contains('/')||!target.endsWith(".desktop")){finish(tr("Kurulu bir uygulama seçin."));return;}
        auto service=KService::serviceByStorageId(target);
        if(!service){
            const auto path=QStandardPaths::locate(QStandardPaths::ApplicationsLocation,target);
            if(!path.isEmpty())service=KService::Ptr(new KService(path));
        }
        if(!service){finish(tr("Seçili uygulama artık kurulu değil."));return;}
        exec=service->exec();
        if(!service->isValid()||exec.isEmpty()){finish(tr("Uygulamanın başlatma komutu okunamadı."));return;}
    }else if(action=="website"){
        const QUrl url(target,QUrl::StrictMode);
        if(target.size()>2048||!url.isValid()||url.host().isEmpty()||(url.scheme()!="https"&&url.scheme()!="http")){
            finish(tr("http:// veya https:// ile başlayan geçerli bir web adresi girin."));return;
        }
        exec="/usr/bin/xdg-open "+quotedArgument(QString::fromUtf8(url.toEncoded()));
    }else if(action!="none"){finish(tr("Desteklenmeyen Copilot işlevi."));return;}
    const auto path=SlayerShortcut::desktopPath();QFile original(path);
    const bool existed=QFileInfo::exists(path);
    if(QFileInfo(path).isSymLink()||(existed&&(!original.open(QIODevice::ReadOnly)||original.size()>16384))){finish(tr("Mevcut kısayol dosyası güvenle okunamadı."));return;}
    const auto previous=existed?original.readAll():QByteArray{};original.close();
    const auto desired=action=="none"?Sequences{}:Sequences{Sequence{{sequence[0].toCombined()}}};
    const auto contents=QString("[Desktop Entry]\nType=Application\nName=Slayer · Copilot\nIcon=input-keyboard\nNoDisplay=true\nStartupNotify=false\nX-KDE-GlobalAccel-CommandShortcut=true\nExec=%1\nX-Slayerctl-Action=%2\nX-Slayerctl-Target=%3\n").arg(entry(exec),entry(action),entry(target)).toUtf8();
    working=true;problem.clear();Q_EMIT changed();
    call("shortcutKeys",{actionId()},[this,desired,contents,previous,existed,action,path](const QDBusMessage& snapshotReply){
        const QDBusReply<Sequences> old(snapshotReply);
        if(!old.isValid()){finish(tr("Mevcut kısayol okunamadı: %1").arg(old.error().message()));return;}
        auto commit=[this,desired,contents,previous,existed,action,path,oldKeys=old.value()]{
            if(action=="none"){
                call("unregister",{component,QStringLiteral("_launch")},[this,path](const QDBusMessage& reply){
                    if(reply.type()==QDBusMessage::ErrorMessage){finish(reply.errorMessage());return;}
                    if(QFileInfo::exists(path)&&!QFile::remove(path)){finish(tr("Atama kaldırıldı fakat kısayol dosyası silinemedi."));return;}
                    finish();refresh();
                });return;
            }
            if(!writeDesktop(contents)){finish(tr("Copilot kısayol dosyası kaydedilemedi."));return;}
            auto rollback=[this,previous,existed,path,oldKeys](const QString& reason){
                const bool restored=existed?writeDesktop(previous):(!QFileInfo::exists(path)||QFile::remove(path));
                call("setShortcutKeys",{actionId(),QVariant::fromValue(oldKeys),4u},[this,reason,restored,oldKeys](const QDBusMessage& message){
                    QDBusReply<Sequences> result(message);
                    finish(reason+((!restored||!result.isValid()||result.value()!=oldKeys)?tr(" Önceki atama tam geri yüklenemedi; KDE kısayollarını kontrol edin."):QString{}));
                });
            };
            call("doRegister",{actionId(QString{})},[this,desired,rollback](const QDBusMessage& registered){
                if(registered.type()==QDBusMessage::ErrorMessage){rollback(registered.errorMessage());return;}
                call("unregister",{component,QString{}},[this,desired,rollback](const QDBusMessage& cleaned){
                    if(cleaned.type()==QDBusMessage::ErrorMessage){rollback(cleaned.errorMessage());return;}
                    call("setShortcutKeys",{actionId(),QVariant::fromValue(desired),4u},[this,desired,rollback](const QDBusMessage& message){
                        QDBusReply<Sequences> assigned(message);
                        if(!assigned.isValid()||assigned.value()!=desired){rollback(tr("Kısayol atanamadı; mevcut başka bir atama değiştirilmedi."));return;}
                        finish();refresh();
                    });
                });
            });
        };
        if(desired.isEmpty()){commit();return;}
        call("globalShortcutAvailable",{QVariant::fromValue(desired.first()),component},[this,commit](const QDBusMessage& message){
            QDBusReply<bool> available(message);
            if(!available.isValid()){finish(available.error().message());return;}
            if(!available.value()){finish(tr("Bu tuş başka bir KDE kısayoluna atanmış. KDE kısayollarından çakışmayı giderin."));return;}
            commit();
        });
    });
}
