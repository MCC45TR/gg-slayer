# GG Slayer

Developer: **MCC45TR <mcc45tr@gmail.com>**

Fedora 45 ve Rawhide x86_64 paketleri [COPR](https://copr.fedorainfracloud.org/coprs/mcc45tr/gg-slayer/) üzerinde derlenir.

```sh
sudo dnf copr enable mcc45tr/gg-slayer
sudo dnf install slayerctl slayer-control-qt slayer-face-auth
sudo dnf upgrade --refresh slayerctl slayer-control-qt slayer-face-auth
```

`slayerctl` arka uçtur; Qt arayüzü isteğe bağlıdır. `slayer-control-gtk` GNOME için
henüz hazırlık penceresidir. Ana RPM, sonraki güncellemeler için COPR depo dosyasını
kurar. [Dağıtım akışı](docs/COPR.md).

---

> Yeni bağımsız uygulama Kirigami/QML arayüzünü ve istekle uyanan D-Bus
> servisini kullanır. Derleme, önizleme, TPM geçişi ve doğrulama sınırları:
> [Kirigami uygulama notları](docs/KIRIGAMI-IMPLEMENTATION.md).
> Aşağıdaki eski sürüm açıklamalarındaki Widgets ve düz metin yapılandırma
> bilgileri yeni bağımsız arayüz için bu belgeyle güncellenmiştir.

# Slayer Control 0.0.7

Fedora için ayrı CLI, Qt, KDE, GNOME ve yüz tanıma yardımcı RPM'leri:
`./packaging/build-rpm.sh`. Yapılandırma güncellemelerde korunur;
uygulama kurulumu donanım ayarı uygulamaz. [RPM kurulum notları](docs/RPM-KURULUM.txt).

0.0.7: Klavye etiketleri KDE/GNOME Wayland oturumunun gerçek XKB haritasından
okunur; Türkçe F/Q, başka diller ve tuş yeniden atamaları izlenir. Kullanıcının
onayladığı ISO düzeni L biçimli Enter ve yüksek numpad tuşlarıyla çizilir.
Shift/AltGr etiketleri ve LED'lerin son statik RGB kaydı görünür. LED konumu
kalibrasyonu masaüstü tuş haritasından ayrıdır; doğrulanmamış eşlemeler sayılır.
Qt/KDE arayüzünde yalnız doğrulanmış donanım efektleri sunulur. Efektin hızı,
parlaklığı, dalga yönü ve profil kaydı vardır; animasyon için süreç tutulmaz.

Hakkında sayfası varsayılan Fedora duvar kâğıdını sistem paketinden okuyan
hafif laptop çizimi, uygulama/BIOS/sürücü sürümleri ve bağlantı özeti içerir.
Üreticiye göre 3 USB-A + 2 USB-C fiziksel port, bağlı aygıt sayısından ayrı
sunulur. [Hakkında önizlemesi](docs/ui-0.0.7/about-light.png).
Yerleşim tercihi kullanıcıya ait `slayerctl/interface.ini` dosyasındadır;
donanım LED kalibrasyonu mevcut yönetici denetimli yapılandırmadadır.
Ayrıntılar: [0.0.7 notları](docs/UI-0.0.7.txt).

Qt uygulaması Kirigami'ye yakın düz kartlar, sistem ikonları ve temaya uyumlu
renklerle düzenlendi. On bölüm: Genel Bakış, Klavye ve RGB, RGB Profilleri,
Güç ve Pil, Performans ve Fan, Grafik ve MUX, Aygıtlar ve USB, Otomasyon,
Tanılama ve Hakkında. Donanım destek tablosu Tanılama'nın ikinci sekmesindedir.
Klavye önizlemesi fare/yön tuşlarıyla kullanılabilir; renk ve parlaklık
değişiklikleri yalnız Uygula ile gönderilir. Gelişmiş ayarlar açılır bölümlerdedir.
Qt Widgets korunur; ek QML çalışma zamanı veya arka plan hizmeti eklenmez.
`Alt+1`…`Alt+9` ve `Alt+0` bölüm değiştirir; `Ctrl+R` bilgileri yeniler.

[Koyu tema önizlemesi](docs/ui-0.0.7/overview-dark.png) ·
[Klavye önizlemesi](docs/ui-0.0.7/keyboard-dark.png) ·
[Grafik modu şeması](docs/ui-0.0.7/graphics-dark.png).

Doğrudan bölüm açmak için örnek: `slayer-control --page graphics`.
Diğer sayfa adları: `overview`, `keyboard`, `profiles`, `power`, `performance`,
`devices`, `automation`, `diagnostics`, `about`; eski `hardware` adı destek tablosunu açar.

0.0.6 kod incelemesi: EFI işlemlerinde ortak kernel kilidi, başarısız yazma
sonrası doğrulanan geri alma, RGB tampon temizliği ve AC/DC olaylarının yalnız
ilgili ayarları uygulaması düzeltildi. LED metaverisi kopyaları ve ortak
parlaklığın yinelenen okumaları azaltıldı. CLI, Qt ve KDE sürümü `VERSION`
dosyasından üretilir. Testler, ölçümler ve kalan doğrulama sınırları:
[0.0.6 inceleme raporu](docs/REVIEW-0.0.6.md).

Game Garaj Slayer R9T için C++20 CLI, Qt Widgets uygulaması ve yerel masaüstü entegrasyonu.

Bu sürüm, doğrulanmış Linux arayüzlerini kullanır. Windows ControlCenterX'te görülen her özellik bu firmware üzerinde güvenle yazılabilir durumda değildir. `slayerctl capabilities` ve KDE sayfası hazır, doğrulanmamış ve desteklenmeyen özellikleri açıkça gösterir. **Genel EC/NVRAM, özel fan eğrisi, CPU watt/OC/voltaj ve OEM cTGP yazmaları kapalıdır.** İmzalı R9T köprüsü üç pil koruma profili, Fn kilidi, fabrika fan/termal modları, iki fan RPM ve sabit alanla sonraki açılış MUX seçimi sunar. MUX geçişinin fiziksel açılış doğrulaması henüz yapılmadı. Uydurma başarı mesajı veya zorla açma seçeneği yoktur.

## Uygulama, systemd ve masaüstleri

```sh
slayer-control                     # Slayer Control Qt uygulaması
slayer-control --page keyboard     # Doğrudan görsel klavye
slayerctl gui                      # Aynı uygulamayı aç
slayerctl kde                      # KDE Sistem Ayarları KCM
slayerctl power status             # KDE/GNOME ortak masaüstü güç servisi
slayerctl power set balanced       # OEM Turbo/MUX ile aynı işlem değildir
slayerctl service status
sudo systemctl enable --now slayerctl.service
sudo systemctl enable slayerctl-resume.service
```

Uygulama altı bölüme ayrılır: Genel Bakış, Klavye/RGB, Profiller, Güç/Pil,
Donanım Desteği ve Tanılama/USB. Renk seçici, ölçeklenen görsel klavye ve
Türkçe efekt adları bulunur. Pencere Qt Widgets/raster kullanır; OpenGL widget,
web motoru ve sürekli sensör taraması yoktur. Normal kullanıcı olarak açın;
gerekli değişikliklerde polkit yetki ister. Uygulama kapalıyken GUI çalışmaz.

`slayerctl.service` açılışta bir kez çalışıp çıkar. Başarılı çalıştıktan sonra
`inactive` görünmesi beklenir; Result=success önemlidir. Uyanma hizmeti
sleep.target boyunca süreç tutmadan hazır kalır ve uyanınca seçili ayarları
uygular. Boş yapılandırma donanım değişikliği yapmaz. Önceki
slayerctl-restore.service yeni kurulumda devre dışı bırakıldı; iki açılış
hizmetini birlikte etkinleştirmeyin. Gerçek suspend/uyanma testi yapılmadı.

KDE KCM beş sekmeye ayrılır. GNOME için native Quick Settings eklentisi
`/usr/share/gnome-shell/extensions/slayer-control@local` altına kurulur. GNOME
49–51 oturumunda, oturumu yeniden açtıktan sonra:

```sh
gnome-extensions enable slayer-control@local
```

Eklenti menü açılınca bilgileri okur; profil/parlaklık/efekt ve mevcutsa
fan/pil koruma eylemleri sunar. Polling yoktur. Bu makinede GNOME Shell kurulu
olmadığından yalnız ES-module sözdizimi denetlendi; GNOME çalışma zamanı testi
yapılmadı. GNOME Ayarları için üçüncü taraf panel eklenti API'si sunulmadığından
onun ikilisi yamalanmadı. KDE/GNOME güç servisi ve kernelin standart LED/pil
arayüzleri paylaşılır; Qt uygulaması GTK uygulaması olarak sunulmaz.

Kapsamlı kaynak ve yetenek incelemesi: [araştırma raporu](docs/ARASTIRMA-0.0.3.md).
MUX/fan eğrisi/OC ve OEM firmware yazmaları hâlâ model protokolü gerektiriyor;
araştırma bunların tamamının açıldığı anlamına gelmez.

## Kullanım

```sh
slayerctl status
slayerctl bios
slayerctl doctor
slayerctl capabilities
slayerctl sensors
slayerctl usb list
slayerctl kde
sudo slayerctl keyboard brightness 50
sudo slayerctl keyboard color 3DAEE9
sudo slayerctl keyboard key 42 FF8000
sudo slayerctl keyboard effect wave --duration 10 --fps 10
slayerctl keyboard accent on
slayerctl keyboard accent off
slayerctl --dry-run battery limit 80 60
```

Vurgu rengi KDE Kameleon üzerinden oturumda yönetilir; `accent` için sudo kullanmayın. Klavye standart multicolor LED sürücüsünü gerektirir. Secure Boot sürücüyü reddediyorsa uygulama bunu aşamaz. Tuş indeksleri sürücünün 6×21 matrisidir; fiziksel tuş adları değildir. `keyboard status` mevcut indeksleri gösterir.

Efektler: `rainbow`, `wave`, `breathe`, `cycle`. Varsayılan süre 10 saniye, üst sınır 3600 saniye; FPS 1–20. ITE tampon desteğiyle kare başına toplu USB güncellemesi yapılır. Bu destek yoksa çok tuşlu renk efektleri en fazla 2 FPS çalışır. SIGINT/TERM/HUP ve normal çıkışta önceki renk/parlaklık geri yüklenir. SIGKILL veya güç kesintisinde bu garanti edilemez; `keyboard reset` tamponu sıfırlar ve beyaz rengi yükler. KDE vurgu eşlemesi ve başka RGB yazılımları efektlerle aynı anda kullanılmamalıdır.

## Fiziksel klavye, profiller ve donanım efektleri (0.0.2)

KDE sayfasındaki Türkçe Q/ISO klavyede tuşa tıklayıp renk seçebilirsiniz. Bu,
R9T için bir görsel taslaktır; tüm fiziksel LED eşleşmeleri bu cihazda henüz
tek tek doğrulanmadı. `keyboard layout list` her tuşu `template` veya
`calibrated` olarak belirtir. Yanlış LED yanıyorsa KDE'de seçili tuş için
LED indeksini değiştirin, “LED'i 2 saniye göster” ile kontrol edin ve eşlemeyi
kaydedin. Tanıma işlemi sonunda önceki statik renk/parlaklık geri yüklenir.
Aktif donanım animasyonunu tanıma veya statik renk yazması sonlandırabilir.

```sh
slayerctl keyboard layout list
sudo slayerctl keyboard identify 66
sudo slayerctl keyboard layout set W 66
sudo slayerctl keyboard key W FF0000
sudo slayerctl keyboard profile create kendi 101010 50
sudo slayerctl keyboard profile set-key kendi W FF0000
sudo slayerctl keyboard profile set-group kendi wasd FF0000
sudo slayerctl keyboard profile set-group kendi arrows 3DAEE9
sudo slayerctl keyboard profile base kendi 101010 40
sudo slayerctl keyboard profile apply kendi
sudo slayerctl keyboard profile capture mevcut
slayerctl keyboard profile list
slayerctl keyboard profile show kendi
sudo slayerctl keyboard profile delete kendi
```

`capture` sürücünün statik renk önbelleğini kopyalar; aktif donanım efektini
algıladığı anlamına gelmez. Gruplar: `wasd`, `arrows`, `all`. Temel renk yalnız
ayrı renk atanmamış LED'leri etkiler. Dosyalar `/etc/slayerctl/rgb-profiles/`
altında sade `anahtar=değer` biçimindedir. Profil oluşturmak/düzenlemek donanıma
yazmaz; `apply` uygular. Var olan dosyalar oluşturma sırasında ezilmez.
`oyun`, `gece`, `dalga` örnek profilleri kurulur; otomatik etkinleştirilmez.
Kalibrasyon `/etc/slayerctl/keyboard-layout.conf` dosyasındadır.

```sh
slayerctl keyboard hardware list
sudo slayerctl keyboard hardware wave --speed 5 --brightness 50 --direction right
sudo slayerctl keyboard hardware off
sudo slayerctl keyboard profile effect benim-dalgam wave 5 50 right
sudo slayerctl config set rgb_profile benim-dalgam
sudo slayerctl apply-config
sudo slayerctl config unset rgb_profile
```

Donanım efektleri: `wave`, `breathe`, `rainbow`, `reactive`, `ripple`, `marquee`,
`raindrop`, `aurora`, `spark`; `off` ışığı kapatır. Hız 1–10, parlaklık %0–100;
yön yalnız wave için `right`, `left`, `up`, `down`. Bu yol tek HID LED raporu
ile çıkar: slayerctl döngüsü, USB polling veya arka plan süreci kalmaz.
Animasyon aygıtta, güç sürdüğü ve başka denetleyici modu değiştirmediği sürece
çalışır. **Flash/EEPROM'a kaydetmez**; sonraki açılış için `rgb_profile` seçin.
Uyku/uyanma sonrasında firmware efekti sıfırlayabilir; etkin
`slayerctl-resume.service` açıkça kaydedilmiş RGB profilini yeniden uygular. KDE vurgu rengi eşlemesini donanım efekti öncesinde kapatın.

Protokol [TUXEDO sürücü kaynağı](https://github.com/tuxedocomputers/tuxedo-drivers/blob/master/src/ite_8291/ite_8291.c)
ve yerel ITE8291r3 kaynaklarıyla karşılaştırıldı. R9T model kimliği, USB
048d:600b, rev0003 ve HID arayüz 1 birlikte denetlenir. Genel hidraw yolu,
ham paket argümanı, USB ayırma veya rastgele kontrol komutu sunulmaz.
Bu komutun hidraw erişimi ITE LED modülünden ayrıdır; standart LED/tuş renkleri
ise ITE modülünün yüklenmesini gerektirir. Gerçek cihaz üzerinde tüm
animasyonların görsel davranışı henüz doğrulanmadı; paket ve güvenlik sınırları
taklit aygıt üzerinde test edildi.

Açılışta `rgb_profile` varsa eski `keyboard_color` ve `keyboard_brightness`
ayarlarına öncelik verir. `config unset rgb_profile` ile bu seçimi kaldırın.
KDE'nin zorunlu eklenti metaverisi dışında kullanıcı ayarlarında JSON yoktur.

## Pil, fan, MUX ve USB

`battery limit ÜST [ALT]` yalnız kernel `charge_control_*_threshold` sunarsa çalışır. Üst eşik 50–100, alt eşik 0–95 ve üstten küçük olmalıdır. Değerler geri okunur; başarısız işlemde önceki eşikler geri yüklenmeye çalışılır. Pil firmware'inin gerçek şarj davranışı ayrıca gözlemlenmelidir.

`profile set PROFİL` yalnız kernelin bildirdiği `platform_profile_choices` listesini kabul eder. `fan auto|boost` ve `battery profile ...` ancak doğrulanmış bir platform sürücüsü ilgili arayüzleri sağlarsa etkinleşir. Salt okunur `slayerctl_probe` modülü yazma arayüzü sağlamaz; `slayerctl_r9t` köprüsü pil koruma, Fn kilidi, iki fan RPM, Auto/Boost ve standart dengeli/performans termal profillerini sağlar. Özel eğri yazılmaz.

`gpu status` sürücü/panel bilgisini gösterir. `gpu mode hybrid|dedicated|integrated`, yalnız tam eşleşen köprü üzerinden firmware'in bir sonraki açılış seçimini kaydeder. Otomatik yeniden başlatma veya canlı ekran geçişi yapmaz. Yalnız iGPU modunda NVIDIA'ya bağlı harici portlar kapanabilir. Dönüş yolu `gpu mode hybrid` veya BIOS Advanced Display Mode'dur. Fiziksel yeniden başlatma testi bekliyor. MUX ile uygulama bazında GPU seçimi aynı şey değildir.

`usb powersave PORT on|off` standart çalışma zamanı güç yönetimini; `usb wake PORT on|off` uyandırmayı ayarlar. Portu `usb list` çıktısından alın. Hub, depolama, HID ve arayüz sınıfı okunamayan aygıtlar korunur. Composite aygıtların arayüzleri de denetlenir. Genel USB reset, ham kontrol paketi veya test/tarama yazması yoktur. Ayrı `settings camera` komutu yalnız sabit kimlikli dahili RGB/IR kameranın USB yetkilendirmesini değiştirir; bu işlem kamera arayüzlerini kaldırıp yeniden ekler.

## Kalıcılık

```sh
sudo slayerctl config set keyboard_brightness 50 keyboard_color 3DAEE9
sudo slayerctl config set battery_end 80 battery_start 60
slayerctl config show
sudo slayerctl apply-config
sudo systemctl enable slayerctl.service slayerctl-resume.service
```

Dosya: `/etc/slayerctl/slayerctl.conf`. Kullanıcı ayarları JSON değildir. Anahtarlar: `rgb_profile`, `rgb_profile_ac`, `rgb_profile_battery`, `keyboard_brightness`, `keyboard_color`, `battery_profile`, `battery_end`, `battery_start`, `profile`, `cpu_boost`, `cpu_min_mhz`, `cpu_max_mhz`, `desktop_profile_ac`, `desktop_profile_battery`. CPU frekans çifti birlikte kaydedilir. Tek komutta birden fazla anahtar atomik kaydedilebilir. `config clear` tüm seçilmiş ayarları temizler. Varsayılan dosya boştur ve donanım değişikliği yapmaz. Açılış hizmeti bir kez çalışıp çıkar; daemon yoktur. KDE vurgu rengi kalıcılığını KDE kendi yönetir.

## Derleme / kurulum

Fedora için çekirdek: `gcc-c++ cmake`. KDE modülü için ayrıca `extra-cmake-modules qt6-qtbase-devel qt6-qtdeclarative-devel kf6-kcmutils-devel` gerekir.

```sh
make core        # CLI-only temiz yapılandırma Qt gerektirmez
make gui         # Qt Widgets uygulaması + CLI
make kde         # Qt uygulaması + KDE modülü
make check       # geçici dosya ağacında donanım taklidi testleri
sudo make install
```

Kurulum öneki `/usr`, yapılandırma `/etc` olmalıdır; polkit ve hizmet yolları bu düzene aittir. Makefile en fazla iki derleyici işi kullanır; `JOBS=1` ile azaltabilirsiniz. `MinSizeRel`, LTO ve sembol ayıklama kullanılır. Sürekli sensör/NVML sorgulaması yoktur. CLI çıktısındaki `--format=tsv` ve KDE'nin iç TSV iletişimi otomasyon içindir. KDE'nin zorunlu eklenti metaverisi JSON biçimindedir; kullanıcı ayarı değildir.

Kaldırma: `sudo systemctl disable --now slayerctl.service slayerctl-resume.service slayerctl-restore.service` ardından `docs/KURULUM.txt` listesindeki proje dosyalarını kaldırın; akmods NVIDIA/ITE sürücülerini kaldırmayın.

## İsteğe bağlı salt okunur EC tanı modülü

`make -C kernel` yalnız mevcut kernel için derler. Normal kurulum bu modülü kurmaz/yüklemez. `kernel/slayerctl_probe.c`, tam R9T/BIOS/GPU eşleşmesiyle yalnız sabit izin listesindeki ECRR okumalarını sunar; genel adres veya yazma parametresi yoktur. Modül yüklenince EC okunmaz; kullanıcı `probe` isterse her alan en fazla saniyede bir okunur. Secure Boot için imzalama gerekir. Otomatik kabul testleri taklit dosya ağacını kullanır; gerçek EC’ye erişmez. 0.0.4 geliştirmesinde ayrı bir gerçek cihaz doğrulaması yapıldı; ayrıntı ve sınırlar aşağıdaki inceleme raporunda.

Araştırma: komşu `../slayerctl-arastirma/ARASTIRMA-SONUCU.txt`. 0.0.5'te firmware'in yerel 180 bayt OEM yapısı ve MUX alanı ayrı doğrulandı; Windows managed 188 bayt yapısı yazılmaz. Diğer belirsiz alanlar kapalıdır. Yeni sözleşme ve sınırlar: `docs/REVIEW-0.0.5.md`. Kaynaklar ve doğrulama sınırları `docs/GUVENLIK.txt` dosyasındadır.


## 0.0.4: pil koruma, CPU ve AC/DC profilleri

```sh
slayerctl cpu status
sudo slayerctl cpu boost off
sudo slayerctl cpu limits 500 2500   # Donanımın bildirdiği aralık içinde
sudo slayerctl cpu limits reset     # Yalnız o andaki kernel sınırlarına döner
slayerctl gpu limits                # Açık istekle tek NVIDIA sorgusu
sudo slayerctl battery profile balanced
sudo slayerctl config set battery_profile balanced
sudo slayerctl config set rgb_profile_ac oyun rgb_profile_battery gece
sudo slayerctl config set desktop_profile_ac balanced desktop_profile_battery power-saver
sudo slayerctl apply-config
```

CPU kontrolü standart CPUFreq arayüzüdür. Bütün policy aralıkları önce
kontrol edilir; yarım kalan işlem önceki değerleri geri yüklemeyi dener ve
başarısız geri dönüşü bildirir. Tuned/masaüstü profili daha sonra bunları
değiştirebilir. `reset` boost açmaz ve önceki özel ayarı hatırlamaz.

Bu cihazda NVIDIA min/max/default sınırlarını veriyor ancak mevcut limiti
`N/A` bildiriyor. Bu yüzden **GPU güç yazması reddedilir**. `gpu power-limit`
komutunun bulunması bu kartta cTGP/Dynamic Boost kontrolünün açıldığı anlamına
gelmez. Başlangıç ekranı NVIDIA’yı aktif sorgulamaz; yalnız `gpu limits` yapar.

`slayerctl_r9t` yalnız GAME GARAJ / SLAYER R9T / R9T V1, BIOS N.1.46GAR16,
RTX5060 alt kimliği, EC proje26 ve pil destek biti birlikte eşleşirse yüklenir.
Standart `/sys/class/power_supply/BAT0/charge_types` ile cihaza özel
`/sys/devices/platform/slayerctl/charging_profile` aynı kontrolü sunar:

| Slayer adı | Linux şarj türü | EC profil değeri |
|---|---|---|
| high-capacity | Standard | 0 |
| balanced | Long_Life | 1 |
| stationary | Trickle | 2 |

Bu modlar **yüzde dolum sınırı değildir**. Yalnız EC0x07a6 içindeki iki profil
biti değiştirilir, diğer altı bit korunur. Genel adres/yazma veya force
seçeneği yoktur. Yeni BIOS otomatik güvenli sayılmaz; tam BIOS eşleşmesi
başarısızsa modül yüklenmez. Akmod paketi yeni kernel için modülü derleyip
mevcut kayıtlı MOK ile imzalar; API değişikliği derlemeyi engelleyebilir.
Modülü yüklemek/kaldırmak profil değiştirmez. Kullanıcı seçimi config ile
ayrıca kalıcılaştırılır. Bu kernel arayüzü, masaüstünün her yerleşik pil
panelinin üç modu kendiliğinden göstereceği anlamına gelmez; Slayer KCM,
Qt uygulaması ve GNOME eklentisi bunları açıkça sunar.

`slayerctl-power-event.service` udev adaptör/USB-PD olayında bir kez çalışır.
Aynı kaynak ve aynı ayar için tekrar yazma yapmaz. Kalıcı izleme süreci yoktur.
RGB ve masaüstü AC/DC seçimlerini kaldırmak için:

```sh
sudo slayerctl config unset rgb_profile_ac rgb_profile_battery desktop_profile_ac desktop_profile_battery
```

`fan curve save AD CPU_EĞRİSİ GPU_EĞRİSİ` **çevrimdışı** dosya hazırlar.
3–12 nokta, artan35–95°C, azalmayan%30–100 ve90–95°C’de%100 son nokta gerektirir.
Bu kontroller güvenli donanım eğrisi kanıtı değildir; `fan curve apply` reddedilir.
`settings fn-lock|win-lock|usb-charging|ac-recovery` yalnız kernelin gerçekten
sunduğu anlamlı arayüzleri kullanır; mevcut R9T köprüsü bunları sunmaz.

İnceleme, test ve ölçümler: [0.0.4 inceleme raporu](docs/REVIEW-0.0.4.md).
Aynı ailedeki diğer üreticiler: [karşılaştırma](docs/DIGER-MARKALAR-KARSILASTIRMA.md).


BIOS kilidi CLI/helper, HID efekt yolu ve sysfs yazmaları için de geçerlidir.
`kernel/model_contract.h` kernel köprüsü ile aynı model/SKU/BIOS sözleşmesini
paylaşır. Desteklenen BIOS tam olarak `N.1.46GAR16` olmalıdır; farklı/eksik
sürümde bilgi komutları çalışır, donanım yazmaları reddedilir. `--force` yoktur.
`slayerctl status` beklenen BIOS'u ve `hardware_writes` durumunu gösterir.

## 0.0.5: Fn, kamera, fan RPM, güç, SMT ve MUX

```sh
slayerctl settings fn-lock status
sudo slayerctl settings fn-lock on       # off: normal işlev
sudo slayerctl config set fn_lock on     # Açılış / uyanma
slayerctl fan status                     # İki fan RPM ve sıcaklık
sudo slayerctl fan boost                 # EC maksimum soğutma
sudo slayerctl fan auto                  # EC otomatik denetim
sudo slayerctl config set fan_mode auto
sudo slayerctl profile set balanced      # Fabrika dengeli modu
sudo slayerctl profile set performance   # Fabrika Turbo modu
slayerctl power set power-saver          # KDE/GNOME masaüstü güç profili
slayerctl settings camera status
sudo slayerctl settings camera off       # Dahili RGB/IR USB erişimini kapat
sudo slayerctl settings camera on        # Yeniden yetkilendir
sudo slayerctl config set camera off     # Tercih edilirse kalıcı
slayerctl cpu topology                   # Gerçek çekirdek / L3 grupları
slayerctl cpu smt status
sudo slayerctl cpu smt off               # 16 fiziksel çekirdek, 16 iş parçacığı
sudo slayerctl cpu smt on                # 16 fiziksel çekirdek, 32 iş parçacığı
sudo slayerctl config set cpu_smt on
slayerctl gpu status                     # Panel bağlantısı + firmware seçimi
sudo slayerctl gpu mode integrated       # Yalnız iGPU: sonraki açılış
sudo slayerctl gpu mode dedicated        # Yalnız NVIDIA: sonraki açılış
sudo slayerctl gpu mode hybrid           # Hibrit: sonraki açılış
```

SMT fiziksel CCD kapatma değildir. BIOS'taki CCD0/CCD1 kapatma seçeneği
bulundu; güvenli runtime commit yolu doğrulanmadığı için BIOS'ta bırakıldı.
`forceoff` ve firmware kilidi aşılmaz. SMT için periyodik CPU hotplug yoktur.

`settings camera` standart Linux USB yetkilendirmesidir; firmware E-shutter
veya elektriksel güç kesildiği iddiası değildir. Yalnız dahili, sabit
2b7e:c906 rev0004 aygıtı seçilir; görüntülü görüşme kesilebilir.

Kapalı USB şarj için kod destek bitini denetler. Bu cihazın N.1.46GAR16 EC'si
USB şarj destek bitini bildirmiyor; özellik kilitli gösterilir. Başka modelin
adresine yazılarak açıkmış gibi gösterilmez.

MUX firmware içinde kalıcıdır; yapılandırma dosyasına tekrar eklenmez.
Klavye/fan/SMT/kamera için `config unset ANAHTAR` seçimi kaldırır. Kullanıcı
bir tercih kaydetmedikçe güncelleme mevcut seçimleri değiştirmez. Arka planda
sürekli çalışan yeni süreç yoktur. Qt uygulaması ve yerel KDE KCM tüm yeni
kontrolleri sunar; GNOME Quick Settings uzantısı aynı CLI'yi çağırır.
