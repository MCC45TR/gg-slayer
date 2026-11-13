import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
RowLayout {
    id: root
    property string title
    property var rpm
    property bool animate: true
    Item {
        Layout.preferredWidth: Kirigami.Units.iconSizes.large
        Layout.preferredHeight: width
        Accessible.ignored: true
        Item {
            id: rotor
            anchors.fill: parent
            Repeater {
                model: 3
                delegate: Rectangle {
                    required property int index
                    width: rotor.width * 0.28
                    height: rotor.height * 0.46
                    radius: width / 2
                    x: (rotor.width - width) / 2
                    y: 1
                    color: Kirigami.Theme.highlightColor
                    transform: Rotation { origin.x: rotor.width * 0.14; origin.y: rotor.height / 2 - 1; angle: index * 120 }
                }
            }
            Rectangle { anchors.centerIn: parent; width: parent.width * 0.22; height: width; radius: width / 2; color: Kirigami.Theme.highlightColor }
            Timer {
                interval: 80
                repeat: true
                running: root.visible && root.animate && control.monitoring && !control.reducedMotion && root.rpm > 0
                onTriggered: rotor.rotation = (rotor.rotation + Math.min(24, root.rpm / 350)) % 360
            }
        }
    }
    ColumnLayout {
        Layout.fillWidth: true
        QQC2.Label { text: root.title; font.bold: true; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        QQC2.Label { text: root.rpm === undefined ? qsTr("Kullanılamıyor") : qsTr("%1 dev/dk").arg(Math.round(root.rpm)); wrapMode: Text.WordWrap; Layout.fillWidth: true }
    }
}
