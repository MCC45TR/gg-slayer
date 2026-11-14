# R9T şarj desteği: doğrulanmış durum

Geliştirici: MCC45TR <mcc45tr@gmail.com>

2026-11-14, SLAYER R9T V1, BIOS N.1.46GAR16, EC projesi 26.
Talep edilen hedef %80; bu hedef **uygulanmış değildir**.

Standart `charge_types` arayüzü üç gerçek firmware profili sunar:
Standard / Long_Life / Trickle. Bunlar high-capacity / balanced / stationary
profilleridir; sayısal yüzde eşiği olarak sunulmaz. Firmware bu profillerde
pilin gerçek şarj durumunu işletim sisteminden saklayabilir. Gerçek akım
`current_now` ile ayrıca değerlendirilmelidir.

Salt okuma tanısı:

- Yüzde kontrolünün destek/etkinlik biti (`0x0742[2]`) kapalı: `0x22`.
- Canlı üst ve alt eşikler sıfır: `0x07b9=0`, `0x07d0=0`.
- BIOS'un kendi WMI okuma posta kutusuyla kalıcı eşik de sıfır:
  `0x087f=0`. Okuma kanalı önce proje 26 ile doğrulandı.
- Standart `charge_control_end_threshold` mevcut değil.
- UPower firmware profil desteği bildiriyor; sayısal eşik desteği bildirmiyor.

KDE PowerDevil yerel yüzde ayarını standart başlangıç/bitiş eşiği sysfs
arayüzünden oluşturur. Bu cihazda bu arayüz bulunmadığı için %80 seçeneği
çıkarılamıyor. Arayüzde desteklenmeyen bir varsayılan %80 gösterilmez;
CLI da uygulanmadığını açıkça bildirir. Profil değiştirmek, istenen %80
sınırını karşılamış sayılmaz. Pil zaten %100 iken sınır koymak ayrıca
onu kendiliğinden %80'e boşaltmaz.

Bu bulgular yüzde kontrolünün mevcut firmware durumunda açık olmadığını
gösterir. Bu BIOS için güvenilir etkinleştirme/yazma protokolü henüz
doğrulanmadı. Ürün kimliği/ROMID veya platform bayrakları değiştirilmedi;
sayısal eşik dosyası taklit edilmedi. Linux'un Uniwill belgesi, desteklenmeyen
modellerde bu arayüzün zorlanmasının pile zarar verebileceğini belirtiyor.

Desteklenen profillerin adaptör/uyku geçişlerinde kaybolmasına karşı sürücü
son başarılı seçimi tutar ve yalnız adaptör durumu değişiminde/uykudan dönüşte
bir iş planlar. Mod zaten doğruysa yazma ve bildirim yapılmaz. Gerçek geri
okuma korunur; periyodik EC yoklaması eklenmez.

Kaynaklar:

- [Linux Uniwill pil denetimi](https://docs.kernel.org/admin-guide/laptops/uniwill-laptop.html#battery-charging-control)
- [KDE PowerDevil Linux eşik yardımcısı](https://invent.kde.org/plasma/powerdevil/-/blob/v6.8.3/daemon/chargethresholdhelper_linux.cpp)
