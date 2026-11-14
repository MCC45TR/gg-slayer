# KDE kamera göstergesi ve azami pil dolumu

2026-11-14 · MCC45TR <mcc45tr@gmail.com>

0.1.6 sürümünde ayrı `org.gamegaraj.slayerctl.camera` widget'ı kaldırıldı.
Kaynak kodu, QML eklentisi, USB olay izleyicisi, kamera widget testleri ve paket
bağımlılıkları çıkarıldı. KDE Plasma'nın kendi **Kamera Göstergesi** korunur.
Slayer ek bir kamera simgesi kurmaz. Uygulamadaki **Aygıtlar → Dahili kamera**
ayarının mevcut, yetkili ctl üzerinden RGB/IR erişimi denetimi devam eder.
KDE'nin kendi applet dosyaları değiştirilmez.

## Azami dolum alanı

**Güç ve Pil → Şarj sınırı → Azami dolum miktarı (%)** desteklenmeyen cihazlarda
da görünür. Destek yoksa sayı yerine `—` ve açıklama gösterilir. Gerçek eşik
sunulan cihazda değeri düzenlemek donanıma yazmaz; yalnız **Uygula ve kaydet**
eylemi yazar. Okunamayan veya geçersiz eşik uygulanamaz.

KDE Sistem Ayarları Slayer modülünde de desteklenmeyen eşikler `—` gösterir.
Yalnız üst eşiği olan cihazlarda mevcut olmayan alt eşik gönderilmez.

Bu R9T/BIOS N.1.46GAR16'da `charge_control_end_threshold` yoktur. Mevcut
`charge_types` üç firmware koruma profili sunar. Bunlar belirli bir yüzdeyi
garanti etmez. Sayısal yüzde eşiği etkinleştirilmedi; yapay bir eşik dosyası
oluşturulmadı. Salt okuma bulguları [CHARGING-R9T.md](CHARGING-R9T.md) içinde.
%80 sınırı uygulanmış değildir.

## Paket denetimi

Paket doğrulayıcısı, kaldırılan applet ve QML eklenti dizinlerinin yeni pakette
bulunmadığını da kontrol eder. Pil eşiğinin görünürlüğü, bozuk/eksik değer ve
açık Uygula davranışı Qt Quick Test ile doğrulanır. 0.1.5'te yapılan eski kamera
widget deneyinin kanıtları [arşivde](native-kde-2026-11-14/) tutulmuştur;
bu widget güncel paketin parçası değildir.

Kaynaklar:

- [KDE 6.8.2 kamera göstergesi](https://github.com/KDE/plasma-workspace/blob/v6.8.2/applets/cameraindicator/qml/main.qml)
- [KDE 6.8.3 pil eşiği yardımcısı](https://invent.kde.org/plasma/powerdevil/-/blob/v6.8.3/daemon/chargethresholdhelper_linux.cpp)
- [Linux Uniwill pil arayüzleri](https://docs.kernel.org/admin-guide/laptops/uniwill-laptop.html#battery-charging-control)
