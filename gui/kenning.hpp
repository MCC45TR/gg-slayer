// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QVariantMap>

namespace SlayerKenning {
// Explicit paths keep read-only fixture checks separate from the live system.
struct Paths {
    QString engine, module, launcher, config;
    QString videoClass = QStringLiteral("/sys/class/video4linux");
};
bool knownIrCamera(const QString& node, const QString& videoClass);
QVariantMap inspect(const Paths& paths);
}

// Frontend-only adapter. Never connects to the recognition socket or opens a camera.
class KenningIntegration final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    bool m_preview;
    QVariantMap m_state;
    QString m_error;
    static SlayerKenning::Paths installedPaths();
public:
    explicit KenningIntegration(bool preview, QObject* parent = nullptr)
        : QObject(parent), m_preview(preview) {}
    QVariantMap state() const { return m_state; }
    QString error() const { return m_error; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void openSettings();
Q_SIGNALS:
    void changed();
};
