// SPDX-License-Identifier: MIT
#include "../service/command_queue.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <algorithm>
#include <iostream>
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);CommandQueue queue;
    std::atomic<int> active=0,peak=0;int completed=0,ticks=0;QList<int> order;
    QTimer heartbeat;heartbeat.setInterval(5);QObject::connect(&heartbeat,&QTimer::timeout,&app,[&]{++ticks;});heartbeat.start();
    const auto mainThread=QThread::currentThread();bool callbackThread=true;
    for(int i=0;i<16;++i){
        if(!queue.run([&,i]{const int running=++active;peak.store(std::max(peak.load(),running));QThread::msleep(20);--active;return CommandResult{true,QString::number(i)};},[&](CommandResult result){callbackThread&=QThread::currentThread()==mainThread;order.append(result.output.toInt());++completed;}))return 1;
    }
    if(queue.run([]{return CommandResult{true,{}};},[](CommandResult){}))return 2;
    QElapsedTimer clock;clock.start();
    while(completed<16&&clock.elapsed()<4000){QCoreApplication::processEvents(QEventLoop::AllEvents,10);QThread::msleep(1);}
    if(completed!=16||peak!=1||!callbackThread||ticks<10||queue.pending())return 3;
    for(int i=0;i<16;++i)if(order[i]!=i)return 4;
    bool handled=false;
    if(!queue.run([]()->CommandResult{throw std::runtime_error("fixture failure");},[&](CommandResult result){handled=!result.ok&&result.output.contains("fixture failure");}))return 5;
    clock.restart();while(!handled&&clock.elapsed()<2000){QCoreApplication::processEvents(QEventLoop::AllEvents,10);QThread::msleep(1);}
    if(!handled)return 6;
    std::cout<<"Slow commands stay serial and bounded while Qt timers run; exceptions return to the owner thread.\n";
}
