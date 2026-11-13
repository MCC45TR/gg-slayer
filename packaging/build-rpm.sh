#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu
task_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
version=$(cat "$task_root/VERSION")
case "$version" in *[!0-9.]*|'') echo 'Invalid VERSION' >&2; exit 2;; esac
top="$task_root/packaging/rpm"
spec_version=$(rpmspec -q --srpm --qf '%{VERSION}' "$top/SPECS/slayerctl.spec")
[ "$version" = "$spec_version" ] || { echo 'VERSION/spec mismatch' >&2; exit 2; }
mkdir -p "$top/SOURCES" "$top/SPECS" "$top/RPMS" "$top/SRPMS"
# Exclude OEM downloads, build caches and private data using an explicit list.
tar -cJf "$top/SOURCES/slayerctl-$version.tar.xz" \
  -C "$task_root" --transform="s,^,slayerctl-$version/," \
  --exclude=__pycache__ --exclude='*.o' --exclude='*.ko' --exclude='*.mod' --exclude='*.mod.c' \
  --exclude='*.cmd' --exclude=Module.symvers --exclude=modules.order \
  VERSION LICENSE README.md CMakeLists.txt Makefile src service gui kcm kernel tests gnome tools \
  packaging/slayer-face packaging/slayerctl.preset packaging/check-payload.py \
  packaging/build-rpm.sh packaging/sign-rpm.py packaging/rpm/SPECS/slayerctl.spec \
  packaging/org.gamegaraj.Slayer1.conf packaging/org.gamegaraj.Slayer1.service \
  packaging/slayerctl-control.service packaging/49-slayerctl-admin.rules \
  packaging/85-slayerctl-power.rules packaging/org.gamegaraj.slayerctl.policy \
  packaging/slayerctl.conf packaging/rgb-profiles packaging/slayerctl.desktop \
  packaging/slayerctl.service packaging/slayerctl-restore.service \
  packaging/slayerctl-resume.service packaging/slayerctl-power-event.service \
  docs/RPM-KURULUM.txt docs/KIRIGAMI-IMPLEMENTATION.md
exec rpmbuild -ba --define "_topdir $top" "$top/SPECS/slayerctl.spec"
