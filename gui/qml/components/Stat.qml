import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
Section {
    id: root
    property string value: qsTr("Kullanılamıyor")
    property string detail
    property real percentage: -1
    Kirigami.Heading { text: root.value; level: 1; font.pointSize: Qt.application.font.pointSize * 1.8; font.weight: Font.DemiBold; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 4
        radius: 2
        visible: root.percentage >= 0
        color: Qt.alpha(Kirigami.Theme.textColor, 0.1)
        Accessible.role: Accessible.ProgressBar
        Accessible.name: root.title
        Rectangle { width: parent.width * Math.max(0, Math.min(100, root.percentage)) / 100; height: parent.height; radius: 2; color: Kirigami.Theme.highlightColor }
    }
    QQC2.Label { text: root.detail; visible: text.length > 0; wrapMode: Text.WordWrap; color: Kirigami.Theme.disabledTextColor; Layout.fillWidth: true }
}
