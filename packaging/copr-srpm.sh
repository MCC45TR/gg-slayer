#!/bin/bash
# SPDX-License-Identifier: MIT
# Source packaging only. Compilation and checks run exclusively in COPR.
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
spec_name=$(basename "$1")
output=$(realpath -m "$2")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work"/{SOURCES,SPECS,SRPMS} "$output"
if ! command -v rpmbuild >/dev/null; then dnf -y install rpm-build; fi
if ! command -v git >/dev/null; then dnf -y install git-core; fi
if test "$spec_name" != slayerctl.spec && ! command -v kmodtool >/dev/null; then dnf -y install kmodtool; fi
cp "$root/packaging/rpm/SPECS/$spec_name" "$work/SPECS/"
case "$spec_name" in
 slayerctl.spec)
  version=$(cat "$root/VERSION")
  git -C "$root" archive --format=tar --prefix="slayerctl-$version/" HEAD | xz -c > "$work/SOURCES/slayerctl-$version.tar.xz"
  ;;
 slayerctl-r9t-kmod.spec)
  stage="$work/r9t";mkdir -p "$stage"
  for file in slayerctl_r9t.c model_contract.h firmware_contract.h profile_bits.h mux_transaction.h thermal_profile.h; do cp "$root/kernel/$file" "$stage/"; done
  cp "$root/kernel/COPYING" "$stage/COPYING"
  printf 'obj-m += slayerctl_r9t.o\n' > "$stage/Kbuild"
  tar -czf "$work/SOURCES/slayerctl_r9t.tar.gz" -C "$stage" .
  ;;
 ite8291r3-kmod.spec)
  tar -czf "$work/SOURCES/ite_8291.tar.gz" -C "$root/kernel/ite8291" Kbuild ite_8291.c color_contract.h color_model.h brightness_contract.h COPYING
  ;;
 *) echo 'Unsupported COPR source target' >&2;exit 2;;
esac
rpmbuild -bs --define "_topdir $work" --define "_srcrpmdir $output" "$work/SPECS/$spec_name"
