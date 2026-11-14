// SPDX-License-Identifier: MIT
#pragma once
#include "command.hpp"
#include <QObject>
#include <QThreadPool>
#include <functional>
#include <utility>
#include <exception>

// Core commands redirect process-wide C++ streams. Run them serially, away
// from the Qt event loop that owns keyboard notifications and brightness.
class CommandQueue final:public QObject {
    QThreadPool pool;
    unsigned count=0;
public:
    CommandQueue(){pool.setMaxThreadCount(1);pool.setExpiryTimeout(1000);}
    ~CommandQueue()override{stop();}
    unsigned pending()const{return count;}
    void stop(){pool.clear();pool.waitForDone();}
    bool run(std::function<CommandResult()> work,std::function<void(CommandResult)> done){
        if(count>=16)return false;
        ++count;
        pool.start([this,work=std::move(work),done=std::move(done)]()mutable{
            CommandResult result;
            try{result=work();}
            catch(const std::exception& e){result={false,QString::fromUtf8(e.what())};}
            catch(...){result={false,QStringLiteral("Komut yürütülemedi.")};}
            QMetaObject::invokeMethod(this,[this,done=std::move(done),result=std::move(result)]()mutable{
                --count;done(std::move(result));
            },Qt::QueuedConnection);
        });
        return true;
    }
};
