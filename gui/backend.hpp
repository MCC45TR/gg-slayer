// SPDX-License-Identifier: MIT
#pragma once
#include <QObject>
#include <QProcess>
#include <QVariant>
#include <QTimer>
#include "keyboard_map.hpp"

class SlayerBackend final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap info READ info NOTIFY dataChanged)
    Q_PROPERTY(QVariantList capabilities READ capabilities NOTIFY dataChanged)
    Q_PROPERTY(QVariantList keys READ keys NOTIFY dataChanged)
    Q_PROPERTY(QStringList rgbProfiles READ rgbProfiles NOTIFY dataChanged)
    Q_PROPERTY(QString output READ output NOTIFY updated)
    Q_PROPERTY(QString message READ message NOTIFY updated)
    Q_PROPERTY(bool busy READ busy NOTIFY updated)
    QVariantMap m_info;
    QVariantList m_caps,m_keys;
    QStringList m_profiles;
    QString m_output,m_message;
    DesktopKeyboard *m_keyboard;
    void updateKeyboard();
    QProcess *m_process;
    QTimer *m_readTimeout;
    bool m_preview=false;
    bool m_timedOut=false;
    bool m_busy=false,m_refresh=false,m_mutating=false;
    void start(const QString&,const QStringList&,bool refresh);
public:
    explicit SlayerBackend(QObject* parent=nullptr,bool autoRead=true);
    QVariantMap info()const{return m_info;}
    QVariantList capabilities()const{return m_caps;}
    QVariantList keys()const{return m_keys;}
    QStringList rgbProfiles()const{return m_profiles;}
    QString output()const{return m_output;}
    QString message()const{return m_message;}
    bool busy()const{return m_busy;}
    void setPreviewMode(bool value){m_preview=value;if(value)m_keyboard->loadNames("tr");}
    void acceptData(const QString& text);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setKeyboardFormat(const QString& value){m_keyboard->setFormat(value,!m_preview);}
    Q_INVOKABLE void openCalibration();
    Q_INVOKABLE void openColorCalibration();
    Q_INVOKABLE void openSettings(const QString& module);
    Q_INVOKABLE void run(const QStringList&args,bool privileged=false);
    Q_INVOKABLE bool ready(const QString&id)const;
    Q_INVOKABLE QString reason(const QString&id)const;
Q_SIGNALS:
    void updated();
    void dataChanged();
};
