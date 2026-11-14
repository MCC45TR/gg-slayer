// SPDX-License-Identifier: MIT
#pragma once
#include <QElapsedTimer>
#include <QTimer>
#include <QString>
#include <functional>
#include <vector>
#include "../src/idle_curve.hpp"

// Owns a transient driver scale, never the LED class brightness or RGB frame.
// There is no timer or hardware traffic while the animation is settled.
class IdleDimmer final: public QObject {
    QTimer timer;
    QElapsedTimer elapsed;
    int fd=-1,current=1000;
    slayer::IdleTransition transition;
    std::vector<std::function<void(bool)>> restored;
    void complete(bool);
    bool writeScale(int);
public:
    explicit IdleDimmer(QObject* parent=nullptr);
    ~IdleDimmer() override;
    bool available() const;
    bool fade(bool dim);
    void restore(std::function<void(bool)> ready);
    void reset();
    int scale() const { return current; }
};
