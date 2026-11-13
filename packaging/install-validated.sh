#!/bin/bash
# SPDX-License-Identifier: MIT
# Compatibility installer. All payloads now come from the signed COPR repo.
set -euo pipefail
if (( EUID != 0 )); then echo 'Yönetici olarak çalıştırın veya doğrudan dnf kullanın.' >&2; exit 3; fi
/usr/bin/dnf5 -y copr enable mcc45tr/gg-slayer
exec /usr/bin/dnf5 install slayerctl slayer-control-qt slayer-face-auth slayer-control-gtk slayerctl-r9t-kmod ite8291r3-kmod
