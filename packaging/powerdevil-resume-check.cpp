// SPDX-License-Identifier: MIT
// Run the actual upstream/patched Core methods against deterministic dependencies.
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QVector>
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <cstdlib>

class KIdleTime {
public:
    int activities=0;
    static KIdleTime* instance(){static KIdleTime instance;return &instance;}
    void simulateUserActivity(){++activities;}
};
namespace PowerDevil {
class SuspendController {
public:
    enum WakeupSource { Unknown=0,Timer=1,Telephony=2,Network=4,Button=8 };
    unsigned wakeup=Button;
    unsigned lastWakeupType()const{return wakeup;}
};
class BatteryController {
public:
    int updates=0;
    void updateAcAdapterState(){++updates;}
};
class Action {
public:
    virtual ~Action()=default;
    virtual void onWakeupFromIdle()=0;
};
class KeyboardAction final:public Action {
public:
    int value=0,saved=42,calls=0,writes=0;
    void onWakeupFromIdle()override{++calls;if(saved>0){value=saved;++writes;}}
};
class Core final:public QObject {
public:
    std::unique_ptr<BatteryController> m_batteryController=std::make_unique<BatteryController>();
    std::unique_ptr<SuspendController> m_suspendController=std::make_unique<SuspendController>();
    QVector<Action*> m_pendingResumeFromIdleActions;
    int notifications=0;
    int currentChargePercent()const{return 70;}
    void emitBatteryChargePercentNotification(int,int){++notifications;}
    void onResumeFromSuspend();
    void onResumingFromIdle();
};
#include "resume-methods.inc"
}
static void require(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
static void dispatch(){QCoreApplication::processEvents(QEventLoop::AllEvents);}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    using namespace PowerDevil;
    {
        Core core;KeyboardAction keyboard;core.m_pendingResumeFromIdleActions.append(&keyboard);
        core.onResumeFromSuspend();
        require(core.notifications==1&&core.m_batteryController->updates==1,"Existing resume battery update lost");
        require(keyboard.calls==0,"Brightness restored before action resume slots completed");
        dispatch();
        require(keyboard.value==42&&keyboard.writes==1,"Interactive wake did not restore remembered keyboard brightness");
        require(core.m_pendingResumeFromIdleActions.isEmpty(),"Restored action retained in pending set");
        core.onResumeFromSuspend();dispatch();
        require(keyboard.writes==1,"Completed resume caused duplicate hardware work");
    }
    for(unsigned wakeup:{1u,2u,4u,3u,5u,6u,7u}){
        Core core;KeyboardAction keyboard;core.m_pendingResumeFromIdleActions.append(&keyboard);
        core.m_suspendController->wakeup=wakeup;core.onResumeFromSuspend();dispatch();
        require(keyboard.calls==0&&keyboard.value==0,"Background wake lit keyboard");
        require(core.m_pendingResumeFromIdleActions.size()==1,"Background wake discarded restore state");
        core.m_suspendController->wakeup=SuspendController::Button;core.onResumeFromSuspend();dispatch();
        require(keyboard.value==42&&keyboard.writes==1,"Later human wake lost saved brightness");
    }
    {
        Core core;KeyboardAction keyboard;keyboard.saved=0;core.m_pendingResumeFromIdleActions.append(&keyboard);
        core.onResumeFromSuspend();dispatch();require(keyboard.value==0&&keyboard.writes==0,"Explicit off was changed");
    }
    {
        Core core;KeyboardAction keyboard;core.m_pendingResumeFromIdleActions.append(&keyboard);
        core.onResumeFromSuspend();core.onResumingFromIdle();dispatch();
        require(keyboard.calls==1&&keyboard.writes==1,"Real idle event raced queued restore");
    }
    {
        KeyboardAction keyboard;
        {Core core;core.m_pendingResumeFromIdleActions.append(&keyboard);core.onResumeFromSuspend();}
        dispatch();require(keyboard.calls==0,"Queued restore outlived service");
    }
    std::cout<<"Actual PowerDevil Core methods: interactive/background wake, deferred restore, explicit off, duplicate/racing events and lifetime passed.\n";
}
