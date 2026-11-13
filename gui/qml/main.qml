import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami
import "components"

Kirigami.ApplicationWindow {
    id: root
    width: 1200
    height: 820
    minimumWidth: 320
    minimumHeight: 400
    title: control.preview ? qsTr("Slayer Control · Önizleme") : qsTr("Slayer Control")
    property int section: 0
    property bool saved: false
    Connections { target: control; function onSucceeded() { root.saved = true; savedTimer.restart() } }
    Timer { id: savedTimer; interval: 1800; onTriggered: root.saved = false }
    readonly property var destinations: [qsTr("Genel Bakış"), qsTr("Klavye ve RGB"), qsTr("Güç ve Pil"), qsTr("Performans ve Grafik"), qsTr("Aygıtlar"), qsTr("Otomasyon"), qsTr("Destek")]
    readonly property var icons: ["view-grid", "input-keyboard", "battery", "speedometer", "preferences-devices", "system-run", "help-about"]
    readonly property bool foreground: visible && visibility !== Window.Minimized
    Binding { target: control; property: "active"; value: root.foreground }
    Binding { target: control; property: "monitoring"; value: root.foreground && root.section === 0 }
    globalDrawer: Kirigami.GlobalDrawer {
        title: qsTr("Slayer Control")
        titleIcon: "computer-laptop"
        isMenu: false
        modal: root.width < Kirigami.Units.gridUnit * 45
        collapsible: false
        onModalChanged: Qt.callLater(function() { drawerOpen = !modal })
        drawerOpen: !modal
        handleVisible: modal
        handleClosedToolTip: qsTr("Bölümleri aç")
        handleOpenToolTip: qsTr("Bölümleri kapat")
        actions: [
            Kirigami.Action { text: root.destinations[0]; icon.name: root.icons[0]; checked: root.section === 0; onTriggered: root.section = 0 },
            Kirigami.Action { text: root.destinations[1]; icon.name: root.icons[1]; checked: root.section === 1; onTriggered: root.section = 1 },
            Kirigami.Action { text: root.destinations[2]; icon.name: root.icons[2]; checked: root.section === 2; onTriggered: root.section = 2 },
            Kirigami.Action { text: root.destinations[3]; icon.name: root.icons[3]; checked: root.section === 3; onTriggered: root.section = 3 },
            Kirigami.Action { text: root.destinations[4]; icon.name: root.icons[4]; checked: root.section === 4; onTriggered: root.section = 4 },
            Kirigami.Action { text: root.destinations[5]; icon.name: root.icons[5]; checked: root.section === 5; onTriggered: root.section = 5 },
            Kirigami.Action { text: root.destinations[6]; icon.name: root.icons[6]; checked: root.section === 6; onTriggered: root.section = 6 }
        ]
    }
    pageStack.initialPage: Kirigami.Page {
        title: root.destinations[root.section]
        padding: 0
        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: control.error.length > 0
                text: control.error
                type: Kirigami.MessageType.Error
                showCloseButton: true
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.minimumHeight: Kirigami.Units.gridUnit
                Layout.maximumHeight: Kirigami.Units.gridUnit
                Layout.margins: Kirigami.Units.smallSpacing
                QQC2.BusyIndicator { running: control.busy; visible: running; implicitWidth: Kirigami.Units.gridUnit; implicitHeight: implicitWidth }
                QQC2.Label { text: control.busy ? qsTr("Uygulanıyor…") : root.saved ? qsTr("✓ Tamamlandı") : ""; Layout.fillWidth: true }
            }
            QQC2.ScrollView {
                id: scroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                Loader {
                    id: content
                    width: scroll.availableWidth
                    asynchronous: false
                    source: ["pages/Overview.qml", "pages/Lighting.qml", "pages/Power.qml", "pages/Performance.qml", "pages/Devices.qml", "pages/Automation.qml", "pages/Support.qml"][root.section]
                    onLoaded: scroll.contentItem.contentY = 0
                }
            }
        }
    }
    Shortcut { sequence: "Ctrl+R"; onActivated: control.refresh() }
}
