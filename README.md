# GG Slayer

Geliştirici: **MCC45TR <mcc45tr@gmail.com>**

Game Garaj Slayer R9T için donanım kontrolü ve yerel Linux arayüzleri.
Fedora **45 x86_64** ve **Rawhide x86_64** paketleri
[COPR](https://copr.fedorainfracloud.org/coprs/mcc45tr/gg-slayer/) üzerinde derlenir.

```sh
sudo dnf copr enable mcc45tr/gg-slayer
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
Gizli sayfalar sensör yenilemez; fan animasyonları görünmeyince durur.

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

`slayer-face-auth` Howdy kayıt/test ön yüzüdür; Howdy motorunun kendisi değildir.
RPM kurulumu kamera açmaz, kullanıcı kaydetmez veya PAM politikalarını değiştirmez.
KDE giriş, ekran kilidi ve KWallet yolları hakkında
[birincil kaynaklı inceleme](docs/KDE-FACE.md) bulunur.

## Geliştirme ve derleme

GitHub `main` dalına gönderimler, proje kapsamlı webhook ile üç COPR kaynak
paketini derler. `.copr/Makefile` yalnız sunucuda kaynak RPM hazırlar; C++/QML
kontrolleri, arayüz çizimleri ve kernel modülleri COPR chroot'larında üretilir.

```sh
copr-cli build-package mcc45tr/gg-slayer --name slayerctl --nowait
copr-cli build-package mcc45tr/gg-slayer --name slayerctl-r9t-kmod --nowait
copr-cli build-package mcc45tr/gg-slayer --name ite8291r3-kmod --nowait
```

Yerel Qt/Kirigami beceri yönergeleri `skills/` içindedir. Eski Widgets uygulaması
sadece regresyon testi kaynağı olarak tutulur. Yeni arayüzün geçmiş uygulama
notları: [Kirigami](docs/KIRIGAMI-IMPLEMENTATION.md).
