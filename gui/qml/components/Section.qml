import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Rectangle {
    id: root
    property string title
    property string subtitle
    property string iconName
    default property alias content: body.data
    implicitHeight: column.implicitHeight + Kirigami.Units.largeSpacing * 3
    color: Kirigami.Theme.backgroundColor
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    radius: Kirigami.Units.cornerRadius
    border.color: Qt.alpha(Kirigami.Theme.textColor, 0.12)
    ColumnLayout {
        id: column
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: Kirigami.Units.largeSpacing * 1.5 }
        spacing: Kirigami.Units.largeSpacing
        RowLayout {
            Layout.fillWidth: true
            Kirigami.Icon { source: root.iconName; visible: root.iconName.length > 0; implicitWidth: Kirigami.Units.iconSizes.smallMedium; implicitHeight: implicitWidth; Accessible.ignored: true }
            Kirigami.Heading { text: root.title; level: 3; font.bold: true; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
        QQC2.Label { text: root.subtitle; visible: text.length > 0; wrapMode: Text.WordWrap; color: Kirigami.Theme.disabledTextColor; Layout.fillWidth: true }
        ColumnLayout { id: body; spacing: Kirigami.Units.largeSpacing; Layout.fillWidth: true }
    }
}
