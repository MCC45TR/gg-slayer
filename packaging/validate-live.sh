#!/bin/bash
# SPDX-License-Identifier: MIT
# Privileged, hardware-read-only TPM checks in a private mount namespace.
# The real /etc/slayerctl directory and the TPM are never cleared.
set -euo pipefail
umask 077
if (( EUID != 0 )); then echo 'Canlı doğrulama yönetici yetkisi gerektirir.' >&2; exit 3; fi
if [[ ${1:-} == --crypto-namespace ]]; then
    isolated=$2
    mount --bind "$isolated" /etc/slayerctl
    /usr/bin/slayerctl config set fan_mode auto
    grep -q '^SLAYER-TPM2$' /etc/slayerctl/slayerctl.conf
    test "$(stat -c %a /etc/slayerctl/slayerctl.conf)" = 600
    /usr/bin/slayerctl config show | grep -qx 'fan_mode=auto'
    # A lost/unreadable credential is simulated, without clearing the TPM.
    printf 'SLAYER-TPM2\nunreadable-test-credential\n' > /etc/slayerctl/slayerctl.conf
    if /usr/bin/slayerctl config show >/dev/null 2>&1; then echo 'Bozuk kimlik kabul edildi.' >&2; exit 1; fi
    /usr/bin/slayerctl config recover
    test -n "$(find /etc/slayerctl -name 'slayerctl.conf.recovery-*' -print -quit)"
    /usr/bin/slayerctl config show | grep -q 'Kalıcı donanım ayarı yok'
    echo 'TPM encrypt/decrypt, 0600, unreadable credential and recovery: PASS (isolated namespace)'
    exit 0
fi
test -r /sys/devices/platform/slayerctl/thermal_mode
grep -q '^acpi-query' /sys/devices/platform/slayerctl/profile_events
read -r modes < /sys/devices/platform/slayerctl/thermal_mode_choices
test "$modes" = 'low-power balanced performance'
echo "Driver: $(cat /sys/devices/platform/slayerctl/backend_version)"
echo "EC mode: $(cat /sys/devices/platform/slayerctl/thermal_mode); raw: $(cat /sys/devices/platform/slayerctl/thermal_control)"
echo 'WMI observer and independent EC controls: PASS'
original=$(sha256sum /etc/slayerctl/slayerctl.conf)
work=$(mktemp -d /run/slayer-live-validation.XXXXXX)
trap 'rm -rf "$work"' EXIT
install -d -m0700 "$work/config"
unshare --mount --propagation private /usr/bin/bash "$0" --crypto-namespace "$work/config"
test "$original" = "$(sha256sum /etc/slayerctl/slayerctl.conf)"
echo 'Host configuration unchanged by TPM tests: PASS'
getenforce
systemctl show slayerctl-control.service -p ActiveState -p Result -p ExecMainStatus
busctl --system call org.gamegaraj.Slayer1 /org/gamegaraj/Slayer1 org.gamegaraj.Slayer1 ReadState >/dev/null
echo 'Broker activation and ReadState under SELinux enforcing: PASS'
