# Fedora COPR distribution

Developer: **MCC45TR <mcc45tr@gmail.com>**

Source: https://github.com/MCC45TR/gg-slayer
Repository: https://copr.fedorainfracloud.org/coprs/mcc45tr/mcc-support/
Targets: **fedora-rawhide-x86_64**, **fedora-45-x86_64**.

```
sudo dnf copr enable mcc45tr/mcc-support
sudo dnf install slayerctl slayer-control-qt slayer-face-auth
# GNOME placeholder, when wanted:
sudo dnf install slayer-control-gtk
sudo dnf upgrade --refresh slayerctl slayer-control-qt slayer-face-auth
# KDE: independent keyboard idle policy, without screen/sleep inhibition:
sudo dnf upgrade --refresh powerdevil
```

For an existing installation that still uses the old COPR, disable that repository and enable MCC Support.

```sh
sudo dnf copr disable mcc45tr/gg-slayer
sudo dnf copr enable mcc45tr/mcc-support
sudo dnf --refresh upgrade 'slayer*' 'ite8291r3*' 'kmod-slayerctl*' 'kmod-ite8291r3*'
```


The core RPM includes the signed COPR repository configuration. Installing an
RPM directly also enables future DNF updates. Package GPG verification stays on.
The GTK application is a placeholder. The face tool does not alter PAM or enroll
a user during installation.

The COPR SCM packages use `.copr/Makefile` and `packaging/copr-srpm.sh` to generate
source RPMs on the server. Compilation and all fixture/render checks run in
COPR. No locally compiled application RPMs are uploaded.


Kernel modules are prebuilt for the latest kernel-devel in each target chroot.
Their RPM names and dependencies include the exact kernel ABI. DNF never runs
akmods. A new kernel ABI needs a new COPR module build; use `build-package` above
(or the GitHub manual build workflow), then DNF upgrade. Previously installed
kernel-specific packages remain available for their matching kernels.
Rebuilding a module for the same ABI upgrades its existing payload. The
kernel-specific name already keeps other ABIs separate; the module packages
therefore do not use `installonlypkg(kernel-module)`, which would attempt to
co-install conflicting payload versions for one ABI.

Secure Boot is not disabled. At install, the upstream Linux sign-file utility
(compiled on COPR) can sign the downloaded modules with an existing root-owned
akmods MOK key. The standard root-owned `kmodgenca` links and dedicated akmods
group's read access are supported; writable or unprotected key paths are rejected.
This machine already has that key enrolled. No key is created or
enrolled automatically. On another machine without an enrolled key, Secure Boot
module loading requires the administrator's normal signing/enrollment setup.

Source pushes rebuild all four SCM packages through the GitHub webhook. Its
trigger credential is scoped to this COPR project and is not a COPR account API
token. Personal calibration and machine configuration never enter Git archives.

The fourth source package is a small PowerDevil extension based on Fedora's
6.8.3 packaging. It adds a capability-discoverable, keyboard-only policy bit;
ctl holds and releases the D-Bus lease. On KDE, update PowerDevil explicitly
when adding this integration to an existing installation: DNF can leave an
already installed provider unchanged when only a weak dependency is new.
Headless and GNOME installations do not require PowerDevil or the Qt frontend.
A later upstream KDE update remains eligible; ctl reports a missing delegation
capability instead of silently treating screen/sleep inhibition as equivalent.

For the exact R9T firmware's charging limitations and the unimplemented numeric
80% target, see [the measured charging status](CHARGING-R9T.md).
