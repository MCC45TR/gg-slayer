// SPDX-License-Identifier: MIT
#include "camera_control.hpp"
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QRegularExpression>
#include <QSocketNotifier>
#include <systemd/sd-device.h>
#include <systemd/sd-event.h>
#include <utility>

#ifndef SLAYER_CLI
#define SLAYER_CLI "/usr/bin/slayerctl"
#endif

CameraControl::CameraControl(QObject* parent)
    :CameraControl(QStringLiteral(SLAYER_CLI),QDBusConnection::systemBus(),true,parent){}

CameraControl::CameraControl(QString program,QDBusConnection bus,bool monitor,QObject* parent)
    :QObject(parent),m_program(std::move(program)),m_bus(std::move(bus)){
    m_deadline.setSingleShot(true);
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_debounce,&QTimer::timeout,this,&CameraControl::read);
    connect(&m_deadline,&QTimer::timeout,this,[this]{
        m_error=tr("Kamera durumu okunurken zaman aşımı oluştu.");
        m_reader.kill();
    });
    connect(&m_reader,&QProcess::readyReadStandardOutput,this,[this]{
        m_output+=m_reader.readAllStandardOutput();
        if(m_output.size()>4096){m_error=tr("Kamera durumu yanıtı geçersiz.");m_reader.kill();}
    });
    connect(&m_reader,&QProcess::readyReadStandardError,this,[this]{
        // Do not retain unbounded helper diagnostics in the shell process.
        m_reader.readAllStandardError();
    });
    connect(&m_reader,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart){m_error=tr("slayerctl başlatılamadı. Kurulumu kontrol edin.");finishRead(false);}
    });
    connect(&m_reader,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){
        finishRead(code==0&&status==QProcess::NormalExit&&m_deadline.isActive());
    });
    if(monitor)m_monitoring=startMonitor();
    QTimer::singleShot(0,this,&CameraControl::read);
}

CameraControl::~CameraControl(){
    delete m_notifier;
    if(m_monitor){sd_device_monitor_stop(m_monitor);sd_device_monitor_unref(m_monitor);}
    sd_event_unref(m_event);
    // Never leave a reader child owned by a removed widget/plugin.
    if(m_reader.state()!=QProcess::NotRunning){m_reader.kill();m_reader.waitForFinished(1000);}
}

bool CameraControl::startMonitor(){
    if(sd_event_new(&m_event)<0||sd_device_monitor_new(&m_monitor)<0||
       sd_device_monitor_filter_add_match_subsystem_devtype(m_monitor,"usb",nullptr)<0||
       sd_device_monitor_attach_event(m_monitor,m_event)<0||
       sd_device_monitor_start(m_monitor,[](sd_device_monitor*,sd_device*,void* data){
           static_cast<CameraControl*>(data)->scheduleRead();return 0;
       },this)<0)return false;
    const int fd=sd_event_get_fd(m_event);
    if(fd<0){sd_device_monitor_stop(m_monitor);return false;}
    m_notifier=new QSocketNotifier(fd,QSocketNotifier::Read,this);
    connect(m_notifier,&QSocketNotifier::activated,this,[this]{
        // Bound a burst of USB events so the shell remains responsive.
        for(int i=0;i<32;++i){
            const int result=sd_event_run(m_event,0);
            if(result<0){m_notifier->setEnabled(false);sd_device_monitor_stop(m_monitor);m_monitoring=false;Q_EMIT changed();}
            if(result<=0)break;
        }
    });
    return true;
}

void CameraControl::scheduleRead(){
    if(busy()){m_pending=true;return;}
    m_debounce.start();
}

void CameraControl::refresh(){
    m_error.clear();
    scheduleRead();
    Q_EMIT changed();
}

void CameraControl::read(){
    if(busy()){m_pending=true;return;}
    m_pending=false;m_reading=true;m_output.clear();
    Q_EMIT changed();
    m_deadline.start(2000);
    // Read only the validated internal-camera identity, without waking ctl's
    // system broker or reading telemetry/keyboard/configuration.
    m_reader.start(m_program,{QStringLiteral("settings"),QStringLiteral("camera"),QStringLiteral("status")});
}

void CameraControl::finishRead(bool ok){
    if(!m_reading)return;
    m_deadline.stop();m_reading=false;
    static const QRegularExpression expected(QStringLiteral("\\Acamera: ([01]) \\((on|off)\\)\\s*\\z"));
    const auto match=expected.match(QString::fromUtf8(m_output));
    const bool consistent=match.hasMatch()&&((match.captured(1)==u"1")== (match.captured(2)==u"on"));
    m_state=ok&&m_output.size()<=4096&&consistent?match.captured(1).toInt():-1;
    if(m_state<0&&m_error.isEmpty())m_error=tr("Desteklenen dahili kamera bulunamadı veya durumu okunamadı.");
    Q_EMIT changed();
    if(m_pending)scheduleRead();
}

void CameraControl::setEnabled(bool enabled){
    if(busy()||!available()||enabled==(m_state==1))return;
    m_debounce.stop();m_error.clear();m_applying=true;
    Q_EMIT changed();
    const QString value=enabled?QStringLiteral("on"):QStringLiteral("off");
    auto request=QDBusMessage::createMethodCall(QStringLiteral("org.gamegaraj.Slayer1"),QStringLiteral("/org/gamegaraj/Slayer1"),QStringLiteral("org.gamegaraj.Slayer1"),QStringLiteral("Apply"));
    request.setArguments({QStringList{QStringLiteral("settings"),QStringLiteral("camera"),value},QStringList{QStringLiteral("camera"),value}});
    auto* watcher=new QDBusPendingCallWatcher(m_bus.asyncCall(request,90000),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher]{
        const QDBusPendingReply<QString> reply=*watcher;
        watcher->deleteLater();m_applying=false;
        if(reply.isError())m_error=tr("Kamera ayarı uygulanamadı: %1").arg(reply.error().message());
        // The icon follows actual readback, including rejected/partial writes.
        // Never infer device state from the requested value or a saved setting.
        read();
    });
}
