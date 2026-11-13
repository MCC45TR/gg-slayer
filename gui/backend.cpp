// SPDX-License-Identifier: MIT
#include "backend.hpp"
#include <QTimer>
#include <QHash>
#include <QStandardPaths>
#ifndef SLAYER_READ_TIMEOUT_MS
#define SLAYER_READ_TIMEOUT_MS 5000
#endif


SlayerBackend::SlayerBackend(QObject*parent,bool autoRead)
    : QObject(parent),m_keyboard(new DesktopKeyboard(this)),m_process(new QProcess(this)),m_readTimeout(new QTimer(this)) {
    m_readTimeout->setSingleShot(true);
#ifdef SLAYER_UI_FIXTURE
    m_preview=true;m_keyboard->loadNames("tr");
#endif
    connect(m_keyboard,&DesktopKeyboard::changed,this,[this]{if(!m_keys.isEmpty()){updateKeyboard();Q_EMIT dataChanged();}});
    connect(m_readTimeout,&QTimer::timeout,this,[this]{m_timedOut=true;m_process->kill();});
    connect(m_process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){
        if(e==QProcess::FailedToStart){m_readTimeout->stop();if(m_refresh){m_info.clear();m_caps.clear();m_keys.clear();m_profiles.clear();Q_EMIT dataChanged();}m_busy=false;m_message=QStringLiteral("slayerctl başlatılamadı. Kurulum yolunu kontrol edin.");Q_EMIT updated();}
    });
    connect(m_process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus state){
        m_readTimeout->stop();
        auto text=QString::fromUtf8(m_process->readAllStandardOutput());
        auto error=QString::fromUtf8(m_process->readAllStandardError());
        if(m_refresh&&code==0&&state==QProcess::NormalExit){
            acceptData(text);
        }else if(!m_refresh){
            m_output=text+error;
            m_message=code==0&&state==QProcess::NormalExit?QStringLiteral("İşlem tamamlandı."):(m_timedOut?QStringLiteral("Bilgi okuma zaman aşımına uğradı."):QStringLiteral("İşlem uygulanmadı: ")+error.trimmed());
        }else {m_info.clear();m_caps.clear();m_keys.clear();m_profiles.clear();Q_EMIT dataChanged();m_message=m_timedOut?QStringLiteral("Donanım bilgisi okuma zaman aşımına uğradı."):QStringLiteral("Donanım bilgisi okunamadı: ")+error.trimmed();}
        const bool needsRefresh=!m_refresh&&m_mutating;
        m_busy=false;Q_EMIT updated();
        if(needsRefresh)QTimer::singleShot(0,this,&SlayerBackend::refresh);
    });
    if(autoRead)QTimer::singleShot(0,this,&SlayerBackend::refresh);
}
void SlayerBackend::start(const QString&program,const QStringList&args,bool refresh){
    if(m_busy)return;
    m_busy=true;m_refresh=refresh;m_timedOut=false;Q_EMIT updated();
    if(refresh||!m_mutating)m_readTimeout->start(args.value(0)==u"power"?8000:SLAYER_READ_TIMEOUT_MS);
    m_process->setProgram(program);m_process->setArguments(args);m_process->start();
}
void SlayerBackend::openCalibration(){
    if(m_preview)return;
    const auto path=QStandardPaths::locate(QStandardPaths::GenericDataLocation,QStringLiteral("slayerctl/tools/white_balance.py"));
    if(path.isEmpty()||!QProcess::startDetached(QStringLiteral("/usr/bin/python3"),{path})){
        m_message=tr("Beyaz dengesi aracı başlatılamadı.");Q_EMIT updated();
    }
}
void SlayerBackend::openColorCalibration(){
    if(m_preview)return;
    const auto path=QStandardPaths::locate(QStandardPaths::GenericDataLocation,QStringLiteral("slayerctl/tools/color_calibration.py"));
    if(path.isEmpty()||!QProcess::startDetached(QStringLiteral("/usr/bin/python3"),{path})){
        m_message=tr("Ekran renk eşleştirme aracı başlatılamadı.");Q_EMIT updated();
    }
}
void SlayerBackend::openSettings(const QString&module){
    if(m_preview)return;
    if(module!=QStringLiteral("display")&&module!=QStringLiteral("touchpad")&&module!=QStringLiteral("power"))return;
    QProcess::startDetached(QStringLiteral(SLAYER_CLI),{QStringLiteral("desktop"),QStringLiteral("settings"),module});
}
void SlayerBackend::refresh(){if(m_busy)return;m_mutating=false;start(QStringLiteral(SLAYER_CLI),{QStringLiteral("ui-data")},true);}
void SlayerBackend::run(const QStringList&args,bool privileged){
    if(m_preview)return;
    if(m_busy||args.isEmpty()||args.size()>12)return;
    m_mutating=privileged||(args.size()>1&&args[0]==u"power"&&args[1]==u"set")||(args.size()==3&&args[0]==u"keyboard"&&args[1]==u"accent"&&args[2]!=u"status");
    // No shell interpolation; the privileged binary independently validates
    // the complete command and accepts no filesystem paths or test roots.
    if(privileged){QStringList a{QStringLiteral(SLAYER_HELPER)};a.append(args);start(QStringLiteral("/usr/bin/pkexec"),a,false);}
    else start(QStringLiteral(SLAYER_CLI),args,false);
}
bool SlayerBackend::ready(const QString&id)const{
    for(const auto&c:m_caps){auto map=c.toMap();if(map.value(QStringLiteral("id")).toString()==id)return map.value(QStringLiteral("state"))==QStringLiteral("ready");}return false;
}
QString SlayerBackend::reason(const QString&id)const{
    for(const auto&c:m_caps){auto map=c.toMap();if(map.value(QStringLiteral("id")).toString()==id)return map.value(QStringLiteral("reason")).toString();}return {};
}


void SlayerBackend::updateKeyboard(){
    for(auto& entry:m_keys)entry=m_keyboard->decorate(entry.toMap());
    m_info.insert(QStringLiteral("keyboard_physical_format"),m_keyboard->format());
    if(!m_keyboard->name().isEmpty()){
        m_info.insert(QStringLiteral("keyboard_layout"),m_keyboard->name());
        m_info.insert(QStringLiteral("keyboard_layout_source"),m_keyboard->origin());
    }
}

void SlayerBackend::acceptData(const QString& text){
            const auto oldInfo=m_info;const auto oldCaps=m_caps;const auto oldKeys=m_keys;const auto oldProfiles=m_profiles;
            m_info.clear();m_caps.clear();m_keys.clear();m_profiles.clear();
            QHash<int,QVariantList> colors;
            for(const auto&line:text.split(u'\n')){
                auto parts=line.split(u'\t');
                if(parts.size()==3&&parts[0]==u"value")m_info.insert(parts[1],parts[2]);
                else if(parts.size()==8&&parts[0]==u"key")m_keys.append(QVariantMap{{QStringLiteral("id"),parts[1]},{QStringLiteral("label"),parts[2]},{QStringLiteral("row"),parts[3].toInt()},{QStringLiteral("column"),parts[4].toInt()},{QStringLiteral("width"),parts[5].toInt()},{QStringLiteral("led"),parts[6].toInt()},{QStringLiteral("calibrated"),parts[7]==u"1"}});
                else if(parts.size()==5&&parts[0]==u"key-rgb"){
                    bool okay=false;const int index=parts[1].toInt(&okay);okay=okay&&index>=0&&index<=1024;QVariantList rgb;
                    for(int i=2;i<5;++i){bool valid=false;int value=parts[i].toInt(&valid);okay=okay&&valid&&value>=0&&value<=255;rgb.append(value);}
                    if(okay)colors.insert(index,rgb);
                }
                else if(parts.size()==2&&parts[0]==u"rgb-profile")m_profiles.append(parts[1]);
                else if(parts.size()==5&&parts[0]==u"cap")m_caps.append(QVariantMap{{QStringLiteral("id"),parts[1]},{QStringLiteral("state"),parts[2]},{QStringLiteral("name"),parts[3]},{QStringLiteral("reason"),parts[4]}});
            }
            for(auto& entry:m_keys){auto key=entry.toMap();const auto color=colors.constFind(key.value(QStringLiteral("led")).toInt());if(color!=colors.cend()){key.insert(QStringLiteral("rgb"),*color);entry=key;}}
            updateKeyboard();
            if(oldInfo!=m_info||oldCaps!=m_caps||oldKeys!=m_keys||oldProfiles!=m_profiles)Q_EMIT dataChanged();
}
