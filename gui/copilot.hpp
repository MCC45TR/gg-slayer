// SPDX-License-Identifier: MIT
#pragma once
#include <QObject>
#include <QVariantMap>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QKeySequence>
#include <functional>

namespace SlayerShortcut {
// KGlobalAccel v2 wire format: a list of key sequences, each a struct of ai.
struct Sequence {QList<int> keys;bool operator==(const Sequence&)const=default;};
using Sequences=QList<Sequence>;
QDBusArgument& operator<<(QDBusArgument&,const Sequence&);
const QDBusArgument& operator>>(const QDBusArgument&,Sequence&);
QString desktopPath();
}
Q_DECLARE_METATYPE(SlayerShortcut::Sequence)
Q_DECLARE_METATYPE(SlayerShortcut::Sequences)

// Configuration only. KWin launches the desktop entry after this UI exits.
class CopilotIntegration final:public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
    Q_PROPERTY(QVariantList applications READ applications NOTIFY applicationsChanged)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    bool preview,working=false;
    QVariantMap saved;
    QVariantList apps;
    QString problem;
    void call(const QString&,const QVariantList&,std::function<void(QDBusMessage)>);
    void finish(const QString& error={});
public:
    explicit CopilotIntegration(bool fixture=false,QObject* parent=nullptr);
    QVariantMap state()const{return saved;}
    QVariantList applications()const{return apps;}
    QString error()const{return problem;}
    bool busy()const{return working;}
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadApplications();
    Q_INVOKABLE void save(const QString& action,const QString& target,const QKeySequence& sequence);
Q_SIGNALS:
    void changed();
    void applicationsChanged();
};
