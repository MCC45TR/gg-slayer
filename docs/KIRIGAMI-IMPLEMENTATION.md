> Güncel dağıtım: [COPR](COPR.md). Aşağıdaki yerel derleme/kurulum
> kayıtları geçmiş doğrulama notlarıdır. Yeni paketler yalnız COPR üzerinde
> derlenir; kurulum ve güncelleme DNF ile yapılır. Eski build/install betikleri
> COPR/DNF girişlerine yönlendirilmiştir.

# Kirigami uygulaması

Bağımsız `slayer-control` artık Qt Quick/Kirigami kullanır. Yedi bölüm, genişliğe
bağlı kart düzenleri, dar pencerede çekmece menü ve RGB iç sekmeleri vardır.
KDE Sistem Ayarları modülü mevcut arayüzünü korur. Eski Widgets arayüzü yalnız
regresyon testi için `gui/legacy_main.cpp` içinde tutulur; kurulmaz.

## Derleme ve önizleme

```
make kde
build/slayer-control --fixture
ctest --test-dir build --output-on-failure
```

Önizleme donanım servisine bağlanmaz, hiçbir ayar yazmaz. Üretim çalıştırması
kurulmuş D-Bus hizmeti gerektirir. Ekran görüntüsü almak için:

```
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
SLAYER_UI_SCREENSHOTS=/tmp/slayer-preview build/slayer-control --fixture
```

Bu komut 6 boyutta 7 sayfa ve 14 ek panel görünümü üretir. Tema işletim sisteminden
gelir. `QT_SCALE_FACTOR=1.5` veya `2` ile ölçekli denetim yapılabilir.

## İletişim ve yetki

`org.gamegaraj.Slayer1`, `/org/gamegaraj/Slayer1` nesnesinde ReadState,
ReadTelemetry, Diagnose, Apply(command, remember), Subscribe ve Unsubscribe
metotlarını sunar. D-Bus/systemd aktivasyonu ile başlar. Görünür istemci varken
olayları izler; istemci kapanınca 30 saniye boşta kalınca sonlanır.

Servis aynı slayerctl komut uygulamasını bağlar; shell çalıştırmaz. Apply izin
listesiyle sınırlıdır, değerleri CLI yeniden doğrular. Polkit öznesi istemcinin
D-Bus gönderen kimliğidir. Etkin yerel wheel/sudo yöneticileri control eylemini
parolasız kullanır. Diğer yerel kullanıcılar auth_admin_keep ile doğrulanır;
uzak ve etkin olmayan oturumlar yetki alamaz. Kurtarma tüm kullanıcılar için
ayrı auth_admin eylemidir. Genel pkexec/helper politikası gevşetilmez.

Paket kurulumu hizmeti başlatmaz veya donanım ayarlarını uygulamaz.
Bu geliştirme oturumunda uygulama ve model sürücüsü gerçek sisteme imzalı RPM
olarak kuruldu. Secure Boot ve SELinux Enforcing korundu. Canlı TPM şifreleme,
yalıtılmış kurtarma, yönetici ilkesi ve çift yönlü eşitleme doğrulandı.

## Kaynak kullanımı

Yalnız görünür Genel Bakış 3 saniyede bir hafif telemetry ister. Diğer sayfalar,
küçültülmüş pencere ve kapanmış uygulama ölçüm yapmaz. Telemetry LED durumunu,
BIOS'u ve profil dosyalarını tekrar taramaz. Fan hareketi en fazla 12,5 kez/saniye
ilerler ve azaltılmış harekette durur. Sayfalar Loader ile ihtiyaç anında yüklenir.
NVIDIA kullanımı yalnız runtime-PM durumu active olan cihazda salt okunur NVML
ile alınır; askıdaki cihaz sorgulanmaz. Veriler yoksa sıfır üretilmez.

D-Bus aboneliği ölçüm zamanlayıcısından ayrıdır: diğer sayfalarda durum olayları
alınmaya devam eder. Güç kaynağı/LED/R9T değişiklikleri netlink ile önbelleği
geçersiz kılar. Gereksiz aynı yapılandırma yazmaları atlanır.

## Ayar koruması ve geçiş

Yeni gerçek sistem yazmaları `systemd-creds --with-key=tpm2 --tpm2-pcrs=7`
ile kimliği doğrulanan şifrelemeyi kullanır. Gizli düz metin yalnız root’a ait anonim bellek dosyasında (memfd) tutulur;
kalıcı veya geçici bir disk dosyasına yazılmaz, süreç kapanınca otomatik bırakılır. Şifreli dosya
atomik olarak 0600 izinle kaydedilir. Eski düz metin kurulumlar okunabilir;
GUI'nin ilk yetkili değişikliğinde ana yapılandırma şifrelenir. Yeni/değiştirilen
RGB profilleri de şifrelenir. TPM başarısızsa düz metne sessiz dönüş yapılmaz.
CLI'da `config protect` mevcut ana yapılandırmayı dönüştürür.

TPM kaybında `config recover` eski şifreli dosyayı zaman damgalı kurtarma dosyasına
korur ve yeni TPM bağlı boş ayar dosyası oluşturur. Bu eski dosyayı çözmez veya
kaybolan RGB profillerini kurtarmaz. GUI yönetici doğrulaması ve açık açıklama sunar.
Yeni ayar seçilene kadar donanım durumu esas alınır. Mevcut RGB dosyaları silinmez.

KDE renk yönetimi oturumdaki KDED Kameleon arayüzü üzerinden yönetilir. Özel
modda vurgu takibi kapatılır; vurgu modunda KDE'nin kendi takibi açılır ve uygulama
kapandığında da KDE tarafından sürdürülür. Parlaklık ayrıca devredilebilir.
KDE'nin kendi panelindeki kontrolleri gizlemek için sahte destek bilgisi üretilmez.

## Office ve bağımsız performans eşitlemesi

0.0.13 sürücüsü bu cihazın ACPI PMSC yöntemindeki TBME/UFME bitlerinden Office,
Balance ve Performance durumlarını çözümler. Canlı fiziksel tuş testi Office 0xa0 → Performance 0x30 geçişini gösterdi.
Bu nedenle yalnız 0x90 maskesindeki mod bitleri değiştirilir. Bit 5 ayrı fan
Tablosu bayrağıdır ve Fan Boost (bit 6) gibi bütün mod geçişlerinde korunur.
0x20 Balance, 0x30 Performance ve 0xa0 Office olarak doğru okunur.

Fabrika modu /sys/devices/platform/slayerctl/thermal_mode ve thermal_mode_choices
üzerinden sunulur. Standart platform_profile sağlayıcısı kaldırıldı: TuneD'nin
bu sağlayıcıya otomatik yazması ve onu izlemesi, eşitleme kapalıyken EC/KDE
bağımsızlığını bozuyordu. CLI eski sürücüler için standart ABI'ye salt uyumluluk
sağlar; yeni eşitleme bağımsız kontrolü ve ACPI sorgu izleyicisini zorunlu tutar.

KDE ile eşitle açılınca ilk olarak donanımdaki mevcut mod esas alınır.
Office ↔ Pil tasarrufu, Balance ↔ Dengeli ve Performance ↔ Performans eşlenir.
Kapatılınca iki tarafın mevcut durumu korunur ve bağımsız değişiklikler yapılabilir.
Başlangıçta kayıtlı eski EC/KDE tercihleri, eşitleme açıkken gerçek donanım
modunun üstüne yazılmaz. Tanınmayan durumlarda yeni değer tahmin edilmez.

Üç Uniwill WMI olay kanalı ve bu BIOS'un EC _Q88 sorgusu izlenir. Fiziksel
mod düğmesi WMI bildirimi üretmediği için yalnız WMI yeterli değildir. GPL ile
sunulan acpi_ec_add_query_handler kullanılır; orijinal _Q88 AML yöntemi tam bir
kez çalıştırılır, ardından gerçek EC durumu okunur. Sökme sırasında firmware'in
özgün işleyicisi geri gelir. profile_event_count gözlemi doğrulamak için okunabilir.
Durum değişirse sysfs ve platform uevent bildirimi üretilir. EC tarama döngüsü yoktur.
KDE değişimleri mevcut UPower PowerProfiles (bu sistemde tuned-ppd)
PropertiesChanged sinyaliyle izlenir. Eşitleme açıkken broker olay beklemek için
açık kalır; eşitleme kapalı ve istemci yokken 30 saniyede kapanır. Ölçüm zamanlayıcısı
bundan bağımsızdır.

CLI: profile sync on|off|status. Olay aktarımı: profile sync from-hardware veya
from-desktop. İş mantığı aynı CLI çekirdeğindedir; arayüz yalnız istek gönderir.

## TR klavye ve KDE ışık bütünleşmesi

R9T TR fotoğrafı klavye geometrisinin referansıdır: kademeli satırlar, L biçimli
Enter, alçaltılmış ters-T ok grubu ve sayısal takım çizilir. Karakterler mevcut
XKB düzeninden gelir; Shift/AltGr simgeleri de gösterilir. Fotoğraf LED indeksini
kanıtlamaz: doğrulanmamış konumlar kalibrasyon şablonu olarak kalır.

Renk seçici seçili tuşun okunan RGB değeriyle açılır. Alanı yalnız odaklamak veya
tuş seçmek renk yazmaz. Kullanıcının değiştirdiği geçerli kod alan bitince, renk
iletişim kutusu da kabul edildiğinde uygulanır. Parlaklık sürükleme sırasında
75 ms aralıkla birleştirilir; arayüz geri bildirim satırı içerik yerini oynatmaz.

ITE sürücüsü `kernel/ite8291/` altında GPL kaynağı ve köken bilgisiyle bulunur.
Bu modelde genel TUXEDO yeşil/mavi azaltımı yapılmaz. USB tanımlayıcısındaki
numarasız rapor biçimi kullanılır. KDE'nin ayrı tuş renk yazımları 16 ms sessiz
aralıktan sonra tek altı satırlık kareye birleştirilir. Denetleyici için doğrulanmış
atomik kare komutu yoktur; altı satır USB üzerinden taşınır, 126 tam kare tekrar
edilmez. `buffer_input` kapanmadan LED çekirdeğinin bekleyen işleri tamamlanır.

Parlaklık ayrı 09/02 komutuyla değişir; donanım efekti korunur. Doğrulanan
`hardware_effect` arayüzü LED önbelleğini ve UPower bildirimini birlikte günceller.
Başarılı kareler/değişimler `controls/transfer_stats` ile ölçülebilir. Boşta tarama
yoktur. Gerçek test komutu `pkexec python3 packaging/validate-lighting-live.py`;
`--visual` ana renkler ve KDE parlaklık yolu için kullanıcı gözlemi ekler. Önceki
renk, efekt ve parlaklık sonunda geri yüklenir.

Yerel RPM üretimi: packaging/build-rpm.sh, packaging/build-kmod.sh ve
packaging/build-ite-kmod.sh.
packaging/install-validated.sh önceden üretilen paketleri yönetici doğrulamasıyla
kurar, mevcut kayıtlı akmods anahtarıyla imzalatır ve modülü yükler. TPM veya MOK
sıfırlamaz, otomatik eşitlemeyi açmaz. TuneD eski platform_profile izleyicisini
bırakırken masaüstündeki önceki seçim korunur. Yapılandırma yedeği TPM ile şifrelenir.

Canlı kurulum ve fiziksel tuş doğrulaması tamamlanmadan, örnek veri testleri gerçek
TPM/polkit/SELinux davranışının kanıtı sayılmaz. Güncel sonuçlar ui-kirigami altında
kaydedilir. SELinux'u devre dışı bırakan genel bir kural eklenmez.

Kaynaklar: bu cihazın DSDT PMSC ve 0x751 TBME/UFME alanları,
[Uniwill fabrika profilleri](https://github.com/tuxedocomputers/tuxedo-keyboard/blob/master/src/tuxedo_io/tuxedo_io.c),
[systemd-creds](https://www.freedesktop.org/software/systemd/man/latest/systemd-creds.html),
[polkit pkcheck](https://www.freedesktop.org/software/polkit/docs/latest/pkcheck.1.html).

## Ölçülen beyaz noktası

Kullanıcı canlı R/G/B sürgüleriyle nötr beyazı **160 / 255 / 255** olarak
doğruladı. Saf kırmızı, yeşil ve mavi kanal sırası da tek tek doğrulandı.
Statik/KDE karelerinde her kanal `round(input * white / 255)` ile düzeltilir;
KDE ve renk seçici mantıksal RGB değerlerini korur. Örneğin #71743A girişinin
USB çerçevesi 71 / 116 / 58 olur. Optik sonuç kullanıcıyla ayrıca doğrulanır.

`keyboard white-point R G B [--white]` yalnız güncel model sürücüsünün
white_point arayüzünü kullanır. --white kalibrasyon ile beyaz önizlemeyi tek
karede uygular. keyboard_white_point=160,255,255 TPM korumalı ayarda saklanır
ve KDE ışık sahibi olsa da açılışta geri yüklenir. Donanımın kendi animasyon
paletleri korunur; bu kalibrasyon statik/KDE renkleri içindir.

Araç: tools/white_balance.py (isteğe bağlı python3-pyside6 gerekir). Pencere
normal kullanıcıyla, sınırlı ve geçici LED yardımcısı yetkili olarak çalışır.
Sürgüler bellekte ve tek kareyle uygulanır; iptalde önceki renkler geri gelir.
Beyaz bu seçilince ölçüm D-Bus servisine gönderilir ve şifreli olarak saklanır.
KDE vurgu takibi ölçüm boyunca durur, bitince önceki duruma döner.

## Ekranla 20 renk eşleştirmesi

`tools/color_calibration.py` büyük, sabit sRGB kareyi gösterir ve yalnız klavyeyi
anlık sürgülerle değiştirir. Beyaz, üç ana renk, üç ara renk, yedi gri, turuncu,
mor, koyu camgöbeği, pembe, KDE yeşili ve siyah ölçülür. Her onay atomik olarak
kullanıcının veri dosyasına yazılır; yarıda kapanırsa kaldığı yerden devam edilir.
Ekran parlaklığı ve gece rengi ölçüm boyunca sabit tutulur.

`tools/fit_color_profile.py` 20 ölçümden RGB küpünü kapsayan Delaunay dört yüzlü
ağı üretir. Tek kanal eğrilerinin temsil edemediği karışımlar da modellenir.
Barycentrik ağırlıklarla ara renkler hesaplanır; sonuçlar 0..255 içinde kalır.
Mevcut ölçüm 65 hücre içerir. 20 ölçüm noktası aynen korunur. Bu sıfır fark,
ölçülmeyen renklerde optik doğruluk iddiası değildir. Eşleştirme bu ekranın
ölçümdeki koşullarına aittir.

Çalışma sırasında bu hesap driver içindedir; sürekli Python veya RGB servisi
gerekmez. `color_model.h` aynı tamsayı hesabını kullanıcı alanında ve kernelde
kullanır. 4913 RGB noktası Python ile kernel hesabı arasında karşılaştırılır;
tüm küp hacmi, indeksler, boyut, köşeler, siyah ve beyaz doğrulanır. Renk modeli
profil olarak TPM ile şifrelenir. KDE doğrudan LED sınıfına yazdığında da aynı
dönüşüm uygulanır. Açılışta ve sürücü güncellemesinde geri yüklenir.

CLI: `keyboard color-profile set AD HEX_PARÇALARI...`, `apply AD`, `off`, `reset`.
Yalnız sabit biçimli renk verisi kabul edilir; genel dosya/USB/EC komutu yoktur.
Fabrika donanım animasyonlarının kendi paleti korunur; model statik/KDE renklerine
uygulanır. Ölçümler ve nötr ton eğrisi calibration/ altında tutulur.

Algoritma referansları:
https://docs.scipy.org/doc/scipy/reference/generated/scipy.spatial.Delaunay.html
https://opencolorio.readthedocs.io/en/latest/api/processors.html
