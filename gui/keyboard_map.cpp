// SPDX-License-Identifier: MIT
#include "keyboard_map.hpp"
#include <QGuiApplication>
#include <QLocale>
#include <QTimer>
#include <QSettings>
#include <QStandardPaths>
#include <QtGui/qguiapplication_platform.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace Qt::Literals::StringLiterals;

DesktopKeyboard::DesktopKeyboard(QObject* parent):QObject(parent),context(xkb_context_new(XKB_CONTEXT_NO_FLAGS)){
    QSettings settings(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)+u"/slayerctl/interface.ini"_s,QSettings::IniFormat);
    const auto saved=settings.value(u"keyboard/format"_s).toString();
    if(saved==u"ansi")physicalFormat=saved;
    auto* app=qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    if(!app)return;
    connect(app,&QGuiApplication::applicationStateChanged,this,[this](Qt::ApplicationState){attach();});
    QTimer::singleShot(0,this,[this]{attach();});
}
DesktopKeyboard::~DesktopKeyboard(){
    if(observer){if(wl_keyboard_get_version(observer)>=3)wl_keyboard_release(observer);else wl_keyboard_destroy(observer);}
    if(map)xkb_keymap_unref(map);if(context)xkb_context_unref(context);
}
void DesktopKeyboard::replace(xkb_keymap* value,const QString& from){
    if(!value)return;if(map)xkb_keymap_unref(map);map=value;group=0;source=from;Q_EMIT changed();
}
bool DesktopKeyboard::loadNames(const QByteArray& layout,const QByteArray& variant,const QByteArray& options){
    if(!context)return false;
    xkb_rule_names names{};names.rules="evdev";names.model="pc105";names.layout=layout.constData();names.variant=variant.constData();names.options=options.constData();
    auto* value=xkb_keymap_new_from_names(context,&names,XKB_KEYMAP_COMPILE_NO_FLAGS);
    if(!value)return false;replace(value,u"XKB"_s);return true;
}
bool DesktopKeyboard::loadText(const char* data,size_t length){
    if(!context||!data||!length||length>4*1024*1024)return false;
    auto* value=xkb_keymap_new_from_buffer(context,data,length,XKB_KEYMAP_FORMAT_TEXT_V1,XKB_KEYMAP_COMPILE_NO_FLAGS);
    if(!value)return false;replace(value,u"Wayland"_s);return true;
}
void DesktopKeyboard::setGroup(xkb_layout_index_t value){
    if(!map||value>=xkb_keymap_num_layouts(map)||value==group)return;group=value;Q_EMIT changed();
}
void DesktopKeyboard::attach(){
#if QT_CONFIG(wayland)
    if(observer||!context||QGuiApplication::platformName()!=u"wayland")return;
    auto* native=qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if(!native||!native->seat()||!native->keyboard())return;
    // A second keyboard resource on Qt's existing seat inherits its event
    // queue. Observe only keymap/group metadata; key events are discarded.
    static const wl_keyboard_listener listener={
        [](void* ptr,wl_keyboard*,uint32_t format,int32_t fd,uint32_t size){
            struct stat info{};
            if(format!=WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1||size<2||size>4*1024*1024||fstat(fd,&info)||info.st_size<size){close(fd);return;}
            void* data=mmap(nullptr,size,PROT_READ,MAP_PRIVATE,fd,0);close(fd);
            if(data==MAP_FAILED)return;
            if(static_cast<const char*>(data)[size-1]=='\0')static_cast<DesktopKeyboard*>(ptr)->loadText(static_cast<const char*>(data),size-1);
            munmap(data,size);
        },
        [](void*,wl_keyboard*,uint32_t,wl_surface*,wl_array*){},
        [](void*,wl_keyboard*,uint32_t,wl_surface*){},
        [](void*,wl_keyboard*,uint32_t,uint32_t,uint32_t,uint32_t){},
        [](void* ptr,wl_keyboard*,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t layout){static_cast<DesktopKeyboard*>(ptr)->setGroup(layout);},
        [](void*,wl_keyboard*,int32_t,int32_t){}
    };
    observer=wl_seat_get_keyboard(native->seat());
    wl_keyboard_add_listener(observer,&listener,this);wl_display_flush(native->display());
#endif
}
QString DesktopKeyboard::name()const{
    if(!map)return {};const auto* value=xkb_keymap_layout_get_name(map,group);return value?QString::fromUtf8(value):QString();
}
QString DesktopKeyboard::symbol(xkb_keysym_t sym,bool uppercase)const{
    static const QHash<xkb_keysym_t,QString> special={{XKB_KEY_Escape,u"Esc"_s},{XKB_KEY_BackSpace,u"⌫"_s},{XKB_KEY_Tab,u"Tab"_s},{XKB_KEY_Return,u"Enter"_s},{XKB_KEY_KP_Enter,u"Enter"_s},{XKB_KEY_Caps_Lock,u"Caps"_s},{XKB_KEY_Shift_L,u"Shift"_s},{XKB_KEY_Shift_R,u"Shift"_s},{XKB_KEY_Control_L,u"Ctrl"_s},{XKB_KEY_Control_R,u"Ctrl"_s},{XKB_KEY_Alt_L,u"Alt"_s},{XKB_KEY_Alt_R,u"Alt"_s},{XKB_KEY_ISO_Level3_Shift,u"AltGr"_s},{XKB_KEY_Super_L,u"Meta"_s},{XKB_KEY_Super_R,u"Meta"_s},{XKB_KEY_Menu,u"Menü"_s},{XKB_KEY_space,u"Boşluk"_s},{XKB_KEY_Left,u"←"_s},{XKB_KEY_Right,u"→"_s},{XKB_KEY_Up,u"↑"_s},{XKB_KEY_Down,u"↓"_s},{XKB_KEY_Num_Lock,u"Num"_s},{XKB_KEY_Insert,u"Ins"_s},{XKB_KEY_Delete,u"Del"_s},{XKB_KEY_Print,u"PrtSc"_s},{XKB_KEY_Pause,u"Pause"_s},{XKB_KEY_Page_Up,u"PgUp"_s},{XKB_KEY_Page_Down,u"PgDn"_s},{XKB_KEY_End,u"End"_s},{XKB_KEY_dead_acute,u"´"_s},{XKB_KEY_dead_grave,u"`"_s},{XKB_KEY_dead_circumflex,u"^"_s},{XKB_KEY_dead_diaeresis,u"¨"_s},{XKB_KEY_dead_tilde,u"~"_s}};
    if(special.contains(sym))return special.value(sym);
    if(sym>=XKB_KEY_F1&&sym<=XKB_KEY_F35)return u"F"_s+QString::number(sym-XKB_KEY_F1+1);
    char buffer[64]{};if(xkb_keysym_to_utf8(sym,buffer,sizeof(buffer))<=1)return {};
    auto text=QString::fromUtf8(buffer);if(text.size()!=1||!text[0].isPrint())return {};
    if(uppercase){const auto locale=name().contains(u"Turkish",Qt::CaseInsensitive)?QLocale(u"tr"_s):QLocale::c();text=locale.toUpper(text);}
    return text;
}
QString DesktopKeyboard::keyName(const QString& id){
    static const QHash<QString,QString> direct={{u"Esc"_s,u"ESC"_s},{u"Grave"_s,u"TLDE"_s},{u"Minus"_s,u"AE11"_s},{u"Equal"_s,u"AE12"_s},{u"Backspace"_s,u"BKSP"_s},{u"Tab"_s,u"TAB"_s},{u"CapsLock"_s,u"CAPS"_s},{u"Enter"_s,u"RTRN"_s},{u"Comma"_s,u"BKSL"_s},{u"ShiftLeft"_s,u"LFSH"_s},{u"ISO"_s,u"LSGT"_s},{u"ShiftRight"_s,u"RTSH"_s},{u"CtrlLeft"_s,u"LCTL"_s},{u"CtrlRight"_s,u"RCTL"_s},{u"Meta"_s,u"LWIN"_s},{u"Alt"_s,u"LALT"_s},{u"AltGr"_s,u"RALT"_s},{u"Menu"_s,u"MENU"_s},{u"Space"_s,u"SPCE"_s},{u"Left"_s,u"LEFT"_s},{u"Right"_s,u"RGHT"_s},{u"Down"_s,u"DOWN"_s},{u"Up"_s,u"UP"_s},{u"Insert"_s,u"INS"_s},{u"Delete"_s,u"DELE"_s},{u"PrtSc"_s,u"PRSC"_s},{u"Pause"_s,u"PAUS"_s},{u"PageUp"_s,u"PGUP"_s},{u"PageDown"_s,u"PGDN"_s},{u"End"_s,u"END"_s},{u"NumLock"_s,u"NMLK"_s},{u"NumDivide"_s,u"KPDV"_s},{u"NumMultiply"_s,u"KPMU"_s},{u"NumSubtract"_s,u"KPSU"_s},{u"NumAdd"_s,u"KPAD"_s},{u"NumEnter"_s,u"KPEN"_s},{u"NumDot"_s,u"KPDL"_s}};
    if(direct.contains(id))return direct.value(id);
    static const QList<QStringList> rows={{u"1"_s,u"2"_s,u"3"_s,u"4"_s,u"5"_s,u"6"_s,u"7"_s,u"8"_s,u"9"_s,u"0"_s},{u"Q"_s,u"W"_s,u"E"_s,u"R"_s,u"T"_s,u"Y"_s,u"U"_s,u"I"_s,u"O"_s,u"P"_s,u"Gbreve"_s,u"Udia"_s},{u"A"_s,u"S"_s,u"D"_s,u"F"_s,u"G"_s,u"H"_s,u"J"_s,u"K"_s,u"L"_s,u"Scedilla"_s,u"Idot"_s},{u"Z"_s,u"X"_s,u"C"_s,u"V"_s,u"B"_s,u"N"_s,u"M"_s,u"Odia"_s,u"Ccedilla"_s,u"Period"_s}};
    static const QStringList names={u"AE"_s,u"AD"_s,u"AC"_s,u"AB"_s};
    for(int r=0;r<rows.size();++r){const int column=rows[r].indexOf(id);if(column>=0)return names[r]+QString::number(column+1).rightJustified(2,u'0');}
    if(id.startsWith(u"Num")&&id.size()==4&&id[3].isDigit())return u"KP"_s+id[3];
    if(id.startsWith(u'F')){bool okay=false;const int n=id.mid(1).toInt(&okay);if(okay&&n>=1&&n<=12)return u"FK"_s+QString::number(n).rightJustified(2,u'0');}
    return {};
}
QVariantMap DesktopKeyboard::shape(const QVariantMap& key)const{
    const auto id=key.value(u"id"_s).toString();const int row=key.value(u"row"_s).toInt();
    qreal x=key.value(u"column"_s).toDouble(),y=row,w=key.value(u"width"_s).toDouble(),h=1;
    QString kind;
    // R9T TR photo reference: compact function row, staggered ISO block,
    // single-width numpad zero and a lowered inverted-T arrow cluster.
    if(row==0){x=(x==0?0:x-1)*1.05;y=0;h=.65;w=1.05;}
    else {
        y=row-.05;
        if(x>=17){x=17.3+(x-17)*.925;w=.925;}
        else if(row==1){x*=1.12;w=1.12;if(id==u"Backspace")w=2.64;}
        else if(row==2){x=x>=2?1.55+(x-2)*1.12:0;w=id==u"Tab"?1.55:1.12;}
        else if(row==3){x=x>=2?1.83+(x-2)*1.12:0;w=id==u"CapsLock"?1.83:1.12;}
        else if(row==4){x=x>=1?1.25+(x-1)*1.12:0;w=id==u"ShiftLeft"?1.25:1.12;}
    }
    if(id==u"ShiftRight"){x=13.57;w=3.63;}
    else if(id==u"Enter"){x=15.02;y=1.95;w=2.18;h=2;kind=u"iso-enter"_s;}
    else if(id==u"NumAdd"||id==u"NumEnter")h=2;
    else if(id==u"Num0")w=1.85;
    if(row==5&&key.value(u"column"_s).toInt()<17){
        static const QHash<QString,QPair<qreal,qreal>> bottom={
            {u"CtrlLeft"_s,{0,1.2}},{u"Fn"_s,{1.2,1.12}},{u"Meta"_s,{2.32,1.12}},
            {u"Alt"_s,{3.44,1.12}},{u"Space"_s,{4.56,5.84}},{u"AltGr"_s,{10.4,1.12}},
            {u"Menu"_s,{11.52,2.0}},{u"Left"_s,{13.55,1.12}},
            {u"Down"_s,{14.67,1.12}},{u"Right"_s,{15.79,1.12}}};
        if(bottom.contains(id)){x=bottom[id].first;w=bottom[id].second;}
        if(id==u"Left"||id==u"Down"||id==u"Right")y=6.05;
    }
    if(id==u"Up"){x=14.67;y=4.95;w=1.12;}
    if(physicalFormat==u"ansi"){
        if(id==u"Enter"){x=14.15;y=2.95;w=3.05;h=1;kind.clear();}
        else if(id==u"Comma"){x=14.99;y=1.95;w=2.21;}
        else if(id==u"ShiftLeft")w=2.37;
    }
    return {{u"x"_s,x},{u"y"_s,y},{u"w"_s,w},{u"h"_s,h},{u"shape"_s,kind},{u"present"_s,!(physicalFormat==u"ansi"&&id==u"ISO")&&id!=u"CtrlRight"}};
}
QVariantMap DesktopKeyboard::decorate(QVariantMap key)const{
    if(!key.contains(u"fallbackLabel"_s))key.insert(u"fallbackLabel"_s,key.value(u"label"_s));
    key.insert(u"label"_s,key.value(u"fallbackLabel"_s));key.remove(u"shift"_s);key.remove(u"altgr"_s);
    const auto geometry=shape(key);for(auto it=geometry.begin();it!=geometry.end();++it)key.insert(it.key(),it.value());
    const auto id=key.value(u"id"_s).toString();const auto native=keyName(id);key.insert(u"xkb"_s,native);
    if(!map||native.isEmpty())return key;
    const auto code=xkb_keymap_key_by_name(map,native.toLatin1().constData());if(code==XKB_KEYCODE_INVALID)return key;
    key.insert(u"scan"_s,code);
    const auto layout=group<xkb_keymap_num_layouts_for_key(map,code)?group:0;
    for(unsigned level=0;level<3;++level){
        const xkb_keysym_t* syms=nullptr;const auto count=xkb_keymap_key_get_syms_by_level(map,code,layout,level,&syms);if(count!=1)continue;
        const auto text=symbol(syms[0],level==0);if(text.isEmpty())continue;
        key.insert(level==0?u"label"_s:level==1?u"shift"_s:u"altgr"_s,text);
    }
    return key;
}

bool DesktopKeyboard::setFormat(const QString& value,bool persist){
    if(value!=u"iso"&&value!=u"ansi")return false;
    if(value==physicalFormat)return true;physicalFormat=value;
    if(persist){QSettings settings(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)+u"/slayerctl/interface.ini"_s,QSettings::IniFormat);settings.setValue(u"keyboard/format"_s,value);}
    Q_EMIT changed();return true;
}
