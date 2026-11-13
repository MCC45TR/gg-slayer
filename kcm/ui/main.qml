// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: page
    function ready(id) {
        var list = kcm.capabilities
        for (var i = 0; i < list.length; ++i) if (list[i].id === id) return list[i].state === "ready"
        return false
    }
    function reason(id) {
        var list = kcm.capabilities
        for (var i = 0; i < list.length; ++i) if (list[i].id === id) return list[i].reason
        return ""
    }
    property int section: 0
    property string selectedKey: "W"
    property string selectedProfile: ""
    implicitWidth: 900
    implicitHeight: 640
    ColumnLayout {
        width: parent.width
        spacing: Kirigami.Units.largeSpacing
        RowLayout {
            Layout.fillWidth: true
            Kirigami.Heading { text: "Slayer Control"; level: 1; Layout.fillWidth: true }
            QQC2.BusyIndicator { running: kcm.busy; visible: running; implicitWidth: 28; implicitHeight: 28 }
            QQC2.Button { text: "Yenile"; icon.name: "view-refresh"; enabled: !kcm.busy; onClicked: kcm.refresh() }
        }
        QQC2.TabBar {
            Layout.fillWidth: true
            onCurrentIndexChanged: page.section = currentIndex
            QQC2.TabButton { text: "Genel Bakış" }
            QQC2.TabButton { text: "Klavye / RGB" }
            QQC2.TabButton { text: "Profiller" }
            QQC2.TabButton { text: "Güç / Pil" }
            QQC2.TabButton { text: "Tanı / Destek" }
        }
        QQC2.Label { text: "slayerctl " + (kcm.info.version || "…") + " · Arka planda sürekli çalışan bir hizmet gerektirmez."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: ((kcm.info.nvidia || "") !== "loaded" && (kcm.info.mux_mode || "") !== "integrated") || (kcm.info.keyboard_driver || "") !== "ite_8291"
            type: Kirigami.MessageType.Warning
            text: "NVIDIA veya klavye sürücüsü yüklü değil. Güvenli Önyükleme anahtar kaydını tamamlayıp yeniden başlatmanız gerekebilir."
        }
        Kirigami.InlineMessage { Layout.fillWidth: true; visible: kcm.message.length > 0; text: kcm.message; type: Kirigami.MessageType.Information }
        Kirigami.InlineMessage { Layout.fillWidth: true; visible: (kcm.info.hardware_writes || "") === "blocked"; text: "Donanım yazmaları kilitli: " + (kcm.info.hardware_write_reason || ""); type: Kirigami.MessageType.Error }
        QQC2.GroupBox {
            title: "Cihaz"; Layout.fillWidth: true; visible: page.section === 0
            Kirigami.FormLayout {
                anchors.fill: parent
                QQC2.Label { Kirigami.FormData.label: "Model:"; text: (kcm.info.model || "—") + " · " + (kcm.info.sku || "") }
                QQC2.Label { Kirigami.FormData.label: "BIOS:"; text: (kcm.info.bios || "—") + " · " + (kcm.info.bios_date || "") }
                QQC2.Label { Kirigami.FormData.label: "Desteklenen BIOS:"; text: kcm.info.supported_bios || "—" }
                QQC2.Label { Kirigami.FormData.label: "İşlemci:"; text: kcm.info.cpu || "—"; wrapMode: Text.WordWrap }
                QQC2.Label { Kirigami.FormData.label: "GPU sürücüsü:"; text: kcm.info.nvidia || "—" }
                QQC2.Label { Kirigami.FormData.label: "İç ekran:"; text: kcm.info.panel || "—" }
                QQC2.Label { Kirigami.FormData.label: "Pil:"; text: (kcm.info.battery || "—") + "% · " + (kcm.info.battery_status || "") }
            }
        }
        QQC2.GroupBox {
            title: "Klavye ve RGB"; Layout.fillWidth: true; visible: page.section === 1
            ColumnLayout {
                anchors.fill: parent
                QQC2.Label { text: page.ready("keyboard") ? "Parlaklık ve renkler Linux'un yerleşik klavye arayüzünden yönetilir." : page.reason("keyboard"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
                RowLayout {
                    QQC2.Label { text: "Parlaklık (%)" }
                    QQC2.SpinBox { id: brightness; from: 0; to: 100; value: parseInt(kcm.info.keyboard_brightness || "50"); editable: true }
                    QQC2.Button { text: "Uygula"; enabled: page.ready("keyboard") && !kcm.busy; onClicked: kcm.run(["keyboard", "brightness", brightness.value.toString()], true) }
                }
                RowLayout {
                    QQC2.Label { text: "Renk" }
                    QQC2.TextField { id: color; text: "3DAEE9"; maximumLength: 6; placeholderText: "RRGGBB"; validator: RegularExpressionValidator { regularExpression: /[0-9a-fA-F]{6}/ } }
                    QQC2.Button { text: "Uygula"; enabled: page.ready("rgb") && color.acceptableInput && !kcm.busy; onClicked: kcm.run(["keyboard", "color", color.text], true) }
                }
                QQC2.Label { text: "Fiziksel klavye · " + (kcm.info.keyboard_layout || "Düzen bekleniyor"); font.bold: true }
                QQC2.ComboBox { model: ["ISO · L biçimli Enter", "ANSI · Yatay Enter"]; currentIndex: kcm.info.keyboard_physical_format === "ansi" ? 1 : 0; onActivated: kcm.setKeyboardFormat(currentIndex === 1 ? "ansi" : "iso") }
                QQC2.Label { text: "Tuşa tıklayın, üstteki renkle boyayın. LED eşleşmeleri kalibre edilene kadar taslaktır; yanlış tuş yanarsa aşağıdan düzeltin."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                QQC2.ScrollView {
                    Layout.fillWidth: true; implicitHeight: 255
                    contentWidth: 840; contentHeight: 250
                    Item {
                        width: 840; height: 250
                        Repeater {
                            model: kcm.keys
                            delegate: QQC2.Button {
                                id: physicalKey
                                required property var modelData
                                property color keyFill: checked ? Qt.tint(Kirigami.Theme.backgroundColor, Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.3)) : modelData.rgb ? Qt.tint(Kirigami.Theme.backgroundColor, Qt.rgba(modelData.rgb[0] / 255, modelData.rgb[1] / 255, modelData.rgb[2] / 255, 0.25)) : Kirigami.Theme.backgroundColor
                                background: Item {
                                    Rectangle { width: parent.width; height: physicalKey.modelData.shape === "iso-enter" ? 36 : parent.height; radius: 3; color: physicalKey.keyFill; border.color: physicalKey.checked ? Kirigami.Theme.highlightColor : Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.2) }
                                    Rectangle { visible: physicalKey.modelData.shape === "iso-enter"; x: 10; y: 35; width: parent.width - 10; height: parent.height - 35; radius: 3; color: physicalKey.keyFill; border.color: physicalKey.checked ? Kirigami.Theme.highlightColor : Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.2) }
                                    Rectangle { visible: physicalKey.modelData.shape === "iso-enter"; x: 11; y: 34; width: parent.width - 12; height: 3; color: physicalKey.keyFill }
                                }
                                visible: modelData.present !== false
                                x: modelData.x * 40; y: modelData.y * 38
                                width: modelData.w * 40 - 3; height: modelData.h * 38 - 3
                                text: modelData.label; font.pixelSize: 11
                                checkable: true; checked: page.selectedKey === modelData.id
                                QQC2.ToolTip.visible: hovered
                                QQC2.ToolTip.text: modelData.id + " · LED " + modelData.led + (modelData.calibrated ? " · Kalibre edildi" : " · Taslak eşleme") + (modelData.shift ? " · Shift: " + modelData.shift : "") + (modelData.altgr ? " · AltGr: " + modelData.altgr : "")
                                onClicked: { page.selectedKey = modelData.id; keyIndex.value = modelData.led }
                            }
                        }
                    }
                }
                Flow {
                    Layout.fillWidth: true; spacing: 6
                    QQC2.Label { text: "Seçili tuş: " + page.selectedKey; topPadding: 8 }
                    QQC2.Button { text: "Tuşu renklendir"; enabled: page.ready("rgb") && color.acceptableInput && !kcm.busy; onClicked: kcm.run(["keyboard", "key", page.selectedKey, color.text], true) }
                    QQC2.SpinBox { id: keyIndex; from: 0; to: 125; value: 66; editable: true }
                    QQC2.Button { text: "LED'i 2 saniye göster"; enabled: page.ready("rgb") && !kcm.busy; onClicked: kcm.run(["keyboard", "identify", keyIndex.value.toString()], true) }
                    QQC2.Button { text: "Eşlemeyi kaydet"; enabled: !kcm.busy; onClicked: kcm.run(["keyboard", "layout", "set", page.selectedKey, keyIndex.value.toString()], true) }
                }
                RowLayout {
                    QQC2.Button { text: "Vurgu rengini takip et"; enabled: page.ready("rgb") && !kcm.busy; onClicked: kcm.run(["keyboard", "accent", "on"]) }
                    QQC2.Button { text: "Renk eşlemesini kapat"; enabled: !kcm.busy; onClicked: kcm.run(["keyboard", "accent", "off"]) }
                }
                QQC2.Label { text: "Animasyonlar klavye denetleyicisinde çalışır. Aşağıdaki donanım efektlerini kullanın; özel renk/efekt öncesinde vurgu eşlemesini kapatın."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }
        QQC2.GroupBox {
            title: "RGB profilleri"; Layout.fillWidth: true; visible: page.section === 2
            ColumnLayout {
                anchors.fill: parent
                QQC2.Label { text: "Yeni statik profil Klavye / RGB sayfasındaki renk ve parlaklığı kullanır. Seçili fiziksel tuş için ayrı renk kaydedebilirsiniz. Dosyalar sade metindir; kaydetmek donanımı değiştirmez."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Flow {
                    Layout.fillWidth: true; spacing: 6
                    QQC2.TextField { id: profileName; placeholderText: "Profil adı"; maximumLength: 32; validator: RegularExpressionValidator { regularExpression: /[a-zA-Z0-9_-]{1,32}/ } }
                    QQC2.Button { text: "Statik profil oluştur"; enabled: profileName.acceptableInput && color.acceptableInput && !kcm.busy; onClicked: { page.selectedProfile = profileName.text; kcm.run(["keyboard", "profile", "create", profileName.text, color.text, brightness.value.toString()], true) } }
                    QQC2.Button { text: "Statik renkleri kopyala"; enabled: profileName.acceptableInput && page.ready("rgb") && !kcm.busy; onClicked: { page.selectedProfile = profileName.text; kcm.run(["keyboard", "profile", "capture", profileName.text], true) } }
                    QQC2.ComboBox { id: rgbProfile; model: kcm.rgbProfiles; currentIndex: Math.max(0, kcm.rgbProfiles.indexOf(page.selectedProfile)); onActivated: page.selectedProfile = currentText }
                }
                Flow {
                    Layout.fillWidth: true; spacing: 6
                    QQC2.Button { text: "Seçili tuşun rengini profile kaydet"; enabled: rgbProfile.currentText.length > 0 && color.acceptableInput && !kcm.busy; onClicked: kcm.run(["keyboard", "profile", "set-key", rgbProfile.currentText, page.selectedKey, color.text], true) }
                    QQC2.Button { text: "WASD rengini kaydet"; enabled: rgbProfile.currentText.length > 0 && color.acceptableInput && !kcm.busy; onClicked: kcm.run(["keyboard", "profile", "set-group", rgbProfile.currentText, "wasd", color.text], true) }
                    QQC2.Button { text: "Okların rengini kaydet"; enabled: rgbProfile.currentText.length > 0 && color.acceptableInput && !kcm.busy; onClicked: kcm.run(["keyboard", "profile", "set-group", rgbProfile.currentText, "arrows", color.text], true) }
                    QQC2.Button { text: "Arka plan/parlaklığı kaydet"; enabled: rgbProfile.currentText.length > 0 && color.acceptableInput && !kcm.busy; onClicked: kcm.run(["keyboard", "profile", "base", rgbProfile.currentText, color.text, brightness.value.toString()], true) }
                    QQC2.Button { text: "Profili uygula"; enabled: rgbProfile.currentText.length > 0 && !kcm.busy; onClicked: kcm.run(["keyboard", "profile", "apply", rgbProfile.currentText], true) }
                    QQC2.Button { text: "Açılış profili yap"; enabled: rgbProfile.currentText.length > 0 && !kcm.busy; onClicked: kcm.run(["config", "set", "rgb_profile", rgbProfile.currentText], true) }
                    QQC2.Button { text: "Açılış RGB profilini bırak"; enabled: !kcm.busy; onClicked: kcm.run(["config", "unset", "rgb_profile"], true) }
                    QQC2.Button { text: "İçeriği göster"; enabled: rgbProfile.currentText.length > 0 && !kcm.busy; onClicked: kcm.run(["keyboard", "profile", "show", rgbProfile.currentText]) }
                    QQC2.Button { text: "Profili sil"; enabled: rgbProfile.currentText.length > 0 && !kcm.busy; onClicked: kcm.run(["keyboard", "profile", "delete", rgbProfile.currentText], true) }
                }
            }
        }
        QQC2.GroupBox {
            title: "Klavyede çalışan efektler"; Layout.fillWidth: true; visible: page.section === 1
            ColumnLayout {
                anchors.fill: parent
                QQC2.Label { text: "Efekt başlatılınca slayerctl kapanır; animasyonu klavyenin denetleyicisi yürütür. Sürekli CPU/RAM kullanımı yoktur. Flash'a yazılmaz. KDE vurgu eşlemesini önce kapatın; sonraki statik renk/parlaklık yazması efekti sonlandırabilir."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Flow {
                    Layout.fillWidth: true; spacing: 6
                    QQC2.ComboBox { id: hardwareEffect; model: ["wave", "breathe", "rainbow", "reactive", "ripple", "marquee", "raindrop", "aurora", "spark"] }
                    QQC2.Label { text: "Hız"; topPadding: 8 }
                    QQC2.SpinBox { id: hwSpeed; from: 1; to: 10; value: 5 }
                    QQC2.Label { text: "Parlaklık %"; topPadding: 8 }
                    QQC2.SpinBox { id: hwBrightness; from: 0; to: 100; value: 50 }
                    QQC2.ComboBox { id: hwDirection; model: ["right", "left", "up", "down"]; enabled: hardwareEffect.currentText === "wave" }
                }
                Flow {
                    Layout.fillWidth: true; spacing: 6
                    QQC2.Button { text: "Donanım efektini başlat"; enabled: page.ready("hardware-rgb") && !kcm.busy; onClicked: { var args = ["keyboard", "hardware", hardwareEffect.currentText, "--speed", hwSpeed.value.toString(), "--brightness", hwBrightness.value.toString()]; if (hardwareEffect.currentText === "wave") args.push("--direction", hwDirection.currentText); kcm.run(args, true) } }
                    QQC2.Button { text: "Efekti kapat"; enabled: page.ready("hardware-rgb") && !kcm.busy; onClicked: kcm.run(["keyboard", "hardware", "off"], true) }
                    QQC2.Button { text: "Efekti profil olarak kaydet"; enabled: profileName.acceptableInput && !kcm.busy; onClicked: { page.selectedProfile = profileName.text; kcm.run(["keyboard", "profile", "effect", profileName.text, hardwareEffect.currentText, hwSpeed.value.toString(), hwBrightness.value.toString(), hwDirection.currentText], true) } }
                }
                QQC2.Label { visible: !page.ready("hardware-rgb"); text: page.reason("hardware-rgb"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }
        QQC2.GroupBox {
            title: "Pil dolum sınırı"; Layout.fillWidth: true; visible: page.section === 3
            ColumnLayout {
                anchors.fill: parent
                QQC2.Label { text: page.reason("battery-limit"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
                RowLayout {
                    QQC2.Label { text: "Alt (%)" }
                    QQC2.SpinBox { id: lower; from: 0; to: 95; value: parseInt(kcm.info.battery_start || "60"); editable: true }
                    QQC2.Label { text: "Üst (%)" }
                    QQC2.SpinBox { id: upper; from: 50; to: 100; value: parseInt(kcm.info.battery_limit || "80"); editable: true }
                    QQC2.Button { text: "Uygula"; enabled: page.ready("battery-limit") && lower.value < upper.value && !kcm.busy; onClicked: kcm.run(["battery", "limit", upper.value.toString(), lower.value.toString()], true) }
                }
            }
        }
        QQC2.GroupBox {
            title: "OEM pil koruma profili"; Layout.fillWidth: true; visible: page.section === 3
            ColumnLayout {
                anchors.fill: parent
                QQC2.Label { text: page.reason("battery-profile"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
                RowLayout {
                    QQC2.ComboBox { id: batteryProfile; model: ["high-capacity", "balanced", "stationary"]; currentIndex: Math.max(0, model.indexOf(kcm.info.battery_profile || "")) }
                    QQC2.Button { text: "Uygula"; enabled: page.ready("battery-profile") && !kcm.busy; onClicked: kcm.run(["battery", "profile", batteryProfile.currentText], true) }
                    QQC2.Button { text: "Açılış / uyanma için kaydet"; enabled: page.ready("battery-profile") && !kcm.busy; onClicked: kcm.run(["config","set","battery_profile",batteryProfile.currentText],true) }
                }
            }
        }
        QQC2.GroupBox {
            title: "Güç profili ve fan"; Layout.fillWidth: true; visible: page.section === 3
            ColumnLayout {
                anchors.fill: parent
                QQC2.Label { text: "Geçerli profil: " + (kcm.info.profile || "Kernel arayüzü yok"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
                RowLayout {
                    QQC2.ComboBox { id: profile; model: (kcm.info.profile_choices || "").split(" ").filter(function(s) { return s.length > 0 }) }
                    QQC2.Button { text: "Profili uygula"; enabled: page.ready("profile") && !kcm.busy; onClicked: kcm.run(["profile", "set", profile.currentText], true) }
                    QQC2.Button { text: "Fan otomatik"; enabled: page.ready("fan") && !kcm.busy; onClicked: kcm.run(["fan", "auto"], true) }
                    QQC2.Button { text: "Fan Boost"; enabled: page.ready("fan") && !kcm.busy; onClicked: kcm.run(["fan", "boost"], true) }
                }
                QQC2.Label { text: page.reason("fan"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }
        QQC2.GroupBox {
            title: "CPU boost / frekans"; Layout.fillWidth: true; visible: page.section === 3
            ColumnLayout {
                anchors.fill: parent
                Flow { Layout.fillWidth: true; spacing: 6
                    QQC2.Button { text: "Boost aç"; enabled: page.ready("cpu-boost") && !kcm.busy; onClicked: kcm.run(["cpu","boost","on"],true) }
                    QQC2.Button { text: "Boost kapat"; enabled: page.ready("cpu-boost") && !kcm.busy; onClicked: kcm.run(["cpu","boost","off"],true) }
                    QQC2.SpinBox { id: cpuMin; from: 100; to: 10000; value: 500; editable: true }
                    QQC2.SpinBox { id: cpuMax; from: 100; to: 10000; value: 2500; editable: true }
                    QQC2.Button { text: "MHz sınırlarını uygula"; enabled: page.ready("cpu-frequency") && !kcm.busy && cpuMin.value <= cpuMax.value; onClicked: kcm.run(["cpu","limits",cpuMin.value.toString(),cpuMax.value.toString()],true) }
                    QQC2.Button { text: "Kernel sınırlarına dön"; enabled: page.ready("cpu-frequency") && !kcm.busy; onClicked: kcm.run(["cpu","limits","reset"],true) }
                    QQC2.Button { text: "MHz sınırlarını kaydet"; enabled: page.ready("cpu-frequency") && !kcm.busy && cpuMin.value <= cpuMax.value; onClicked: kcm.run(["config","set","cpu_min_mhz",cpuMin.value.toString(),"cpu_max_mhz",cpuMax.value.toString()],true) }
                }
                QQC2.Label { text: "Her policy için gerçek kernel sınırı doğrulanır. Watt/SMU/voltaj kontrolü değildir; masaüstü güç yöneticisi değiştirebilir."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }
        QQC2.GroupBox {
            title: "NVIDIA güç limitleri"; Layout.fillWidth: true; visible: page.section === 3
            ColumnLayout {
                anchors.fill: parent
                QQC2.Button { text: "Sürücü limitlerini oku"; enabled: !kcm.busy; onClicked: kcm.run(["gpu","limits"]) }
                QQC2.SpinBox { id: gpuWatts; from: 1; to: 10000; value: 50; editable: true }
                QQC2.Button { text: "Güç limiti isteği gönder"; enabled: page.ready("model-bios") && !kcm.busy && kcm.output.indexOf("power_settable: candidate") >= 0; onClicked: kcm.run(["gpu","power-limit",gpuWatts.value.toString()],true) }
                QQC2.Label { text: "Min/max yeterli değildir. Mevcut limit N/A ise güç yazması reddedilir."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                QQC2.TextArea { text: kcm.output; readOnly: true; selectByMouse: true; Layout.fillWidth: true; wrapMode: Text.Wrap }
            }
        }
        QQC2.GroupBox {
            title: "Fan eğrisi dosyası"; Layout.fillWidth: true; visible: page.section === 3
            ColumnLayout {
                anchors.fill: parent
                QQC2.TextField { id: curveName; placeholderText: "Eğri adı"; maximumLength: 32 }
                QQC2.TextField { id: cpuCurve; text: "40:30,60:60,90:100"; Layout.fillWidth: true }
                QQC2.TextField { id: gpuCurve; text: "40:30,65:65,90:100"; Layout.fillWidth: true }
                QQC2.Button { text: "Çevrimdışı eğriyi kaydet"; enabled: !kcm.busy; onClicked: kcm.run(["fan","curve","save",curveName.text,cpuCurve.text,gpuCurve.text],true) }
                QQC2.Label { text: "Dosya hazırlamak fanı değiştirmez. Donanım eğrisi uygulaması henüz kapalıdır."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }
        QQC2.GroupBox {
            title: "AC/DC RGB profilleri"; Layout.fillWidth: true; visible: page.section === 2
            ColumnLayout {
                anchors.fill: parent
                QQC2.ComboBox { id: acRgbProfile; model: kcm.rgbProfiles }
                QQC2.ComboBox { id: dcRgbProfile; model: kcm.rgbProfiles }
                QQC2.Button { text: "Adaptör / pil sırasıyla kaydet"; enabled: !kcm.busy && acRgbProfile.currentText.length > 0 && dcRgbProfile.currentText.length > 0; onClicked: kcm.run(["config","set","rgb_profile_ac",acRgbProfile.currentText,"rgb_profile_battery",dcRgbProfile.currentText],true) }
                QQC2.Label { text: "Adaptör değişiminde systemd tek sefer uygular; sürekli polling yoktur."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }
        QQC2.GroupBox {
            title: "Adaptör / pil masaüstü güç profilleri"; Layout.fillWidth: true; visible: page.section === 3
            ColumnLayout {
                anchors.fill: parent
                QQC2.Label { text: "KDE / GNOME'un mevcut güç servisini kullanır. Kaydetmek anında uygulamaz."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                RowLayout {
                    QQC2.Label { text: "Adaptörde" }
                    QQC2.ComboBox { id: acDesktopPower; model: ["power-saver","balanced","performance"]; currentIndex: 1 }
                    QQC2.Label { text: "Pilde" }
                    QQC2.ComboBox { id: dcDesktopPower; model: ["power-saver","balanced","performance"] }
                }
                Flow { Layout.fillWidth: true; spacing: 6
                    QQC2.Button { text: "Güç seçimlerini kaydet"; enabled: page.ready("acdc-profiles") && !kcm.busy; onClicked: kcm.run(["config","set","desktop_profile_ac",acDesktopPower.currentText,"desktop_profile_battery",dcDesktopPower.currentText],true) }
                    QQC2.Button { text: "Kaydedilmiş ayarları uygula"; enabled: page.ready("model-bios") && !kcm.busy; onClicked: kcm.run(["apply-config"],true) }
                    QQC2.Button { text: "AC/DC otomasyonunu bırak"; enabled: !kcm.busy; onClicked: kcm.run(["config","unset","rgb_profile_ac","rgb_profile_battery","desktop_profile_ac","desktop_profile_battery"],true) }
                }
            }
        }
        QQC2.GroupBox {
            title: "Grafik geçişi (MUX)"; Layout.fillWidth: true; visible: page.section === 3
            ColumnLayout {
                anchors.fill: parent
                QQC2.Label { text: page.reason("mux"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
                QQC2.Label { text: "Kaydedilen seçim: " + (kcm.info.mux_mode || "—"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
                QQC2.ComboBox { id: muxMode; textRole: "label"; valueRole: "mode"; model: [{label:"Hibrit",mode:"hybrid"},{label:"Yalnız NVIDIA",mode:"dedicated"},{label:"Yalnız AMD iGPU",mode:"integrated"}] }
                QQC2.Button { text: "Sonraki açılış için seç"; enabled: page.ready("mux") && !kcm.busy; onClicked: kcm.run(["gpu","mode",muxMode.currentValue],true) }
                QQC2.Label { text: "Uygulama yeniden başlatmaz. Geçiş sonraki açılışta olur; yalnız iGPU seçilirse NVIDIA'ya bağlı harici portlar kapanabilir. Hibrit'e bu menüden veya BIOS'tan dönün."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                QQC2.Button { text: "KDE ekran ayarları"; onClicked: kcm.openSettings("display") }
                QQC2.Button { text: "KDE touchpad ayarları"; onClicked: kcm.openSettings("touchpad") }
            }
        }
        QQC2.GroupBox {
            title: "Fn, kamera ve kapalı USB şarj"; Layout.fillWidth: true; visible: page.section === 3
            ColumnLayout {
                anchors.fill: parent
                Repeater {
                    model: [{label:"Fn kilidi",setting:"fn-lock",key:"fn_lock",cap:"fn"},{label:"Dahili kamera RGB / IR",setting:"camera",key:"camera",cap:"camera"},{label:"Kapalı USB şarj",setting:"usb-charging",key:"usb_charging",cap:"usb-charging"}]
                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        QQC2.Label { text: modelData.label + " · " + (kcm.info[modelData.key] || "—"); font.bold: true }
                        Flow { Layout.fillWidth: true; spacing: 6
                            QQC2.ComboBox { id: settingValue; model: ["on","off"] }
                            QQC2.Button { text: "Uygula"; enabled: page.ready(modelData.cap) && !kcm.busy; onClicked: kcm.run(["settings",modelData.setting,settingValue.currentText],true) }
                            QQC2.Button { text: "Açılış / uyanma için kaydet"; enabled: page.ready(modelData.cap) && !kcm.busy; onClicked: kcm.run(["config","set",modelData.key,settingValue.currentText],true) }
                        }
                        QQC2.Label { text: page.reason(modelData.cap); wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    }
                }
            }
        }
        QQC2.GroupBox {
            title: "SMT / iş parçacıkları"; Layout.fillWidth: true; visible: page.section === 3
            ColumnLayout {
                anchors.fill: parent
                QQC2.Label { text: "SMT: " + (kcm.info.cpu_smt || "—") + " · Online CPU: " + (kcm.info.cpu_online || "—"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
                QQC2.Label { text: "Açık: 32, kapalı: 16 iş parçacığı. 16 fiziksel çekirdek korunur. Fiziksel CCD kapatma BIOS'tan yapılır."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Flow { Layout.fillWidth: true; spacing: 6
                    QQC2.ComboBox { id: smtValue; model: ["on","off"] }
                    QQC2.Button { text: "SMT uygula"; enabled: page.ready("cpu-smt") && !kcm.busy; onClicked: kcm.run(["cpu","smt",smtValue.currentText],true) }
                    QQC2.Button { text: "Kalıcı kaydet"; enabled: page.ready("cpu-smt") && !kcm.busy; onClicked: kcm.run(["config","set","cpu_smt",smtValue.currentText],true) }
                    QQC2.Button { text: "Topoloji / CCD oku"; enabled: !kcm.busy; onClicked: kcm.run(["cpu","topology"]) }
                }
            }
        }
        QQC2.GroupBox {
            title: "USB ve tanılama"; Layout.fillWidth: true; visible: page.section === 4
            ColumnLayout {
                anchors.fill: parent
                RowLayout {
                    QQC2.Button { text: "USB aygıtları"; enabled: !kcm.busy; onClicked: kcm.run(["usb", "list"]) }
                    QQC2.Button { text: "Sensörler"; enabled: !kcm.busy; onClicked: kcm.run(["sensors"]) }
                    QQC2.Button { text: "Denetle"; enabled: !kcm.busy; onClicked: kcm.run(["doctor"]) }
                }
                RowLayout {
                    QQC2.TextField { id: usbPort; placeholderText: "Port (ör. 3-2.1)"; maximumLength: 32; validator: RegularExpressionValidator { regularExpression: /[0-9]{1,3}-[0-9]{1,3}(\.[0-9]{1,3}){0,5}/ } }
                    QQC2.Button { text: "USB güç tasarrufu aç"; enabled: usbPort.acceptableInput && !kcm.busy; onClicked: kcm.run(["usb", "powersave", usbPort.text, "on"], true) }
                    QQC2.Button { text: "Kapat"; enabled: usbPort.acceptableInput && !kcm.busy; onClicked: kcm.run(["usb", "powersave", usbPort.text, "off"], true) }
                }
                RowLayout {
                    QQC2.Button { text: "USB uyandırmayı aç"; enabled: usbPort.acceptableInput && !kcm.busy; onClicked: kcm.run(["usb", "wake", usbPort.text, "on"], true) }
                    QQC2.Button { text: "USB uyandırmayı kapat"; enabled: usbPort.acceptableInput && !kcm.busy; onClicked: kcm.run(["usb", "wake", usbPort.text, "off"], true) }
                }
                QQC2.Label { text: "Hub, depolama, klavye/fare ve sınıfı okunamayan aygıtlar korunur. USB aygıtı sıfırlanmaz."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                QQC2.TextArea { text: kcm.output; visible: text.length > 0; readOnly: true; selectByMouse: true; wrapMode: Text.Wrap; Layout.fillWidth: true; font.family: "monospace" }
            }
        }
        QQC2.GroupBox {
            title: "Kalıcı ayarlar"; Layout.fillWidth: true; visible: page.section === 2
            ColumnLayout {
                anchors.fill: parent
                QQC2.Label { text: "Kaydetmek mevcut ayarı değiştirmez. Seçtiğiniz ayarlar sonraki açılışta bir kez uygulanır; sürekli çalışan servis yoktur."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                RowLayout {
                    QQC2.Button { text: "Klavye rengini/parlaklığını kaydet"; enabled: page.ready("rgb") && color.acceptableInput && !kcm.busy; onClicked: kcm.run(["config", "set", "keyboard_color", color.text, "keyboard_brightness", brightness.value.toString()], true) }
                    QQC2.Button { text: "Pil eşiklerini kaydet"; enabled: page.ready("battery-limit") && lower.value < upper.value && !kcm.busy; onClicked: kcm.run(["config", "set", "battery_end", upper.value.toString(), "battery_start", lower.value.toString()], true) }
                }
                RowLayout {
                    QQC2.Button { text: "Güç profilini kaydet"; enabled: page.ready("profile") && !kcm.busy; onClicked: kcm.run(["config", "set", "profile", profile.currentText], true) }
                    QQC2.Button { text: "Kaydedilenleri göster"; enabled: !kcm.busy; onClicked: kcm.run(["config", "show"]) }
                    QQC2.Button { text: "Kalıcı ayarları temizle"; enabled: !kcm.busy; onClicked: kcm.run(["config", "clear"], true) }
                }
            }
        }
        QQC2.GroupBox {
            title: "Tüm yetenekler"; Layout.fillWidth: true; visible: page.section === 4
            ColumnLayout {
                anchors.fill: parent
                Repeater {
                    model: kcm.capabilities
                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        QQC2.Label { text: modelData.name + " · " + ({"ready":"Hazır", "blocked":"Doğrulama gerekiyor", "session":"KDE oturumu", "unsupported":"Bu modelde sunulmuyor"}[modelData.state] || modelData.state); font.bold: true; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        QQC2.Label { text: modelData.reason; wrapMode: Text.WordWrap; Layout.fillWidth: true; opacity: 0.8 }
                    }
                }
            }
        }
    }
}
