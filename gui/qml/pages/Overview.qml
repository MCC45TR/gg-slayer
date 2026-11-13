import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../components"
PageBody {
    id: root
    function metric(key, suffix) { return control.metrics[key] === undefined ? qsTr("Kullanılamıyor") : Math.round(control.metrics[key]) + suffix }
    function info(key, suffix) { return device.info[key] === undefined ? qsTr("Kullanılamıyor") : device.info[key] + suffix }
    Section {
        Layout.fillWidth: true
        title: device.info.model || qsTr("Bilgisayarınız")
        subtitle: device.info.cpu || qsTr("Cihaz bilgisi okunuyor…")
        iconName: "computer-laptop"
        QQC2.Label { text: device.info.hardware_writes === "blocked" ? qsTr("BIOS doğrulanmalı · donanım değişiklikleri kapalı") : qsTr("Günlük kontrolleriniz, tek yerde."); wrapMode: Text.WordWrap; Layout.fillWidth: true }
    }
    GridLayout {
        Layout.fillWidth: true
        columns: root.width >= 1350 ? 4 : root.width >= 650 ? 2 : 1
        columnSpacing: Kirigami.Units.largeSpacing
        rowSpacing: Kirigami.Units.largeSpacing
        Stat { Layout.fillWidth: true; Layout.fillHeight: true; title: qsTr("İşlemci"); iconName: "cpu"; value: root.metric("cpu_usage", " %"); detail: root.metric("cpu_temp", " °C"); percentage: control.metrics.cpu_usage ?? -1 }
        Stat { Layout.fillWidth: true; Layout.fillHeight: true; title: qsTr("Grafik işlemcisi"); iconName: "video-display"; value: root.metric("gpu_usage", " %"); detail: (control.metrics.nvidia_usage !== undefined ? "NVIDIA · " : control.metrics.amd_usage !== undefined ? "AMD iGPU · " : "") + root.metric("gpu_temp", " °C"); percentage: control.metrics.gpu_usage ?? -1 }
        Stat { Layout.fillWidth: true; Layout.fillHeight: true; title: qsTr("Pil"); iconName: "battery"; value: root.info("battery", " %"); percentage: Number(device.info.battery ?? -1); detail: device.info.ac_online === "1" ? qsTr("Adaptöre bağlı") : qsTr("Pille çalışıyor") }
        Section {
            Layout.fillWidth: true; Layout.fillHeight: true
            title: qsTr("Soğutma"); iconName: "speedometer"
            Fan { title: qsTr("CPU fanı"); rpm: control.metrics.cpu_fan }
            Fan { title: qsTr("GPU fanı"); rpm: control.metrics.gpu_fan }
        }
    }
    GridLayout {
        Layout.fillWidth: true
        columns: root.width >= 1000 ? 2 : 1
        columnSpacing: Kirigami.Units.largeSpacing
        rowSpacing: Kirigami.Units.largeSpacing
        Section {
            Layout.fillWidth: true; Layout.fillHeight: true
            title: qsTr("Hızlı kontroller"); iconName: "preferences-system"
            Setting { title: qsTr("KDE güç profili"); options: control.powerChoices; labels: options.map(v => v === "power-saver" ? qsTr("Pil tasarrufu") : v === "balanced" ? qsTr("Dengeli") : qsTr("Performans")); current: control.powerProfile; enabled: control.powerAvailable; onChosen: value => control.setPower(value) }
            Setting { title: qsTr("Soğutma"); capability: "fan"; options: ["auto", "boost"]; labels: [qsTr("Otomatik"), qsTr("Fan Boost")]; current: device.info.fan_mode || ""; onChosen: value => control.apply(["fan", value], ["fan_mode", value]) }
            Brightness { }
        }
        Section {
            Layout.fillWidth: true; Layout.fillHeight: true
            title: qsTr("Pil ve sistem"); iconName: "battery"
            QQC2.Label { text: qsTr("Pil sağlığı: %1").arg(root.info("battery_health", " %")); wrapMode: Text.WordWrap; Layout.fillWidth: true }
            QQC2.Label { text: qsTr("Şarj döngüsü: %1").arg(root.info("battery_cycles", "")); wrapMode: Text.WordWrap; Layout.fillWidth: true }
            QQC2.Label { text: qsTr("Şarj sınırı: %1").arg(root.info("battery_limit", " %")); wrapMode: Text.WordWrap; Layout.fillWidth: true }
            QQC2.Label { text: qsTr("Grafik modu: %1").arg(device.info.mux_mode || "—"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
            QQC2.Label { text: qsTr("Fabrika modu: %1").arg(device.info.profile === "low-power" ? qsTr("Office") : device.info.profile === "custom" ? qsTr("Özel firmware modu") : device.info.profile || "—"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
}
