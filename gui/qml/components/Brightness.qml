import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
ColumnLayout {
    id: root
    Layout.fillWidth: true
    property int pendingValue: -1
    function commit() {
        if (pendingValue < 0) return
        const target = pendingValue
        pendingValue = -1
        if (target !== Number(device.info.keyboard_brightness))
            control.apply(["keyboard", "brightness", String(target)], ["keyboard_brightness", String(target)])
    }
    QQC2.Label { text: qsTr("Klavye parlaklığı"); font.bold: true }
    QQC2.Slider {
        id: slider
        Layout.fillWidth: true
        from: 0; to: 100; stepSize: 1
        enabled: device.ready("keyboard") && device.info.saved_brightness_owner === "custom"
        Accessible.name: qsTr("Klavye parlaklığı")
        onMoved: { root.pendingValue = Math.round(value); if (!throttle.running) throttle.start() }
        onPressedChanged: if (!pressed) { throttle.stop(); root.commit() }
    }
    Binding { target: slider; property: "value"; when: !slider.pressed && !control.busy && root.pendingValue < 0; value: Number(device.info.keyboard_brightness || 0) }
    Timer { id: throttle; interval: 75; onTriggered: root.commit() }
    QQC2.Label {
        text: device.info.saved_brightness_owner === "custom" ? qsTr("%1 %").arg(Math.round(slider.value)) : qsTr("Parlaklık yönetimi KDE’de veya henüz seçilmedi.")
        wrapMode: Text.WordWrap; Layout.fillWidth: true
    }
}
