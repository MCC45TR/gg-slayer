// SPDX-License-Identifier: MIT
#include "../gui/kenning.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
void write(const QString& path, const QByteArray& data) {
    require(QDir().mkpath(QFileInfo(path).absolutePath()), "Create fixture directory");
    QFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write(data) == data.size(), "Write fixture");
}
QString field(const QVariantMap& state, const char* key) { return state.value(QString::fromLatin1(key)).toString(); }
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    require(directory.isValid(), "Temporary fixture directory");
    const auto root = directory.path();
    const auto marker = root + QStringLiteral("/unexpected-execution");
    SlayerKenning::Paths paths{root + QStringLiteral("/kenning"), root + QStringLiteral("/kcm_kenning.so"),
        root + QStringLiteral("/kcmshell6"), root + QStringLiteral("/config.ini"), root + QStringLiteral("/video4linux")};
    auto state = SlayerKenning::inspect(paths);
    require(!state.value(QStringLiteral("installed")).toBool() && !state.value(QStringLiteral("settingsAvailable")).toBool(), "Optional engine may be absent");
    const auto executable = QByteArray("#!/bin/sh\ntouch '") + marker.toUtf8() + "'\n";
    write(paths.engine, executable);
    write(paths.launcher, executable);
    for (const auto& path : {paths.engine, paths.launcher})
        require(QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner), "Executable fixture permissions");
    state = SlayerKenning::inspect(paths);
    require(state.value(QStringLiteral("installed")).toBool() && !state.value(QStringLiteral("settingsAvailable")).toBool(), "Engine without KDE module");
    write(paths.module, "fixture: must never be loaded");
    state = SlayerKenning::inspect(paths);
    require(state.value(QStringLiteral("settingsAvailable")).toBool() && field(state, "engineState") == u"unknown", "Settings availability does not imply authentication enabled");
    write(paths.config, "[core]\ndisabled = false\n[video]\ncolour = false\ndevice_path = none\n");
    state = SlayerKenning::inspect(paths);
    require(field(state, "engineState") == u"enabled" && field(state, "cameraState") == u"unconfigured", "Fresh engine still needs camera setup");
    write(paths.config, "[core]\ndisabled = true\n[video]\ncolour = true\ndevice_path = /dev/null\n");
    state = SlayerKenning::inspect(paths);
    require(field(state, "engineState") == u"disabled" && field(state, "cameraState") == u"rgb", "RGB fallback is reported explicitly");
    write(paths.config, "[core]\ndisabled = false\n[video]\ncolour = false\ndevice_path = /dev/slayer-no-such-camera\n");
    require(field(SlayerKenning::inspect(paths), "cameraState") == u"unavailable", "Disconnected camera never reported ready");
    write(paths.config, "[core]\ndisabled = false\n[video]\ncolour = false\ndevice_path = /dev/null\n");
    require(field(SlayerKenning::inspect(paths), "cameraState") == u"unverified", "IR mode alone is not hardware identity");
    write(paths.config, "[core]\ndisabled = unexpected\n[video]\ncolour = false\ndevice_path = /dev/null\n");
    require(field(SlayerKenning::inspect(paths), "engineState") == u"unknown", "Unknown config does not imply enabled");
    write(paths.config, "[core]\ndisabled=false\n[video]\ncolour=false\ndevice_path=/dev/null\ncolour=true\n");
    require(field(SlayerKenning::inspect(paths), "cameraState") == u"rgb", "Last duplicate key matches Kenning semantics");
    write(paths.config, QByteArray(65537, 'x'));
    require(field(SlayerKenning::inspect(paths), "configState") == u"unavailable", "Config reads are bounded");
    require(QFile::remove(paths.config) && QDir().mkpath(paths.config), "Non-file configuration fixture");
    require(field(SlayerKenning::inspect(paths), "configState") == u"unavailable", "Non-file config refused");

    const auto usb = root + QStringLiteral("/usb-device");
    const auto interface = usb + QStringLiteral("/interface");
    const auto entry = paths.videoClass + QStringLiteral("/video2");
    write(entry + QStringLiteral("/name"), "FHD WebCam: IR Camera\n");
    write(interface + QStringLiteral("/bInterfaceNumber"), "02\n");
    write(usb + QStringLiteral("/idVendor"), "2b7e\n");
    write(usb + QStringLiteral("/idProduct"), "c906\n");
    write(usb + QStringLiteral("/bcdDevice"), "0004\n");
    write(usb + QStringLiteral("/removable"), "fixed\n");
    require(QFile::link(interface, entry + QStringLiteral("/device")), "Sysfs interface link");
    require(SlayerKenning::knownIrCamera(QStringLiteral("video2"), paths.videoClass), "Known internal IR interface");
    write(interface + QStringLiteral("/bInterfaceNumber"), "00\n");
    require(!SlayerKenning::knownIrCamera(QStringLiteral("video2"), paths.videoClass), "RGB interface on the same USB camera refused");
    write(interface + QStringLiteral("/bInterfaceNumber"), "02\n");
    write(usb + QStringLiteral("/bcdDevice"), "9999\n");
    require(!SlayerKenning::knownIrCamera(QStringLiteral("video2"), paths.videoClass), "Unknown camera revision is not verified");
    require(!SlayerKenning::knownIrCamera(QStringLiteral("../video2"), paths.videoClass), "Camera node cannot traverse directories");
    require(!QFileInfo::exists(marker), "Inspect never executes engine, launcher or modules");

    KenningIntegration preview(true);
    require(preview.state().isEmpty(), "Construction performs no discovery");
    int notifications = 0;
    QObject::connect(&preview, &KenningIntegration::changed, [&] { ++notifications; });
    preview.refresh();
    const auto snapshot = preview.state();
    for (int count = 0; count < 100; ++count) preview.refresh();
    require(notifications == 1 && snapshot == preview.state(), "Unchanged state does not notify or accumulate work");
    require(preview.findChildren<QTimer*>().isEmpty(), "No recognition polling timers");
    preview.openSettings();
    require(!preview.error().isEmpty() && !QFileInfo::exists(marker), "Preview never launches external settings");
    std::cout << "Optional Kenning discovery, literal bounded config, IR identity, no camera/process activation or polling: OK\n";
    return 0;
}
