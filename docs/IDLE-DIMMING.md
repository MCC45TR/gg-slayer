# Beklemede klavye ışığı

Geliştirici: MCC45TR <mcc45tr@gmail.com>

`slayerctl` tek başına sistem brokerini, kullanıcı oturumu bağlantısını ve DE
köprülerini kurar. Qt/GTK arayüzlerinin açık olması veya kurulu olması gerekmez.

```
slayerctl idle on
slayerctl idle timeout 30
slayerctl idle status
slayerctl idle off
```

Süreler: 5, 10, 15, 30, 60, 120, 180, 300, 600, 900, 1200, 1800 saniye.
Varsayılan süre 30 saniye. Özellik açıkken yalnız masaüstünün `power-saver`
profilinde etkinlik izlenir. Seçili parlaklık sıfırsa ışık kapalı kalır;
pozitifse beş saniyelik smoothstep geçişiyle donanımda görünür olan **2/50 (%4)**
seviyesine iner. Bu cihazda 1/50 (%2) optik olarak kapalıdır; pozitif değerler
bu yüzden en az 2/50 uygulanır. Yalnız laptopun yerleşik klavyesindeki basış,
ışığı beş saniyede seçili seviyeye geri döndürür. Harici USB/Bluetooth klavye,
fare ve touchpad uyanmayı veya bekleme süresini etkilemez. Geçiş sırasında
yerleşik klavyeye basılırsa bulunduğu seviyeden döner.

Sürücünün `controls/brightness_scale` özelliği yalnız geçici fiziksel ölçeği
değiştirir. LED sınıfı parlaklığı ve KDE/UPower değeri korunur; bu yol parlaklık
bildirimi veya OSD üretmez. RGB verisi ve donanım efektleri yeniden gönderilmez.
En fazla 48 USB parlaklık değişikliği bir geçiş için yeterlidir; yerleşik
durumda animasyon zamanlayıcısı tamamen durur.

Tam modele ait sürücü, yalnız `i8042/serio0` üzerindeki `atkbd` aygıtını pasif
olarak gözler. USB, Bluetooth ve sanal aygıtlar adlarından bağımsız dışlanır.
Çekirdek dışına tuş kodu, karakter veya olay akışı çıkmaz; yalnız root'un
okuyabildiği son etkinlik zamanı vardır. Ekran görüntüsü veya evdev/grab yoktur.
Etkin yazım sırasında her tuş için kullanıcı alanı uyandırılmaz: ctl yalnız
bekleme süresinin sonundaki zamanlayıcıda son etkinlik zamanını okur. Beklemede
zamanlayıcı durur ve sonraki yerleşik tuş basışı tek bir sysfs bildirimi üretir.
Bildirim kurulurken yeniden okuma, aynı anda gelen tuşun kaybolmasını önler.
Bu yol Wayland, GNOME ve X11 için aynı ctl backendini kullanır; oturum köprüsü
artık görüntü sunucusu kitaplıklarına bağlı değildir.

Sistem brokeri gönderen UID'sini D-Bus'tan, etkin kullanıcıyı logind seat0'dan
doğrular. İstemci keyfi aygıt yolu/USB raporu veya masaüstü etkinlik bildirimi
gönderemez. Etkin kullanıcı kalmazsa, sistem uykuya girerse veya broker
sonlanırsa ölçek sıfırlanır. Sonlandırma temizliği etkinlik gözlemcisini de
kapatır. Seçili parlaklık sıfırken etkinlik gözlemi çalışmaz. Güç tasarrufundan
çıkış ve özelliği kapatma normal
beş saniyelik geri dönüşü kullanır. Kullanıcı oturum hizmeti donanım dosyalarına
doğrudan yazmaz; bütün geçişleri sistem brokeri yürütür.

Kalibrasyon araçları ctl oturum köprüsünden geçici bir kalibrasyon hakkı alır.
Bekleme takibi durdurulur; sistem brokeri seçili seviyeye dönüşü gerçekten
tamamlamadan ölçüm yardımcısı başlamaz. Pencere kapatılırsa, araç çökerse veya
hazırlık başarısız olursa hak otomatik bırakılır ve bekleme takibi yeniden
başlar. Kalibrasyon arayüzü DE etkinliğini kendi başına sorgulamaz.

Referans: [Linux input handler ve pasif gözlemci API'si](https://docs.kernel.org/driver-api/input.html).

KDE'de ctl oturum köprüsü PowerDevil'in `ChangeKeyboardBacklight` (8) hakkını
alır. Bu hak ekranı açık tutmaz, ekran kısılmasını veya uykuyu engellemez;
yalnız PowerDevil'in DimDisplay/DPMS işlemlerinin klavyeyi sıfırlamasını önler.
Elle seçilen sıfır dahil normal KDE parlaklık ayarı çalışmaya devam eder.
Hak bağlantı kopunca otomatik bırakılır; PowerDevil yeniden başlayınca ctl
tekrar alır. Özellik kapatıldığında veya güç tasarrufundan çıkıldığında bırakılır.
Gerekli PowerDevil uzantısı aynı COPR'dan sağlanır. Kurulu KDE bu arayüzü
sunmuyorsa ctl oturum durumu eksik entegrasyonu açıkça bildirir.
