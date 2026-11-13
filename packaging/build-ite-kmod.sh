#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu
project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cp "$project_root/kernel/ite8291/ite_8291.c" "$project_root/kernel/ite8291/Kbuild" "$project_root/kernel/ite8291/color_contract.h" "$project_root/kernel/ite8291/color_model.h" "$work/"
tar -czf "$project_root/packaging/rpm/SOURCES/ite_8291.tar.gz" -C "$work" .
exec rpmbuild -ba --define "_topdir $project_root/packaging/rpm" "$project_root/packaging/rpm/SPECS/ite8291r3-kmod.spec"
