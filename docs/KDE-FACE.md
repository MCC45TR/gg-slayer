# KDE yüz tanıma bağlantısı

Geliştirici: MCC45TR <mcc45tr@gmail.com> · İnceleme: 14 Kasım 2026

Bu cihazda etkin görüntü yöneticisi **Plasma Login Manager** (`plasmalogin.service`).
SDDM kurulu değil. Oturum açma yolu `/usr/lib/pam.d/plasmalogin`; ekran kilidi
KScreenLocker tarafından kullanılan `kde` PAM hizmeti. Dağıtımın `/etc/pam.d`
geçersiz kılmaları ayrıca değerlendirilir.

| İşlem | Kimlik doğrulama yolu | Sonuç |
| --- | --- | --- |
| İlk giriş | Plasma Login Manager / `plasmalogin` | PAM üzerinden yüz motoru bağlanabilir. |
| Ekran kilidi | KScreenLocker / normalde `kde` | Girişten ayrı test gerekir. |
| Yönetici işlemi | Polkit ajanı / dağıtımın PAM politikası | Giriş ayarı kendiliğinden burayı kapsamaz. |
| KWallet | `pam_kwallet` + kullanıcının parolası | Yüz eşleşmesi şifreli kasanın anahtarını üretmez. |

## Kenning ve Qt ön yüzü

`slayer-control-qt` içindeki **Aygıtlar → IR yüz tanıma**, Kenning'in mevcut
`kcm_kenning` ayar modülüne bağlanır. Kenning isteğe bağlıdır; Slayer'ın hiçbir
paketine zorunlu veya otomatik önerilen bir bağımlılık olarak eklenmez. Motor,
OpenCV, modeller ve PAM modülü Slayer'a bağlanmaz veya kopyalanmaz.

Adaptör yalnız Qt Core kullanır ve sadece Qt uygulamasına bağlanır. CLI, sistem
brokeri, oturum köprüsü ve mevcut Slayer KCM'i bu adaptörü yüklemez. Kurulum
durumu sabit Qt eklenti yolu ve çalıştırılabilir dosya metadata'sından bulunur;
`kenning --version`, systemctl, D-Bus aktivasyonu veya tanıma soketi kullanılmaz.
En fazla 64 KiB `/etc/kenning/config.ini` okunur. Kayıtlı kamera düğümünün sysfs
kimliği, mevcut Howdy aracındaki aynı dahili R9T IR kimliğiyle karşılaştırılır:
`2b7e:c906`, revizyon `0004`, sabit USB aygıtı, arayüz `02`,
`FHD WebCam: IR Camera`. Kamera düğümü görüntü almak veya ioctl ile yoklamak için
açılmaz. Sadece IR modunun seçili olması, bilinen kamera kimliği yerine geçmez.

Durum sayfa yüklenince ve kullanıcının yenileme eyleminde okunur. Sürekli
sorgulama, dosya izleyicisi, yeni servis veya zamanlayıcı yoktur. Ayar ekranı
yalnız açık kullanıcı eylemiyle `kcmshell6 kcm_kenning` olarak, normal kullanıcı
yetkisiyle açılır. Kenning'in kayıt/ayar değişikliklerini kendi KAuth yardımcısı
yürütür. Slayer kamera seçimini, yüz modelini veya PAM dosyalarını değiştirmez.

Kenning 0.8.1'in otomatik kamera bulucusu IR bulamazsa RGB kamera seçebilir.
Slayer bu seçimi açıkça gösterir; IR kullanımı Kenning ayarlarından sabitlenmeli
ve kullanıcı tarafından ayrıca denenmelidir. Bu entegrasyon sistem genelinde
IR zorunluluğu uygulamaz. Motor ayarının açık görünmesi yüz kaydının, PAM
bağlantısının veya başarılı kimlik doğrulamasının kanıtı değildir.

Doğrulama: geçici dosyalarla eksik motor/modül, RGB seçimi, bozuk/büyük ayar,
kayıp kamera ve yanlış IR kimliği senaryoları test edilir. Sahte çalıştırılabilir
dosyalar bu okumanın motor/ayar uygulaması başlatmadığını kontrol eder. QML
önizlemesi örnek durum kullanır; ayar uygulamasını da açmaz. Gerçek yüz kaydı,
kilit ekranı, sudo ve PAM denemeleri kullanıcı kurulumu sonrasına bırakılmıştır.

14 Kasım doğrulaması: Qt uygulaması, Slayer KCM'i ve CLI MinSizeRel olarak
derlendi; 18/18 CTest geçti. Yedi sayfa altı pencere boyutunda çizildi. Yeni
bölüm ayrıca Breeze Light/Dark ile 360/1200 piksel genişlikte ve %150 yazı
boyutunda incelendi. Qt uygulamasının doğrudan bağımlılıklarında tanıma motoru
yok. Yeni CLI ve önceden derlenmiş broker/masaüstü ikilisinin headless bağımlılık
kontrolü geçti. Ortamda `libsystemd` geliştirme paketi bulunmadığı için bu
oturumda broker/oturum servisleri yeniden derlenmedi; CTest yapılandırmasında
`SLAYER_BUILD_DESKTOP=OFF` kullanıldı. Sisteme paket kurulmadı.

İncelenen upstream: Kenning **0.8.1** (14 Kasım 2026).

- [KDE modül adı ve bağımlılıklar](https://github.com/RishonDev/kenning/blob/v0.8.1/kcm/src/CMakeLists.txt)
- [Motor ayar biçimi](https://github.com/RishonDev/kenning/blob/v0.8.1/engine/native/src/config.h)
- [IR/RGB kamera bulucu](https://github.com/RishonDev/kenning/blob/v0.8.1/kcm/src/common/camerafinder.cpp)
- [Kenning güvenlik sınırları](https://github.com/RishonDev/kenning#security-and-limitations)

## Mevcut Howdy aracı

Howdy bir PAM yüz doğrulama motorudur. `slayer-face-auth` cihaz kimliği
doğrulanmış IR kamerayla açık kayıt/test komutları sunar. Howdy henüz bu
cihazda kurulu değil. Paket onu önerir; kullanıcı kaydı, kamera açılması veya
PAM değişikliği RPM kurulumu sırasında yapılmaz. Mevcut parola yolu korunarak
yapılacak entegrasyon ve giriş/kilit/Polkit denemeleri sonraki güvenlik
incelemesinin ayrı kapsamıdır.

IR kamera tek başına canlılık kanıtı değildir. Howdy'nin kendi güvenlik
açıklaması da yüz eşleştirmesinin sınırlarını belirtir. Kullanıcının planladığı
Dayblue incelemesinde sahte görüntü, yanlış kullanıcı, karanlık, kamera
bağlantısının kesilmesi ve parola ile kurtarma değerlendirilmelidir.

Birincil kaynaklar:

- [Plasma Login Manager PAM backend](https://github.com/KDE/plasma-login-manager/blob/master/src/helper/backend/PamBackend.cpp)
- [Fedora giriş PAM tanımı](https://github.com/KDE/plasma-login-manager/blob/master/data/pam/fedora/plasmalogin)
- [KScreenLocker PAM belgesi](https://github.com/KDE/kscreenlocker/blob/master/README.pam)
- [KWallet PAM uygulaması](https://github.com/KDE/kwallet-pam/blob/master/pam_kwallet.c)
- [Howdy projesi ve güvenlik açıklaması](https://github.com/boltgolt/howdy)
