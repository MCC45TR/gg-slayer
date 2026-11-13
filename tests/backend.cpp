// SPDX-License-Identifier: MIT
#include "../gui/backend.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
static bool settle(SlayerBackend& backend){
    QEventLoop loop;
    QTimer deadline;deadline.setSingleShot(true);
    QObject::connect(&deadline,&QTimer::timeout,&loop,&QEventLoop::quit);
    QObject::connect(&backend,&SlayerBackend::updated,&loop,[&]{if(!backend.busy())loop.quit();});
    deadline.start(2000);loop.exec();return deadline.isActive()&&!backend.busy();
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    qputenv("SLAYER_BACKEND_FIXTURE_MODE","good");SlayerBackend backend;
    if(!settle(backend)||!backend.ready(QStringLiteral("model-bios")))return 1;
    if(backend.keys().size()!=2)return 5;
    auto red=backend.keys()[0].toMap().value(QStringLiteral("rgb")).toList();
    auto blue=backend.keys()[1].toMap().value(QStringLiteral("rgb")).toList();
    if(red.size()!=3||blue.size()!=3||red[0].toInt()!=255||red[2].toInt()!=0||blue[0].toInt()!=0||blue[2].toInt()!=255)return 6;
    qputenv("SLAYER_BACKEND_FIXTURE_MODE","error");backend.refresh();
    if(!settle(backend)||backend.ready(QStringLiteral("model-bios"))||!backend.info().isEmpty())return 2;
    qputenv("SLAYER_BACKEND_FIXTURE_MODE","good");backend.refresh();
    if(!settle(backend)||!backend.ready(QStringLiteral("model-bios")))return 3;
    qputenv("SLAYER_BACKEND_FIXTURE_MODE","hang");backend.refresh();
    if(!settle(backend)||backend.ready(QStringLiteral("model-bios"))||!backend.message().contains(QStringLiteral("zaman aşımına")))return 4;
    return 0;
}
