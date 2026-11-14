# KDE kamera gizliliği ve azami pil dolumu

2026-11-14 · MCC45TR <mcc45tr@gmail.com>

Kamera için `org.gamegaraj.slayerctl.camera` adlı yerel Plasma 6 sistem tepsisi
bileşeni eklendi. `slayer-control-qt` paketiyle kurulur; Kirigami/Plasma renkleri,
tema simgeleri, doğal açılır görünüm, klavye kullanımı ve kaydırma desteğini
kullanır. Ana Slayer Control penceresi kapalıyken de erişilebilir.

KDE'nin kendi Kamera Göstergesi, PipeWire üzerinden kamera kullanımını izler;
donanımı açıp kapatan bir API sağlamaz. Bu nedenle mevcut KDE paketinin üzerine
yazılmadı. Yeni bileşen **kamera erişimi açık/kapalı** durumunu bildirir; bu
ifade bir uygulamanın o anda görüntü aldığı anlamına gelmez. Sistem Tepsisi
Ayarları → Girdiler → **Slayer Kamera Gizliliği** ile görünürlüğü değiştirilebilir.
Kullanıcının panel ayarları otomatik değiştirilmez.

## Veri ve yetki sınırı

- Başlangıçta, USB olayları sonrasında ve açılır görünüm açıldığında dar kapsamlı
  `slayerctl settings camera status` sorgusu çalışır. Telemetri, parola veya
  kamera görüntüsü okunmaz; broker salt okuma için başlatılmaz.
- Bir QML motorundaki bütün örnekler aynı singleton ve USB olay izleyicisini
  paylaşır. İzleme systemd'nin sd-device/sd-event arayüzlerinden Qt olay
  döngüsüne bağlanır. Periyodik yoklama ve ayrı resident süreç yoktur.
- Kullanıcı düğmeye bastığında sistem D-Bus brokerinin `Apply` çağrısına yalnız
  `settings camera on|off` ve ilgili kaydetme tercihi gönderilir. Mevcut polkit,
  tam model/BIOS, sabit dahili USB kimliği ve geri okuma kontrolleri korunur.
  Firmware E-shutter veya donanımsal lens kapağı olarak sunulmaz.
- Simge gerçek durum yeniden okunduktan sonra değişir. Yetki reddi, başarısız
  komut ve okuma hatası açıkça görünür. İşlem sürerken yeni komut kabul edilmez.
  RGB ve IR aynı aygıt olduğu için kapatma, yüzle oturum açmayı da etkiler.
- Okuma iki saniyeyle ve sınırlı çıktı boyutuyla kısıtlanır; USB olayları
  birleştirilir. Olay izleyicisi hata verirse durdurulur ve elle yenileme sunulur.
  Broker aboneliği açılmaz; bu bileşen brokerin beş saniyelik boşta çıkışını engellemez.

## Azami dolum alanı

Qt arayüzündeki azami dolum alanı desteklenmeyen cihazlarda da görünür.
Destek yoksa sayı yerine `—` ve açıklama gösterilir. Gerçek eşik sunulan cihazda
değeri düzenlemek donanıma yazmaz; yalnız **Uygula ve kaydet** eylemi yazar.
Okunamayan/geçersiz eşik ayrı bir durumdur; uygulanamaz.

KDE Sistem Ayarları Slayer modülünde de desteklenmeyen eşikler `—` gösterir.
Yalnız üst eşiği olan cihazlarda mevcut olmayan alt eşik gönderilmez.

Bu R9T/BIOS N.1.46GAR16'da `charge_control_end_threshold` yoktur. Mevcut
`charge_types` değeri `Trickle [Standard] Long_Life` olup üç firmware profili
sunmaktadır. Sayısal yüzde eşiğini desteklemeyen firmware için PowerDevil'in
yerel yüzde seçicisi etkinleştirilmedi; yapay bir eşik dosyası oluşturulmadı.
Önceki salt okuma EC/BIOS bulguları [CHARGING-R9T.md](CHARGING-R9T.md) içinde.
%80 sınırı uygulanmış değildir.

## Doğrulama

Ortam: Fedora 46/Rawhide, Plasma 6.8.2, Qt 6.12.0, systemd 262; CMake
MinSizeRel. Bileşen Plasma 6, Qt 6.5 ve üstünü hedefler; en eski sürümde
ayrı çalışma testi yapılmadı.

- Genel CTest paketi: **28/28** başarılı; 67 donanım fixture senaryosu ve yedi
  arayüz sayfasının altı boyutta yüklenmesi dahil.
- Yeni kamera C++ sözleşmesi: ilk açılışta yazma olmaması, boşta tekrarlı okuma
  olmaması, doğru komut/kaydetme argümanları, tekrarlı tıklama, yetki reddi,
  gerçek geri okuma, bozuk/tutarsız/büyük/eksik çıktı, zaman aşımı ve iyileşme.
- Gerçek ikili QML modülü derleme ve kurulum dizinlerinden yüklenir; Qt
  eklentisi ismi ve `$ORIGIN` bağımlılık çözümü doğrulanır.
- Qt Quick Test: kamera fare/klavye ve busy durumları, dar hata görünümü,
  desteklenmeyen pil alanının görünürlüğü, geçersiz eşik ve açık Uygula işlemi.
- Yeni üç CTest grubu ASan/UBSan yapılandırmasında da başarılı. C++ test
  hedefleri sanitizelidir; QML davranış testinde sistemin Qt Quick Test aracı kullanılır.
- `qmllint`: yeni bileşenlerde uyarı yok. Paket metadata kontrolü başarılı.
- İzole XDG dizinleriyle `plasmawindowed --smoke-test`: paketlenen applet
  offscreen/software host içinde yüklenir; kişisel panel değiştirilmez.
- Aşamalı kurulumun paket yolları, dosya izinleri, helper/CLI özdeşliği ve
  var olan yapılandırmayı koruma kontrolü başarılı.
- Gerçek kameranın mevcut durumu salt okunur sorgulandı: açık; sd-device
  izleyicisi etkin ve ilk okuma tamamlandı. Fiziksel kapatma/açma veya gerçek
  USB geçişi testi yapılmadı. Panel içindeki yerleşim ve gerçek çoklu ekran
  davranışı ayrıca kullanıcı oturumunda denenmelidir.
- Breeze açık tema/normal font ve koyu tema/%150 fontta 280/440 genişliklerinde
  20 fixture görüntüsü alındı; dar hata ve desteklenmeyen pil görünümleri
  incelendi. [Görüntüler ve ham kanıt](native-kde-2026-11-14/).

Kamera arka ucunu barındıran ayrı QtQml/PySide6 test sürecinde, bir saniye
ısınma sonrası beş saniye/100 Hz örneklemede 0 ek CPU tiki ve 0 çalışan alt
süreç görüldü. RSS 60308 KiB, PSS 24153 KiB bütün test host'una aittir.
Basit QtQml host'u da 0 ek CPU tiki üretmiştir; paylaşımlı sayfalardan ötürü
PSS farkı bellek kazancı olarak yorumlanmaz. Bu değerler kamera bileşeninin
veya plasmashell'in tek başına bellek maliyeti değildir; evrensel tüketim
garantisi vermez. [Ham ölçüm](native-kde-2026-11-14/backend-resources.json).

Çalışma ağacına değişiklik yapıldı. Sistem paketleri kurulmadı/yayınlanmadı;
kamera, pil profili ve firmware eşikleri test amacıyla değiştirilmedi.

Kaynaklar:

- [KDE 6.8.2 kamera göstergesi](https://github.com/KDE/plasma-workspace/blob/v6.8.2/applets/cameraindicator/qml/main.qml)
- [KDE 6.8.3 pil eşiği yardımcısı](https://invent.kde.org/plasma/powerdevil/-/blob/v6.8.3/daemon/chargethresholdhelper_linux.cpp)
- [Linux Uniwill pil arayüzleri](https://docs.kernel.org/admin-guide/laptops/uniwill-laptop.html#battery-charging-control)
