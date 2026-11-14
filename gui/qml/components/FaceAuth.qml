import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Section {
    id: root
    title: qsTr("IR yüz tanıma")
    iconName: "user-identity"
    subtitle: qsTr("Ekran kilidi ve sudo için Kenning entegrasyonu")
    readonly property var status: faceAuth.state
    readonly property string cameraState: status.cameraState || "unknown"
    Component.onCompleted: faceAuth.refresh()

    QQC2.Label {
        Layout.fillWidth: true
        text: !root.status.installed ? qsTr("Kenning kurulu değil.")
            : !root.status.settingsAvailable ? qsTr("Kenning bulundu; KDE ayar modülü veya ayar uygulaması eksik.")
            : root.status.engineState === "disabled" ? qsTr("Kenning kurulu · Motor ayarı kapalı")
            : root.status.engineState === "enabled" ? qsTr("Kenning kurulu · Motor ayarı açık")
            : qsTr("Kenning kurulu · Motor ayarı okunamadı")
        wrapMode: Text.WordWrap
        font.bold: true
    }
    QQC2.Label {
        Layout.fillWidth: true
        visible: !root.status.installed
        text: qsTr("Kenning'i ayrıca kurduktan sonra durumu yenileyin.")
        wrapMode: Text.WordWrap
    }
    QQC2.Label {
        Layout.fillWidth: true
        visible: root.status.installed
        text: root.cameraState === "known-ir" ? qsTr("Seçili kamera: bilinen dahili IR arayüzü.")
            : root.cameraState === "rgb" ? qsTr("RGB kamera seçili. Kenning ayarlarından dahili IR kamerayı seçin.")
            : root.cameraState === "unconfigured" ? qsTr("Kamera henüz seçilmemiş. Kenning ayarlarından IR kamerayı seçin.")
            : root.cameraState === "unavailable" ? qsTr("Seçili kameraya erişilemiyor. Kamera erişimini ve aygıt bağlantısını kontrol edin.")
            : qsTr("Seçili kameranın IR kimliği doğrulanamadı.")
        wrapMode: Text.WordWrap
    }
    QQC2.Label {
        Layout.fillWidth: true
        visible: text.length > 0
        text: root.status.devicePath || ""
        textFormat: Text.PlainText
        wrapMode: Text.WrapAnywhere
        color: Kirigami.Theme.disabledTextColor
    }
    QQC2.Label {
        Layout.fillWidth: true
        text: qsTr("Yüz kaydı, kamera seçimi ve kilit/sudo tercihleri Kenning ayarlarından yönetilir. Buradaki durum, yüz doğrulamasının çalıştığı anlamına gelmez.")
        wrapMode: Text.WordWrap
    }
    Flow {
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing
        QQC2.Button {
            text: qsTr("Kenning ayarlarını aç")
            icon.name: "configure"
            enabled: !!root.status.settingsAvailable
            onClicked: faceAuth.openSettings()
        }
        QQC2.Button {
            text: qsTr("Durumu yenile")
            icon.name: "view-refresh"
            onClicked: faceAuth.refresh()
        }
    }
    Kirigami.InlineMessage {
        Layout.fillWidth: true
        visible: faceAuth.error.length > 0
        text: faceAuth.error
        type: Kirigami.MessageType.Error
    }
}
