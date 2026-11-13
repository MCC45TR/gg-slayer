import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "../components"
PageBody {
    id: pageRoot
    GridLayout {
        Layout.fillWidth: true
        columns: pageRoot.width >= 1050 ? 2 : 1
        columnSpacing: 18
        rowSpacing: 18
    Section {
        Layout.fillWidth: true; Layout.alignment: Qt.AlignTop; title: qsTr("Fabrika performans modu"); iconName: "speedometer"
        QQC2.Label { text: qsTr("Donanımdaki durum: %1").arg(device.info.profile === "low-power" ? qsTr("Office") : device.info.profile === "custom" ? qsTr("Özel firmware modu") : device.info.profile || "—"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Setting { title: qsTr("Fabrika modu"); capability: "profile"; options: (device.info.profile_choices || "").split(" ").filter(v => v.length > 0); labels: options.map(v => v === "low-power" ? qsTr("Office · Pil tasarrufu") : v === "balanced" ? qsTr("Balance · Dengeli") : v === "performance" ? qsTr("Performance") : v); current: device.info.profile || ""; onChosen: value => control.apply(["profile", "set", value], ["profile", value]) }
        QQC2.Switch {
            text: qsTr("KDE ile eşitle")
            checked: device.info.saved_performance_sync === "on"
            enabled: device.ready("performance-sync") && !control.busy
            onToggled: {
                control.apply(["profile", "sync", checked ? "on" : "off"])
                checked = Qt.binding(function() { return device.info.saved_performance_sync === "on" })
            }
        }
        QQC2.Label {
            text: device.info.performance_sync_error || (device.ready("performance-sync") ? qsTr("Office ↔ Pil tasarrufu · Balance ↔ Dengeli · Performance ↔ Performans. İlk eşitlemede cihazın mevcut modu korunur.") : device.reason("performance-sync"))
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Setting { title: qsTr("Soğutma"); capability: "fan"; options: ["auto", "boost"]; labels: [qsTr("Otomatik"), qsTr("Fan Boost")]; current: device.info.fan_mode || ""; onChosen: value => control.apply(["fan", value], ["fan_mode", value]) }
    }
    Section {
        Layout.fillWidth: true; Layout.alignment: Qt.AlignTop; title: qsTr("Grafik modu"); iconName: "video-display"
        Setting { title: qsTr("Sonraki açılışın grafik modu"); description: qsTr("Değişiklik yeniden başlattığınızda uygulanır; açık oturumda GPU değiştirilmez."); capability: "mux"; options: ["hybrid", "dedicated", "integrated"]; labels: [qsTr("Hibrit · AMD + NVIDIA"), qsTr("Yalnız NVIDIA"), qsTr("Yalnız AMD iGPU")]; current: device.info.mux_mode || ""; automatic: false; onChosen: value => control.apply(["gpu", "mode", value]) }
        QQC2.Label { text: qsTr("iGPU modunda NVIDIA’ya bağlı harici ekranlar kullanılamayabilir. NVIDIA modu pil tüketimini artırabilir."); wrapMode: Text.WordWrap; Layout.fillWidth: true }
        QQC2.Button { text: qsTr("Şimdi yeniden başlat…"); onClicked: restart.open() }
        QQC2.Dialog {
            id: restart; title: qsTr("Bilgisayarı yeniden başlat")
            modal: true; width: Math.min(400, pageRoot.width - 24); height: 220
            standardButtons: QQC2.Dialog.Ok | QQC2.Dialog.Cancel
            contentItem: QQC2.Label { text: qsTr("Kaydedilmemiş çalışmalarınızı kaydedin. Devam ettiğinizde bilgisayar yeniden başlatılır."); wrapMode: Text.WordWrap }
            onAccepted: control.reboot()
        }
        QQC2.Button { text: qsTr("Sistem ekran ayarları"); onClicked: device.openSettings("display") }
    }
    }
    QQC2.CheckBox { id: advanced; objectName: "cpuAdvanced"; text: qsTr("Gelişmiş işlemci ve fan ayarları") }
    Loader {
        Layout.fillWidth: true; active: advanced.checked
        sourceComponent: Section {
            title: qsTr("İşlemci"); subtitle: qsTr("Bu işlemler açıkça uygulanır. Sistem güç yöneticisi frekans sınırlarını sonradan değiştirebilir.")
            Setting { title: qsTr("CPU Boost"); capability: "cpu-boost"; options: ["on", "off"]; labels: [qsTr("Açık"), qsTr("Kapalı")]; current: device.info.cpu_boost === "1" ? "on" : "off"; automatic: false; onChosen: value => control.apply(["cpu", "boost", value], ["cpu_boost", value]) }
            Setting { title: qsTr("SMT · ek iş parçacıkları"); capability: "cpu-smt"; options: ["on", "off"]; labels: [qsTr("Açık"), qsTr("Kapalı")]; current: device.info.cpu_smt || ""; automatic: false; onChosen: value => control.apply(["cpu", "smt", value], ["cpu_smt", value]) }
            QQC2.Label { text: qsTr("En düşük / en yüksek frekans (MHz)"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
            QQC2.SpinBox { id: minimum; from: 100; to: 10000; value: Math.max(100, Number(device.info.cpu_scaling_min_freq || 500000) / 1000); editable: true; Accessible.name: qsTr("En düşük frekans") }
            QQC2.SpinBox { id: maximum; from: 100; to: 10000; value: Math.max(100, Number(device.info.cpu_scaling_max_freq || 2500000) / 1000); editable: true; Accessible.name: qsTr("En yüksek frekans") }
            QQC2.Button { text: qsTr("Frekans sınırlarını uygula"); enabled: device.ready("cpu-frequency") && minimum.value <= maximum.value && !control.busy; onClicked: control.apply(["cpu", "limits", String(minimum.value), String(maximum.value)], ["cpu_min_mhz", String(minimum.value), "cpu_max_mhz", String(maximum.value)]) }
            QQC2.Button { text: qsTr("Varsayılan sınırlara dön"); enabled: device.ready("cpu-frequency") && !control.busy; onClicked: control.apply(["cpu", "limits", "reset"]) }
            QQC2.Label { text: qsTr("NVIDIA güç limiti"); font.bold: true }
            QQC2.Button { text: qsTr("Sürücü limitlerini oku"); onClicked: control.diagnose("gpu-limits") }
            QQC2.SpinBox { id: watts; from: 1; to: 10000; value: 50; editable: true; Accessible.name: qsTr("GPU güç limiti (W)") }
            QQC2.Button { text: qsTr("GPU limitini uygula"); enabled: device.ready("model-bios") && control.output.indexOf("power_settable: candidate") >= 0 && !control.busy; onClicked: control.apply(["gpu", "power-limit", String(watts.value)]) }
            QQC2.TextArea { Layout.fillWidth: true; text: control.output; readOnly: true; wrapMode: TextEdit.WrapAnywhere; visible: text.length > 0; Accessible.name: qsTr("Gelişmiş denetim çıktısı") }
            QQC2.Label { text: qsTr("Fan eğrisi taslağı"); font.bold: true }
            QQC2.TextField { id: curveName; Layout.fillWidth: true; placeholderText: qsTr("Eğri adı"); validator: RegularExpressionValidator { regularExpression: /[A-Za-z0-9_-]{1,32}/ } Accessible.name: placeholderText }
            QQC2.TextField { id: cpuCurve; Layout.fillWidth: true; text: "40:30,60:60,90:100"; Accessible.name: qsTr("CPU sıcaklık:yüzde noktaları") }
            QQC2.TextField { id: gpuCurve; Layout.fillWidth: true; text: "40:30,65:65,90:100"; Accessible.name: qsTr("GPU sıcaklık:yüzde noktaları") }
            QQC2.Button { text: qsTr("Taslağı kaydet"); enabled: curveName.acceptableInput && !control.busy; onClicked: control.apply(["fan", "curve", "save", curveName.text, cpuCurve.text, gpuCurve.text]) }
            QQC2.Button { text: qsTr("Kayıtlı taslakları oku"); onClicked: control.diagnose("fan-curves") }
            QQC2.Label { text: qsTr("Özel fan eğrisi uygulama bu donanımda doğrulanmadı. Desteklenmeyen fan tabloları yazılmaz."); wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
}
