// SPDX-License-Identifier: MIT
#include "idle_dimmer.hpp"
#include "../src/idle_curve.hpp"
#include <QDir>
#include <QFile>
#include <algorithm>
#include <cmath>
#include <fcntl.h>
#include <unistd.h>
#include <utility>

static QString scalePath() {
    // No client-supplied paths, USB reports, or EC register access.
    QDir devices("/sys/bus/hid/devices");
    for(const auto& name:devices.entryList({"0003:048D:600B.*"},QDir::Dirs|QDir::NoDotAndDotDot)) {
        const auto base=devices.filePath(name);
        const auto driver=QFileInfo(base+"/driver/module").canonicalFilePath();
        const auto path=base+"/controls/brightness_scale";
        if(driver=="/sys/module/ite_8291"&&QFile::exists(path))return path;
    }
    return {};
}
IdleDimmer::IdleDimmer(QObject* parent):QObject(parent) {
    timer.setInterval(50);
    timer.setTimerType(Qt::PreciseTimer);
    connect(&timer,&QTimer::timeout,this,[this]{
        const auto ms=elapsed.elapsed();
        const int next=slayer::idleScaleAt(from,target,ms);
        if(!writeScale(next)){reset();return;}
        if(ms>=slayer::idleFadeMilliseconds){timer.stop();if(target==1000){if(fd>=0){close(fd);fd=-1;}complete(true);}}
    });
    reset();
}
IdleDimmer::~IdleDimmer(){reset();}
bool IdleDimmer::available() const{
    const auto path=scalePath();if(path.isEmpty())return false;
    QFile minimum(QFileInfo(path).path()+"/brightness_minimum");
    return minimum.open(QIODevice::ReadOnly)&&minimum.read(16).trimmed()=="2";
}
bool IdleDimmer::writeScale(int value){
    if(value==current&&fd>=0)return true;
    if(fd<0){const auto path=scalePath();if(path.isEmpty())return false;fd=open(QFile::encodeName(path).constData(),O_WRONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return false;}
    const auto bytes=QByteArray::number(value)+'\n';
    if(pwrite(fd,bytes.constData(),size_t(bytes.size()),0)!=bytes.size())return false;
    current=value;return true;
}
bool IdleDimmer::fade(bool dim){
    if(dim)complete(false);
    const int next=dim?1:1000;
    if(next==target&&(timer.isActive()||current==next))return true;
    if(!writeScale(current))return false;
    from=current;target=next;elapsed.start();timer.start();return true;
}
void IdleDimmer::complete(bool ok){auto callbacks=std::move(restored);restored.clear();for(auto& ready:callbacks)ready(ok);}
void IdleDimmer::restore(std::function<void(bool)> ready){
    if(restored.size()>=8||!fade(false)){ready(false);return;}
    if(!timer.isActive()){
        const bool ok=writeScale(1000);if(fd>=0){close(fd);fd=-1;}
        ready(ok);return;
    }
    restored.push_back(std::move(ready));
}
void IdleDimmer::reset(){
    timer.stop();writeScale(1000);
    if(fd>=0)close(fd);
    fd=-1;current=from=target=1000;
    complete(false);
}
