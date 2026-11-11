# Canonical Fedora recipe

The maintained COPR recipe is `../uke-plasma-keyboard-custom.spec`, invoked by
`.copr/Makefile` and `packaging/copr/prepare-source.sh`. The legacy container
recipe in this directory predates the native KWin integration and is not a
qualified delivery path. Do not use it for Uke tablet builds. Native compilation
and tests for this work run only in COPR; device delivery uses signed RPMs/DNF.
