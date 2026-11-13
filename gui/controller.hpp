// SPDX-License-Identifier: MIT
#pragma once
#include "backend.hpp"
#include <QDBusConnection>
#include <QVariantMap>
#include <QFileSystemWatcher>
#include <QQueue>
class Controller:public QObject {
    Q_OBJECT
    Q_PROPERTY(bool preview READ preview CONSTANT)
    Q_PROPERTY(QVariantMap metrics READ metrics NOTIFY metricsChanged)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString output READ output NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString powerProfile READ powerProfile NOTIFY changed)
    Q_PROPERTY(QStringList powerChoices READ powerChoices NOTIFY changed)
    Q_PROPERTY(bool powerAvailable READ powerAvailable NOTIFY changed)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY changed)
    Q_PROPERTY(bool monitoring READ monitoring WRITE setMonitoring NOTIFY changed)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY changed)
    struct Pending{QStringList command,remember;};
    SlayerBackend* device;
    bool foreground=false;
    bool fixture=false,working=false,visible=false,sampling=false,reduce=false,ppAvailable=false;
    QString lastError,diagnostic,power;
    QStringList ppChoices;
    QVariantMap values;
    QTimer timer;
    QFileSystemWatcher paletteWatcher;
    QQueue<Pending> queue;
    Pending inFlight;
    quint64 previousTotal=0,previousIdle=0;
    void drain();
    void telemetry();
    void readPower();
    void followAccent();
public:
    Controller(SlayerBackend*,bool fixtureMode,QObject* parent=nullptr);
    bool preview()const{return fixture;}
    QVariantMap metrics()const{return values;}
    QString error()const{return lastError;}
    QString output()const{return diagnostic;}
    bool busy()const{return working;}
    QString powerProfile()const{return power;}
    QStringList powerChoices()const{return ppChoices;}
    bool powerAvailable()const{return ppAvailable;}
    bool active()const{return foreground;}
    void setActive(bool);
    bool monitoring()const{return visible;}
    bool reducedMotion()const{return reduce;}
    void setMonitoring(bool);
    void setReducedMotion(bool);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void apply(const QStringList& command,const QStringList& remember={});
    Q_INVOKABLE void setPower(const QString& value);
    Q_INVOKABLE void reboot();
    Q_INVOKABLE void diagnose(const QString& topic);
    Q_INVOKABLE void clearError(){lastError.clear();Q_EMIT changed();}
private Q_SLOTS:
    void stateChanged(){if(foreground)refresh();}
    void powerChanged(const QString&,const QVariantMap&,const QStringList&);
Q_SIGNALS:
    void changed();
    void metricsChanged();
    void succeeded();
};
