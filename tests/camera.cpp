// SPDX-License-Identifier: MIT
#include "../plasma/camera_control.hpp"
#include <QCoreApplication>
#include <QDBusContext>
#include <QDBusMessage>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <functional>

static QString stateFile;
static void writeState(const QByteArray& value){QFile file(stateFile);if(!file.open(QIODevice::WriteOnly|QIODevice::Truncate))qFatal("fixture write");file.write(value);}
static bool until(const std::function<bool()>& predicate,int timeout=4000){
    QElapsedTimer time;time.start();
    while(!predicate()&&time.elapsed()<timeout){QCoreApplication::processEvents(QEventLoop::AllEvents,10);QThread::msleep(1);}
    return predicate();
}
static void pump(int duration){QElapsedTimer time;time.start();until([&]{return time.elapsed()>=duration;},duration+100);}

class Broker final:public QObject,protected QDBusContext{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.gamegaraj.Slayer1")
public:
    int calls=0;
    bool reject=false;
    QStringList lastCommand,lastRemember;
public Q_SLOTS:
    QString Apply(const QStringList& command,const QStringList& remember){
        ++calls;lastCommand=command;lastRemember=remember;
        const auto request=message();setDelayedReply(true);
        const auto denied=reject;
        QTimer::singleShot(40,this,[request,command,denied]{
            if(denied){QDBusConnection::sessionBus().send(request.createErrorReply("org.gamegaraj.Slayer1.NotAuthorized","fixture denied"));return;}
            writeState(command.value(2)==u"on"?"1":"0");
            QDBusConnection::sessionBus().send(request.createReply(QVariantList{QStringLiteral("applied")}));
        });
        return {};
    }
};

int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    if(argc!=2)return 1;
    QTemporaryDir fixture;stateFile=fixture.path()+"/state";
    qputenv("SLAYER_CAMERA_FIXTURE",fixture.path().toUtf8());writeState("1");
    auto bus=QDBusConnection::sessionBus();Broker broker;
    if(!bus.registerObject("/org/gamegaraj/Slayer1",&broker,QDBusConnection::ExportAllSlots)||!bus.registerService("org.gamegaraj.Slayer1"))return 2;
    const auto client=QDBusConnection::connectToBus(QDBusConnection::SessionBus,"camera-test-client");
    CameraControl control(QString::fromLocal8Bit(argv[1]),client,false);
    if(!until([&]{return control.available()&&!control.busy();})||control.state()!=1)return 3;
    auto reads=[&]{QFile f(fixture.path()+"/reads");if(!f.open(QIODevice::ReadOnly))return qsizetype(0);return f.readAll().count('\n');};
    const auto initial=reads();pump(400);
    if(reads()!=initial||broker.calls)return 4; // no periodic reader or initial write
    control.setEnabled(false);control.setEnabled(false);control.setEnabled(true);
    if(!control.busy()||control.state()!=1)return 5; // readback drives the icon
    if(!until([&]{return !control.busy();})||control.state()!=0||broker.calls!=1){qWarning()<<"Camera apply"<<control.state()<<broker.calls<<control.error();return 6;}
    if(broker.lastCommand!=QStringList{"settings","camera","off"}||broker.lastRemember!=QStringList{"camera","off"})return 7;
    broker.reject=true;control.setEnabled(true);
    if(!until([&]{return !control.busy();})||control.state()!=0||!control.error().contains("fixture denied"))return 8;
    broker.reject=false;control.setEnabled(true);
    if(!until([&]{return !control.busy();})||control.state()!=1||!control.error().isEmpty())return 9;
    for(const auto& invalid:{QByteArray("invalid"),QByteArray("inconsistent"),QByteArray("large"),QByteArray("missing"),QByteArray("hang")}){
        writeState(invalid);const auto count=reads();control.refresh();
        if(!until([&]{return reads()>count&&!control.busy();})||control.available()||control.error().isEmpty())return 10;
        const auto calls=broker.calls;control.setEnabled(true);pump(20);
        if(broker.calls!=calls)return 11;
    }
    writeState("0");control.refresh();
    if(!until([&]{return control.available()&&!control.busy();})||control.state()!=0||!control.error().isEmpty())return 12;
    const auto before=reads();for(int i=0;i<20;++i)control.refresh();
    if(!until([&]{return reads()>before&&!control.busy();})||reads()!=before+1)return 13;
    pump(300);if(reads()!=before+1)return 14;
    CameraControl missing(QStringLiteral("/nonexistent/slayerctl-camera"),client,false);
    if(!until([&]{return !missing.error().isEmpty();})||missing.available()||missing.busy())return 15;
    return 0;
}
#include "camera.moc"
