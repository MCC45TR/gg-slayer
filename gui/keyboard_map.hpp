// SPDX-License-Identifier: MIT
#pragma once
#include <QObject>
#include <QVariantMap>
#include <xkbcommon/xkbcommon.h>
struct wl_keyboard;
class DesktopKeyboard final:public QObject {
    Q_OBJECT
    xkb_context* context=nullptr;
    xkb_keymap* map=nullptr;
    wl_keyboard* observer=nullptr;
    xkb_layout_index_t group=0;
    QString source;
    QString physicalFormat=QStringLiteral("iso");
    void attach();
    void replace(xkb_keymap*,const QString&);
    QString symbol(xkb_keysym_t,bool uppercase=false)const;
public:
    explicit DesktopKeyboard(QObject* parent=nullptr);
    ~DesktopKeyboard()override;
    bool loadNames(const QByteArray& layout,const QByteArray& variant={},const QByteArray& options={});
    bool loadText(const char* data,size_t length);
    void setGroup(xkb_layout_index_t);
    QString name()const;
    QString origin()const{return source;}
    QVariantMap decorate(QVariantMap)const;
    static QString keyName(const QString& id);
    QVariantMap shape(const QVariantMap& key)const;
    QString format()const{return physicalFormat;}
    bool setFormat(const QString&,bool persist=true);
Q_SIGNALS:
    void changed();
};
