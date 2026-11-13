import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "../components"
PageBody {
    Section {
        Layout.fillWidth: true; title: qsTr("Adaptör ve pil"); iconName: "system-run"
        subtitle: qsTr("Kurallar güç kaynağı olaylarıyla çalışır. Sürekli sensör sorgulaması gerekmez.")
        Setting { title: qsTr("Adaptöre bağlıyken güç profili"); options: ["", "power-saver", "balanced", "performance"]; labels: [qsTr("Değiştirme"), qsTr("Pil tasarrufu"), qsTr("Dengeli"), qsTr("Performans")]; current: device.info.saved_desktop_profile_ac || ""; onChosen: value => control.apply(value ? ["config", "set", "desktop_profile_ac", value] : ["config", "unset", "desktop_profile_ac"]) }
        Setting { title: qsTr("Pille çalışırken güç profili"); options: ["", "power-saver", "balanced", "performance"]; labels: [qsTr("Değiştirme"), qsTr("Pil tasarrufu"), qsTr("Dengeli"), qsTr("Performans")]; current: device.info.saved_desktop_profile_battery || ""; onChosen: value => control.apply(value ? ["config", "set", "desktop_profile_battery", value] : ["config", "unset", "desktop_profile_battery"]) }
    }
    Section {
        Layout.fillWidth: true; title: qsTr("RGB otomasyonu"); iconName: "input-keyboard"
        subtitle: device.info.saved_lighting_owner === "custom" ? qsTr("Olay gerçekleştiğinde seçilen profil uygulanır.") : qsTr("RGB otomasyonu için Klavye ve RGB bölümünde Özel yönetimini seçin.")
        Setting { title: qsTr("Adaptöre bağlıyken"); options: [""].concat(device.rgbProfiles); labels: [qsTr("Değiştirme")].concat(device.rgbProfiles); current: device.info.saved_rgb_profile_ac || ""; enabled: device.info.saved_lighting_owner === "custom"; onChosen: value => control.apply(value ? ["config", "set", "rgb_profile_ac", value] : ["config", "unset", "rgb_profile_ac"]) }
        Setting { title: qsTr("Pille çalışırken"); options: [""].concat(device.rgbProfiles); labels: [qsTr("Değiştirme")].concat(device.rgbProfiles); current: device.info.saved_rgb_profile_battery || ""; enabled: device.info.saved_lighting_owner === "custom"; onChosen: value => control.apply(value ? ["config", "set", "rgb_profile_battery", value] : ["config", "unset", "rgb_profile_battery"]) }
    }
    Section {
        Layout.fillWidth: true; title: qsTr("Kaydedilmiş tercihler")
        QQC2.Label { text: qsTr("Günlük değişiklikler uygulanıp hatırlanır. İlk açılışta mevcut donanım durumu okunur; seçmediğiniz ayarlar değiştirilmez."); wrapMode: Text.WordWrap; Layout.fillWidth: true }
    }
}
