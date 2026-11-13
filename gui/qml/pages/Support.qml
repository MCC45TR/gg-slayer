import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../components"
PageBody {
    QQC2.TabBar {
        id: tabs
        objectName: "supportTabs"
        Layout.fillWidth: true
        QQC2.TabButton { text: qsTr("Tanılama") }
        QQC2.TabButton { text: qsTr("Donanım") }
        QQC2.TabButton { text: qsTr("Hakkında") }
    }
    Loader { Layout.fillWidth: true; sourceComponent: tabs.currentIndex === 0 ? diagnostics : tabs.currentIndex === 1 ? hardware : about }
    Component {
        id: diagnostics
Section {
        Layout.fillWidth: true; title: qsTr("Tanılama"); iconName: "utilities-terminal"
        QQC2.ComboBox { id: topic; Layout.fillWidth: true; model: [qsTr("Sistem denetimi"), qsTr("Sensörler"), qsTr("Cihaz bilgisi"), qsTr("Yetenekler")]; Accessible.name: qsTr("Tanılama konusu") }
        QQC2.Button { text: qsTr("Oku"); onClicked: control.diagnose(["doctor", "sensors", "info", "capabilities"][topic.currentIndex]) }
        QQC2.TextArea { Layout.fillWidth: true; text: control.output; readOnly: true; wrapMode: TextEdit.WrapAnywhere; selectByMouse: true; placeholderText: qsTr("Bir denetim seçtiğinizde sonuçlar burada görünür."); Accessible.name: qsTr("Tanılama çıktısı") }
        QQC2.CheckBox { text: qsTr("Hareketi azalt"); checked: control.reducedMotion; onToggled: control.reducedMotion = checked }
    }
    }
    Component {
        id: hardware
Section {
        Layout.fillWidth: true; title: qsTr("Donanım desteği"); iconName: "dialog-information"
        Repeater {
            model: device.capabilities
            delegate: ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                QQC2.Label { text: modelData.name + " · " + (modelData.state === "ready" ? qsTr("Hazır") : qsTr("Kullanılamıyor")); font.bold: true; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                QQC2.Label { text: modelData.reason; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Kirigami.Theme.disabledTextColor }
                Kirigami.Separator { Layout.fillWidth: true }
            }
        }
    }
    }
    Component {
        id: about
Section {
        Layout.fillWidth: true; title: qsTr("Hakkında"); iconName: "computer-laptop"
        Image {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(width * .6, Kirigami.Units.gridUnit * 14)
            source: "../assets/slayer-r9t-front.webp"
            sourceSize.width: 622; sourceSize.height: 622
            sourceClipRect: Qt.rect(90, 145, 450, 330)
            fillMode: Image.PreserveAspectFit
            visible: status === Image.Ready
            Accessible.ignored: true
        }
        QQC2.Label { text: qsTr("Slayer Control · %1").arg(device.info.version || "—"); font.bold: true; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        QQC2.Label { text: qsTr("Geliştirici: MCC45TR · mcc45tr@gmail.com"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
        QQC2.Label { text: qsTr("Cihaz: %1\nBIOS: %2\nKernel: %3").arg(device.info.model || "—").arg(device.info.bios || "—").arg(device.info.kernel || "—"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
        QQC2.Label { text: qsTr("Şifreli ayarlar bu bilgisayarın TPM’ine bağlıdır. BIOS güncellemesinde anahtar kaybolursa eski tercihler çözülemeyebilir."); wrapMode: Text.WordWrap; Layout.fillWidth: true }
        QQC2.Button { text: qsTr("TPM ayar korumasını yeniden oluştur…"); onClicked: recovery.open() }
        QQC2.Dialog {
            id: recovery; title: qsTr("TPM ayar kurtarma")
            modal: true
            width: Math.min(420, parent.width)
            standardButtons: QQC2.Dialog.Ok | QQC2.Dialog.Cancel
            height: Math.min(320, parent.height)
            contentHeight: 180
            contentItem: QQC2.Label { id: recoveryText; width: recovery.availableWidth; text: qsTr("Yönetici doğrulamasından sonra eski şifreli dosya korunur ve boş yapılandırma oluşturulur. Eski tercihler kurtarılmaz; donanım ayarları değiştirilmez."); wrapMode: Text.WordWrap }
            onAccepted: control.apply(["config", "recover"])
        }
    }
    }
}
