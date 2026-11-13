// SPDX-License-Identifier: MIT
#pragma once
#include <QObject>
#include <QString>
#include <memory>
class IdleWatch final: public QObject {
    Q_OBJECT
    struct State;
    std::unique_ptr<State> state;
public:
    explicit IdleWatch(QObject* parent=nullptr);
    ~IdleWatch() override;
    bool start(unsigned seconds);
    void stop();
    QString backend() const;
Q_SIGNALS:
    void idleChanged(bool idle);
    void unavailable(const QString& reason);
private Q_SLOTS:
    void gnomeWatchFired(uint id);
};
