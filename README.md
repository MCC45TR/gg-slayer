# GG Slayer

Geliştirici: **MCC45TR <mcc45tr@gmail.com>**

Game Garaj Slayer R9T için donanım kontrolü ve yerel Linux arayüzleri.
Fedora **45 x86_64** ve **Rawhide x86_64** paketleri
[COPR](https://copr.fedorainfracloud.org/coprs/mcc45tr/mcc-support/) üzerinde derlenir.

```sh
sudo dnf copr enable mcc45tr/mcc-support
sudo dnf install slayerctl slayer-control-qt slayer-face-auth
sudo dnf upgrade --refresh 'slayer*' 'ite8291r3*' 'kmod-slayerctl*' 'kmod-ite8291r3*'
```

| Paket | İşlev |
| --- | --- |
| `slayerctl` | CLI, sistem D-Bus brokeri, DE bağlantıları, oturum ve donanım yürütmesi |
| `slayer-control-qt` | Kirigami Qt arayüzü ve KDE Sistem Ayarları modülü |
| `slayer-face-auth` | Doğrulanmış IR kamerayla açık Howdy kayıt/test aracı |
| `slayer-control-gtk` | GNOME için yer tutucu pencere; işlevsel kontrol paneli henüz yok |
| `slayerctl-r9t-kmod` | COPR'da derlenen, tam modelle sınırlandırılmış R9T sürücüsü |
| `ite8291r3-kmod` | COPR'da derlenen RGB, parlaklık ve kalibrasyon sürücüsü |

RPM doğrudan kurulsa da ctl paketi etkin COPR depo dosyasını kurar. DNF sonraki
paketleri bu depodan günceller; GPG paket doğrulaması açıktır. Eski `slayerctl-gui`,
`slayerctl-kde`, `slayerctl-face` ve `slayerctl-gnome` adları yeni paketlere taşınır.
Yerel CMake/rpmbuild/akmods derlemesi dağıtım yolunun parçası değildir.
[Dağıtım ve kernel ABI ayrıntıları](docs/COPR.md).

## ctl ve masaüstü

Arayüz isteğe bağlıdır. `slayerctl` tek başına DE'nin standart güç, LED ve pil
arayüzleriyle çalışır. Oturum köprüsü Qt Core/DBus kullanır; QML, Widgets,
Kirigami veya ekran penceresi gerektirmez. Donanım işleri yalnız ctl'de yürür.

```sh
slayer-control-qt
slayerctl power status
slayerctl power set power-saver
slayerctl idle on
slayerctl idle timeout 30
slayerctl idle status
slayerctl capabilities
slayerctl doctor
```

Yedi arayüz bölümü: Genel Bakış, Aydınlatma, Güç ve Pil, Performans, Aygıtlar,
Otomasyon, Destek. TR ISO klavye çizimi sistemin tuş düzenini izler. Günlük
seçimler doğrudan uygulanır; gelişmiş işlemler açık eylemlerle gönderilir.
Sensörler varsayılan olarak Genel Bakış açıldığında ve **Sensörleri yenile** ile
okunur. **Canlı sensörler** isteğe bağlıdır; etkin penceredeki Genel Bakış'ta
10 saniyede bir çalışır. Fan animasyonları varsayılan olarak kapalıdır.
Pencere odağı kaybolduğunda, küçültüldüğünde veya sayfa değiştiğinde canlı takip durur.

Kamera için KDE Plasma'nın kendi **Kamera Göstergesi** kullanılır. Slayer ayrı
bir kamera widget'ı veya tepsi simgesi kurmaz. Uygulamadaki dahili RGB/IR kamera
erişimi ayarı **Aygıtlar → Dahili kamera** bölümündedir.

**Güç ve Pil → Şarj sınırı → Azami dolum miktarı (%)** her zaman görünür.
Desteklenen cihazda değişikliği **Uygula ve kaydet** ile gönderin. R9T'nin mevcut
BIOS'unda sayısal eşik sunulmadığından alan kapalıdır ve nedeni yazılır.
Mevcut üç pil koruma profili belirli bir yüzde garantisi değildir.
[KDE kamera ve dolum sınırı doğrulaması](docs/NATIVE-KDE.md).

## Copilot tuşu

**Aygıtlar → Copilot tuşu** bölümünde işlevi kullanıcı seçer: uygulama veya web
adresi açma, Slayer Control, Fan Boost aç/kapat, otomatik fan, güç profili
döngüsü, doğrudan tasarruf/dengeli/performans veya özel komut.
Kısayol düğmesine tıklayıp fiziksel Copilot tuşuna basın, sonra **Atamayı kaydet**
ile uygulayın. Başlangıç önerisi `Meta+Shift+F23`'tür; tuş kaydı cihazın gerçek
dizilimini kullanmanızı sağlar. Başka bir KDE ataması çakışırsa mevcut kısayol
korunur ve KDE kısayollarını açarak çözebilirsiniz. Atama kaldırılabilir.

KDE, kullanıcıya ait native başlatıcıyı pencere kapalıyken de çalıştırır.
Copilot için resident Slayer süreci, giriş aygıtı izleyicisi veya root ayarı
gerekmez. Özel komut kendi kullanıcı yetkisiyle başlatılır; program ve
argümanları ayrı tutulur. Kabuk işlemleri istenirse kullanıcı kendi betiğini
seçebilir. Fan/güç eylemleri yalnız tuşa basıldığında mevcut ctl brokerine
gider, yetkilendirilir ve kullanıcı tercihini kaydeder. Donanım destek/model
denetimleri geçerlidir. KDE Plasma genel kısayol hizmeti gerekir.

## Arka plan çalışması

Arayüz kapatılınca sonlanır. Aktif istemci, kalibrasyon, profil eşitlemesi ve
bekleme ışığı özelliği yoksa sistem brokeri beş saniye içinde kapanır.
Kullanıcı oturum köprüsü yalnız etkin KDE bekleme ışığı devri veya kalibrasyon
sırasında açık kalır; diğer durumda aboneliğini bırakıp yaklaşık 2,5 saniyede
kapanır. Salt okunur durum sorgusu kapalı oturum köprüsünü başlatmaz.

Bekleme ışığı veya profil eşitlemesi özellikle açılmışsa gerekli sistem
gözlemcisi olayları bekler. Periyodik sensör taraması yapmaz; yerleşik klavye
izlemesi yalnız güç tasarrufunda ve ışık açıkken etkinleşir. Geçiş dışında
parlaklık zamanlayıcısı durur. Copilot ataması bu servisleri açık tutmaz.

Güç tasarrufunda bekleme ışığı, seçili parlaklığı KDE'de değiştirmeden beş
saniyede en düşük açık seviyeye iner ve etkinlikte geri yükselir. Sıfır parlaklık
sıfır kalır. İlk bekleme süresi 30 saniyedir; 5 saniye–30 dakika seçenekleri
vardır. [Bekleme mimarisi ve komutları](docs/IDLE-DIMMING.md).

Beyaz noktası ve kullanıcı tarafından eşleştirilen 20 ekran rengi, native tamsayı
3D renk dönüşümüyle uygulanır. Kalibrasyon kişiseldir; depoya yüklenmez. Statik
renkler ve KDE vurgu rengi düzeltilir; firmware efektlerinin kendi renk paleti
ayrıdır. Renk, efekt ve parlaklık yolları birbirlerinin durumunu sıfırlamaz.

Donanım ayarları ve renk profilleri TPM2 ile şifrelenir. Güncelleme mevcut
seçimleri korur. Genel EC/NVRAM, özel fan eğrisi, CPU OC/voltaj ve OEM cTGP
komutları açılmaz. Destek bildirimi gerçek yetenek ve doğrulama durumunu gösterir.

## Yüz tanıma

Qt uygulamasında **Aygıtlar → IR yüz tanıma**, isteğe bağlı
[Kenning](https://github.com/RishonDev/kenning) entegrasyonunu sunar. Kurulumu ve
kayıtlı kamera seçimini salt okunur gösterir; **Kenning ayarlarını aç** düğmesi
Kenning'in yerel KDE modülünü açar. Yüz kaydı, IR kamera seçimi, kilit ekranı ve
`sudo` tercihleri bu modülde yönetilir. Kenning ayrıca kurulmalıdır; Slayer
paketi onu indirmez, başlatmaz veya PAM politikasını değiştirmez.

Durum yalnız Aygıtlar sayfası açıldığında veya **Durumu yenile** ile okunur.
Slayer tarafında kamera açılmaz, yüz modeli yüklenmez, Kenning soketine bağlanılmaz
ve yeni arka plan işi/zamanlayıcı eklenmez. `slayerctl` ve servislerinin
bağımlılıkları değişmez. Bilinen dahili IR USB arayüzü ayrıca kontrol edilir;
RGB seçimi ve doğrulanamayan kamera açıkça gösterilir. Bu kontrol Kenning'in
ayarlarını zorlamaz ve gerçek yüz doğrulama testi yerine geçmez.

`slayer-face-auth` Howdy kayıt/test ön yüzüdür; Howdy motorunun kendisi değildir.
RPM kurulumu kamera açmaz, kullanıcı kaydetmez veya PAM politikalarını değiştirmez.
KDE giriş, ekran kilidi ve KWallet yolları hakkında
[birincil kaynaklı inceleme](docs/KDE-FACE.md) bulunur.

## Geliştirme ve derleme

GitHub `main` dalına gönderimler, proje kapsamlı webhook ile üç COPR kaynak
paketini derler. `.copr/Makefile` yalnız sunucuda kaynak RPM hazırlar; C++/QML
kontrolleri, arayüz çizimleri ve kernel modülleri COPR chroot'larında üretilir.

```sh
copr-cli build-package mcc45tr/mcc-support --name slayerctl --nowait
copr-cli build-package mcc45tr/mcc-support --name slayerctl-r9t-kmod --nowait
copr-cli build-package mcc45tr/mcc-support --name ite8291r3-kmod --nowait
```

Yerel Qt/Kirigami beceri yönergeleri `skills/` içindedir. Eski Widgets uygulaması
sadece regresyon testi kaynağı olarak tutulur. Yeni arayüzün geçmiş uygulama
notları: [Kirigami](docs/KIRIGAMI-IMPLEMENTATION.md).
