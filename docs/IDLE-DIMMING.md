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
pozitifse beş saniyelik yumuşak geçişle donanımda görünür olan **2/50 (%4)**
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

Oturum köprüsü güç tasarrufu dışında klavye devrini ve sistem brokerindeki
aboneliğini bırakır; kalibrasyon yoksa yaklaşık 2,5 saniyede kapanır. Güç
tasarrufuna geçildiğinde gerekli köprüyü sistem brokeri yeniden etkinleştirir.
Bekleme ışığı özellikle açılmışsa sistem brokeri profil olaylarını almak için
bekler; sürekli sensör taraması yapmaz. Bekleme ışığı/profil eşitlemesi kapalı,
kalibrasyon ve aktif arayüz istemcisi yoksa broker beş saniyede kapanır.
Salt okunur durum sorgusu kapalı kullanıcı köprüsünü başlatmaz.

## Uykudan dönüşte KDE parlaklığı

PowerDevil `6.8.3-3.slayer3`, kullanıcı kaynaklı uyanışta beklemeden çıkış için
bekleyen yerel eylemleri yeniden çalıştırır. Böylece DimDisplay/DPMS'nin
hatırladığı klavye parlaklığı, güç profili/adaptör durumu değişmese ve henüz
bir giriş olayı gelmese de geri yüklenir. İşlem, diğer uyanış işleyicileri
tamamlandıktan sonra tek bir Qt olayı olarak çalışır; sürekli sorgu eklemez.

Zamanlayıcı, ağ ve telefon kaynaklı arka plan uyanışlarında ışık açılmaz.
Sıfır parlaklık ve ctl klavye devriyle ilgili mevcut eylem denetimleri korunur;
kullanıcının KDE ayarlarını değiştirmesi veya profili yeniden yüklemesi gerekmez.
Test, gerçek PowerDevil Core uyanış/idle yöntemlerini sınırlı Qt fixture'ında
çalıştırır; fiziksel uyku/uyanma kabulünün yerini almaz.

0.1.4'te aktif/bekleme geçişi tek bir hareket modeli kullanır. Aktif hedef
KDE'nin seçtiği mantıksal parlaklık; bekleme hedefi görünür %4 tabandır.
Yarıda gelen yerleşik tuş basışı, son USB kademesinden bağımsız olarak o anki
kesintisiz konumu ve hızı örnekler. Mevcut hareket en fazla 120 ms içinde
kademeli frenlenir; kalan sürede yeni hedefe yumuşakça ulaşır. Toplam geçiş,
yön değişiminden itibaren beş saniyedir. Konum ve hız süreklidir; iki uçta
hız sıfırlanır. Arka arkaya aynı hedefi bildirmek geçişi yeniden başlatmaz.

Bekleme süresi değiştirildiğinde son fiziksel etkinlik zamanı korunur.
Kalibrasyon hakkının bırakılması/iptali aktif seviyeye ani sıçrama yapmaz;
normal geri dönüş eğrisini sürdürür. Açıkça sıfır seçmek, uyku ve hizmetin
sonlandırılması donanım temizleme yollarıdır; sıfır seçiminde ışık kapalıdır.
