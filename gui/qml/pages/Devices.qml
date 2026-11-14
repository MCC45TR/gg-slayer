import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "../components"
PageBody {
    Section {
        Layout.fillWidth: true; title: qsTr("Cihaz kontrolleri"); iconName: "preferences-devices"
        Setting { title: qsTr("Fn kilidi"); description: qsTr("İşlev tuşlarının davranışını değiştirir."); capability: "fn"; options: ["on", "off"]; labels: [qsTr("Açık"), qsTr("Kapalı")]; current: device.info.fn_lock === "1" ? "on" : device.info.fn_lock === "0" ? "off" : ""; onChosen: value => control.apply(["settings", "fn-lock", value], ["fn_lock", value]) }
        Setting { title: qsTr("Dahili kamera"); description: qsTr("RGB ve IR kameranın erişimini değiştirir; kapatma görüntülü görüşmeyi kesebilir."); capability: "camera"; options: ["on", "off"]; labels: [qsTr("Açık"), qsTr("Kapalı")]; current: device.info.camera === "1" ? "on" : device.info.camera === "0" ? "off" : ""; onChosen: value => control.apply(["settings", "camera", value], ["camera", value]) }
        QQC2.Button { text: qsTr("Sistem touchpad ayarları"); onClicked: device.openSettings("touchpad") }
    }
    FaceAuth { Layout.fillWidth: true }
    Copilot { Layout.fillWidth: true }
    QQC2.CheckBox { id: advanced; objectName: "deviceAdvanced"; text: qsTr("Gelişmiş USB ayarları") }
    Loader {
        Layout.fillWidth: true; active: advanced.checked
        sourceComponent: Section {
            title: qsTr("USB güç yönetimi")
            Setting { title: qsTr("Kapalıyken USB şarj"); capability: "usb-charging"; options: ["on", "off"]; labels: [qsTr("Açık"), qsTr("Kapalı")]; current: device.info.usb_charging === "1" ? "on" : "off"; onChosen: value => control.apply(["settings", "usb-charging", value], ["usb_charging", value]) }
            QQC2.TextField { id: port; Layout.fillWidth: true; placeholderText: qsTr("USB portu · örneğin 3-2.1"); validator: RegularExpressionValidator { regularExpression: /[1-9][0-9]*-[1-9][0-9]*(\.[1-9][0-9]*)*/ } Accessible.name: placeholderText }
            QQC2.Button { text: qsTr("Güç tasarrufunu aç"); enabled: port.acceptableInput && device.ready("model-bios") && !control.busy; onClicked: control.apply(["usb", "powersave", port.text, "on"]) }
            QQC2.Button { text: qsTr("Güç tasarrufunu kapat"); enabled: port.acceptableInput && device.ready("model-bios") && !control.busy; onClicked: control.apply(["usb", "powersave", port.text, "off"]) }
            QQC2.Label { text: qsTr("Hub, giriş ve depolama aygıtlarının güç kontrolleri arka uç tarafından korunur."); wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
}
