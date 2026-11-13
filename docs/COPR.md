# Fedora COPR distribution

Developer: **MCC45TR <mcc45tr@gmail.com>**

Source: https://github.com/MCC45TR/gg-slayer
Repository: https://copr.fedorainfracloud.org/coprs/mcc45tr/gg-slayer/
Targets: **fedora-rawhide-x86_64**, **fedora-45-x86_64**.

```
sudo dnf copr enable mcc45tr/gg-slayer
sudo dnf install slayerctl slayer-control-qt slayer-face-auth
# GNOME placeholder, when wanted:
sudo dnf install slayer-control-gtk
sudo dnf upgrade --refresh slayerctl slayer-control-qt slayer-face-auth
```

The core RPM includes the signed COPR repository configuration. Installing an
RPM directly also enables future DNF updates. Package GPG verification stays on.
The GTK application is a placeholder. The face tool does not alter PAM or enroll
a user during installation.

The COPR SCM packages use `.copr/Makefile` and `packaging/copr-srpm.sh` to generate
source RPMs on the server. Compilation and all fixture/render checks run in
COPR. No locally compiled application RPMs are uploaded.
