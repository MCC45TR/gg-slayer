#!/bin/bash
# SPDX-License-Identifier: MIT
# One administrator authentication installs the already-built local packages.
# Does not change the selected EC mode, enable synchronization or reset a TPM.
set -euo pipefail
umask 077
if (( EUID != 0 )); then echo 'Bu kurulum yönetici doğrulaması gerektirir.' >&2; exit 3; fi
project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
version=$(cat "$project_root/VERSION")
kernel_release=$(uname -r)
output="$project_root/packaging/rpm/RPMS"
packages=("$output/x86_64/slayerctl-$version-1.local"*.rpm "$output/x86_64/slayerctl-gui-$version-1.local"*.rpm "$output/x86_64/slayerctl-kde-$version-1.local"*.rpm "$output/x86_64/akmod-slayerctl-r9t-$version-1.local"*.rpm "$output/noarch/slayerctl-r9t-kmod-common-$version-1.local"*.rpm)
ite_version=0.0.20261113.4
packages+=("$output/x86_64/akmod-ite8291r3-$ite_version-1.local"*.rpm "$output/noarch/ite8291r3-kmod-common-$ite_version-1.local"*.rpm)
for optional in face gnome; do
    if rpm -q "slayerctl-$optional" >/dev/null 2>&1; then packages+=("$output/noarch/slayerctl-$optional-$version-1.local"*.rpm); fi
done
for package in "${packages[@]}"; do test -f "$package" || { echo "Paket bulunamadı: $package" >&2; exit 1; }; done
# Sign with the same dedicated root-owned RPM key used by the installed release.
/usr/bin/python3 "$project_root/packaging/sign-rpm.py"
# Refuse a dependency problem before changing the running installation.
rpm -U --replacepkgs --test "${packages[@]}"
backup="/var/lib/slayerctl/backups/$(date -u +%Y%m%dT%H%M%SZ)"
install -d -m 0700 "$backup"
old_module=$(modinfo -n slayerctl_r9t)
cp -a "$old_module" "$backup/"
old_ite_module=$(modinfo -n ite_8291)
cp -a "$old_ite_module" "$backup/"
/usr/bin/python3 - "$backup/lighting.json" <<'PYBACKUP'
import json, pathlib, sys
leds = sorted(pathlib.Path('/sys/class/leds').glob('rgb:kbd_backlight*'))
state = {p.name:(p.joinpath('multi_intensity').read_text(),p.joinpath('brightness').read_text()) for p in leds}
if not state:
    previous = sorted(pathlib.Path('/var/lib/slayerctl/backups').glob('*/lighting.json'))
    state = json.loads(previous[-1].read_text()) if previous else {}
assert len(state) == 126, 'No restorable lighting snapshot'
pathlib.Path(sys.argv[1]).write_text(json.dumps(state))
PYBACKUP
printf '%s\n' "$old_module" > "$backup/module-path"
if test -d /etc/slayerctl; then tar -C /etc -czf - slayerctl | systemd-creds encrypt --with-key=tpm2 --tpm2-pcrs=7 --name=slayerctl-backup - "$backup/config.cred"; fi
if test -r /sys/bus/hid/devices/*048D:600B.*/controls/hardware_effect; then cat /sys/bus/hid/devices/*048D:600B.*/controls/hardware_effect > "$backup/lighting-effect"; fi
if test -r /sys/bus/hid/devices/*048D:600B.*/controls/white_point; then cat /sys/bus/hid/devices/*048D:600B.*/controls/white_point > "$backup/white-point"; fi
if test -r /sys/bus/hid/devices/*048D:600B.*/controls/color_profile; then
    cat /sys/bus/hid/devices/*048D:600B.*/controls/color_profile | systemd-creds encrypt --with-key=tpm2 --tpm2-pcrs=7 --name=slayer-color-profile - "$backup/color-profile.cred"
fi
/usr/bin/slayerctl power status > "$backup/desktop-before.txt"
if test -r /sys/devices/platform/slayerctl/thermal_mode; then cat /sys/devices/platform/slayerctl/thermal_mode > "$backup/ec-before.txt"; else cat /sys/firmware/acpi/platform_profile > "$backup/ec-before.txt" 2>/dev/null || true; fi
umask 022
rpm -Uvh --replacepkgs "${packages[@]}"
# akmods uses the existing enrolled signing key, never creates/imports a new MOK.
/usr/bin/akmods --force --rebuild --kernels "$kernel_release" --akmod slayerctl-r9t
/usr/bin/akmods --force --rebuild --kernels "$kernel_release" --akmod ite8291r3
depmod -a "$kernel_release"
new_module=$(modinfo -n slayerctl_r9t)
if test "$(modinfo -F version "$new_module")" != "$version"; then echo 'Yeni kernel modülü bulunamadı.' >&2; exit 1; fi
if mokutil --sb-state | grep -q 'SecureBoot enabled'; then
    test -n "$(modinfo -F signer "$new_module")" || { echo 'Kernel modülü imzalanmamış.' >&2; exit 1; }
fi
modprobe -r slayerctl_r9t
if ! modprobe slayerctl_r9t; then
    cp -a "$backup/$(basename "$old_module")" "$old_module"
    depmod -a "$kernel_release"
    modprobe slayerctl_r9t
    echo "Yeni modül yüklenemedi; önceki modül geri yüklendi. Yedek: $backup" >&2
    exit 1
fi
test "$(modinfo -F version ite_8291)" = "$ite_version"
if mokutil --sb-state | grep -q 'SecureBoot enabled'; then test -n "$(modinfo -F signer ite_8291)"; fi
modprobe -r ite_8291
if ! modprobe ite_8291; then
    cp -a "$backup/$(basename "$old_ite_module")" "$old_ite_module"
    depmod -a "$kernel_release"
    modprobe ite_8291
    echo "RGB modülü yüklenemedi; önceki modül geri yüklendi. Yedek: $backup" >&2
    exit 1
fi
# A pre-fix module may have left a stale sysfs group. Recreate only the
# verified lighting USB interface; the keyboard's input interface stays bound.
/usr/bin/python3 - <<'PYREBIND'
from pathlib import Path
import time
for hdev in Path('/sys/bus/hid/devices').glob('*048D:600B.*'):
    interface = hdev.resolve().parent
    if not interface.joinpath('bInterfaceNumber').exists() or interface.joinpath('bInterfaceNumber').read_text().strip() != '01': continue
    usb = interface.parent
    assert usb.joinpath('idVendor').read_text().strip() == '048d'
    assert usb.joinpath('idProduct').read_text().strip() == '600b'
    assert usb.joinpath('bcdDevice').read_text().strip() == '0003'
    if not hdev.joinpath('driver').exists():
        bus = Path('/sys/bus/usb/drivers/usbhid')
        bus.joinpath('unbind').write_text(interface.name)
        try: pass
        finally: bus.joinpath('bind').write_text(interface.name)
        time.sleep(.3)
        print('Rebound verified RGB interface:', interface.name)
        break
assert Path('/sys/class/leds/rgb:kbd_backlight/multi_intensity').exists(), 'RGB device did not bind'
PYREBIND
/usr/bin/python3 - "$backup/lighting.json" "$backup/white-point" "$backup/color-profile.cred" "$backup/lighting-effect" <<'PYRESTORE'
import json, pathlib, sys, subprocess
state = json.loads(pathlib.Path(sys.argv[1]).read_text())
controls = next(pathlib.Path('/sys/bus/hid/devices').glob('*048D:600B.*/controls'))
controls.joinpath('buffer_input').write_text('1\n')
try:
    point=pathlib.Path(sys.argv[2])
    if point.exists() and controls.joinpath('white_point').exists(): controls.joinpath('white_point').write_text(point.read_text())
    model=pathlib.Path(sys.argv[3])
    if model.exists() and controls.joinpath('color_profile').exists():
        payload=subprocess.check_output(['systemd-creds','decrypt','--name=slayer-color-profile',str(model),'-'],text=True)
        controls.joinpath('color_profile').write_text(payload)
    for name,(color,brightness) in state.items():
        pathlib.Path('/sys/class/leds',name,'multi_intensity').write_text(color)
    pathlib.Path('/sys/class/leds/rgb:kbd_backlight/brightness').write_text(state['rgb:kbd_backlight'][1])
finally:
    controls.joinpath('buffer_input').write_text('0\n')
effect=pathlib.Path(sys.argv[4])
if effect.exists() and effect.read_text().split()[0]!='51': controls.joinpath('hardware_effect').write_text(effect.read_text())
PYRESTORE
# UPower must attach its hardware-brightness notification to the new LED node.
systemctl try-restart upower.service
# TuneD must drop its watch on the old platform_profile provider. Its existing
# desktop policy is restored; the new independent EC control is unaffected.
if systemctl is-active --quiet tuned-ppd.service; then
    desktop=$(sed -n 's/^profile: //p' "$backup/desktop-before.txt" | head -1)
    systemctl restart tuned-ppd.service
    case "$desktop" in power-saver|balanced|performance) /usr/bin/slayerctl power set "$desktop";; esac
fi
restorecon -RF /usr/bin/slayerctl /usr/bin/slayer-control /usr/libexec/slayerctl-helper /usr/libexec/slayerctl-service /usr/libexec/slayerctl-desktop /etc/slayerctl
systemctl daemon-reload
systemctl try-restart slayerctl-control.service
busctl --system call org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus ReloadConfig
# Activates the broker without applying saved hardware settings.
busctl --system call org.gamegaraj.Slayer1 /org/gamegaraj/Slayer1 org.gamegaraj.Slayer1 ReadState > /dev/null
echo "Kurulum tamamlandı. Yedek: $backup"
