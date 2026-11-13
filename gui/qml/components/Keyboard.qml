import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Shapes
import org.kde.kirigami as Kirigami
Flickable {
    id: root
    property string selected: "W"
    signal chosen(string key)
    function navigate(model, dx, dy) {
        const centerX = Number(model.x) + Number(model.w) / 2
        const centerY = Number(model.y) + Number(model.h) / 2
        let best = null, score = 1e9
        for (let i = 0; i < keys.count; ++i) {
            const candidate = keys.itemAt(i)
            if (!candidate || !candidate.visible) continue
            const data = candidate.modelData
            const x = Number(data.x) + Number(data.w) / 2 - centerX
            const y = Number(data.y) + Number(data.h) / 2 - centerY
            if ((dx && x * dx < 0.1) || (dy && y * dy < 0.1)) continue
            const distance = dx ? Math.abs(x) + Math.abs(y) * 5 : Math.abs(y) + Math.abs(x) * 2
            if (distance < score) { score = distance; best = candidate }
        }
        if (best) { selected = best.modelData.id; chosen(selected); best.forceActiveFocus() }
    }
    implicitHeight: contentHeight + Kirigami.Units.smallSpacing
    contentWidth: Math.max(width, Kirigami.Units.gridUnit * 46)
    contentHeight: contentWidth / 21 * 7.1
    boundsBehavior: Flickable.StopAtBounds
    clip: true
    QQC2.ScrollBar.horizontal: QQC2.ScrollBar { }
    Repeater {
        id: keys
        model: device.keys
        delegate: QQC2.AbstractButton {
            id: key
            required property var modelData
            visible: modelData.present !== false
            x: Number(modelData.x ?? modelData.column) * root.contentWidth / 21
            y: Number(modelData.y ?? modelData.row) * root.contentWidth / 21
            width: Number(modelData.w ?? modelData.width) * root.contentWidth / 21 - 3
            height: Number(modelData.h ?? 1) * root.contentWidth / 21 - 3
            text: modelData.label
            checkable: true
            checked: root.selected === modelData.id
            readonly property bool isoEnter: modelData.shape === "iso-enter"
            readonly property color faceColor: checked ? Kirigami.Theme.highlightColor : hovered || down ? Kirigami.Theme.alternateBackgroundColor : Kirigami.Theme.backgroundColor
            readonly property color labelColor: checked ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
            background: Item {
                Rectangle {
                    anchors.fill: parent
                    visible: !key.isoEnter
                    radius: Kirigami.Units.smallSpacing
                    color: key.faceColor
                    border.color: key.activeFocus ? Kirigami.Theme.highlightColor : Qt.alpha(Kirigami.Theme.textColor, .25)
                    border.width: key.activeFocus ? 2 : 1
                }
                Shape {
                    anchors.fill: parent
                    visible: key.isoEnter
                    ShapePath {
                        fillColor: key.faceColor
                        strokeColor: key.activeFocus ? Kirigami.Theme.highlightColor : Qt.alpha(Kirigami.Theme.textColor, .25)
                        strokeWidth: key.activeFocus ? 2 : 1
                        startX: 1; startY: 1
                        PathLine { x: key.width - 1; y: 1 }
                        PathLine { x: key.width - 1; y: key.height - 1 }
                        PathLine { x: key.width * .2; y: key.height - 1 }
                        PathLine { x: key.width * .2; y: key.height * .5 }
                        PathLine { x: 1; y: key.height * .5 }
                        PathLine { x: 1; y: 1 }
                    }
                }
            }
            contentItem: Item {
                QQC2.Label {
                    anchors.centerIn: parent
                    text: key.text
                    color: key.labelColor
                    font.pointSize: key.font.pointSize * .85
                }
                QQC2.Label {
                    anchors { top: parent.top; left: parent.left; margins: 3 }
                    text: key.modelData.shift || ""
                    visible: text.length > 0 && text !== key.text
                    color: key.labelColor
                    font.pointSize: key.font.pointSize * .65
                }
                QQC2.Label {
                    anchors { bottom: parent.bottom; right: parent.right; margins: 3 }
                    text: key.modelData.altgr || ""
                    visible: text.length > 0 && text !== key.text
                    color: key.labelColor
                    font.pointSize: key.font.pointSize * .65
                }
            }
            Accessible.name: qsTr("%1 tuşu").arg(modelData.label)
            onClicked: { root.selected = modelData.id; root.chosen(modelData.id) }
            Keys.onLeftPressed: root.navigate(modelData, -1, 0)
            Keys.onRightPressed: root.navigate(modelData, 1, 0)
            Keys.onUpPressed: root.navigate(modelData, 0, -1)
            Keys.onDownPressed: root.navigate(modelData, 0, 1)
            QQC2.ToolTip.text: qsTr("%1 · LED %2").arg(modelData.label).arg(modelData.led)
            QQC2.ToolTip.visible: hovered || activeFocus
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 4 }
                height: 2
                visible: key.modelData.rgb !== undefined
                color: visible ? Qt.rgba(key.modelData.rgb[0] / 255, key.modelData.rgb[1] / 255, key.modelData.rgb[2] / 255, 1) : "transparent"
            }
        }
    }
}
