// SPDX-License-Identifier: MIT
#pragma once
#include <KQuickConfigModule>
#include "backend.hpp"
#include <QProcess>
#include <QVariant>

class SlayerModule final : public KQuickConfigModule {
    Q_OBJECT
    Q_PROPERTY(QVariantMap info READ info NOTIFY dataChanged)
    Q_PROPERTY(QVariantList capabilities READ capabilities NOTIFY dataChanged)
    Q_PROPERTY(QVariantList keys READ keys NOTIFY dataChanged)
    Q_PROPERTY(QStringList rgbProfiles READ rgbProfiles NOTIFY dataChanged)
    Q_PROPERTY(QString output READ output NOTIFY updated)
    Q_PROPERTY(QString message READ message NOTIFY updated)
    Q_PROPERTY(bool busy READ busy NOTIFY updated)
    SlayerBackend m_backend;
public:
    SlayerModule(QObject*,const KPluginMetaData&,const QVariantList&);
    QVariantMap info()const{return m_backend.info();}
    QVariantList capabilities()const{return m_backend.capabilities();}
    QVariantList keys()const{return m_backend.keys();}
    QStringList rgbProfiles()const{return m_backend.rgbProfiles();}
    QString output()const{return m_backend.output();}
    QString message()const{return m_backend.message();}
    bool busy()const{return m_backend.busy();}
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setKeyboardFormat(const QString& value){m_backend.setKeyboardFormat(value);}
    Q_INVOKABLE void openSettings(const QString& module);
    Q_INVOKABLE void run(const QStringList&args,bool privileged=false);
    Q_INVOKABLE bool ready(const QString&id)const;
    Q_INVOKABLE QString reason(const QString&id)const;
Q_SIGNALS:
    void updated();
    void dataChanged();
};
