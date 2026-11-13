import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import "../components"
PageBody {
    id: root
    property string colorHex: selectedColorHex
    readonly property string selectedColorHex: {
        const keys = device.keys
        for (let i = 0; i < keys.length; ++i) {
            if (keys[i].id !== selectedKey || !keys[i].rgb) continue
            return keys[i].rgb.map(value => Number(value).toString(16).padStart(2, "0")).join("").toUpperCase()
        }
        return "FFFFFF"
    }
    onSelectedColorHexChanged: colorHex = selectedColorHex
    onSelectedKeyChanged: colorHex = selectedColorHex
    property string selectedKey: "W"
    property bool allKeys: true
    function commitColor() {
        if (!custom || !device.ready("rgb")) return
        if (allKeys) control.apply(["keyboard", "color", colorHex], ["keyboard_color", colorHex])
        else control.apply(["keyboard", "key", selectedKey, colorHex])
    }
    readonly property bool custom: device.info.saved_lighting_owner === "custom"
    readonly property var effects: ["wave", "breathe", "rainbow", "reactive", "ripple", "marquee", "raindrop", "aurora", "spark"]
    Section {
        Layout.fillWidth: true; title: qsTr("Klavye yönetimi"); iconName: "input-keyboard"
        Setting {
            title: qsTr("Işık ve renk")
            description: qsTr("KDE’ye bırakıldığında Slayer renk ve efekt komutu göndermez.")
            options: ["kde", "accent", "custom"]; labels: [qsTr("KDE’ye bırak"), qsTr("Sistem vurgu rengini izle"), qsTr("Özel")]
            current: device.info.saved_lighting_owner || ""
            onChosen: value => control.apply(["config", "set", "lighting_owner", value, "brightness_owner", value === "accent" ? "kde" : (device.info.saved_brightness_owner || "kde")])
        }
        Setting {
            title: qsTr("Parlaklık")
            options: ["kde", "custom"]; labels: [qsTr("KDE’ye bırak"), qsTr("Özel")]
            current: device.info.saved_brightness_owner || ""
            enabled: device.info.saved_lighting_owner !== "accent"
            onChosen: value => control.apply(["config", "set", "brightness_owner", value])
        }
        Loader { Layout.fillWidth: true; active: device.info.saved_brightness_owner === "custom"; sourceComponent: Brightness { } }
        QQC2.Label { text: qsTr("İlk açılışta mevcut ışıklar değiştirilmez. Yönetim tercihinizi siz seçersiniz."); visible: !device.info.saved_lighting_owner; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    }
    Section {
        Layout.fillWidth: true
        title: qsTr("Güç tasarrufunda bekleme")
        subtitle: qsTr("Kullanılmadığında ışık beş saniyede en düşük seviyeye iner. Dokunduğunuzda seçili parlaklığa yumuşakça döner. Kapalı klavye kapalı kalır.")
        Setting {
            title: qsTr("Beklemede ışığı kıs")
            description: qsTr("Yalnızca güç tasarrufu profilinde çalışır. Masaüstünün parlaklık değeri değişmez.")
            options: ["on", "off"]; labels: [qsTr("Açık"), qsTr("Kapalı")]
            current: device.info.saved_keyboard_idle_dim || "off"
            enabled: device.info.keyboard_idle_available === "1"
            onChosen: value => control.apply(["config", "set", "keyboard_idle_dim", value])
        }
        Setting {
            title: qsTr("Bekleme süresi")
            options: ["5", "10", "15", "30", "60", "120", "180", "300", "600", "900", "1200", "1800"]
            labels: [qsTr("5 saniye"), qsTr("10 saniye"), qsTr("15 saniye"), qsTr("30 saniye"), qsTr("1 dakika"), qsTr("2 dakika"), qsTr("3 dakika"), qsTr("5 dakika"), qsTr("10 dakika"), qsTr("15 dakika"), qsTr("20 dakika"), qsTr("30 dakika")]
            current: device.info.saved_keyboard_idle_timeout || "30"
            enabled: device.info.saved_keyboard_idle_dim === "on"
            onChosen: value => control.apply(["config", "set", "keyboard_idle_timeout", value])
        }
        QQC2.Label {
            visible: device.info.keyboard_idle_available !== "1"
            text: qsTr("Beklemede kısma için güncel Slayer klavye sürücüsü gerekli.")
            Layout.fillWidth: true; wrapMode: Text.WordWrap
        }
    }
    QQC2.CheckBox { id: whiteBalance; objectName: "whiteBalanceOptions"; text: qsTr("Renk kalibrasyonu") }
    Loader {
        Layout.fillWidth: true
        active: whiteBalance.checked
        sourceComponent: Section {
        Layout.fillWidth: true
        title: qsTr("Beyaz dengesi")
        subtitle: qsTr("LED’lerin renk dengesini klavyede gördüğünüz beyaza göre ayarlayın. Kalibrasyon statik renklere ve KDE vurgu rengine uygulanır.")
        QQC2.Label { text: qsTr("Beyaz noktası · R / G / B: %1").arg(device.info.keyboard_white_point || "—"); Layout.fillWidth: true; wrapMode: Text.WordWrap }
        QQC2.Label { text: device.info.keyboard_color_profile_active === "1" ? qsTr("Ekran renk eşleştirmesi etkin · %1").arg(device.info.saved_keyboard_color_profile || qsTr("Özel")) : qsTr("Beyaz dengesi kullanılıyor"); Layout.fillWidth: true; wrapMode: Text.WordWrap }
        QQC2.Button { text: qsTr("Ekranla 20 renk eşleştir…"); enabled: device.info.keyboard_color_profile_active !== undefined && !control.busy; onClicked: device.openColorCalibration() }
        QQC2.Button { text: qsTr("Beyaz dengesini ayarla…"); enabled: !!device.info.keyboard_white_point && !control.busy; onClicked: device.openCalibration() }
        QQC2.Button { text: qsTr("Renk düzeltmesini sıfırla"); enabled: !!device.info.keyboard_white_point && !control.busy; onClicked: control.apply(["keyboard", "color-profile", "reset"], ["keyboard_white_point", "255,255,255", "keyboard_color_profile", "off"]) }
    }    }
    QQC2.TabBar {
        id: tabs; objectName: "lightingTabs"; Layout.fillWidth: true; visible: root.custom
        QQC2.TabButton { text: qsTr("Renkler") }
        QQC2.TabButton { text: qsTr("Efektler") }
        QQC2.TabButton { text: qsTr("Profiller") }
    }
    Loader {
        Layout.fillWidth: true
        active: root.custom
        sourceComponent: tabs.currentIndex === 0 ? colors : tabs.currentIndex === 1 ? effects : profiles
    }
    Component {
        id: colors
        Section {
            title: qsTr("Klavye renkleri"); subtitle: qsTr("Bir tuş seçin veya rengi tüm klavyeye gönderin. Dar ekranda klavyeyi yatay kaydırabilirsiniz.")
            Setting { title: qsTr("Fiziksel düzen"); options: ["iso", "ansi"]; labels: [qsTr("ISO · L biçimli Enter"), qsTr("ANSI · yatay Enter")]; current: device.info.keyboard_physical_format || "iso"; onChosen: value => device.setKeyboardFormat(value) }
            Keyboard { Layout.fillWidth: true; selected: root.selectedKey; onChosen: key => { root.selectedKey = key; root.allKeys = false } }
            QQC2.ComboBox {
                Layout.fillWidth: true
                model: [qsTr("Renk hedefi: tüm klavye"), qsTr("Renk hedefi: seçili tuş")]
                currentIndex: root.allKeys ? 0 : 1
                Accessible.name: qsTr("Renk hedefi")
                onActivated: root.allKeys = currentIndex === 0
            }
            RowLayout {
                Layout.fillWidth: true
                Rectangle {
                    implicitWidth: Kirigami.Units.gridUnit * 2
                    implicitHeight: implicitWidth
                    radius: Kirigami.Units.cornerRadius
                    color: "#" + root.colorHex
                    border.color: Kirigami.Theme.disabledTextColor
                    Accessible.ignored: true
                }
                QQC2.TextField {
                    id: hex
                    property bool edited: false
                    Layout.fillWidth: true
                    text: root.colorHex
                    maximumLength: 6
                    validator: RegularExpressionValidator { regularExpression: /[0-9A-Fa-f]{6}/ }
                    Accessible.name: qsTr("RGB renk kodu")
                    onTextEdited: edited = true
                    onEditingFinished: {
                        if (!edited || !acceptableInput) return
                        edited = false
                        root.colorHex = text.toUpperCase()
                        root.commitColor()
                    }
                }
                QQC2.Button { text: qsTr("Renk seç"); onClicked: picker.open() }
            }
            ColorDialog { id: picker; title: qsTr("Klavye rengi"); selectedColor: "#" + root.colorHex; onAccepted: { root.colorHex = selectedColor.toString().slice(1).toUpperCase(); root.commitColor() } }
            QQC2.Label { text: qsTr("Seçili tuş: %1. Renk seçildiğinde hedefe uygulanır; tuş seçmek ışığı değiştirmez.").arg(root.selectedKey); wrapMode: Text.WordWrap; Layout.fillWidth: true }
            QQC2.Label {
                text: qsTr("Tuş konumları R9T TR klavyeye göre çizilir. LED eşlemesi doğrulanmamış tuşlarda kalibrasyon gerekebilir.")
                wrapMode: Text.WordWrap; Layout.fillWidth: true
                color: Kirigami.Theme.disabledTextColor
            }
            QQC2.CheckBox { id: calibration; text: qsTr("Tuş eşlemesini düzelt") }
            Loader {
                Layout.fillWidth: true; active: calibration.checked
                sourceComponent: ColumnLayout {
                    QQC2.Label { text: qsTr("LED’i göstermek çalışan efekti kesebilir."); wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    QQC2.SpinBox { id: led; from: 0; to: 125; Accessible.name: qsTr("LED indeksi") }
                    QQC2.Button { text: qsTr("LED’i göster"); enabled: device.ready("rgb"); onClicked: control.apply(["keyboard", "identify", String(led.value)]) }
                    QQC2.Button { text: qsTr("Seçili tuşa eşle"); enabled: device.ready("rgb"); onClicked: control.apply(["keyboard", "layout", "set", root.selectedKey, String(led.value)]) }
                }
            }
        }
    }
    Component {
        id: effects
        Section {
            title: qsTr("Donanım ışık efektleri"); subtitle: qsTr("Efekt klavyenin denetleyicisinde çalışır; bilgisayarda sürekli animasyon işlemi gerekmez.")
            QQC2.ComboBox { id: effect; Layout.fillWidth: true; model: [qsTr("Dalga"), qsTr("Nefes"), qsTr("Gökkuşağı"), qsTr("Tuşa tepki"), qsTr("Halka"), qsTr("Kayan ışık"), qsTr("Yağmur"), qsTr("Kutup ışıkları"), qsTr("Kıvılcım")]; Accessible.name: qsTr("Efekt") }
            QQC2.Label { text: qsTr("Hız") }
            QQC2.SpinBox { id: speed; from: 1; to: 10; value: 5; Accessible.name: qsTr("Efekt hızı") }
            QQC2.Label { text: qsTr("Efekt parlaklığı (%)") }
            QQC2.SpinBox { id: brightness; from: 0; to: 100; value: 50; Accessible.name: qsTr("Efekt parlaklığı") }
            QQC2.ComboBox { id: direction; Layout.fillWidth: true; enabled: effect.currentIndex === 0; model: [qsTr("Sağ"), qsTr("Sol"), qsTr("Yukarı"), qsTr("Aşağı")]; Accessible.name: qsTr("Dalga yönü") }
            QQC2.Button {
                text: qsTr("Efekti başlat"); enabled: device.ready("hardware-rgb") && !control.busy
                onClicked: { let args = ["keyboard", "hardware", root.effects[effect.currentIndex], "--speed", String(speed.value), "--brightness", String(brightness.value)]; if (effect.currentIndex === 0) args.push("--direction", ["right", "left", "up", "down"][direction.currentIndex]); control.apply(args) }
            }
            QQC2.Button { text: qsTr("Işığı kapat"); enabled: device.ready("hardware-rgb") && !control.busy; onClicked: control.apply(["keyboard", "hardware", "off"]) }
            QQC2.TextField { id: effectName; Layout.fillWidth: true; placeholderText: qsTr("Efekt profili adı"); validator: RegularExpressionValidator { regularExpression: /[A-Za-z0-9_-]{1,32}/ } Accessible.name: placeholderText }
            QQC2.Button { text: qsTr("Profili kaydet"); enabled: effectName.acceptableInput && !control.busy; onClicked: control.apply(["keyboard", "profile", "effect", effectName.text, root.effects[effect.currentIndex], String(speed.value), String(brightness.value), ["right", "left", "up", "down"][direction.currentIndex]]) }
        }
    }
    Component {
        id: profiles
        Section {
            title: qsTr("Kayıtlı profiller"); subtitle: qsTr("Kaydetmek adlandırılmış profili değiştirir; uygulamak klavyeyi değiştirir.")
            QQC2.ComboBox { id: profile; Layout.fillWidth: true; model: device.rgbProfiles; Accessible.name: qsTr("RGB profili") }
            QQC2.Button { text: qsTr("Profili kullan"); enabled: profile.currentText.length > 0 && device.ready("rgb") && !control.busy; onClicked: control.apply(["keyboard", "profile", "apply", profile.currentText], ["rgb_profile", profile.currentText]) }
            QQC2.Button { text: qsTr("Profilde seçili tuşu güncelle"); enabled: profile.currentText.length > 0 && !control.busy; onClicked: control.apply(["keyboard", "profile", "set-key", profile.currentText, root.selectedKey, root.colorHex]) }
            QQC2.ComboBox { id: group; Layout.fillWidth: true; model: [qsTr("WASD"), qsTr("Ok tuşları"), qsTr("Tüm tuşlar")]; Accessible.name: qsTr("Profil tuş grubu") }
            QQC2.Button { text: qsTr("Seçili renkle grubu kaydet"); enabled: profile.currentText.length > 0 && !control.busy; onClicked: control.apply(["keyboard", "profile", "set-group", profile.currentText, ["wasd", "arrows", "all"][group.currentIndex], root.colorHex]) }
            QQC2.Label { text: qsTr("Kaydedilecek renk: #%1 · Seçili tuş: %2").arg(root.colorHex).arg(root.selectedKey); wrapMode: Text.WordWrap; Layout.fillWidth: true }
            QQC2.Button { text: qsTr("Profil içeriğini oku"); enabled: profile.currentText.length > 0; onClicked: control.diagnose("profile:" + profile.currentText) }
            QQC2.TextArea { Layout.fillWidth: true; text: control.output; visible: text.length > 0; readOnly: true; wrapMode: TextEdit.WrapAnywhere; Accessible.name: qsTr("Profil içeriği") }
            QQC2.TextField { id: name; Layout.fillWidth: true; placeholderText: qsTr("Yeni profil adı"); validator: RegularExpressionValidator { regularExpression: /[A-Za-z0-9_-]{1,32}/ } Accessible.name: placeholderText }
            QQC2.Button { text: qsTr("Seçili renkle yeni profil oluştur"); enabled: name.acceptableInput && !control.busy; onClicked: control.apply(["keyboard", "profile", "create", name.text, root.colorHex, String(device.info.keyboard_brightness || 50)]) }
            QQC2.Button { text: qsTr("Klavyenin renklerini kaydet"); enabled: name.acceptableInput && device.ready("rgb") && !control.busy; onClicked: control.apply(["keyboard", "profile", "capture", name.text]) }
        }
    }
}
