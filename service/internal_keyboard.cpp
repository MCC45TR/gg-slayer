// SPDX-License-Identifier: MIT
#include "internal_keyboard.hpp"
#include <QFile>
#include <QSocketNotifier>
#include <algorithm>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

namespace {
const QString base=QStringLiteral("/sys/devices/platform/slayerctl/");
qint64 bootMilliseconds(){timespec value{};if(clock_gettime(CLOCK_BOOTTIME,&value))return -1;return qint64(value.tv_sec)*1000+value.tv_nsec/1000000;}
}
InternalKeyboard::InternalKeyboard(QObject* parent):QObject(parent){
    deadline.setSingleShot(true);
    connect(&deadline,&QTimer::timeout,this,&InternalKeyboard::evaluate);
}
InternalKeyboard::~InternalKeyboard(){stop();}
bool InternalKeyboard::available()const{
    QFile present(base+"keyboard_activity_present");
    return present.open(QIODevice::ReadOnly)&&present.read(16).trimmed()=="1";
}
bool InternalKeyboard::write(const char* name,bool value){
    QFile file(base+QLatin1String(name));
    return file.open(QIODevice::WriteOnly)&&file.write(value?"1\n":"0\n",2)==2;
}
qint64 InternalKeyboard::lastActivity(){
    char buffer[32];const auto count=pread(fd,buffer,sizeof(buffer),0);
    if(count<=0)return -1;
    bool ok=false;const auto value=QByteArray(buffer,qsizetype(count)).trimmed().toLongLong(&ok);
    return ok?value:-1;
}
void InternalKeyboard::setIdle(bool value){
    if(dimmed==value)return;
    dimmed=value;Q_EMIT idleChanged(value);Q_EMIT statusChanged();
}
void InternalKeyboard::fail(const QString& message){stop();lastError=message;Q_EMIT statusChanged();}
void InternalKeyboard::stop(){
    deadline.stop();notifier.reset();
    if(running){write("keyboard_activity_arm",false);write("keyboard_activity_monitor",false);}
    if(fd>=0)close(fd);
    fd=-1;running=false;setIdle(false);
}
void InternalKeyboard::configure(bool enabled,unsigned timeout){
    if(!enabled&&!running&&seconds==timeout&&lastError.isEmpty())return;
    if(enabled&&running){
        if(seconds!=timeout){seconds=timeout;evaluate();Q_EMIT statusChanged();}
        return;
    }
    stop();seconds=timeout;lastError.clear();
    if(!enabled){Q_EMIT statusChanged();return;}
    if(!available()){lastError=tr("Yerleşik klavye etkinlik sürücüsü kullanılamıyor.");Q_EMIT statusChanged();return;}
    fd=open(QFile::encodeName(base+"keyboard_activity").constData(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0||lastActivity()<0||!write("keyboard_activity_monitor",true)){
        // The monitor may have been enabled even if a following operation fails.
        write("keyboard_activity_monitor",false);
        fail(tr("Yerleşik klavye etkinliği başlatılamadı."));return;
    }
    running=true;
    notifier=std::make_unique<QSocketNotifier>(fd,QSocketNotifier::Exception,this);
    connect(notifier.get(),&QSocketNotifier::activated,this,[this]{evaluate();});
    evaluate();Q_EMIT statusChanged();
}
void InternalKeyboard::evaluate(){
    if(!running)return;
    if(!available()){fail(tr("Yerleşik klavye bağlantısı kesildi."));return;}
    auto last=lastActivity();auto now=bootMilliseconds();
    if(last<0||now<last){fail(tr("Yerleşik klavye etkinliği okunamadı."));return;}
    const qint64 timeout=qint64(seconds)*1000;
    if(now-last>=timeout){
        // Arm before the second read: a press between the deadline check and
        // arming must either be seen here or wake poll(PRI), never get lost.
        if(!write("keyboard_activity_arm",true)){fail(tr("Yerleşik klavye bildirimi kurulamadı."));return;}
        last=lastActivity();now=bootMilliseconds();
        if(last<0||now<last){fail(tr("Yerleşik klavye etkinliği okunamadı."));return;}
        if(now-last>=timeout){deadline.stop();setIdle(true);return;}
    }
    if(!write("keyboard_activity_arm",false)){fail(tr("Yerleşik klavye bildirimi temizlenemedi."));return;}
    setIdle(false);
    deadline.start(int(std::clamp<qint64>(timeout-(now-last),1,1800000)));
}
