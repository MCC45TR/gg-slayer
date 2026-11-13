// SPDX-License-Identifier: MIT
// Exercise the actual widgets with a reader that cannot access hardware.
#define main slayer_gui_entrypoint
#include "../gui/legacy_main.cpp"
#undef main
#include <QEventLoop>
#include <QFileInfo>
#include <QImage>
#include <QDir>
#include <iostream>
static void settle(int delay=30){QEventLoop loop;QTimer::singleShot(delay,&loop,&QEventLoop::quit);loop.exec();}
static void require(bool condition,const char* message){if(!condition){std::cerr<<message<<'\n';std::exit(1);}}
int main(int argc,char** argv){
    QApplication app(argc,argv);
    Window window;window.show();settle(350);
    require(window.windowTitle()==u"Slayer Control","Window title");
    auto* nav=window.findChild<QListWidget*>(u"navigation"_s);
    auto* pages=window.findChild<QStackedWidget*>();
    require(nav&&pages&&pages->count()==10,"Ten destinations");
    int destinations=0;for(int row=0;row<nav->count();++row)if(nav->item(row)->flags()&Qt::ItemIsSelectable){
        ++destinations;
        if(nav->item(row)->icon().isNull())std::cerr<<nav->item(row)->text().toStdString()<<": icon missing\n";
        require(!nav->item(row)->icon().isNull(),"System theme navigation icons");
    }
    require(destinations==10,"Navigation headings must not be selectable");
    const auto capture=qEnvironmentVariable("SLAYER_UI_SCREENSHOTS");if(!capture.isEmpty())QDir().mkpath(capture);
    for(const auto& size:{QSize(1180,820),QSize(900,640)}){
        window.resize(size);settle();
        for(int index=0;index<10;++index){
            window.selectPage(index);settle();
            auto* scroll=qobject_cast<QScrollArea*>(pages->currentWidget());
            require(scroll&&scroll->widget()->width()<=scroll->viewport()->width(),"Page must fit the viewport");
            require(scroll->horizontalScrollBar()->maximum()==0,"No hidden horizontal page overflow");
            for(auto* button:scroll->widget()->findChildren<QPushButton*>()){
                if(!button->isVisibleTo(scroll->widget()))continue;
                const auto point=button->mapTo(scroll->widget(),QPoint());
                require(point.x()>=0&&point.x()+button->width()<=scroll->widget()->width(),"Buttons must fit at the minimum window width");
            }
            const auto image=window.grab().toImage();require(!image.isNull(),"Page rendering");
            if(!capture.isEmpty()&&size.width()==1180)image.save(capture+u"/page-"_s+QString::number(index)+u".png"_s);
        }
    }
    auto* gpu=window.findChild<QPushButton*>(u"gpuLimitApply"_s);require(gpu&&!gpu->isEnabled(),"GPU limits require validated driver response");
    auto* color=window.findChild<QLineEdit*>(u"rgbColor"_s);require(color,"Color input");color->setText(u"bad"_s);
    for(auto* button:window.findChildren<QPushButton*>())if(button->text()==u"Tüm klavyeye uygula")require(!button->isEnabled(),"Invalid colors cannot be applied");
    auto* name=window.findChild<QLineEdit*>(u"profileName"_s);require(name,"Profile name");
    for(auto* button:window.findChildren<QPushButton*>())if(button->text()==u"Seçili renkle oluştur")require(!button->isEnabled(),"Empty profile names cannot be saved");
    auto* port=window.findChild<QLineEdit*>(u"usbPort"_s);require(port,"USB port input");
    require(!port->hasAcceptableInput(),"An empty USB port is invalid");port->setText(u"3-2.1"_s);require(port->hasAcceptableInput(),"Valid USB port syntax");
    window.selectPage(1);auto* keyboard=static_cast<Keyboard*>(window.findChild<QWidget*>(u"keyboardPreview"_s));require(keyboard,"Keyboard preview");
    keyboard->setFocus();QKeyEvent right(QEvent::KeyPress,Qt::Key_Right,Qt::NoModifier);QApplication::sendEvent(keyboard,&right);
    require(keyboard->accessibleDescription().startsWith(u"Seçili tuş: "),"Keyboard preview supports arrow keys");
    keyboard->capturePhysical(true);
    QKeyEvent physical(QEvent::KeyPress,Qt::Key_W,Qt::NoModifier,25,0,0);
    QApplication::sendEvent(keyboard,&physical);
    require(keyboard->accessibleDescription()==u"Seçili tuş: W","Physical scancode selection");
    keyboard->capturePhysical(false);
    color->setText(u"3DAEE9"_s);
    keyboard->clearFocus();
    window.showHardware();settle();require(window.findChild<QTabWidget*>(u"diagnosticTabs"_s)->currentIndex()==1,"Legacy hardware destination");
    // Dark and light palettes must preserve text and geometry. No action that
    // changes hardware, configuration, services, camera or PAM is activated.
    for(bool dark:{true,false}){
        auto palette=app.palette();palette.setColor(QPalette::Window,dark?QColor(35,38,41):QColor(239,240,241));palette.setColor(QPalette::Base,dark?QColor(27,30,32):Qt::white);
        palette.setColor(QPalette::Text,dark?QColor(239,240,241):QColor(35,38,41));palette.setColor(QPalette::WindowText,palette.color(QPalette::Text));
        palette.setColor(QPalette::Button,palette.color(QPalette::Window));palette.setColor(QPalette::ButtonText,palette.color(QPalette::Text));
        palette.setColor(QPalette::PlaceholderText,dark?QColor(164,168,172):QColor(100,104,108));app.setPalette(palette);
        window.resize(1180,820);window.selectPage(0);settle();
        require(!window.grab().isNull(),"Palette rendering");
        auto* hero=window.findChild<QFrame*>(u"card"_s);require(hero,"Theme card");
        const auto pixel=hero->grab().toImage().pixelColor(hero->width()-22,hero->height()-22);
        require(qAbs(pixel.lightness()-palette.color(QPalette::Base).lightness())<8,"Card background must follow palette changes");
        if(!capture.isEmpty())window.grab().save(capture+(dark?u"/overview-dark.png"_s:u"/overview-light.png"_s));
        for(int index=1;index<10;++index){
            window.selectPage(index);settle(10);require(!window.grab().isNull(),"All pages render in either palette");
            if(dark&&!capture.isEmpty()&&(index==1||index==5))window.grab().save(capture+u"/dark-page-"_s+QString::number(index)+u".png"_s);
        }
    }
    window.selectPage(9);settle();
    for(auto* frame:pages->currentWidget()->findChildren<QFrame*>(u"card"_s))for(auto* text:frame->findChildren<QLabel*>()){
        if(!text->isVisibleTo(frame))continue;
        const auto origin=text->mapTo(frame,QPoint());
        if(origin.x()+text->width()>frame->width())std::cerr<<"Label overflow: "<<text->text().left(40).toStdString()<<'\n';
        require(origin.x()+text->width()<=frame->width(),"About text must fit its card");
    }
    auto* laptop=window.findChild<QWidget*>(u"laptopIllustration"_s);require(laptop&&laptop->property("wallpaperLoaded").toBool(),"Default system wallpaper in laptop illustration");
    const auto wallpaperSize=laptop->property("wallpaperSize").toSize();require(wallpaperSize.width()<=640&&wallpaperSize.height()<=400,"Wallpaper cache stays bounded even when decoder ignores scaling hints");
    require(window.findChild<QLabel*>(u"usbSpecification"_s)->text().startsWith(u"5 USB portu"),"Physical USB ports differ from connected devices");
    if(!capture.isEmpty())window.grab().save(capture+u"/about-light.png"_s);
    auto* effect=window.findChild<QComboBox*>(u"hardwareEffectMode"_s);require(effect&&effect->count()==9,"Only verified controller effects are exposed");
    require(window.findChild<QSpinBox*>(u"hardwareEffectBrightness"_s)->value()==50,"Hardware effect brightness is independent of stale static brightness");
    require(effect->findData(u"cycle"_s)<0,"No CPU animation fallback");
    effect->setCurrentIndex(effect->findData(u"breathe"_s));
    require(!window.findChild<QComboBox*>(u"hardwareEffectDirection"_s)->isEnabled(),"Direction is wave-only");
    // A BIOS mismatch must close hardware actions even after ready data was
    // previously shown. Refresh uses only the fixture script.
    qputenv("SLAYER_UI_FIXTURE_MODE","blocked");
    for(auto* button:window.findChildren<QPushButton*>())if(button->text()==u"Yenile")button->click();settle(250);
    for(auto* button:window.findChildren<QPushButton*>())if(button->text()==u"Sonraki açılış için kaydet")require(!button->isEnabled(),"BIOS gate survives refresh");
    std::cout<<"10 pages at 1180x820 and 900x640; light/dark palettes; system icons; input validation; keyboard navigation; BIOS gating passed. No hardware writes.\n";
    return 0;
}
