// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PC3

PC3.Page {
    id: view
    required property int cameraState
    required property bool busy
    required property string errorText
    required property bool eventMonitoring
    signal enableRequested(bool enabled)
    signal refreshRequested
    implicitWidth: Kirigami.Units.gridUnit * 20
    implicitHeight: layout.implicitHeight + topPadding + bottomPadding
    padding: Kirigami.Units.largeSpacing
    PC3.ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth
        contentHeight: layout.implicitHeight
        PC3.ScrollBar.horizontal.policy: PC3.ScrollBar.AlwaysOff
        ColumnLayout {
            id: layout
            width: scroll.availableWidth
            spacing: Kirigami.Units.largeSpacing
            Kirigami.Heading {
                text: qsTr("Dahili kamera")
                level: 2
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            PC3.Label {
                text: view.cameraState === 1 ? qsTr("Kamera erişimi açık") : view.cameraState === 0 ? qsTr("Kamera erişimi kapalı") : qsTr("Kamera durumu bilinmiyor")
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            PC3.Label {
                text: qsTr("RGB ve IR kamerayı birlikte denetler. Kapatmak açık görüntülü görüşmeyi ve yüzle oturum açmayı kesebilir.")
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            PC3.Button {
                objectName: "cameraToggle"
                text: view.cameraState < 0 ? qsTr("Kamera kullanılamıyor") : view.cameraState === 0 ? qsTr("Kamerayı aç") : qsTr("Kamerayı kapat")
                icon.name: view.cameraState === 0 ? "camera-web" : "camera-off-symbolic"
                enabled: view.cameraState >= 0 && !view.busy
                Layout.fillWidth: true
                onClicked: view.enableRequested(view.cameraState === 0)
            }
            PC3.Label {
                text: qsTr("İşlem sürüyor…")
                visible: view.busy
                Layout.fillWidth: true
            }
            Kirigami.InlineMessage {
                visible: view.errorText.length > 0
                text: view.errorText
                type: Kirigami.MessageType.Error
                Layout.fillWidth: true
            }
            PC3.Label {
                text: qsTr("Otomatik durum izleme kullanılamıyor. Güncel durumu yenileyebilirsiniz.")
                visible: !view.eventMonitoring
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            PC3.Button {
                text: qsTr("Durumu yenile")
                icon.name: "view-refresh"
                enabled: !view.busy
                onClicked: view.refreshRequested()
            }
        }
    }
}
