#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu
project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
for name in slayerctl_r9t.c model_contract.h firmware_contract.h profile_bits.h mux_transaction.h thermal_profile.h; do
    cp "$project_root/kernel/$name" "$work/$name"
done
printf 'obj-m += slayerctl_r9t.o\n' > "$work/Kbuild"
tar -czf "$project_root/packaging/rpm/SOURCES/slayerctl_r9t.tar.gz" -C "$work" .
exec rpmbuild -ba --define "_topdir $project_root/packaging/rpm" "$project_root/packaging/rpm/SPECS/slayerctl-r9t-kmod.spec"
