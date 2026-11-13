#!/bin/bash
# SPDX-License-Identifier: MIT
# Compatibility entry point: builds now happen exclusively on COPR.
set -euo pipefail
if test "$#" -ne 0; then echo 'Use copr-cli build-package for explicit source/chroot selection.' >&2; exit 2; fi
for package in slayerctl slayerctl-r9t-kmod ite8291r3-kmod; do
    copr-cli build-package mcc45tr/gg-slayer --name "$package" --nowait
done
