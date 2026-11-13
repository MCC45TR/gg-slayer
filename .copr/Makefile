root := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
outdir ?= /tmp/srpms
spec ?= packaging/rpm/SPECS/slayerctl.spec
.PHONY: srpm
srpm:
	$(root)/packaging/copr-srpm.sh "$(spec)" "$(outdir)"
