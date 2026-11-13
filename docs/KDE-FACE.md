# KDE yüz tanıma bağlantısı

Geliştirici: MCC45TR <mcc45tr@gmail.com> · İnceleme: 14 Kasım 2026

Bu cihazda etkin görüntü yöneticisi **Plasma Login Manager** (`plasmalogin.service`).
SDDM kurulu değil. Oturum açma yolu `/usr/lib/pam.d/plasmalogin`; ekran kilidi
KScreenLocker tarafından kullanılan `kde` PAM hizmeti. Dağıtımın `/etc/pam.d`
geçersiz kılmaları ayrıca değerlendirilir.

| İşlem | Kimlik doğrulama yolu | Sonuç |
| --- | --- | --- |
| İlk giriş | Plasma Login Manager / `plasmalogin` | PAM üzerinden yüz motoru bağlanabilir. |
| Ekran kilidi | KScreenLocker / normalde `kde` | Girişten ayrı test gerekir. |
| Yönetici işlemi | Polkit ajanı / dağıtımın PAM politikası | Giriş ayarı kendiliğinden burayı kapsamaz. |
| KWallet | `pam_kwallet` + kullanıcının parolası | Yüz eşleşmesi şifreli kasanın anahtarını üretmez. |

Howdy bir PAM yüz doğrulama motorudur. `slayer-face-auth` cihaz kimliği
doğrulanmış IR kamerayla açık kayıt/test komutları sunar. Howdy henüz bu
cihazda kurulu değil. Paket onu önerir; kullanıcı kaydı, kamera açılması veya
PAM değişikliği RPM kurulumu sırasında yapılmaz. Mevcut parola yolu korunarak
yapılacak entegrasyon ve giriş/kilit/Polkit denemeleri sonraki güvenlik
incelemesinin ayrı kapsamıdır.

IR kamera tek başına canlılık kanıtı değildir. Howdy'nin kendi güvenlik
açıklaması da yüz eşleştirmesinin sınırlarını belirtir. Kullanıcının planladığı
Dayblue incelemesinde sahte görüntü, yanlış kullanıcı, karanlık, kamera
bağlantısının kesilmesi ve parola ile kurtarma değerlendirilmelidir.

Birincil kaynaklar:

- [Plasma Login Manager PAM backend](https://github.com/KDE/plasma-login-manager/blob/master/src/helper/backend/PamBackend.cpp)
- [Fedora giriş PAM tanımı](https://github.com/KDE/plasma-login-manager/blob/master/data/pam/fedora/plasmalogin)
- [KScreenLocker PAM belgesi](https://github.com/KDE/kscreenlocker/blob/master/README.pam)
- [KWallet PAM uygulaması](https://github.com/KDE/kwallet-pam/blob/master/pam_kwallet.c)
- [Howdy projesi ve güvenlik açıklaması](https://github.com/boltgolt/howdy)
