import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "../components"
PageBody {
    id: pageRoot
    GridLayout {
        Layout.fillWidth: true
        columns: pageRoot.width >= 1050 ? 2 : 1
        columnSpacing: 18
        rowSpacing: 18
    Section {
        Layout.fillWidth: true; Layout.alignment: Qt.AlignTop; title: qsTr("Güç profili"); iconName: "preferences-system-power-management"
        Setting { title: qsTr("KDE güç profili"); description: qsTr("Masaüstünün güç yöneticisiyle paylaşılır."); options: control.powerChoices; labels: options.map(v => v === "power-saver" ? qsTr("Pil tasarrufu") : v === "balanced" ? qsTr("Dengeli") : qsTr("Performans")); current: control.powerProfile; enabled: control.powerAvailable; onChosen: value => control.setPower(value) }
        QQC2.Button { text: qsTr("Sistem güç ayarları"); onClicked: device.openSettings("power") }
    }
    Section {
        Layout.fillWidth: true; Layout.alignment: Qt.AlignTop; title: qsTr("Pil koruması"); iconName: "battery"
        Setting { title: qsTr("Koruma profili"); description: qsTr("Koruma profili yüzde şarj sınırı değildir. Pil göstergesi %100 kalabilir."); capability: "battery-profile"; options: ["high-capacity", "balanced", "stationary"]; labels: [qsTr("Yüksek kapasite"), qsTr("Dengeli"), qsTr("Masaüstü kullanımı")]; current: device.info.battery_profile || ""; onChosen: value => control.apply(["battery", "profile", value], ["battery_profile", value]) }
        QQC2.Label { text: qsTr("Sağlık: %1 % · Döngü: %2").arg(device.info.battery_health || "—").arg(device.info.battery_cycles || "—"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
    }
    Section {
        Layout.fillWidth: true; Layout.alignment: Qt.AlignTop; title: qsTr("Şarj sınırı"); subtitle: qsTr("Yalnız kernelin bildirdiği yüzde eşiği destekleniyorsa kullanılır.")
        QQC2.SpinBox { id: limit; from: 50; to: 100; value: Number(device.info.battery_limit || 80); visible: device.ready("battery-limit"); enabled: !control.busy; Accessible.name: qsTr("Şarjı durdur (%)"); onValueModified: control.apply(["battery", "limit", String(value)], ["battery_end", String(value)]) }
        QQC2.Label { text: device.reason("battery-limit"); visible: !device.ready("battery-limit"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
    }
    }
}
