// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    required property bool supported
    required property bool busy
    required property string currentLimit
    required property string reason
    readonly property real reportedLimit: Number(currentLimit)
    readonly property bool known: currentLimit.length > 0 && Number.isInteger(reportedLimit) && reportedLimit >= 50 && reportedLimit <= 100
    signal applyRequested(int percent)
    spacing: Kirigami.Units.smallSpacing
    QQC2.Label {
        text: qsTr("Azami dolum miktarı (%)")
        font.bold: true
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
    }
    QQC2.Label {
        text: !root.supported ? qsTr("Bu cihazın mevcut kernel ve firmware arayüzünde yüzde dolum sınırı kullanılamıyor.") : root.known ? qsTr("Geçerli sınır: %1%.").arg(root.reportedLimit) : qsTr("Güncel dolum eşiği okunamadı. Donanım bilgisini yenileyin.")
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
    }
    QQC2.SpinBox {
        id: limit
        objectName: "batteryLimitValue"
        from: 50
        to: 100
        value: root.known ? root.reportedLimit : 100
        editable: true
        enabled: root.supported && root.known && !root.busy
        textFromValue: (value, locale) => root.supported && root.known ? Number(value).toLocaleString(locale, "f", 0) : "—"
        Accessible.name: qsTr("Azami dolum miktarı (%)")
    }
    QQC2.Button {
        objectName: "batteryLimitApply"
        text: qsTr("Uygula ve kaydet")
        enabled: root.supported && root.known && !root.busy && limit.value !== root.reportedLimit
        onClicked: root.applyRequested(limit.value)
    }
    QQC2.Label {
        text: root.reason
        visible: !root.supported
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
    }
}
