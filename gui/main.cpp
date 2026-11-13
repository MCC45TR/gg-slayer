// SPDX-License-Identifier: MIT
#include "backend.hpp"
#include "controller.hpp"
#include <QApplication>
#include <QIcon>
#include <QFont>
#include <QQuickStyle>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QDir>
#include <QImage>
#include <iostream>
int main(int argc,char** argv){
    if(qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE"))QQuickStyle::setStyle("org.kde.desktop");
    QApplication app(argc,argv);app.setOrganizationName("GameGaraj");app.setApplicationName("Slayer Control");
    app.setDesktopFileName("slayer-control");app.setWindowIcon(QIcon::fromTheme("computer-laptop"));
    const bool fixture=app.arguments().contains("--fixture");
    SlayerBackend device(nullptr,false);Controller control(&device,fixture);
    const auto fixtureText=QString::fromUtf8([]{QFile f(QStringLiteral(SLAYER_FIXTURE_DATA));if(!f.open(QIODevice::ReadOnly))return QByteArray{};return f.readAll()+QByteArray("\nvalue\tsaved_lighting_owner\tcustom\nvalue\tsaved_brightness_owner\tcustom\nvalue\tbattery_cycles\t132\n");}());
    if(fixture){
        device.setPreviewMode(true);device.acceptData(fixtureText);
        bool scaleOk=false;const auto scale=qEnvironmentVariable("SLAYER_UI_FONT_SCALE").toDouble(&scaleOk);
        if(scaleOk&&scale>=1&&scale<=2){auto font=app.font();font.setPointSizeF(font.pointSizeF()*scale);app.setFont(font);}
    }
    QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("device",&device);engine.rootContext()->setContextProperty("control",&control);
    QObject::connect(&engine,&QQmlEngine::warnings,&app,[](const QList<QQmlError>& warnings){for(const auto& warning:warnings)std::cerr<<warning.toString().toStdString()<<'\n';});
    engine.load(QUrl("qrc:/ui/main.qml"));if(engine.rootObjects().isEmpty())return 1;
    // Reproducible read-only render audit; never invokes the hardware service.
    const auto capture=qEnvironmentVariable("SLAYER_UI_SCREENSHOTS");
    if(fixture&&!capture.isEmpty()){
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QDir().mkpath(capture);
        auto* tick=new QTimer(&app);auto step=std::make_shared<int>(0);
        QObject::connect(tick,&QTimer::timeout,&app,[&,window,step,tick]{
            const QList<QSize> sizes{{360,800},{600,900},{900,640},{1200,800},{1920,1080},{800,1400}};
            if(*step>=sizes.size()*14+28){tick->stop();app.quit();return;}
            const int index=*step/2;
            if(index<42){
                const int page=index%7,size=index/7;
                if(*step%2==0){window->resize(sizes[size]);window->setProperty("section",page);}
                else {auto img=window->grabWindow();if(img.isNull()){std::cerr<<"Capture failed\n";app.exit(2);return;}img.save(capture+QString("/%1-page-%2.png").arg(sizes[size].width()).arg(page));}
            }else{
                const int variant=index-42,which=variant%7;
                if(*step%2==0){window->resize(variant<7?360:1200,900);window->setProperty("section",which<2||which==6?1:which==2?3:which==3?4:6);
                    if(auto* item=window->findChild<QObject*>(which==6?"whiteBalanceOptions":which<2?"lightingTabs":which==2?"cpuAdvanced":which==3?"deviceAdvanced":"supportTabs"))item->setProperty(which<2||(which>3&&which<6)?"currentIndex":"checked",which<2?QVariant(which+1):which>3&&which<6?QVariant(which-3):QVariant(true));
                }else window->grabWindow().save(capture+QString("/variant-%1.png").arg(variant));
            }
            ++*step;
        });tick->start(300);
    }
    return app.exec();
}
