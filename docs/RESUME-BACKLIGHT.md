# Uykudan dönüşte klavye parlaklığı

15 Kasım 2026 · MCC45TR <mcc45tr@gmail.com>

Slayer R9T üzerinde, KDE'ye bırakılan klavye ışığı uyanıştan sonra yeniden
uygulanmıyor; bir KDE ayarı değiştirildiğinde geri geliyordu. Kurulu uygulama
0.1.6, PowerDevil 6.8.3-2.slayer2, kernel 7.4.0-0.rc2.261111g067c5c24cef2.21.fc46.

| Olasılık | Gözlem | Sonuç |
| --- | --- | --- |
| RGB seçimi siliniyor | Linux LED renk önbelleği KDE vurgu rengiyle eşleşiyor | Renk önbelleği kayıp değil |
| ctl geçici kısma ölçeği takılıyor | Ölçek 1000, ctl idle özelliği kapalı | Bu örnekte neden değil |
| Uyanma servisi çalışmıyor | Son uyku sonrası apply-config başarıyla çalışmış | Servis tetiklenmiş; KDE sahipliğini koruyup RGB/parlaklık yazımını atlamış |
| KDE'nin idle geri yükleme yolu çağrılmıyor | Core askıdan dönüşte yalnız pil/adaptör bilgisini yeniliyor | Native geri yükleme çağrısı eksik; fixture testi önce başarısız, yamadan sonra başarılı |

PowerDevil 6.8.3-3.slayer3 yaması, kullanıcı kaynaklı uyanışta bekleyen
DimDisplay/DPMS eylemlerinin mevcut geri yükleme yolunu bir Qt olayı sonrasına
kuyruklar. Diğer uyanış işleyicileri önce kendi uyku durumlarını temizler.
Güç profilini yeniden yüklemez, KDE ayar dosyalarına yazmaz ve herhangi bir
parlaklık yüzdesini sabitlemez. Eylemler kendi hatırladıkları değerleri kullanır;
ctl klavye devri ve sıfır parlaklık kontrolleri değiştirilmez.

Zamanlayıcı, ağ ve telefon kaynaklı arka plan uyanışlarında bu yol çalışmaz.
Normal idle olayı daha önce gelirse bekleyen eylemler zaten tüketilmiş
olduğundan ikinci bir donanım yazımı oluşmaz. Sürekli sorgu, ek arka plan
servisi veya tekrarlı zamanlayıcı yoktur.

`packaging/powerdevil-resume-check.py`, derlenen PowerDevil kaynak dosyasından
gerçek Core::onResumeFromSuspend ve Core::onResumingFromIdle gövdelerini alır.
Kalan dış bağımlılıklar fixture'dır. Gerçek Qt olay döngüsünde ertelenmiş
çağrı, pil/adaptör güncellemesinin korunması, arka plan uyanışları, sıfır değer,
yinelenen/yarışan olaylar ve QObject ömrü denetlenir. Test, PowerDevil RPM'nin
%check aşamasında çalışır. Bu, KDE eylemlerinin tamamının veya fiziksel klavye
donanımının uçtan uca sınanması değildir; kurulum sonrası gerçek uyku/uyanma
denemesi ayrıca gereklidir.

Kaynak: [KDE PowerDevil v6.8.3 Core](https://github.com/KDE/powerdevil/blob/v6.8.3/daemon/powerdevilcore.cpp),
[DimDisplay](https://github.com/KDE/powerdevil/blob/v6.8.3/daemon/actions/bundled/dimdisplay.cpp),
[DPMS](https://github.com/KDE/powerdevil/blob/v6.8.3/daemon/actions/bundled/dpms.cpp).
