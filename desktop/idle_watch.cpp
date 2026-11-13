// SPDX-License-Identifier: MIT
#include "idle_watch.hpp"
#include "ext-idle-notify-v1-client.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QSocketNotifier>
#include <wayland-client.h>
#include <X11/Xlib.h>
#include <X11/extensions/sync.h>
#include <cstring>

namespace {
constexpr auto mutter="org.gnome.Mutter.IdleMonitor";
constexpr auto mutterPath="/org/gnome/Mutter/IdleMonitor/Core";
}
struct IdleWatch::State {
    IdleWatch* owner;
    wl_display* display=nullptr;
    wl_registry* registry=nullptr;
    wl_seat* seat=nullptr;
    ext_idle_notifier_v1* manager=nullptr;
    ext_idle_notification_v1* notification=nullptr;
    QSocketNotifier* socket=nullptr;
    Display* xdisplay=nullptr;
    XSyncCounter counter=0;
    XSyncAlarm idleAlarm=0,activeAlarm=0;
    int xEvent=0;
    uint idleWatch=0,activeWatch=0;
    QString name;
    unsigned seconds=0;
    bool idle=false;
    void change(bool value){if(idle==value)return;idle=value;Q_EMIT owner->idleChanged(value);}
    void xArm(){
        XSyncAlarmAttributes attr{};
        attr.trigger.counter=counter;
        attr.trigger.value_type=XSyncAbsolute;
        XSyncIntToValue(&attr.trigger.wait_value,int(seconds*1000));
        attr.trigger.test_type=XSyncPositiveTransition;
        XSyncIntToValue(&attr.delta,0);attr.events=True;
        const unsigned mask=XSyncCACounter|XSyncCAValueType|XSyncCAValue|XSyncCATestType|XSyncCADelta|XSyncCAEvents;
        if(idleAlarm)XSyncChangeAlarm(xdisplay,idleAlarm,mask,&attr);else idleAlarm=XSyncCreateAlarm(xdisplay,mask,&attr);
        attr.trigger.test_type=XSyncNegativeTransition;
        if(activeAlarm)XSyncChangeAlarm(xdisplay,activeAlarm,mask,&attr);else activeAlarm=XSyncCreateAlarm(xdisplay,mask,&attr);
        XFlush(xdisplay);
    }
};
IdleWatch::IdleWatch(QObject* parent):QObject(parent),state(std::make_unique<State>()){state->owner=this;}
IdleWatch::~IdleWatch(){stop();}
QString IdleWatch::backend()const{return state->name;}
void IdleWatch::stop(){
    auto& s=*state;
    if(s.socket){delete s.socket;s.socket=nullptr;}
    if(s.notification){ext_idle_notification_v1_destroy(s.notification);s.notification=nullptr;}
    if(s.manager){ext_idle_notifier_v1_destroy(s.manager);s.manager=nullptr;}
    if(s.seat){wl_seat_destroy(s.seat);s.seat=nullptr;}
    if(s.registry){wl_registry_destroy(s.registry);s.registry=nullptr;}
    if(s.display){wl_display_flush(s.display);wl_display_disconnect(s.display);s.display=nullptr;}
    if(s.xdisplay){if(s.idleAlarm)XSyncDestroyAlarm(s.xdisplay,s.idleAlarm);if(s.activeAlarm)XSyncDestroyAlarm(s.xdisplay,s.activeAlarm);XCloseDisplay(s.xdisplay);s.xdisplay=nullptr;s.idleAlarm=s.activeAlarm=0;}
    if(s.idleWatch||s.activeWatch){
        QDBusInterface iface(mutter,mutterPath,mutter,QDBusConnection::sessionBus());iface.setTimeout(1000);
        if(s.idleWatch)iface.call("RemoveWatch",s.idleWatch);
        if(s.activeWatch)iface.call("RemoveWatch",s.activeWatch);
        QDBusConnection::sessionBus().disconnect(mutter,mutterPath,mutter,"WatchFired",this,SLOT(gnomeWatchFired(uint)));
        s.idleWatch=s.activeWatch=0;
    }
    s.idle=false;s.name.clear();
}
bool IdleWatch::start(unsigned seconds){
    stop();auto& s=*state;s.seconds=seconds;
    if(qEnvironmentVariableIsSet("WAYLAND_DISPLAY")){
        s.display=wl_display_connect(nullptr);
        if(s.display){
            static const wl_registry_listener globals={
                [](void* data,wl_registry* registry,uint32_t id,const char* iface,uint32_t version){
                    auto& st=*static_cast<State*>(data);
                    if(!std::strcmp(iface,"wl_seat")&&!st.seat)st.seat=static_cast<wl_seat*>(wl_registry_bind(registry,id,&wl_seat_interface,1));
                    else if(!std::strcmp(iface,"ext_idle_notifier_v1"))st.manager=static_cast<ext_idle_notifier_v1*>(wl_registry_bind(registry,id,&ext_idle_notifier_v1_interface,qMin(version,2u)));
                },[](void*,wl_registry*,uint32_t){}
            };
            s.registry=wl_display_get_registry(s.display);wl_registry_add_listener(s.registry,&globals,&s);
            if(wl_display_roundtrip(s.display)>=0&&s.seat&&s.manager){
                s.notification=ext_idle_notifier_v1_get_version(s.manager)>=2?
                    ext_idle_notifier_v1_get_input_idle_notification(s.manager,seconds*1000,s.seat):
                    ext_idle_notifier_v1_get_idle_notification(s.manager,seconds*1000,s.seat);
                static const ext_idle_notification_v1_listener events={
                    [](void* data,ext_idle_notification_v1*){static_cast<State*>(data)->change(true);},
                    [](void* data,ext_idle_notification_v1*){static_cast<State*>(data)->change(false);}
                };
                ext_idle_notification_v1_add_listener(s.notification,&events,&s);wl_display_flush(s.display);
                s.socket=new QSocketNotifier(wl_display_get_fd(s.display),QSocketNotifier::Read,this);
                connect(s.socket,&QSocketNotifier::activated,this,[this]{auto& st=*state;if(wl_display_dispatch(st.display)<0){stop();Q_EMIT unavailable(tr("Wayland oturumu kapandı."));}else wl_display_flush(st.display);});
                s.name="wayland-ext-idle-notify";return true;
            }
            stop();s.seconds=seconds;
        }
    }
    QDBusInterface iface(mutter,mutterPath,mutter,QDBusConnection::sessionBus());iface.setTimeout(1000);
    if(iface.isValid()){
        QDBusReply<uint> reply=iface.call("AddIdleWatch",QVariant::fromValue<qulonglong>(seconds*1000));
        if(reply.isValid()){
            s.idleWatch=reply.value();s.name="gnome-mutter-idle-monitor";
            QDBusConnection::sessionBus().connect(mutter,mutterPath,mutter,"WatchFired",this,SLOT(gnomeWatchFired(uint)));return true;
        }
    }
    // XWayland's counter does not represent activity in native Wayland apps.
    if(!qEnvironmentVariableIsSet("WAYLAND_DISPLAY")&&qEnvironmentVariableIsSet("DISPLAY")){
        s.xdisplay=XOpenDisplay(nullptr);int error=0,major=0,minor=0;
        if(s.xdisplay&&XSyncQueryExtension(s.xdisplay,&s.xEvent,&error)&&XSyncInitialize(s.xdisplay,&major,&minor)){
            int count=0;auto* counters=XSyncListSystemCounters(s.xdisplay,&count);
            for(int i=0;i<count;++i)if(!std::strcmp(counters[i].name,"IDLETIME"))s.counter=counters[i].counter;
            if(counters)XSyncFreeSystemCounterList(counters);
            if(s.counter){
                s.xArm();s.socket=new QSocketNotifier(ConnectionNumber(s.xdisplay),QSocketNotifier::Read,this);
                connect(s.socket,&QSocketNotifier::activated,this,[this]{auto& st=*state;while(XPending(st.xdisplay)){XEvent event;XNextEvent(st.xdisplay,&event);if(event.type!=st.xEvent+XSyncAlarmNotify)continue;const auto* alarm=reinterpret_cast<XSyncAlarmNotifyEvent*>(&event);if(alarm->alarm==st.idleAlarm)st.change(true);else if(alarm->alarm==st.activeAlarm){st.change(false);st.xArm();}}});
                s.name="x11-xsync-idletime";return true;
            }
        }
    }
    stop();return false;
}
void IdleWatch::gnomeWatchFired(uint id){
    auto& s=*state;
    if(id==s.idleWatch){
        s.change(true);QDBusInterface iface(mutter,mutterPath,mutter,QDBusConnection::sessionBus());iface.setTimeout(1000);
        QDBusReply<uint> reply=iface.call("AddUserActiveWatch");if(reply.isValid())s.activeWatch=reply.value();else{stop();Q_EMIT unavailable(tr("GNOME etkinlik bildirimi alınamadı."));}
    }else if(id==s.activeWatch){s.activeWatch=0;s.change(false);}
}
