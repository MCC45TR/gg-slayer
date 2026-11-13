// SPDX-License-Identifier: MIT
#include "widgets.hpp"
#include "session.hpp"
#include "laptop.hpp"
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextStream>
#include <QThreadPool>
#include <unistd.h>
using namespace SlayerUi;

class Window final:public QMainWindow {
    enum Page{Overview,Lighting,Profiles,Power,Performance,Graphics,Devices,Automation,Diagnostics,About,PageCount};
    struct Action{QPushButton* widget;QString capability;std::function<bool()> valid;};
    SlayerBackend backend;
    QStackedWidget* pages=new QStackedWidget;
    QListWidget* nav=new QListWidget;
    QLabel* breadcrumb=label({});
    QLabel* readTime=label(u"Bilgiler okunuyor…"_s,true);
    QLabel* deviceName=label(u"Cihaz bilgisi bekleniyor"_s);
    QLabel* connection=label({});
    QLabel* notice=label({});
    QFrame* noticeFrame=new QFrame;
    QPlainTextEdit* output=new QPlainTextEdit;
    QTabWidget* diagnosticTabs=new QTabWidget;
    QTableWidget* capTable=new QTableWidget(0,3);
    QComboBox* profiles=new QComboBox;
    QComboBox* platformProfiles=new QComboBox;
    QLineEdit* colorText=new QLineEdit(u"3DAEE9"_s);
    QSpinBox* brightness=new QSpinBox;
    QSlider* brightnessSlider=new QSlider(Qt::Horizontal);
    QSpinBox* ledIndex=new QSpinBox;
    QLabel* selectedLabel=label(u"W"_s);
    QProgressBar* batteryBar=new QProgressBar;
    QString selected=u"W"_s,lastOutput;
    QMap<QString,QList<QLabel*>> values;
    QList<Action> actionList;
    QList<QComboBox*> profileBoxes;
    QList<QPair<QComboBox*,QString>> initialChoices;
    QMap<QWidget*,QString> inputRequirements;
    QVariantMap lastInfo;
    QVariantList lastCaps;
    QPalette appliedPalette;
    QList<int> navigationRows;
    QStringList titles;
    bool initialized=false,wasBusy=false;

    QPushButton* button(const QString& text,std::function<void()> fn,const QString& cap={},const QString& symbol={},std::function<bool()> valid={}) {
        auto* item=new QPushButton(QIcon::fromTheme(symbol),text);
        item->setProperty("themeIcon",symbol);
        item->setMinimumHeight(34);item->setCursor(Qt::PointingHandCursor);
        connect(item,&QPushButton::clicked,this,[fn=std::move(fn)]{fn();});
        actionList.append({item,cap,std::move(valid)});return item;
    }
    QPushButton* primary(QPushButton* item){item->setProperty("primary",true);return item;}
    void run(const QStringList& args,bool root=false){backend.run(args,root);}
    void inspect(const QStringList& args){selectPage(Diagnostics);diagnosticTabs->setCurrentIndex(0);run(args);}
    QWidget* page(const QString& title,const QString& subtitle) {
        titles.append(title);
        auto* scroll=new QScrollArea;scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* body=new QWidget;body->setObjectName(u"pageBody"_s);
        auto* layout=new QVBoxLayout(body);layout->setContentsMargins(26,24,26,26);layout->setSpacing(18);
        auto* name=label(title);name->setObjectName(u"pageTitle"_s);heading(name,12);layout->addWidget(name);
        auto* intro=label(subtitle,true);intro->setContentsMargins(0,0,0,4);layout->addWidget(intro);layout->addStretch();
        scroll->setWidget(body);pages->addWidget(scroll);return body;
    }
    void add(QWidget* page,QWidget* widget){auto* layout=qobject_cast<QVBoxLayout*>(page->layout());layout->insertWidget(layout->count()-1,widget);}
    Card* card(QWidget* page,const QString& title,const QString& subtitle={},const QString& symbol={}){
        auto* item=new Card(title,subtitle,symbol);add(page,item);return item;
    }
    QLabel* value(const QString& key,bool large=false){
        auto* item=label(u"—"_s);item->setTextInteractionFlags(Qt::TextSelectableByMouse);
        if(large)heading(item,9);
        values[key].append(item);return item;
    }
    QFormLayout* form(Card* item){
        auto* layout=new QFormLayout;layout->setHorizontalSpacing(24);layout->setVerticalSpacing(12);
        layout->setRowWrapPolicy(QFormLayout::WrapLongRows);layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        item->content->addLayout(layout);return layout;
    }
    void field(QFormLayout* layout,const QString& title,QWidget* widget){layout->addRow(label(title,true),widget);}
    QComboBox* choice(const QList<QPair<QString,QString>>& options,const QString& initialKey={}){
        auto* box=new QComboBox;box->setMinimumWidth(170);box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        for(const auto& [name,id]:options)box->addItem(name,id);
        if(!initialKey.isEmpty())initialChoices.append({box,initialKey});
        return box;
    }
    QComboBox* profileBox(){auto* box=new QComboBox;box->setMinimumWidth(160);profileBoxes.append(box);return box;}
    QSpinBox* number(int low,int high,int initial,const QString& suffix){
        auto* item=new QSpinBox;item->setRange(low,high);item->setValue(initial);item->setSuffix(suffix);item->setMinimumWidth(110);return item;
    }
    bool profileSelected()const{return !profiles->currentText().isEmpty();}
    bool validColor()const{return colorText->hasAcceptableInput();}
    QStringList profileArgs(const QString& cmd)const{return {u"keyboard"_s,u"profile"_s,cmd,profiles->currentText()};}
    void watch(QLineEdit* input){connect(input,&QLineEdit::textChanged,this,[this]{updateActions();});}
    void capability(Card* item,const QString& id){
        auto* message=label({},true);item->content->addWidget(message);
        connect(&backend,&SlayerBackend::dataChanged,message,[this,message,id]{
            const bool ready=backend.ready(id);message->setVisible(!ready);
            message->setText(backend.reason(id).isEmpty()?u"Bu özellik için donanım desteği bekleniyor."_s:backend.reason(id));
        });
        message->setText(u"Donanım desteği kontrol ediliyor…"_s);
    }
    QString readable(const QString& key,const QVariantMap& info)const {
        auto text=info.value(key).toString();
        if(text.isEmpty())return u"—"_s;
        if(key==u"keyboard_layout")return text==u"Turkish"?u"Türkçe Q"_s:text.contains(u"Turkish (F)")?u"Türkçe F"_s:text;
        if(key==u"usb_connected_devices")return text+u" (dahili aygıtlar dahil)"_s;
        if(key==u"ec_backend")return text==u"not-loaded"?u"Yüklü değil"_s:text.section(u' ',0,0);
        if(key==u"battery"||key==u"battery_health")return text+u" %";
        if(key==u"keyboard_brightness")return text+u" %";
        if(key==u"ac_online")return text==u"1"?u"Adaptöre bağlı"_s:text==u"0"?u"Pille çalışıyor"_s:u"Kaynak bilinmiyor"_s;
        if(key==u"fn_lock"||key==u"camera"||key==u"usb_charging"||key==u"cpu_boost"){
            if(text==u"1")return u"Açık"_s;
            if(text==u"0")return u"Kapalı"_s;
            if(text==u"unavailable")return u"Bulunamadı"_s;
            if(text==u"ambiguous")return u"Aygıt belirsiz"_s;
        }
        if(key==u"cpu_smt")return text==u"on"?u"Açık · 32 iş parçacığı"_s:text==u"off"?u"Kapalı · 16 iş parçacığı"_s:text;
        if(key==u"keyboard_driver")return text==u"ite_8291"?u"RGB klavye hazır"_s:u"Sürücü yüklü değil"_s;
        if(key==u"nvidia")return text==u"loaded"?u"NVIDIA sürücüsü etkin"_s:info.value(u"mux_mode"_s).toString()==u"integrated"?u"Yalnız iGPU seçili"_s:u"Sürücü yüklü değil"_s;
        if(key==u"mux_mode")return text==u"hybrid"?u"Hibrit"_s:text==u"dedicated"?u"Yalnız NVIDIA"_s:text==u"integrated"?u"Yalnız AMD iGPU"_s:text;
        if(key==u"fan_mode")return text==u"auto"?u"Otomatik"_s:text==u"boost"?u"Fan Boost"_s:u"Firmware özel modu"_s;
        if(key==u"profile")return text==u"balanced"?u"Dengeli"_s:text==u"performance"?u"Performans"_s:text;
        if(key==u"secure_boot")return text==u"enabled"?u"Etkin"_s:text==u"disabled"?u"Devre dışı"_s:u"Okunamadı"_s;
        if(key==u"tpm")return text==u"available"?u"Kullanılabilir"_s:u"Bulunamadı"_s;
        return text;
    }

    void overview(){
        auto* p=page(u"Genel Bakış"_s,u"Bilgisayarınızın durumu ve sık kullandığınız ayarlar."_s);
        auto* hero=card(p,{});auto* top=new QHBoxLayout;top->setSpacing(18);
        top->addWidget(icon(u"computer-laptop"_s,56));
        auto* identity=new QVBoxLayout;auto* name=value(u"model"_s);heading(name,8);identity->addWidget(name);
        identity->addWidget(value(u"cpu"_s));top->addLayout(identity,1);
        connection->setObjectName(u"connectionBadge"_s);connection->setAlignment(Qt::AlignCenter);top->addWidget(connection,0,Qt::AlignTop);hero->content->addLayout(top);
        auto* stats=new QWidget;auto* grid=new QGridLayout(stats);grid->setContentsMargins(0,0,0,0);grid->setSpacing(12);
        const QList<QStringList> entries={{u"Pil"_s,u"battery"_s,u"battery"_s,u"ac_online"_s},{u"Grafik modu"_s,u"video-display"_s,u"mux_mode"_s,u"nvidia"_s},{u"Soğutma"_s,u"speedometer"_s,u"fan_mode"_s,u"profile"_s}};
        for(int i=0;i<entries.size();++i){
            const auto& e=entries[i];auto* item=new Card(e[0],{},e[1]);item->content->addWidget(value(e[2],true));
            auto* sub=value(e[3]);sub->setForegroundRole(QPalette::PlaceholderText);item->content->addWidget(sub);
            if(i==0){batteryBar->setObjectName(u"batteryBar"_s);batteryBar->setRange(0,100);batteryBar->setTextVisible(false);batteryBar->setFixedHeight(4);item->content->addWidget(batteryBar);}
            grid->addWidget(item,0,i);grid->setColumnStretch(i,1);
        }
        add(p,stats);
        auto* details=card(p,u"Sistem bilgileri"_s,{},u"dialog-information"_s);auto* rows=form(details);
        for(const auto& entry:QList<QPair<QString,QString>>{{u"BIOS sürümü"_s,u"bios"_s},{u"İç ekran"_s,u"panel"_s},{u"Klavye"_s,u"keyboard_driver"_s},{u"Secure Boot"_s,u"secure_boot"_s}})field(rows,entry.first,value(entry.second));
        add(p,actionRow({button(u"Klavye ve RGB"_s,[this]{selectPage(Lighting);},{},u"input-keyboard"_s),button(u"Güç ve pil"_s,[this]{selectPage(Power);},{},u"battery"_s),button(u"Ekran ayarları"_s,[this]{backend.openSettings(u"display"_s);},{},u"preferences-desktop-display"_s)}));
    }
    void lighting(){
        auto* p=page(u"Klavye ve RGB"_s,u"Renginizi seçin. Tek bir tuşu veya tüm klavyeyi kişiselleştirin."_s);
        auto* preview=card(p,u"Fiziksel klavye"_s,u"Etiketler sistemin etkin tuş haritasından, renk çizgileri sürücünün son statik kaydından gelir."_s,u"input-keyboard"_s);
        auto* layoutName=value(u"keyboard_layout"_s);layoutName->setObjectName(u"keyboardLayoutName"_s);
        auto* format=choice({{u"ISO · L biçimli Enter"_s,u"iso"_s},{u"ANSI · Yatay Enter"_s,u"ansi"_s}},u"keyboard_physical_format"_s);format->setObjectName(u"physicalKeyboardFormat"_s);
        preview->content->addWidget(actionRow({label(u"Sistem düzeni"_s,true),layoutName,format}));
        connect(format,qOverload<int>(&QComboBox::activated),this,[this,format]{backend.setKeyboardFormat(format->currentData().toString());});
        auto* canvas=new Keyboard(backend,[this](const QString& id,int index){
            selected=id;QString text=id;for(const auto& entry:backend.keys()){const auto key=entry.toMap();if(key.value(u"id"_s).toString()==id){text=key.value(u"label"_s).toString();break;}}
            selectedLabel->setText(text+u" · LED "_s+QString::number(index));ledIndex->setValue(index);
        });preview->content->addWidget(canvas);
        auto* selectByPress=new QCheckBox(u"Bu alandayken fiziksel tuşa basarak seç"_s);selectByPress->setToolTip(u"Yalnız bu önizleme odaktayken çalışır; arka planda tuş kaydı yapılmaz."_s);
        connect(selectByPress,&QCheckBox::toggled,canvas,&Keyboard::capturePhysical);preview->content->addWidget(selectByPress);
        auto* mapping=label({},true);preview->content->addWidget(mapping);
        connect(&backend,&SlayerBackend::dataChanged,mapping,[this,mapping]{int calibrated=0,total=0;for(const auto& entry:backend.keys()){const auto key=entry.toMap();if(key.contains(u"present"_s)&&!key.value(u"present"_s).toBool())continue;++total;if(key.value(u"calibrated"_s).toBool())++calibrated;}
            mapping->setText(u"LED eşlemesi: "_s+QString::number(calibrated)+u" / "_s+QString::number(total)+u" doğrulandı. Shift/AltGr karakterleri tuşların köşelerinde gösterilir."_s);});
        auto* colors=card(p,u"Renk ve parlaklık"_s);
        auto* swatches=new QWidget;auto* line=new QHBoxLayout(swatches);line->setContentsMargins(0,0,0,0);line->setSpacing(8);
        auto* swatchGroup=new QButtonGroup(this);swatchGroup->setExclusive(true);
        for(const auto& hex:QStringList{u"3DAEE9"_s,u"9B7AEB"_s,u"EF6AA6"_s,u"F67470"_s,u"F5B65A"_s,u"67C9A3"_s,u"FFFFFF"_s}){
            auto* swatch=new Swatch(QColor(u"#"_s+hex));swatchGroup->addButton(swatch);line->addWidget(swatch);
            swatch->setChecked(hex==colorText->text());connect(swatch,&QAbstractButton::clicked,this,[this,hex]{colorText->setText(hex);});
        }
        line->addStretch();colors->content->addWidget(swatches);
        colorText->setObjectName(u"rgbColor"_s);colorText->setMaxLength(6);colorText->setMaximumWidth(115);
        colorText->setAccessibleName(u"RGB renk kodu"_s);colorText->setPlaceholderText(u"RRGGBB"_s);
        colorText->setValidator(new QRegularExpressionValidator(QRegularExpression(u"[0-9a-fA-F]{6}"_s),this));
        connect(colorText,&QLineEdit::textChanged,canvas,[this,canvas,swatchGroup]{
            if(validColor())canvas->color(QColor(u"#"_s+colorText->text()));
            swatchGroup->setExclusive(false);for(auto* b:swatchGroup->buttons())b->setChecked(static_cast<Swatch*>(b)->value().name().mid(1).compare(colorText->text(),Qt::CaseInsensitive)==0);swatchGroup->setExclusive(true);updateActions();
        });
        colors->content->addWidget(actionRow({label(u"Renk kodu"_s,true),colorText,button(u"Renk seç…"_s,[this]{
            const auto chosen=QColorDialog::getColor(QColor(u"#"_s+colorText->text()),this,u"Klavye rengi"_s);if(chosen.isValid())colorText->setText(chosen.name().mid(1).toUpper());
        },{},u"color-management"_s)}));
        colors->content->addWidget(actionRow({label(u"Seçili tuş"_s,true),selectedLabel,
            button(u"Tuşa uygula"_s,[this]{run({u"keyboard"_s,u"key"_s,selected,colorText->text()},true);},u"rgb"_s,{},[this]{return validColor();}),
            primary(button(u"Tüm klavyeye uygula"_s,[this]{run({u"keyboard"_s,u"color"_s,colorText->text()},true);},u"rgb"_s,u"dialog-ok-apply"_s,[this]{return validColor();}))}));
        brightness->setObjectName(u"keyboardBrightness"_s);brightness->setRange(0,100);brightness->setValue(50);brightness->setSuffix(u" %"_s);
        brightnessSlider->setRange(0,100);brightnessSlider->setValue(50);brightnessSlider->setMinimumWidth(120);brightnessSlider->setAccessibleName(u"Klavye parlaklığı"_s);
        connect(brightnessSlider,&QSlider::valueChanged,brightness,&QSpinBox::setValue);connect(brightness,qOverload<int>(&QSpinBox::valueChanged),brightnessSlider,&QSlider::setValue);
        auto* brightRow=new QHBoxLayout;brightRow->addWidget(label(u"Parlaklık"_s,true));brightRow->addWidget(brightnessSlider,1);brightRow->addWidget(brightness);
        brightRow->addWidget(button(u"Uygula"_s,[this]{run({u"keyboard"_s,u"brightness"_s,QString::number(brightness->value())},true);},u"keyboard"_s));colors->content->addLayout(brightRow);
        colors->content->addWidget(actionRow({button(u"KDE vurgu rengini kullan"_s,[this]{run({u"keyboard"_s,u"accent"_s,u"on"_s});},u"rgb"_s,u"preferences-desktop-color"_s),button(u"Vurgu eşlemesini kapat"_s,[this]{run({u"keyboard"_s,u"accent"_s,u"off"_s});})}));
        auto* effects=card(p,u"Işık efektleri"_s,u"Efektler klavyenin içinde çalışır. Başlatmadan önce vurgu eşlemesini kapatın."_s,u"preferences-desktop-effects"_s);
        auto* mode=choice({{u"Dalga"_s,u"wave"_s},{u"Nefes"_s,u"breathe"_s},{u"Gökkuşağı"_s,u"rainbow"_s},{u"Tuşa tepki"_s,u"reactive"_s},{u"Halka"_s,u"ripple"_s},{u"Kayan ışık"_s,u"marquee"_s},{u"Yağmur"_s,u"raindrop"_s},{u"Kutup ışıkları"_s,u"aurora"_s},{u"Kıvılcım"_s,u"spark"_s}});
        auto* speed=number(1,10,5,{});auto* effectBrightness=number(0,100,50,u" %"_s);effectBrightness->setObjectName(u"hardwareEffectBrightness"_s);auto* direction=choice({{u"Sağ"_s,u"right"_s},{u"Sol"_s,u"left"_s},{u"Yukarı"_s,u"up"_s},{u"Aşağı"_s,u"down"_s}});
        mode->setObjectName(u"hardwareEffectMode"_s);direction->setObjectName(u"hardwareEffectDirection"_s);
        auto* rows=form(effects);field(rows,u"Efekt"_s,mode);field(rows,u"Hız · 1–10"_s,speed);field(rows,u"Efekt parlaklığı"_s,effectBrightness);field(rows,u"Dalga yönü"_s,direction);
        connect(mode,qOverload<int>(&QComboBox::currentIndexChanged),direction,[mode,direction]{direction->setEnabled(mode->currentData().toString()==u"wave");});
        effects->content->addWidget(actionRow({primary(button(u"Efekti başlat"_s,[this,mode,speed,direction,effectBrightness]{QStringList args{u"keyboard"_s,u"hardware"_s,mode->currentData().toString(),u"--speed"_s,QString::number(speed->value()),u"--brightness"_s,QString::number(effectBrightness->value())};if(mode->currentData().toString()==u"wave")args<<u"--direction"_s<<direction->currentData().toString();run(args,true);},u"hardware-rgb"_s,u"media-playback-start"_s)),button(u"Işığı kapat"_s,[this]{run({u"keyboard"_s,u"hardware"_s,u"off"_s},true);},u"hardware-rgb"_s,u"media-playback-stop"_s)}));capability(effects,u"hardware-rgb"_s);
        auto* effectName=new QLineEdit;effectName->setPlaceholderText(u"Efekt profili adı"_s);effectName->setMaxLength(32);effectName->setValidator(new QRegularExpressionValidator(QRegularExpression(u"[a-zA-Z0-9_-]{1,32}"_s),this));watch(effectName);
        effects->content->addWidget(actionRow({effectName,button(u"Efekti profil olarak kaydet"_s,[this,effectName,mode,speed,direction,effectBrightness]{run({u"keyboard"_s,u"profile"_s,u"effect"_s,effectName->text(),mode->currentData().toString(),QString::number(speed->value()),QString::number(effectBrightness->value()),direction->currentData().toString()},true);},{},u"document-save"_s,[effectName]{return effectName->hasAcceptableInput();})}));
        auto* calibration=new Disclosure(u"Tuş eşlemesini düzelt"_s);ledIndex->setRange(0,125);ledIndex->setValue(66);
        calibration->content->addWidget(label(u"Önizlemeden bir tuş seçin. LED'i iki saniye gösterip doğru tuşa eşleyin. Tanıma, çalışan ışık efektini kesebilir."_s,true));
        calibration->content->addWidget(actionRow({label(u"LED indeksi"_s,true),ledIndex,button(u"LED'i göster"_s,[this]{run({u"keyboard"_s,u"identify"_s,QString::number(ledIndex->value())},true);},u"rgb"_s,u"edit-find"_s),button(u"Tuşa eşle"_s,[this]{run({u"keyboard"_s,u"layout"_s,u"set"_s,selected,QString::number(ledIndex->value())},true);})}));add(p,calibration);
    }
    void rgbProfiles(){
        auto* p=page(u"RGB Profilleri"_s,u"Farklı kullanım alışkanlıkları için renklerinizi saklayın."_s);
        profileBoxes.append(profiles);profiles->setMinimumWidth(200);
        connect(profiles,&QComboBox::currentTextChanged,this,[this]{updateActions();});
        auto* saved=card(p,u"Kayıtlı profiller"_s,u"Bir profili uygulayın veya her açılışta kullanmak üzere seçin."_s,u"folder-favorites"_s);
        saved->content->addWidget(profiles);
        saved->content->addWidget(actionRow({primary(button(u"Profili uygula"_s,[this]{run(profileArgs(u"apply"_s),true);},u"model-bios"_s,u"dialog-ok-apply"_s,[this]{return profileSelected();})),button(u"Açılışta kullan"_s,[this]{run({u"config"_s,u"set"_s,u"rgb_profile"_s,profiles->currentText()},true);},{},u"system-run"_s,[this]{return profileSelected();}),button(u"İçeriği göster"_s,[this]{inspect(profileArgs(u"show"_s));},{},u"document-preview"_s,[this]{return profileSelected();})}));
        auto* edit=card(p,u"Renkleri profile kaydet"_s,u"Klavye sayfasında seçtiğiniz renk ve tuş kullanılır; kaydetmek klavyeyi değiştirmez."_s,u"document-edit"_s);
        edit->content->addWidget(actionRow({button(u"Seçili tuş"_s,[this]{auto a=profileArgs(u"set-key"_s);a<<selected<<colorText->text();run(a,true);},{},{},[this]{return profileSelected()&&validColor();}),button(u"WASD"_s,[this]{auto a=profileArgs(u"set-group"_s);a<<u"wasd"_s<<colorText->text();run(a,true);},{},{},[this]{return profileSelected()&&validColor();}),button(u"Ok tuşları"_s,[this]{auto a=profileArgs(u"set-group"_s);a<<u"arrows"_s<<colorText->text();run(a,true);},{},{},[this]{return profileSelected()&&validColor();}),button(u"Renk seçimine git"_s,[this]{selectPage(Lighting);},{},u"go-next"_s)}));
        auto* create=card(p,u"Yeni profil"_s,{},u"document-new"_s);auto* name=new QLineEdit;name->setObjectName(u"profileName"_s);name->setPlaceholderText(u"Örneğin: oyun, gece, calisma"_s);name->setMaxLength(32);name->setAccessibleName(u"Yeni profil adı"_s);name->setToolTip(u"A–Z, a–z, 0–9, alt çizgi ve kısa çizgi kullanılabilir."_s);
        name->setValidator(new QRegularExpressionValidator(QRegularExpression(u"[a-zA-Z0-9_-]{1,32}"_s),this));watch(name);create->content->addWidget(name);
        create->content->addWidget(actionRow({primary(button(u"Seçili renkle oluştur"_s,[this,name]{run({u"keyboard"_s,u"profile"_s,u"create"_s,name->text(),colorText->text(),QString::number(brightness->value())},true);},{},u"list-add"_s,[this,name]{return name->hasAcceptableInput()&&validColor();})),button(u"Klavyenin renklerini kopyala"_s,[this,name]{run({u"keyboard"_s,u"profile"_s,u"capture"_s,name->text()},true);},u"rgb"_s,u"edit-copy"_s,[name]{return name->hasAcceptableInput();})}));
        add(p,actionRow({button(u"Açılış profilini kaldır"_s,[this]{run({u"config"_s,u"unset"_s,u"rgb_profile"_s},true);},{},u"edit-clear"_s),button(u"Kaydedilmiş ayarlar"_s,[this]{inspect({u"config"_s,u"show"_s});},{},u"document-properties"_s)}));
    }
    void power(){
        auto* p=page(u"Güç ve Pil"_s,u"Pil ömrü ile performans arasında size uygun dengeyi kurun."_s);
        auto* desktop=card(p,u"Güç profili"_s,u"Bu seçim KDE ve GNOME'un sistem güç ayarlarıyla paylaşılır."_s,u"preferences-system-power-management"_s);
        auto* choices=choice({{u"Pil tasarrufu"_s,u"power-saver"_s},{u"Dengeli"_s,u"balanced"_s},{u"Performans"_s,u"performance"_s}});choices->setCurrentIndex(1);
        desktop->content->addWidget(actionRow({choices,primary(button(u"Uygula"_s,[this,choices]{run({u"power"_s,u"set"_s,choices->currentData().toString()});},{},u"dialog-ok-apply"_s)),button(u"Geçerli profili göster"_s,[this]{inspect({u"power"_s,u"status"_s});})}));
        auto* protection=card(p,u"Pil koruması"_s,u"Çoğunlukla adaptörde çalışıyorsanız masaüstü kullanım profilini seçebilirsiniz."_s,u"battery"_s);
        auto* charge=choice({{u"Yüksek kapasite"_s,u"high-capacity"_s},{u"Dengeli"_s,u"balanced"_s},{u"Masaüstü kullanım"_s,u"stationary"_s}},u"battery_profile"_s);
        auto* rows=form(protection);field(rows,u"Pil sağlığı"_s,value(u"battery_health"_s));field(rows,u"Koruma profili"_s,charge);
        protection->content->addWidget(actionRow({primary(button(u"Profili uygula"_s,[this,charge]{run({u"battery"_s,u"profile"_s,charge->currentData().toString()},true);},u"battery-profile"_s,u"dialog-ok-apply"_s)),button(u"Açılış için kaydet"_s,[this,charge]{run({u"config"_s,u"set"_s,u"battery_profile"_s,charge->currentData().toString()},true);},u"battery-profile"_s,u"document-save"_s)}));
        protection->content->addWidget(label(u"Koruma profilleri yüzde dolum sınırı değildir; pil göstergesi %100 kalabilir."_s,true));capability(protection,u"battery-profile"_s);
        auto* limit=card(p,u"Yüzde dolum sınırı"_s,u"Yalnız cihazınız bu özelliği sunuyorsa kullanılabilir."_s);
        auto* end=number(50,100,80,u" %"_s);limit->content->addWidget(actionRow({label(u"Şarjı durdur"_s,true),end,button(u"Sınırı uygula"_s,[this,end]{run({u"battery"_s,u"limit"_s,QString::number(end->value())},true);},u"battery-limit"_s)}));inputRequirements.insert(end,u"battery-limit"_s);capability(limit,u"battery-limit"_s);
        add(p,actionRow({button(u"Ayrıntılı pil durumu"_s,[this]{inspect({u"battery"_s,u"status"_s});},{},u"dialog-information"_s),button(u"Sistem güç ayarları"_s,[this]{backend.openSettings(u"power"_s);},{},u"preferences-system"_s)}));
    }
    void performance(){
        auto* p=page(u"Performans ve Fan"_s,u"Soğutmayı, fabrika performans modunu ve işlemciyi yönetin."_s);
        auto* fan=card(p,u"Soğutma"_s,u"Otomatik mod günlük kullanım içindir. Fan Boost en yüksek soğutmayı seçer."_s,u"speedometer"_s);
        auto* fanMode=choice({{u"Otomatik"_s,u"auto"_s},{u"Fan Boost"_s,u"boost"_s}},u"fan_mode"_s);
        auto* rows=form(fan);field(rows,u"Şu anda"_s,value(u"fan_mode"_s));field(rows,u"İstenen mod"_s,fanMode);
        fan->content->addWidget(actionRow({primary(button(u"Uygula"_s,[this,fanMode]{run({u"fan"_s,fanMode->currentData().toString()},true);},u"fan"_s,u"dialog-ok-apply"_s)),button(u"Açılış için kaydet"_s,[this,fanMode]{run({u"config"_s,u"set"_s,u"fan_mode"_s,fanMode->currentData().toString()},true);},u"fan"_s,u"document-save"_s),button(u"Sıcaklık ve fan devri"_s,[this]{inspect({u"sensors"_s});},{},u"utilities-system-monitor"_s)}));capability(fan,u"fan"_s);
        auto* thermal=card(p,u"Fabrika performans modu"_s,u"Cihazın güç ve soğutma dengesini birlikte değiştirir."_s,u"preferences-system-performance"_s);
        platformProfiles->setMinimumWidth(180);thermal->content->addWidget(actionRow({platformProfiles,button(u"Uygula"_s,[this]{run({u"profile"_s,u"set"_s,platformProfiles->currentData().toString()},true);},u"profile"_s)}));
        auto* cpu=card(p,u"İşlemci"_s,{},u"cpu"_s);
        auto* boost=choice({{u"Açık"_s,u"on"_s},{u"Kapalı"_s,u"off"_s}});
        auto* threads=choice({{u"32 iş parçacığı · SMT açık"_s,u"on"_s},{u"16 iş parçacığı · SMT kapalı"_s,u"off"_s}},u"cpu_smt"_s);
        rows=form(cpu);field(rows,u"Boost durumu"_s,value(u"cpu_boost"_s));field(rows,u"CPU boost"_s,boost);
        cpu->content->addWidget(actionRow({button(u"Boost seçimini uygula"_s,[this,boost]{run({u"cpu"_s,u"boost"_s,boost->currentData().toString()},true);},u"cpu-boost"_s)}));
        auto* smtForm=form(cpu);field(smtForm,u"İş parçacıkları"_s,threads);
        cpu->content->addWidget(actionRow({button(u"SMT uygula"_s,[this,threads]{run({u"cpu"_s,u"smt"_s,threads->currentData().toString()},true);},u"cpu-smt"_s),button(u"SMT'yi açılış için kaydet"_s,[this,threads]{run({u"config"_s,u"set"_s,u"cpu_smt"_s,threads->currentData().toString()},true);},u"cpu-smt"_s)}));
        cpu->content->addWidget(label(u"SMT seçimi 16 fiziksel çekirdeği korur; ek iş parçacıklarını açar veya kapatır."_s,true));
        auto* advanced=new Disclosure(u"Gelişmiş işlemci ve fan ayarları"_s);
        auto* limits=new Card(u"İşlemci frekans sınırları"_s,u"Sistem güç yöneticisi daha sonra bu değerleri değiştirebilir."_s);
        auto* min=number(100,10000,500,u" MHz"_s);auto* max=number(100,10000,2500,u" MHz"_s);
        rows=form(limits);field(rows,u"En düşük"_s,min);field(rows,u"En yüksek"_s,max);
        auto valid=[min,max]{return min->value()<=max->value();};
        connect(min,qOverload<int>(&QSpinBox::valueChanged),this,[this]{updateActions();});connect(max,qOverload<int>(&QSpinBox::valueChanged),this,[this]{updateActions();});
        limits->content->addWidget(actionRow({button(u"Uygula"_s,[this,min,max]{run({u"cpu"_s,u"limits"_s,QString::number(min->value()),QString::number(max->value())},true);},u"cpu-frequency"_s,{},valid),button(u"Açılış için kaydet"_s,[this,min,max]{run({u"config"_s,u"set"_s,u"cpu_min_mhz"_s,QString::number(min->value()),u"cpu_max_mhz"_s,QString::number(max->value())},true);},u"cpu-frequency"_s,{},valid),button(u"Varsayılan sınırlara dön"_s,[this]{run({u"cpu"_s,u"limits"_s,u"reset"_s},true);},u"cpu-frequency"_s)}));advanced->content->addWidget(limits);
        auto* curve=new Card(u"Fan eğrisi taslağı"_s,u"Yalnız dosya olarak hazırlanır. Bu cihazda özel eğriyi fanlara uygulama desteği henüz yok."_s);
        auto* curveName=new QLineEdit;curveName->setPlaceholderText(u"Eğri adı"_s);curveName->setMaxLength(32);curveName->setValidator(new QRegularExpressionValidator(QRegularExpression(u"[a-zA-Z0-9_-]{1,32}"_s),this));watch(curveName);
        auto* cpuCurve=new QLineEdit(u"40:30,60:60,90:100"_s);auto* gpuCurve=new QLineEdit(u"40:30,65:65,90:100"_s);
        rows=form(curve);field(rows,u"Ad"_s,curveName);field(rows,u"CPU · sıcaklık:yüzde"_s,cpuCurve);field(rows,u"GPU · sıcaklık:yüzde"_s,gpuCurve);
        curve->content->addWidget(actionRow({button(u"Taslağı kaydet"_s,[this,curveName,cpuCurve,gpuCurve]{run({u"fan"_s,u"curve"_s,u"save"_s,curveName->text(),cpuCurve->text(),gpuCurve->text()},true);},{},u"document-save"_s,[curveName]{return curveName->hasAcceptableInput();}),button(u"Kayıtlı taslaklar"_s,[this]{inspect({u"fan"_s,u"curve"_s,u"list"_s});})}));advanced->content->addWidget(curve);add(p,advanced);
        add(p,actionRow({button(u"CPU durumu"_s,[this]{inspect({u"cpu"_s,u"status"_s});},{},u"dialog-information"_s),button(u"Çekirdek topolojisi"_s,[this]{inspect({u"cpu"_s,u"topology"_s});})}));
        connect(&backend,&SlayerBackend::dataChanged,this,[this,boost,min,max]{
            if(initialized)return;
            const auto info=backend.info();if(info.isEmpty())return;
            boost->setCurrentIndex(info.value(u"cpu_boost"_s).toString()==u"0"?1:0);
            bool lowOk=false,highOk=false;const int low=info.value(u"cpu_scaling_min_freq"_s).toString().toInt(&lowOk)/1000,high=info.value(u"cpu_scaling_max_freq"_s).toString().toInt(&highOk)/1000;
            if(lowOk&&highOk){min->setValue(low);max->setValue(high);}
        });
    }
    void graphics(){
        auto* p=page(u"Grafik ve MUX"_s,u"Kullanımınıza uygun grafik işlemcisini seçin."_s);
        auto* mux=card(p,u"Grafik modu"_s,u"Seçim kaydedilir ve bilgisayarı yeniden başlattığınızda uygulanır."_s,u"video-display"_s);
        auto* diagram=new GraphicsDiagram;mux->content->addWidget(diagram);
        auto* mode=choice({{u"Hibrit · AMD + NVIDIA"_s,u"hybrid"_s},{u"Yalnız NVIDIA"_s,u"dedicated"_s},{u"Yalnız AMD iGPU"_s,u"integrated"_s}},u"mux_mode"_s);
        auto* rows=form(mux);field(rows,u"Kaydedilen mod"_s,value(u"mux_mode"_s));field(rows,u"İstenen mod"_s,mode);
        connect(mode,qOverload<int>(&QComboBox::currentIndexChanged),diagram,[mode,diagram]{diagram->setMode(mode->currentData().toString());});
        auto* explanation=label({},true);auto explain=[mode,explanation]{
            const auto id=mode->currentData().toString();explanation->setText(id==u"integrated"?u"Pil ömrü önceliklidir. NVIDIA'ya bağlı harici ekran bağlantıları kullanılamayabilir."_s:id==u"dedicated"?u"NVIDIA önceliklidir. Pil tüketimi artabilir."_s:u"Günlük kullanımda AMD, gerektiğinde NVIDIA kullanılır."_s);
        };connect(mode,qOverload<int>(&QComboBox::currentIndexChanged),explanation,explain);explain();mux->content->addWidget(explanation);
        mux->content->addWidget(actionRow({primary(button(u"Sonraki açılış için kaydet"_s,[this,mode]{run({u"gpu"_s,u"mode"_s,mode->currentData().toString()},true);},u"mux"_s,u"document-save"_s))}));capability(mux,u"mux"_s);
        auto* details=card(p,u"Grafik durumu"_s);rows=form(details);field(rows,u"Sürücü"_s,value(u"nvidia"_s));field(rows,u"İç ekran bağlantısı"_s,value(u"panel"_s));
        details->content->addWidget(actionRow({button(u"Sistem ekran ayarları"_s,[this]{backend.openSettings(u"display"_s);},{},u"preferences-desktop-display"_s)}));
        auto* advanced=new Disclosure(u"Gelişmiş · NVIDIA güç limiti"_s);
        advanced->content->addWidget(label(u"Önce sürücünün izin verdiği sınırları okuyun. Geçerli sınırlar bildirilmeden yazma açılmaz."_s,true));
        auto* watts=number(1,10000,50,u" W"_s);
        auto* apply=button(u"Sürücüye limit isteği gönder"_s,[this,watts]{run({u"gpu"_s,u"power-limit"_s,QString::number(watts->value())},true);},u"model-bios"_s,{},[this]{return backend.output().contains(u"power_settable: candidate"_s);});apply->setObjectName(u"gpuLimitApply"_s);
        advanced->content->addWidget(actionRow({button(u"Sürücü limitlerini oku"_s,[this]{inspect({u"gpu"_s,u"limits"_s});}),watts,apply}));add(p,advanced);
    }
    void devices(){
        auto* p=page(u"Aygıtlar ve USB"_s,u"Klavye işlevlerini, kamera erişimini ve USB aygıtlarını yönetin."_s);
        for(const auto& entry:QList<QStringList>{{u"Fn kilidi"_s,u"fn-lock"_s,u"fn_lock"_s,u"fn"_s,u"input-keyboard"_s,u"İşlev tuşlarının davranışını değiştirir."_s},{u"Dahili kamera"_s,u"camera"_s,u"camera"_s,u"camera"_s,u"camera-web"_s,u"RGB ve IR kameranın Linux erişimini açar veya kapatır. Görüntülü görüşme kesilebilir."_s},{u"Kapalıyken USB şarj"_s,u"usb-charging"_s,u"usb_charging"_s,u"usb-charging"_s,u"drive-removable-media-usb"_s,u"Bilgisayar kapalıyken veya uykudayken USB şarjı. Cihazın desteklemesi gerekir."_s}}){
            const auto setting=entry[1],key=entry[2],cap=entry[3];auto* item=card(p,entry[0],entry[5],entry[4]);
            auto* desired=choice({{u"Açık"_s,u"on"_s},{u"Kapalı"_s,u"off"_s}},key);
            auto* rows=form(item);field(rows,u"Şu anda"_s,value(key));field(rows,u"İstenen durum"_s,desired);inputRequirements.insert(desired,cap);
            item->content->addWidget(actionRow({primary(button(u"Uygula"_s,[this,setting,desired]{run({u"settings"_s,setting,desired->currentData().toString()},true);},cap,u"dialog-ok-apply"_s)),button(u"Açılış için kaydet"_s,[this,key,desired]{run({u"config"_s,u"set"_s,key,desired->currentData().toString()},true);},cap,u"document-save"_s)}));capability(item,cap);
        }
        auto* usb=card(p,u"USB güç yönetimi"_s,u"Aygıt listesinden bir port adı alın. Örneğin: 3-2.1."_s,u"drive-removable-media-usb"_s);
        auto* port=new QLineEdit;port->setObjectName(u"usbPort"_s);port->setPlaceholderText(u"USB portu"_s);port->setMaxLength(27);port->setValidator(new QRegularExpressionValidator(QRegularExpression(u"[0-9]{1,3}-[0-9]{1,3}(\\.[0-9]{1,3}){0,5}"_s),this));watch(port);
        usb->content->addWidget(actionRow({button(u"Aygıtları listele"_s,[this]{inspect({u"usb"_s,u"list"_s});},{},u"view-list-details"_s),port}));
        usb->content->addWidget(actionRow({button(u"Güç tasarrufu aç"_s,[this,port]{run({u"usb"_s,u"powersave"_s,port->text(),u"on"_s},true);},u"model-bios"_s,{},[port]{return port->hasAcceptableInput();}),button(u"Güç tasarrufunu kapat"_s,[this,port]{run({u"usb"_s,u"powersave"_s,port->text(),u"off"_s},true);},u"model-bios"_s,{},[port]{return port->hasAcceptableInput();})}));
        add(p,actionRow({button(u"Sistem touchpad ayarları"_s,[this]{backend.openSettings(u"touchpad"_s);},{},u"input-touchpad"_s)}));
    }
    void automation(){
        auto* p=page(u"Otomasyon"_s,u"Adaptör takıldığında ve pille çalışırken uygulanacak profilleri seçin."_s);
        auto* rgb=card(p,u"Işık profilleri"_s,{},u"input-keyboard"_s);auto* ac=profileBox();auto* dc=profileBox();auto* rows=form(rgb);
        field(rows,u"Adaptöre bağlıyken"_s,ac);field(rows,u"Pille çalışırken"_s,dc);
        rgb->content->addWidget(actionRow({primary(button(u"RGB otomasyonunu kaydet"_s,[this,ac,dc]{run({u"config"_s,u"set"_s,u"rgb_profile_ac"_s,ac->currentText(),u"rgb_profile_battery"_s,dc->currentText()},true);},{},u"document-save"_s,[ac,dc]{return !ac->currentText().isEmpty()&&!dc->currentText().isEmpty();}))}));
        auto* power=card(p,u"Güç profilleri"_s,{},u"preferences-system-power-management"_s);
        const QList<QPair<QString,QString>> options={{u"Pil tasarrufu"_s,u"power-saver"_s},{u"Dengeli"_s,u"balanced"_s},{u"Performans"_s,u"performance"_s}};
        auto* acPower=choice(options);auto* dcPower=choice(options);acPower->setCurrentIndex(1);rows=form(power);
        field(rows,u"Adaptöre bağlıyken"_s,acPower);field(rows,u"Pille çalışırken"_s,dcPower);
        power->content->addWidget(actionRow({primary(button(u"Güç otomasyonunu kaydet"_s,[this,acPower,dcPower]{run({u"config"_s,u"set"_s,u"desktop_profile_ac"_s,acPower->currentData().toString(),u"desktop_profile_battery"_s,dcPower->currentData().toString()},true);},{},u"document-save"_s))}));
        auto* saved=card(p,u"Kaydedilmiş ayarlar"_s,u"Açılış ve uyanma için kaydettiğiniz tercihler de burada gösterilir."_s,u"document-properties"_s);
        saved->content->addWidget(actionRow({button(u"Ayarları göster"_s,[this]{inspect({u"config"_s,u"show"_s});},{},u"document-preview"_s),button(u"Kaydedilmiş ayarları uygula"_s,[this]{run({u"apply-config"_s},true);},u"model-bios"_s,u"dialog-ok-apply"_s),button(u"AC/DC otomasyonunu kaldır"_s,[this]{run({u"config"_s,u"unset"_s,u"rgb_profile_ac"_s,u"rgb_profile_battery"_s,u"desktop_profile_ac"_s,u"desktop_profile_battery"_s},true);},{},u"edit-clear"_s)}));
    }
    void diagnostics(){
        auto* p=page(u"Tanılama"_s,u"İhtiyacınız olduğunda cihaz bilgilerini okuyun ve destek durumunu inceleyin."_s);
        diagnosticTabs->setObjectName(u"diagnosticTabs"_s);
        auto* console=new QWidget;auto* layout=new QVBoxLayout(console);layout->setContentsMargins(0,16,0,0);layout->setSpacing(12);
        layout->addWidget(actionRow({button(u"Sistemi denetle"_s,[this]{run({u"doctor"_s});},{},u"tools-check-spelling"_s),button(u"Sensörler"_s,[this]{run({u"sensors"_s});},{},u"utilities-system-monitor"_s),button(u"USB aygıtları"_s,[this]{run({u"usb"_s,u"list"_s});},{},u"drive-removable-media-usb"_s),button(u"Hizmet durumu"_s,[this]{run({u"service"_s,u"status"_s});},{},u"preferences-system"_s)}));
        output->setObjectName(u"diagnosticOutput"_s);output->setReadOnly(true);output->setMinimumHeight(320);output->setMaximumBlockCount(3000);
        output->setPlaceholderText(u"Bir denetim seçin. Sonuçlar burada sade metin olarak görünür."_s);output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        output->setLineWrapMode(QPlainTextEdit::WidgetWidth);layout->addWidget(output);
        layout->addWidget(actionRow({button(u"Çıktıyı kopyala"_s,[this]{QApplication::clipboard()->setText(output->toPlainText());},{},u"edit-copy"_s,[this]{return !output->toPlainText().isEmpty();})}));
        auto* support=new QWidget;auto* supportLayout=new QVBoxLayout(support);supportLayout->setContentsMargins(0,16,0,0);supportLayout->setSpacing(12);
        supportLayout->addWidget(label(u"Pasif ayarların nedeni burada görünür. Destek durumu yalnız doğrulanmış yetenekleri gösterir."_s,true));
        capTable->setHorizontalHeaderLabels({u"Özellik"_s,u"Durum"_s,u"Açıklama"_s});capTable->setObjectName(u"capabilityTable"_s);
        capTable->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);capTable->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);capTable->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch);
        capTable->verticalHeader()->hide();capTable->setEditTriggers(QAbstractItemView::NoEditTriggers);capTable->setSelectionBehavior(QAbstractItemView::SelectRows);capTable->setWordWrap(true);capTable->setShowGrid(false);capTable->setMinimumHeight(380);
        supportLayout->addWidget(capTable);diagnosticTabs->addTab(console,QIcon::fromTheme(u"utilities-terminal"_s),u"Çıktı"_s);diagnosticTabs->addTab(support,QIcon::fromTheme(u"view-list-details"_s),u"Donanım desteği"_s);add(p,diagnosticTabs);
        auto* details=card(p,u"Sürüm ve güvenlik"_s);auto* rows=form(details);field(rows,u"Uygulama"_s,label(QStringLiteral(SLAYERCTL_VERSION)));field(rows,u"Kernel"_s,value(u"kernel"_s));field(rows,u"Desteklenen BIOS"_s,value(u"supported_bios"_s));field(rows,u"Secure Boot"_s,value(u"secure_boot"_s));field(rows,u"TPM"_s,value(u"tpm"_s));
    }
    void about(){
        auto* p=page(u"Hakkında"_s,u"Slayer Control ve bilgisayarınız; kısa sistem özeti."_s);
        auto* laptop=new LaptopIllustration;add(p,laptop);
        auto* summary=new Card(u"Slayer Control"_s,u"Klavye, güç ve cihaz denetimleri."_s,u"computer-laptop"_s);
        auto* rows=form(summary);field(rows,u"Uygulama sürümü"_s,label(QStringLiteral(SLAYERCTL_VERSION)));
        field(rows,u"Cihaz"_s,value(u"model"_s));field(rows,u"BIOS sürümü"_s,value(u"bios"_s));field(rows,u"R9T denetim sürücüsü"_s,value(u"ec_backend"_s));
        auto* ports=new Card(u"Bağlantılar"_s,{},u"drive-removable-media-usb"_s);
        auto* portDetails=new QWidget;rows=new QFormLayout(portDetails);rows->setRowWrapPolicy(QFormLayout::WrapLongRows);rows->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);rows->setVerticalSpacing(10);rows->setHorizontalSpacing(16);rows->setContentsMargins(0,0,0,0);
        auto* specification=label({},true);specification->setObjectName(u"usbSpecification"_s);
        ports->content->addWidget(specification);ports->content->addWidget(portDetails);
        connect(&backend,&SlayerBackend::dataChanged,specification,[this,specification,portDetails]{const auto info=backend.info();
            portDetails->setVisible(info.contains(u"usb_external_a"_s));
            specification->setText(info.value(u"usb_external_a"_s).toString()==u"3"&&info.value(u"usb_external_c"_s).toString()==u"2"?u"5 USB portu: 3 × USB-A ve 2 × USB-C."_s:u"Dış port bilgisi bu model için doğrulanmadı."_s);});
        field(rows,u"USB-A"_s,label(u"3 adet · USB 3.2 Gen 1 · 5 Gb/sn"_s));
        field(rows,u"USB-C"_s,label(u"2 adet · USB 3.2 Gen 2 · 10 Gb/sn · DisplayPort"_s));
        field(rows,u"USB-C PD"_s,label(u"Sol: 100 W · Arka: 140 W (üretici değeri)"_s));
        field(rows,u"Diğer"_s,label(u"HDMI 2.1 · DP 2.1a\n2.5 Gb Ethernet · SD · 3,5 mm ses"_s));
        field(rows,u"Bağlı USB aygıtları"_s,value(u"usb_connected_devices"_s));
        auto* source=new QLabel(u"<a href=\"https://www.gamegaraj.com/laptop/game-garaj-slayer-r9t-5060-amd-ryzen-9-8945hx-rtx5/\">Üretici teknik özellikleri</a>"_s);source->setWordWrap(true);source->setOpenExternalLinks(true);ports->content->addWidget(source);
        add(p,new ResponsiveCards(summary,ports));
    }
    void updateActions(){
        for(const auto& action:actionList){
            const bool available=action.capability.isEmpty()||backend.ready(action.capability);
            action.widget->setEnabled(!backend.busy()&&available&&(!action.valid||action.valid()));
            if(!action.capability.isEmpty())action.widget->setToolTip(backend.reason(action.capability));
        }
        for(auto it=inputRequirements.begin();it!=inputRequirements.end();++it)it.key()->setEnabled(!backend.busy()&&backend.ready(it.value()));
    }
    void updateState(){
        const auto info=backend.info();
        if(info!=lastInfo){
            lastInfo=info;for(auto it=values.begin();it!=values.end();++it)for(auto* item:it.value())item->setText(readable(it.key(),info));
            deviceName->setText(info.value(u"model"_s).toString().isEmpty()?u"Cihaz bilgisi yok"_s:info.value(u"model"_s).toString());
            batteryBar->setValue(info.value(u"battery"_s).toInt());
            const auto previous=platformProfiles->currentData().toString();platformProfiles->clear();
            for(const auto& id:info.value(u"profile_choices"_s).toString().split(u' ',Qt::SkipEmptyParts))platformProfiles->addItem(id==u"balanced"?u"Dengeli"_s:id==u"performance"?u"Performans"_s:id,id);
            const auto desired=previous.isEmpty()?info.value(u"profile"_s).toString():previous;const int index=platformProfiles->findData(desired);if(index>=0)platformProfiles->setCurrentIndex(index);
        }
        if(!initialized&&!info.isEmpty()){
            const QSignalBlocker blocker(brightness);brightness->setValue(info.value(u"keyboard_brightness"_s).toInt());brightnessSlider->setValue(brightness->value());
            for(const auto& [box,key]:initialChoices){auto state=info.value(key).toString();if(state==u"0")state=u"off"_s;else if(state==u"1")state=u"on"_s;const int index=box->findData(state);if(index>=0)box->setCurrentIndex(index);}
            initialized=true;
        }
        for(auto* box:profileBoxes){
            QStringList current;for(int i=0;i<box->count();++i)current.append(box->itemText(i));
            if(current!=backend.rgbProfiles()){const auto previous=box->currentText();box->clear();box->addItems(backend.rgbProfiles());const int index=box->findText(previous);if(index>=0)box->setCurrentIndex(index);}
        }
        if(lastOutput!=backend.output()){lastOutput=backend.output();output->setPlainText(lastOutput);}
        if(lastCaps!=backend.capabilities()){
            lastCaps=backend.capabilities();capTable->setRowCount(lastCaps.size());int row=0;
            for(const auto& entry:lastCaps){const auto cap=entry.toMap();const auto state=cap[u"state"_s].toString();const auto status=state==u"ready"?u"Hazır"_s:state==u"session"?u"Masaüstü"_s:state==u"blocked"?u"Kullanılamıyor"_s:u"Sunulmuyor"_s;
                capTable->setItem(row,0,new QTableWidgetItem(cap[u"name"_s].toString()));auto* item=new QTableWidgetItem(QIcon::fromTheme(state==u"ready"?u"emblem-ok-symbolic"_s:u"dialog-information"_s),status);capTable->setItem(row,1,item);capTable->setItem(row,2,new QTableWidgetItem(cap[u"reason"_s].toString()));++row;
            }
            capTable->resizeRowsToContents();
        }
        const bool blocked=info.value(u"hardware_writes"_s).toString()==u"blocked";
        connection->setText(info.isEmpty()?u"Bilgi yok"_s:blocked?u"BIOS doğrulanmalı"_s:u"Cihaz hazır"_s);
        notice->setText(blocked?u"Donanım ayarları kilitli: "_s+info.value(u"hardware_write_reason"_s).toString():backend.message());
        noticeFrame->setVisible(!notice->text().isEmpty());
        if(backend.busy())readTime->setText(u"İşlem sürüyor…"_s);
        else if(wasBusy||readTime->text()==u"Bilgiler okunuyor…")readTime->setText(info.isEmpty()?u"Bilgiler okunamadı"_s:u"Son okuma "_s+QTime::currentTime().toString(u"HH:mm"_s));
        wasBusy=backend.busy();updateActions();
    }
    void theme(){
        const auto colors=QApplication::palette();
        if(appliedPalette==colors&&!styleSheet().isEmpty())return;
        appliedPalette=colors;setPalette(colors);
        const bool dark=colors.color(QPalette::Window).lightness()<128;
        QIcon::setFallbackThemeName(dark?u"breeze-dark"_s:u"breeze"_s);
        for(auto* item:findChildren<QAbstractButton*>()){
            const auto name=item->property("themeIcon").toString();
            if(!name.isEmpty())item->setIcon(QIcon::fromTheme(name));
        }
        for(int row=0;row<nav->count();++row){
            auto* item=nav->item(row);const auto symbol=item->data(Qt::UserRole+1).toString();
            if(!symbol.isEmpty())item->setIcon(QIcon::fromTheme(symbol));
            else item->setForeground(colors.color(QPalette::PlaceholderText));
        }
        const auto base=colors.color(QPalette::Base),text=colors.color(QPalette::Text),window=colors.color(QPalette::Window);
        const auto border=blend(text,base,.13).name(),sidebar=blend(base,window,.25).name();
        setStyleSheet(QStringLiteral(
            "QWidget#pageBody { background: palette(window); }"
            "QFrame#sidebar { background: %1; border-right: 1px solid %2; }"
            "QFrame#toolbar { border-bottom: 1px solid %2; }"
            "QFrame#card { background: palette(base); border: 1px solid %2; border-radius: 7px; }"
            "QListWidget#navigation { border: none; background: transparent; outline: none; padding: 0; }"
            "QListWidget#navigation::item { padding: 9px 10px; border-radius: 5px; margin: 2px 0; }"
            "QListWidget#navigation::item:selected { background: palette(highlight); color: palette(highlighted-text); }"
            "QListWidget#navigation::item:hover:!selected { background: palette(midlight); }"
            "QPushButton { padding: 7px 12px; border: 1px solid %2; border-radius: 4px; background: palette(button); }"
            "QPushButton:enabled:hover { border-color: palette(highlight); background: %3; }"
            "QPushButton:enabled:pressed { background: %4; }"
            "QPushButton:disabled { background: transparent; }"
            "QPushButton[primary=true]:enabled { border: 1px solid palette(highlight); border-radius: 4px; background: %3; }"
            "QPushButton[primary=true]:enabled:hover { background: %4; }"
            "QLineEdit, QComboBox, QSpinBox { min-height: 28px; }"
            "QLabel#connectionBadge { padding: 5px 9px; border-radius: 4px; background: %3; }"
            "QFrame#notice { background: %3; border-top: 1px solid %2; }"
            "QProgressBar#batteryBar { border: none; background: %2; border-radius: 2px; }"
            "QProgressBar#batteryBar::chunk { background: palette(highlight); border-radius: 2px; }"
            "QTableWidget { border: 1px solid %2; border-radius: 4px; background: palette(base); }"
            "QTableWidget::item { padding: 7px; }"
            "QPlainTextEdit { border: 1px solid %2; border-radius: 5px; padding: 10px; background: palette(base); }"
        ).arg(sidebar,border,blend(colors.color(QPalette::Highlight),base,.10).name(),blend(colors.color(QPalette::Highlight),base,.18).name()));
    }
protected:
    void changeEvent(QEvent* event)override{QMainWindow::changeEvent(event);if(event->type()==QEvent::ApplicationPaletteChange||event->type()==QEvent::PaletteChange)theme();}
    void closeEvent(QCloseEvent* event)override{
        if(backend.busy()){notice->setText(u"İşlem tamamlandığında pencereyi kapatabilirsiniz."_s);noticeFrame->show();event->ignore();}else event->accept();
    }
public:
    void selectPage(int index){if(index<0||index>=navigationRows.size())return;nav->setCurrentRow(navigationRows[index]);}
    void showHardware(){selectPage(Diagnostics);diagnosticTabs->setCurrentIndex(1);}
    Window(){
        QThreadPool::globalInstance()->setMaxThreadCount(1);
        QThreadPool::globalInstance()->setExpiryTimeout(1000);
        // Use the desktop's icon theme first. Breeze supplies missing names
        // on minimal sessions and offscreen checks; no bundled icon copies.
        auto iconPaths=QIcon::themeSearchPaths();
        for(const auto& data:QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)){
            const auto path=data+u"/icons"_s;
            if(QDir(path).exists()&&!iconPaths.contains(path))iconPaths.append(path);
        }
        QIcon::setThemeSearchPaths(iconPaths);
        QIcon::setFallbackThemeName(u"breeze"_s);
        setWindowTitle(u"Slayer Control"_s);setWindowIcon(QIcon::fromTheme(u"computer-laptop"_s));resize(1180,820);setMinimumSize(900,640);
        auto* root=new QWidget;root->setObjectName(u"slayerShell"_s);auto* outer=new QHBoxLayout(root);outer->setContentsMargins(0,0,0,0);outer->setSpacing(0);
        auto* sidebar=new QFrame;sidebar->setObjectName(u"sidebar"_s);sidebar->setFixedWidth(218);auto* side=new QVBoxLayout(sidebar);side->setContentsMargins(12,22,12,14);side->setSpacing(10);
        auto* brand=new QHBoxLayout;brand->setContentsMargins(9,0,0,12);brand->setSpacing(10);brand->addWidget(icon(u"computer-laptop"_s,30));
        auto* wordmark=new QVBoxLayout;wordmark->setSpacing(2);auto* name=label(u"Slayer Control"_s);heading(name,1);wordmark->addWidget(name);wordmark->addWidget(label(u"Cihaz ayarları"_s,true));brand->addLayout(wordmark);brand->addStretch();side->addLayout(brand);
        nav->setObjectName(u"navigation"_s);nav->setIconSize(QSize(20,20));nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);nav->setSpacing(0);nav->setFocusPolicy(Qt::StrongFocus);nav->setAccessibleName(u"Ayar bölümleri"_s);side->addWidget(nav,1);
        auto group=[this](const QString& text){auto* item=new QListWidgetItem(text,nav);item->setFlags(Qt::ItemIsEnabled);auto font=item->font();font.setPointSizeF(qMax(8.0,font.pointSizeF()-1));font.setWeight(QFont::DemiBold);item->setFont(font);item->setForeground(palette().color(QPalette::PlaceholderText));item->setSizeHint(QSize(0,30));item->setData(Qt::UserRole,-1);};
        auto destination=[this](const QString& text,const QString& symbol){const int index=navigationRows.size();navigationRows.append(nav->count());auto* item=new QListWidgetItem(QIcon::fromTheme(symbol),text,nav);item->setData(Qt::UserRole,index);item->setData(Qt::UserRole+1,symbol);item->setToolTip(text);};
        destination(u"Genel Bakış"_s,u"dashboard-show"_s);group(u"KİŞİSELLEŞTİRME"_s);destination(u"Klavye ve RGB"_s,u"input-keyboard"_s);destination(u"RGB Profilleri"_s,u"folder-favorites"_s);
        group(u"SİSTEM"_s);destination(u"Güç ve Pil"_s,u"battery"_s);destination(u"Performans ve Fan"_s,u"speedometer"_s);destination(u"Grafik ve MUX"_s,u"video-display"_s);destination(u"Aygıtlar ve USB"_s,u"drive-removable-media-usb"_s);destination(u"Otomasyon"_s,u"system-run"_s);
        group(u"ARAÇLAR"_s);destination(u"Tanılama"_s,u"utilities-system-monitor"_s);destination(u"Hakkında"_s,u"help-about"_s);
        auto* footer=new QVBoxLayout;footer->setContentsMargins(10,12,0,0);footer->setSpacing(5);heading(deviceName);footer->addWidget(deviceName);footer->addWidget(label(QStringLiteral(SLAYERCTL_VERSION),true));side->addLayout(footer);
        outer->addWidget(sidebar);auto* right=new QWidget;auto* body=new QVBoxLayout(right);body->setContentsMargins(0,0,0,0);body->setSpacing(0);
        auto* toolbar=new QFrame;toolbar->setObjectName(u"toolbar"_s);auto* bar=new QHBoxLayout(toolbar);bar->setContentsMargins(26,12,24,12);bar->setSpacing(14);
        breadcrumb->setWordWrap(false);breadcrumb->setMinimumWidth(0);breadcrumb->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);readTime->setWordWrap(false);
        bar->addWidget(breadcrumb,1);bar->addWidget(readTime);
        auto* refresh=button(u"Yenile"_s,[this]{backend.refresh();},{},u"view-refresh"_s);refresh->setToolTip(u"Bilgileri yenile · Ctrl+R"_s);bar->addWidget(refresh);body->addWidget(toolbar);body->addWidget(pages,1);
        noticeFrame->setObjectName(u"notice"_s);auto* message=new QHBoxLayout(noticeFrame);message->setContentsMargins(24,12,24,12);message->addWidget(icon(u"dialog-information"_s,20));message->addWidget(notice,1);noticeFrame->hide();body->addWidget(noticeFrame);outer->addWidget(right,1);setCentralWidget(root);
        overview();lighting();rgbProfiles();power();performance();graphics();devices();automation();diagnostics();about();
        connect(nav,&QListWidget::currentRowChanged,this,[this](int row){auto* item=nav->item(row);if(!item)return;const int index=item->data(Qt::UserRole).toInt();if(index<0||index>=PageCount)return;pages->setCurrentIndex(index);breadcrumb->setText(u"Slayer Control  /  "_s+titles[index]);});
        auto* refreshShortcut=new QShortcut(QKeySequence::Refresh,this);connect(refreshShortcut,&QShortcut::activated,&backend,&SlayerBackend::refresh);
        auto* ctrlR=new QShortcut(QKeySequence(u"Ctrl+R"_s),this);connect(ctrlR,&QShortcut::activated,&backend,&SlayerBackend::refresh);
        for(int i=0;i<PageCount;++i){auto* shortcut=new QShortcut(QKeySequence(u"Alt+"_s+QString::number((i+1)%10)),this);connect(shortcut,&QShortcut::activated,this,[this,i]{selectPage(i);});}
        // Initialize readers before the general refresh handler marks initial
        // controls as synchronized. Their values are never hardware writes.
        connect(&backend,&SlayerBackend::updated,this,[this]{updateState();});connect(&backend,&SlayerBackend::dataChanged,this,[this]{updateState();});
        theme();selectPage(0);updateState();
    }
};
int main(int argc,char** argv){
    if(argc>1&&QString::fromLocal8Bit(argv[1]).startsWith(u"--power-")){
        QCoreApplication app(argc,argv);return desktopPower(app.arguments().mid(1));
    }
    QApplication app(argc,argv);app.setApplicationName(u"Slayer Control"_s);app.setApplicationVersion(QStringLiteral(SLAYERCTL_VERSION));app.setDesktopFileName(u"slayer-control"_s);
    if(geteuid()==0){QTextStream(stderr)<<"Slayer Control normal kullanıcı oturumunda açılmalı.\n";return 3;}
    Window window;const QStringList names={u"overview"_s,u"keyboard"_s,u"profiles"_s,u"power"_s,u"performance"_s,u"graphics"_s,u"devices"_s,u"automation"_s,u"diagnostics"_s,u"about"_s};
    const auto args=app.arguments();
    if(args.size()==3&&args[1]==u"--page"){
        if(args[2]==u"hardware")window.showHardware();else{const int index=names.indexOf(args[2]);if(index<0)return 2;window.selectPage(index);}
    }else if(args.size()>1)return 2;
    window.show();return app.exec();
}
