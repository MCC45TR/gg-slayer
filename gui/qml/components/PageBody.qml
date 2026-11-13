import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
Item {
    id: root
    default property alias content: column.data
    readonly property real inset: width < 500 ? Kirigami.Units.largeSpacing : Kirigami.Units.largeSpacing * 2
    implicitHeight: column.implicitHeight + inset * 2
    ColumnLayout {
        id: column
        anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: root.inset }
        width: Math.min(root.width - root.inset * 2, Kirigami.Units.gridUnit * 80)
        spacing: Kirigami.Units.largeSpacing * 1.5
    }
}
