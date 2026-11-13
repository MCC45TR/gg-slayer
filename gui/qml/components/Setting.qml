import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
ColumnLayout {
    id: root
    property string title
    property string description
    property string capability
    property var options: []
    property var labels: options
    property string current
    property bool automatic: true
    property string pending: current
    signal chosen(string value)
    readonly property bool supported: capability.length === 0 || device.ready(capability)
    Layout.fillWidth: true
    spacing: Kirigami.Units.smallSpacing
    QQC2.Label { text: root.title; font.bold: true; Layout.fillWidth: true; wrapMode: Text.WordWrap }
    QQC2.Label { text: root.description; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Kirigami.Theme.disabledTextColor }
    QQC2.ComboBox {
        id: box
        Layout.fillWidth: true
        Layout.minimumHeight: Kirigami.Units.gridUnit * 2
        model: root.labels
        currentIndex: root.options.indexOf(root.current)
        enabled: root.supported && !control.busy
        Accessible.name: root.title
        onActivated: {
            root.pending = root.options[currentIndex]
            if (root.automatic) {
                if (root.pending !== root.current) root.chosen(root.pending)
                currentIndex = Qt.binding(function() { return root.options.indexOf(root.current) })
            }
        }
    }
    QQC2.Label { text: device.reason(root.capability); visible: !root.supported; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Kirigami.Theme.disabledTextColor }
    QQC2.Button { text: qsTr("Uygula"); visible: !root.automatic; enabled: root.supported && root.pending !== root.current && !control.busy; onClicked: root.chosen(root.pending) }
}
