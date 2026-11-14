// SPDX-License-Identifier: MIT
#pragma once
#include <QObject>
#include <QDBusConnection>
#include <QProcess>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

struct sd_device_monitor;
struct sd_event;
class QSocketNotifier;

// One instance per QML engine: compact/full views and multiple applets share
// one event monitor. No persistent helper, broker subscription or polling.
class CameraControl final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int state READ state NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool eventMonitoring READ eventMonitoring NOTIFY changed)
public:
    explicit CameraControl(QObject* parent=nullptr);
    CameraControl(QString program,QDBusConnection bus,bool monitor,QObject* parent=nullptr);
    ~CameraControl() override;
    int state()const{return m_state;}
    bool available()const{return m_state>=0;}
    bool busy()const{return m_reading||m_applying;}
    QString error()const{return m_error;}
    bool eventMonitoring()const{return m_monitoring;}
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setEnabled(bool enabled);
Q_SIGNALS:
    void changed();
private:
    void scheduleRead();
    void read();
    void finishRead(bool ok);
    bool startMonitor();
    QString m_program;
    QDBusConnection m_bus;
    QProcess m_reader;
    QTimer m_deadline,m_debounce;
    QByteArray m_output;
    int m_state=-1;
    bool m_reading=false,m_applying=false,m_pending=false,m_monitoring=false;
    QString m_error;
    sd_device_monitor* m_monitor=nullptr;
    sd_event* m_event=nullptr;
    QSocketNotifier* m_notifier=nullptr;
};
