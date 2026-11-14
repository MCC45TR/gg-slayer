# Copilot tuşu ve düşük kaynak tüketimi

14 Kasım 2026. Slayerctl 0.1.4 çalışma ağacı.

## Kullanım

**Aygıtlar → Copilot tuşu** bölümünde işlevi kullanıcı seçer. İlk kurulumda
işlev atanmaz. Kısayol düğmesine basıp fiziksel Copilot tuşunu kaydettikten
sonra **Atamayı kaydet** ile uygulanır. Başlangıç değeri `Meta+Shift+F23`;
kayıt düğmesi farklı donanım/klavye düzenine uyarlamayı sağlar.

| Seçenek | Etki |
| --- | --- |
| Slayer Control | Arayüzü açar. |
| Uygulama | Kurulu uygulamalar arasından kullanıcı seçer. |
| Web adresi | Seçilen HTTP/HTTPS adresini varsayılan uygulamayla açar. |
| Fan Boost aç / kapat | Mevcut auto/boost durumunu değiştirir ve hatırlar. |
| Otomatik fan | Fabrika otomatik fan modunu uygular ve hatırlar. |
| Güç profilini sırayla değiştir | Tasarruf → dengeli → performans; sağlayıcının sunmadığı profilleri atlar. |
| Doğrudan güç modu | Tasarruf, dengeli veya performans seçimini uygular ve hatırlar. |
| Özel komut | Program ve argümanlarını kullanıcı yetkisiyle başlatır; kendi betiği de seçilebilir. |
| Atamayı kaldır | Yalnız Slayer'ın Copilot başlatıcısını kaldırır. |

KDE Plasma genel kısayol hizmeti gerekir. Atama kullanıcıya ait
`$XDG_DATA_HOME/kglobalaccel/org.gamegaraj.slayerctl.copilot.desktop` dosyası ve
KDE'nin kalıcı kısayol kaydıyla çalışır. Slayer penceresi açık olmak zorunda
değildir. Ayrı bir Copilot daemon'u veya giriş aygıtı izleyicisi eklenmedi.
Uygulama listesi yalnız bu işlev seçildiğinde okunur; tuş kaydedici yalnız
gerektiğinde yüklenir.

Dosya yalnız kullanıcı tarafından okunur/yazılır ve atomik kaydedilir. Başka
bir genel kısayolla çakışma otomatik devralınmaz. Kayıt hatasında önceki dosya
ve tuşlar geri yüklenir. Pencere bir kayıt sırasında kapatılırsa bu istek
tamamlanıp uygulama sonlanır. Önizleme gerçek kısayol veya dosya yazmaz.

Fan/güç başlatıcısı yalnız basış sırasında mevcut `slayerctl-desktop` köprüsünü
çalıştırır. İşlem sistem brokerindeki `Apply` yolunu, mevcut polkit kararını,
model/BIOS/yetenek denetimlerini ve TPM ön hazırlığını kullanır. Özel komutta
program ve argümanlar ayrı kodlanır; kabuk işlevleri için kullanıcı açıkça
kendi betiğini veya yorumlayıcısını seçer. Native uygulamanın `%U` gibi alan
kodları korunur; URL/özel argümanlardaki yüzde işaretleri literal kodlanır.
RGB eşlemelerinin bozulmaması için fiziksel çizimde görünen **Copilot** tuşu,
kayıtlı profil/eşleme dosyalarında eski `Menu` LED kimliğini korur.

CLI yardımcıları: `slayerctl shortcut fan-boost`, `slayerctl shortcut fan-auto`,
`slayerctl shortcut power-cycle`, `slayerctl shortcut power-saver`,
`slayerctl shortcut balanced`, `slayerctl shortcut performance`.
`slayerctl fan mode` yalnız fan modunu okur; `sudo slayerctl fan toggle`
doğrulanmış auto/boost geçişini yapar.

## Kaynak davranışı

| Önce | Yeni davranış |
| --- | --- |
| Genel Bakış görünürken 3 saniyelik sensör taraması | Varsayılan talep üzerine; ilk CPU yüzdesi için 250 ms arayla iki örnek, sonra durur. |
| Varsayılan fan animasyonları | Varsayılan kapalı; kullanıcı ayrıca açabilir. |
| Canlı takip açık pencereyle sürebiliyordu | Canlı takip isteğe bağlı, 10 saniye aralıklı ve yalnız odaktaki Genel Bakış'ta. |
| Sistem brokerinde 30 saniyelik boşta bekleme | Gerekli özellik/iş/aktif istemci yoksa 5 saniyede çıkış. |
| Bekleme ışığı yapılandırılınca oturum köprüsü diğer güç modlarında da açık kalıyordu | Gerekmiyorsa aboneliği bırakır ve yaklaşık 2,5 saniyede çıkar. |
| Her ayar güncellemesi oturum yardımcısı başlatabiliyordu | Işık sahibi veya ilgili özellik değişimi/güç tasarrufuna girişte etkinleştirilir. |
| Durum sorgusu kapalı oturum köprüsünü açabiliyordu | Sorguda D-Bus otomatik başlatma kapalı. |

Pencere kapalı ve sürekli özellikler kapalıysa bu sürelerden sonra Slayer
süreçleri kalmaz. Bekleme ışığı veya profil eşitlemesi kullanıcı tarafından
açılmışsa gerekli sistem gözlemcisi olayları almak için açık kalır. Aktif KDE
klavye devri/kalibrasyon sırasında kullanıcı köprüsü de gereklidir. Bu
gözlemciler periyodik sensör taraması yapmaz. Copilot ataması kendi başına
bu süreçleri açık tutmaz.

Üretim MinSizeRel ikilisi, yeni tercih dizini, Genel Bakış ve offscreen/software
çizim ile beş saniyelik örnekte **0 CPU tick**, **0 alt süreç**, **40061 KiB PSS**
ve **105004 KiB RSS** ölçüldü. Saat çözünürlüğü 100 tick/s; bu kısa örnekte
ölçülebilir CPU artışı olmadığını gösterir. Gerçek Wayland/GPU, pil ömrü veya
her koşulda sıfır tüketim sonucu çıkarılamaz. Grafik arayüzün belleği ile
pencere kapalıyken gereken arka plan süreçleri ayrı değerlendirilmelidir.

## Doğrulama

- Üretim ve ASan/UBSan derlemelerinde 25/25 CTest hedefi geçti.
- 67 donanım taklidi testi; fan auto/boost geçişi, salt okunur mod,
  firmware-custom ve doğrulanmayan BIOS'ta yazma yapılmaması dahil.
- Özel D-Bus üzerinde gerçek masaüstü yardımcısı: yetkili fan geçişi,
  desteklenen profillerle döngü, doğrudan profil, tercih kaydı ve süreç çıkışı.
- Copilot yapılandırması: QML'den native tuş dizisi aktarımı, çakışma, hata
  geri alma, uygulama/URL/komut, donanım eylemleri, kaldırma ve önizleme.
- Boşta Copilot adaptörü D-Bus sorgusu yapmaz. Manuel sensör örneklemesi
  tamamlandıktan sonra tekrar etmez; gizlenen sayfa sensör okuyamaz.
- Oturum köprüsü güç tasarrufundan çıkınca aboneliği bırakıp sonlanır;
  gerektiğinde yeniden başlar. Kalibrasyon kayıp/iptal korumaları korunur.
- 64 fixture görünümü; ayrıca Breeze Light ve %150 yazıyla Breeze Dark.
  Copilot uygulama, URL, fan ve özel komut halleri 360/1200 px genişliklerde
  incelendi. Yükleme/binding hatası görülmedi.
- Gerçek KDE'de geçici, tuş atanmamış bir native kayıt yapılandırıldı;
  yapılandırma istemcisi kapandıktan sonra başlatma başarılı oldu. Geçici
  kayıt kaldırıldı; kullanıcının tuşlarına atama yapılmadı.
- Headless bağımlılık kontrolü geçti: CLI/broker/masaüstü/oturum yollarına
  QML, Widgets, Kirigami veya yeni KDE arayüz bağımlılığı eklenmedi.

Testler gerçek Copilot basışını, fan hızını veya güç profilini değiştirmedi.
Sistem paketleri ve kurulu sürüm güncellenmedi; sonuç kaynak ve derleme
çıktılarındadır. Mevcut kullanıcı tercihleri korunur.

Kanıtlar: [üretim testleri](copilot-2026-11-14/ctest-production.txt),
[sanitizer](copilot-2026-11-14/ctest-sanitizer.txt),
[ölçüm](copilot-2026-11-14/resources.json),
[native KDE denemesi](copilot-2026-11-14/native-launch.json).

Native akışın birincil kaynakları:
[KDE kısayol modülündeki desktop kayıt yolu](https://raw.githubusercontent.com/KDE/plasma-desktop/master/kcms/keys/globalaccelmodel.cpp),
[KGlobalAccel D-Bus protokolü](https://raw.githubusercontent.com/KDE/kglobalaccel/master/src/org.kde.KGlobalAccel.xml),
[kalıcı yapılandırma bayrakları](https://raw.githubusercontent.com/KDE/kglobalaccel/master/src/kglobalaccel.h).
