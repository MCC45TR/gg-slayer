import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kquickcontrols as KQuickControls

Section {
    id: root
    objectName: "copilotSection"
    title: qsTr("Copilot tuşu")
    iconName: "input-keyboard"
    subtitle: qsTr("Tuşa basıldığında çalışacak işlevi seçin.")
    readonly property var status: copilot.state
    readonly property list<string> actions: ["none", "control", "application", "website", "fan-boost", "fan-auto", "power-cycle", "power-saver", "balanced", "performance", "command"]
    Component.onCompleted: copilot.refresh()
    QQC2.Label {
        Layout.fillWidth: true
        text: root.status.assigned ? qsTr("Kayıtlı kısayol: %1").arg(root.status.shortcut)
            : qsTr("Slayer tarafından atanmış bir işlev yok.")
        wrapMode: Text.WordWrap
    }
    QQC2.Label {
        Layout.fillWidth: true
        visible: !root.status.available
        text: qsTr("Bu atama KDE Plasma genel kısayol hizmetini gerektirir.")
        wrapMode: Text.WordWrap
    }
    QQC2.ComboBox {
        id: action
        objectName: "copilotAction"
        Layout.fillWidth: true
        model: [qsTr("Atamayı kaldır"), qsTr("Slayer Control’ü aç"), qsTr("Uygulama aç"), qsTr("Web adresi aç"), qsTr("Fan Boost aç / kapat"), qsTr("Fanı otomatiğe al"), qsTr("Güç profilini sırayla değiştir"), qsTr("Pil tasarrufuna geç"), qsTr("Dengeli moda geç"), qsTr("Performans moduna geç"), qsTr("Özel komut çalıştır")]
        currentIndex: Math.max(0, root.actions.indexOf(root.status.action || "none"))
        enabled: root.status.available && !copilot.busy
        Accessible.name: qsTr("Copilot tuşunun işlevi")
        onCurrentIndexChanged: { if (currentIndex === 2) copilot.loadApplications() }
    }
    QQC2.ComboBox {
        id: application
        Layout.fillWidth: true
        visible: action.currentIndex === 2
        model: copilot.applications
        textRole: "name"
        valueRole: "id"
        currentIndex: {
            const list = copilot.applications
            for (let i = 0; i < list.length; ++i) if (list[i].id === root.status.target) return i
            return list.length ? 0 : -1
        }
        enabled: !copilot.busy
        Accessible.name: qsTr("Açılacak uygulama")
    }
    QQC2.TextField {
        id: website
        Layout.fillWidth: true
        visible: action.currentIndex === 3
        text: root.status.action === "website" ? root.status.target : ""
        placeholderText: "https://…"
        maximumLength: 2048
        enabled: !copilot.busy
        Accessible.name: qsTr("Açılacak web adresi")
    }
    QQC2.TextField {
        id: command
        Layout.fillWidth: true
        visible: action.currentIndex === 10
        text: root.status.action === "command" ? root.status.target : ""
        placeholderText: qsTr("Örneğin: konsole --new-tab")
        maximumLength: 2048
        enabled: !copilot.busy
        Accessible.name: qsTr("Çalıştırılacak özel komut")
    }
    QQC2.Label { text: qsTr("Komut kendi kullanıcı yetkinizle, yalnız tuşa basıldığında çalışır."); visible: action.currentIndex === 10; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    QQC2.Label { text: qsTr("Fan ve güç işlemleri cihazın desteklediği kontrolleri kullanır. Gerektiğinde yönetici doğrulaması istenir."); visible: action.currentIndex >= 4 && action.currentIndex <= 9; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    QQC2.Label { text: device.reason("fan"); visible: (action.currentIndex === 4 || action.currentIndex === 5) && !device.ready("fan"); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: Kirigami.Theme.disabledTextColor }
    QQC2.Label { text: qsTr("Kısayol düğmesine tıklayıp klavyedeki Copilot tuşuna basın."); visible: action.currentIndex > 0; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    Loader {
        id: key
        active: action.currentIndex > 0
        visible: active
        enabled: !copilot.busy
        Layout.fillWidth: true
        Layout.maximumWidth: root.width
        sourceComponent: KQuickControls.KeySequenceItem {
            keySequence: root.status.shortcut || "Meta+Shift+F23"
            multiKeyShortcutsAllowed: false
            checkForConflictsAgainst: 0
            showClearButton: false
            showCancelButton: true
        }
    }
    QQC2.Button {
        text: copilot.busy ? qsTr("Kaydediliyor…") : qsTr("Atamayı kaydet")
        enabled: root.status.available && !copilot.busy && (action.currentIndex !== 2 || application.currentIndex >= 0)
        onClicked: copilot.save(root.actions[action.currentIndex], action.currentIndex === 2 ? application.currentValue : action.currentIndex === 3 ? website.text : action.currentIndex === 10 ? command.text : "", key.item ? key.item.keySequence : "")
    }
    QQC2.Button { text: qsTr("KDE kısayollarını aç"); onClicked: device.openSettings("shortcuts") }
    QQC2.Label { text: qsTr("Atama pencere kapalıyken de çalışır. Tuşu KDE yönetir; ek bir Slayer arka plan süreci gerekmez."); wrapMode: Text.WordWrap; Layout.fillWidth: true }
    Kirigami.InlineMessage { text: copilot.error; visible: text.length > 0; type: Kirigami.MessageType.Error; Layout.fillWidth: true }
}
