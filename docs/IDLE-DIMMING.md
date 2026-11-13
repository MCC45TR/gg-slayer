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
pozitifse beş saniyelik smoothstep geçişiyle donanımın en düşük pozitif
seviyesine iner. Klavye/fare/dokunmatik etkinliği beş saniyede seçili seviyeye
geri döndürür. Geçiş sırasında etkinlik gelirse bulunduğu seviyeden döner.

Sürücünün `controls/brightness_scale` özelliği yalnız geçici fiziksel ölçeği
değiştirir. LED sınıfı parlaklığı ve KDE/UPower değeri korunur; bu yol parlaklık
bildirimi veya OSD üretmez. RGB verisi ve donanım efektleri yeniden gönderilmez.
En fazla 49 USB parlaklık değişikliği bir geçiş için yeterlidir; yerleşik
durumda animasyon zamanlayıcısı tamamen durur.

Wayland `ext-idle-notify-v1`, GNOME Mutter IdleMonitor ve X11 XSync IDLETIME
olayları kullanılır. Ekran görüntüsü, tuş içeriği, evdev/grab, periyodik idle
sorgusu veya ekranın görsel içeriği izlenmez. Video oynarken de gerçek kullanıcı
etkinliği esas alınır (Wayland v2 input-idle destekliyorsa).

Sistem brokeri gönderen UID'sini D-Bus'tan, etkin kullanıcıyı logind seat0'dan
doğrular. İstemci keyfi aygıt yolu/USB raporu gönderemez. Oturum bağlantısı
kapanırsa, etkin kullanıcı değişirse, sistem uykuya girerse veya broker
sonlanırsa ölçek sıfırlanır. Güç tasarrufundan çıkış ve özelliği kapatma normal
beş saniyelik geri dönüşü kullanır. Kullanıcı oturum hizmeti donanım dosyalarına
doğrudan yazmaz; bütün geçişleri sistem brokeri yürütür.

Kalibrasyon araçları ctl oturum köprüsünden geçici bir kalibrasyon hakkı alır.
Bekleme takibi durdurulur; sistem brokeri seçili seviyeye dönüşü gerçekten
tamamlamadan ölçüm yardımcısı başlamaz. Pencere kapatılırsa, araç çökerse veya
hazırlık başarısız olursa hak otomatik bırakılır ve bekleme takibi yeniden
başlar. Kalibrasyon arayüzü DE etkinliğini kendi başına sorgulamaz.
