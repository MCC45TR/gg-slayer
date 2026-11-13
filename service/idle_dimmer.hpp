// SPDX-License-Identifier: MIT
#pragma once
#include <QElapsedTimer>
#include <QTimer>
#include <QString>

// Owns a transient driver scale, never the LED class brightness or RGB frame.
// There is no timer or hardware traffic while the animation is settled.
class IdleDimmer final: public QObject {
    QTimer timer;
    QElapsedTimer elapsed;
    int fd=-1, current=1000, from=1000, target=1000;
    bool writeScale(int);
public:
    explicit IdleDimmer(QObject* parent=nullptr);
    ~IdleDimmer() override;
    bool available() const;
    bool fade(bool dim);
    void reset();
    int scale() const { return current; }
};
