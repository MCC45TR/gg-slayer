// SPDX-License-Identifier: MIT
#include "kenning.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QStandardPaths>
#include <sys/stat.h>

namespace {
QByteArray smallFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.read(256).trimmed();
}

QHash<QString, QString> engineConfig(const QByteArray& text) {
    QHash<QString, QString> values;
    QString section;
    // Kenning's INI format uses literal, case-sensitive values, without unquoting
    // or inline comments. Do not interpret it as Qt's extended QSettings format.
    for (const auto& raw : text.split('\n')) {
        const auto line = QString::fromUtf8(raw).trimmed();
        if (line.isEmpty() || line.startsWith(u'#') || line.startsWith(u';')) continue;
        if (line.startsWith(u'[') && line.endsWith(u']')) {
            section = line.mid(1, line.size() - 2);
            continue;
        }
        const auto equals = line.indexOf(u'=');
        if (equals < 0) continue;
        const auto key = section + u'/' + line.left(equals).trimmed();
        if (key == u"core/disabled" || key == u"video/colour" || key == u"video/device_path")
            values.insert(key, line.mid(equals + 1).trimmed());
    }
    return values;
}
}

bool SlayerKenning::knownIrCamera(const QString& node, const QString& videoClass) {
    if (!node.startsWith(u"video") || node.size() <= 5) return false;
    for (const auto c : node.mid(5)) if (c < u'0' || c > u'9') return false;
    const QDir entry(QDir(videoClass).filePath(node));
    if (smallFile(entry.filePath(QStringLiteral("name"))) != "FHD WebCam: IR Camera") return false;
    const auto interfacePath = QFileInfo(entry.filePath(QStringLiteral("device"))).canonicalFilePath();
    if (interfacePath.isEmpty()) return false;
    const QDir interface(interfacePath);
    const QDir usb(QFileInfo(interfacePath).absolutePath());
    // Same fixed R9T IR identity used by the existing slayer-face-auth tool.
    return smallFile(interface.filePath(QStringLiteral("bInterfaceNumber"))) == "02"
        && smallFile(usb.filePath(QStringLiteral("idVendor"))) == "2b7e"
        && smallFile(usb.filePath(QStringLiteral("idProduct"))) == "c906"
        && smallFile(usb.filePath(QStringLiteral("bcdDevice"))) == "0004"
        && smallFile(usb.filePath(QStringLiteral("removable"))) == "fixed";
}

QVariantMap SlayerKenning::inspect(const Paths& paths) {
    const QFileInfo engine(paths.engine), launcher(paths.launcher);
    const bool installed = engine.isFile() && engine.isExecutable();
    QVariantMap state{{QStringLiteral("installed"), installed},
        {QStringLiteral("settingsAvailable"), installed && QFileInfo(paths.module).isFile() && launcher.isFile() && launcher.isExecutable()},
        {QStringLiteral("configState"), QStringLiteral("unavailable")},
        {QStringLiteral("engineState"), QStringLiteral("unknown")},
        {QStringLiteral("cameraState"), QStringLiteral("unknown")},
        {QStringLiteral("devicePath"), QString()}};
    if (!installed) return state;
    constexpr qint64 maxConfigBytes = 65536;
    const QFileInfo configInfo(paths.config);
    QFile config(paths.config);
    if (!configInfo.isFile() || configInfo.size() > maxConfigBytes || !config.open(QIODevice::ReadOnly)) return state;
    const auto bytes = config.read(maxConfigBytes + 1);
    if (bytes.size() > maxConfigBytes || config.error() != QFileDevice::NoError) return state;
    const auto values = engineConfig(bytes);
    const auto disabled = values.value(QStringLiteral("core/disabled"));
    const auto colour = values.value(QStringLiteral("video/colour"));
    const auto device = values.value(QStringLiteral("video/device_path"));
    state[QStringLiteral("configState")] = QStringLiteral("readable");
    if (disabled == u"true" || disabled == u"false")
        state[QStringLiteral("engineState")] = disabled == u"true" ? QStringLiteral("disabled") : QStringLiteral("enabled");
    if (device.isEmpty() || device == u"none") {
        state[QStringLiteral("cameraState")] = QStringLiteral("unconfigured");
        return state;
    }
    state[QStringLiteral("devicePath")] = device;
    if (colour == u"true") {
        state[QStringLiteral("cameraState")] = QStringLiteral("rgb");
        return state;
    }
    if (colour != u"false") return state;
    const auto resolved = QFileInfo(device).canonicalFilePath();
    struct stat metadata{};
    if (!QDir::isAbsolutePath(device) || resolved.isEmpty() || ::stat(QFile::encodeName(resolved).constData(), &metadata) != 0) {
        state[QStringLiteral("cameraState")] = QStringLiteral("unavailable");
    } else {
        const bool known = S_ISCHR(metadata.st_mode) && knownIrCamera(QFileInfo(resolved).fileName(), paths.videoClass);
        state[QStringLiteral("cameraState")] = known ? QStringLiteral("known-ir") : QStringLiteral("unverified");
    }
    return state;
}

SlayerKenning::Paths KenningIntegration::installedPaths() {
    SlayerKenning::Paths paths;
    paths.engine = QStandardPaths::findExecutable(QStringLiteral("kenning"));
    paths.launcher = QStandardPaths::findExecutable(QStringLiteral("kcmshell6"));
    paths.config = QStringLiteral("/etc/kenning/config.ini");
    for (const auto& directory : QCoreApplication::libraryPaths()) {
        const auto module = QDir(directory).filePath(QStringLiteral("plasma/kcms/systemsettings/kcm_kenning.so"));
        if (QFileInfo(module).isFile()) { paths.module = module; break; }
    }
    return paths;
}

void KenningIntegration::refresh() {
    const auto next = m_preview
        ? QVariantMap{{QStringLiteral("installed"), true}, {QStringLiteral("settingsAvailable"), true},
            {QStringLiteral("configState"), QStringLiteral("readable")}, {QStringLiteral("engineState"), QStringLiteral("disabled")},
            {QStringLiteral("cameraState"), QStringLiteral("known-ir")}, {QStringLiteral("devicePath"), QStringLiteral("/dev/video2")}}
        : SlayerKenning::inspect(installedPaths());
    if (m_state == next && m_error.isEmpty()) return;
    m_state = next;
    m_error.clear();
    Q_EMIT changed();
}

void KenningIntegration::openSettings() {
    if (m_preview) {
        m_error = tr("Önizlemede Kenning ayarları açılmaz.");
    } else {
        const auto paths = installedPaths();
        m_state = SlayerKenning::inspect(paths);
        if (!m_state.value(QStringLiteral("settingsAvailable")).toBool())
            m_error = tr("Kenning ve KDE ayar modülü bulunamadı. Kurulumu tamamlayıp yeniden deneyin.");
        else if (!QProcess::startDetached(paths.launcher, {QStringLiteral("kcm_kenning")}))
            m_error = tr("Kenning ayarları açılamadı.");
        else m_error.clear();
    }
    Q_EMIT changed();
}
