# SPDX-License-Identifier: MIT
.PHONY: srpm
srpm:
	dnf5 -y install git-core curl xz rpm-build
	bash packaging/copr/prepare-source.sh "$(outdir)"
