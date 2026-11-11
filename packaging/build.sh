#!/usr/bin/env bash
# Build the uke-plasma-keyboard-custom Arch package from the current git tree.
#
# Creates the source tarball from HEAD (so only committed files are packaged),
# keeps pkgver in sync with the project version and runs makepkg.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
cd "$here"

version="$(sed -n 's/^set(PROJECT_VERSION "\(.*\)")$/\1/p' "$root/CMakeLists.txt" | head -n1)"
if [ -z "$version" ]; then
    echo "Could not determine project version" >&2
    exit 1
fi

sed -i "s/^pkgver=.*/pkgver=$version/" PKGBUILD
sed -i "s/^pkgrel=.*/pkgrel=${pkgrel:-1}/" PKGBUILD

rm -f uke-plasma-keyboard-custom-*.tar.gz
git -C "$root" archive --format=tar.gz \
    --prefix="uke-plasma-keyboard-custom-$version/" HEAD \
    > "uke-plasma-keyboard-custom-$version.tar.gz"

# --nosign: never sign, whatever the machine's makepkg.conf says (BUILDENV
# may enable it); CI has no secret key and makepkg would abort.
makepkg -f --nodeps --noconfirm --nosign
