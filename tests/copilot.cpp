// SPDX-License-Identifier: MIT
#include "../gui/copilot.hpp"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QKeySequence>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <KDesktopFile>
#include <KConfigGroup>
#include <functional>
#include <iostream>
#include <memory>
using SlayerShortcut::Sequence;
using SlayerShortcut::Sequences;
static void require(bool ok,const char* text){if(!ok){std::cerr<<text<<'\n';std::exit(1);}}
static bool until(const std::function<bool()>& predicate){QElapsedTimer timer;timer.start();while(!predicate()&&timer.elapsed()<4000){QCoreApplication::processEvents(QEventLoop::AllEvents,10);QThread::msleep(1);}return predicate();}
static QByteArray content(){QFile file(SlayerShortcut::desktopPath());return file.open(QIODevice::ReadOnly)?file.readAll():QByteArray{};}
class Accel final:public QObject,protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.kde.KGlobalAccel")
    void own(const QStringList& action){require(action.size()==4&&action[0]=="org.gamegaraj.slayerctl.copilot.desktop","Changed another application's shortcut");}
public:
    Sequences keys;
    int calls=0,sets=0;
    bool conflict=false,rejectNext=false;
public Q_SLOTS:
    Sequences shortcutKeys(const QStringList& action){own(action);++calls;return keys;}
    bool globalShortcutAvailable(const Sequence& sequence,const QString& component){require(sequence.keys.size()==1&&component=="org.gamegaraj.slayerctl.copilot.desktop","Bad availability request");++calls;return !conflict;}
    void doRegister(const QStringList& action){own(action);require(action[1].isEmpty(),"Native desktop launch not used");++calls;}
    bool unregister(const QString& component,const QString& action){require(component=="org.gamegaraj.slayerctl.copilot.desktop","Unregistered another component");++calls;if(action=="_launch")keys.clear();return true;}
    Sequences setShortcutKeys(const QStringList& action,const Sequences& desired,uint flags){own(action);require(action[1]=="_launch"&&flags==4,"Shortcut is owned by the UI instead of native desktop launcher");++calls;++sets;if(rejectNext){rejectNext=false;return {};}keys=desired;return keys;}
};
int main(int argc,char** argv){
    QTemporaryDir directory;require(directory.isValid(),"Temporary home");
    qputenv("XDG_DATA_HOME",directory.path().toUtf8());
    QCoreApplication app(argc,argv);CopilotIntegration copilot;Accel accel;
    require(accel.calls==0,"Constructor did background work");
    auto bus=QDBusConnection::sessionBus();
    require(bus.registerService("org.kde.kglobalaccel")&&bus.registerObject("/kglobalaccel",&accel,QDBusConnection::ExportAllSlots),"Private shortcuts service");
    copilot.refresh();require(until([&]{return !copilot.busy();}),"Initial shortcut read");
    require(copilot.state().value("available").toBool()&&!copilot.state().value("assigned").toBool(),"Initial state");
    const int idleCalls=accel.calls;QElapsedTimer elapsed;elapsed.start();
    require(until([&]{return elapsed.elapsed()>120;}),"Idle sample");require(accel.calls==idleCalls,"Unrequested shortcut polling");
    const auto trigger=QKeySequence::fromString("Meta+Shift+F23",QKeySequence::PortableText);
    QQmlEngine engine;engine.rootContext()->setContextProperty("copilot",&copilot);engine.rootContext()->setContextProperty("recordedKey",QVariant::fromValue(trigger));
    QQmlComponent component(&engine);component.setData("import QtQml; QtObject { function commit() { copilot.save(\"control\", \"\", recordedKey) } }",QUrl{});
    std::unique_ptr<QObject> qml(component.create());require(bool(qml),qPrintable(component.errorString()));
    require(QMetaObject::invokeMethod(qml.get(),"commit"),"QML shortcut submission");require(until([&]{return !copilot.busy();}),"Assign shortcut");
    require(copilot.error().isEmpty(),qPrintable(copilot.error()));
    require(copilot.state().value("assigned").toBool()&&accel.keys.size()==1,"Shortcut missing");
    const auto control=content();require(control.contains("Exec=/usr/bin/slayer-control-qt"),"Native launch entry missing");
    const auto oldKeys=accel.keys;
    accel.conflict=true;copilot.save("website","https://example.org",trigger);require(until([&]{return !copilot.busy();}),"Conflict result");
    require(!copilot.error().isEmpty()&&content()==control&&accel.keys==oldKeys,"Conflict changed an existing assignment");
    accel.conflict=false;accel.rejectNext=true;copilot.save("website","https://example.org",trigger);require(until([&]{return !copilot.busy();}),"Rejected assignment result");
    require(!copilot.error().isEmpty()&&content()==control&&accel.keys==oldKeys,"Failed assignment was not rolled back");
    copilot.save("website","file:///etc/passwd",trigger);require(!copilot.busy()&&!copilot.error().isEmpty()&&content()==control,"Unsafe URL accepted");
    copilot.save("website","https://example.org/a%20b?q=$x&v=%22test%22",trigger);require(until([&]{return !copilot.busy();}),"URL assignment");
    require(copilot.error().isEmpty(),qPrintable(copilot.error()));
    KDesktopFile file(SlayerShortcut::desktopPath());const auto exec=file.desktopGroup().readEntry("Exec");
    require(exec.startsWith("/usr/bin/xdg-open \"")&&exec.contains("%%20")&&exec.contains("%%22")&&!exec.contains('\n'),"URL Exec escaping");
    const auto apps=QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);QDir().mkpath(apps);QFile application(apps+"/fixture.desktop");
    require(application.open(QIODevice::WriteOnly),"Application fixture");application.write("[Desktop Entry]\nType=Application\nName=Fixture\nExec=/usr/bin/true %U\n");application.close();
    copilot.save("application","fixture.desktop",trigger);require(until([&]{return !copilot.busy();}),"Application assignment");
    require(copilot.error().isEmpty()&&content().contains("Exec=/usr/bin/true %U"),"Installed application field codes changed");
    for(const auto& action:QStringList{"fan-boost","fan-auto","power-cycle","power-saver","balanced","performance"}){
        copilot.save(action,{},trigger);require(until([&]{return !copilot.busy();}),"Hardware action assignment");
        require(copilot.error().isEmpty()&&content().contains(("Exec=/usr/libexec/slayerctl-desktop --shortcut "+action).toUtf8()),"Hardware action bypassed ctl");
    }
    copilot.save("command","/usr/bin/printf \"a b\" \"100%\"",trigger);require(until([&]{return !copilot.busy();}),"Custom command assignment");
    require(copilot.error().isEmpty(),qPrintable(copilot.error()));
    KDesktopFile commandFile(SlayerShortcut::desktopPath());const auto custom=commandFile.desktopGroup().readEntry("Exec");
    require(custom.contains("\"a b\"")&&custom.contains("100%%"),"Custom command argument quoting");
    const auto customBytes=content();
    copilot.save("command","/usr/bin/true; /usr/bin/false",trigger);
    require(!copilot.busy()&&!copilot.error().isEmpty()&&content()==customBytes,"Custom command acquired implicit shell semantics");
    copilot.save("none",{},{});require(until([&]{return !copilot.busy();}),"Remove assignment");
    require(copilot.error().isEmpty()&&!QFile::exists(SlayerShortcut::desktopPath())&&accel.keys.isEmpty(),"Removal retained a shortcut");
    CopilotIntegration preview(true);preview.refresh();const int previousCalls=accel.calls;preview.save("control",{},trigger);
    require(accel.calls==previousCalls&&!QFile::exists(SlayerShortcut::desktopPath()),"Preview changed native shortcuts");
    int stateChanges=0;QObject::connect(&preview,&CopilotIntegration::changed,&app,[&]{++stateChanges;});
    preview.loadApplications();require(stateChanges==0&&!preview.applications().isEmpty(),"Application discovery reset the user's pending function selection");
    std::cout<<"Native Copilot launch, conflicts, rollback, applications, URLs, custom commands, hardware actions and no idle work passed.\n";
}
#include "copilot.moc"
