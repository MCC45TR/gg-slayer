// SPDX-License-Identifier: MIT
#pragma once
#include <QObject>
#include <QTimer>
#include <QString>
#include <memory>
class QSocketNotifier;

// Only a root-only timestamp from the exact-model driver. Never opens evdev,
// reads key codes, grabs a device or accepts a client-provided device path.
class InternalKeyboard final:public QObject {
    Q_OBJECT
    QTimer deadline;
    std::unique_ptr<QSocketNotifier> notifier;
    int fd=-1;
    unsigned seconds=30;
    bool running=false,dimmed=false;
    QString lastError;
    bool write(const char*,bool);
    qint64 lastActivity();
    void evaluate();
    void setIdle(bool);
    void fail(const QString&);
public:
    explicit InternalKeyboard(QObject* parent=nullptr);
    ~InternalKeyboard() override;
    bool available() const;
    void configure(bool enabled,unsigned timeout);
    void stop();
    bool active()const{return running;}
    bool idle()const{return dimmed;}
    QString error()const{return lastError;}
Q_SIGNALS:
    void idleChanged(bool);
    void statusChanged();
};
