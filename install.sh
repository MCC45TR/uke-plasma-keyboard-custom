#!/bin/sh
#
# SPDX-FileCopyrightText: 2026 Plasma Keyboard contributors
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Install or update uke-plasma-keyboard-custom from a GitHub release, without
# setting up a package repository first:
#
#   curl -fsSL https://raw.githubusercontent.com/mops1k/uke-plasma-keyboard-custom/master/install.sh | sh
#
# Options are passed after "sh -s --", for example:
#
#   curl -fsSL ... | sh -s -- --tag v6.7.90-custom-r9
#
#   --tag <tag>    install that release instead of the latest one
#   --overwrite    let pacman replace files it does not track (needed after a
#                  manual "cmake --install", where /usr holds unowned files)
#   --no-restart   do not restart a running keyboard afterwards
#   --dry-run      download and verify the package, install nothing
#   -h, --help     this text
#
# The package is the same one attached to the release, so it can be installed
# next to the official plasma-keyboard and upgraded later from the pacman
# repository (see the README) instead of running this script again.
#
# On SteamOS the system is mounted read-only: the script runs
# "sudo steamos-readonly disable" for the installation and enables it again
# afterwards, including when the installation fails.

set -eu

repo=mops1k/uke-plasma-keyboard-custom
binary=/usr/bin/uke-plasma-keyboard-custom

tag=
overwrite=0
restart=1
dry_run=0

while [ $# -gt 0 ]; do
    case "$1" in
    --tag)
        if [ $# -lt 2 ]; then
            echo "error: --tag needs a release tag" >&2
            exit 2
        fi
        tag=$2
        shift 2
        ;;
    --overwrite)
        overwrite=1
        shift
        ;;
    --no-restart)
        restart=0
        shift
        ;;
    --dry-run)
        dry_run=1
        shift
        ;;
    -h | --help)
        cat <<'EOF'
Install or update uke-plasma-keyboard-custom from a GitHub release.

Usage:
  curl -fsSL https://raw.githubusercontent.com/mops1k/uke-plasma-keyboard-custom/master/install.sh | sh
  curl -fsSL ... | sh -s -- [options]

Options:
  --tag <tag>    install that release instead of the latest one
  --overwrite    let pacman replace files it does not track (after a manual
                 "cmake --install", where /usr holds unowned files)
  --no-restart   do not restart a running keyboard afterwards
  --dry-run      download and verify the package, install nothing
  -h, --help     this text

The keyboard is installed with pacman, so it can be removed with
"sudo pacman -R uke-plasma-keyboard-custom" and upgraded from the pacman
repository described in the README.

On SteamOS the read-only filesystem is disabled for the installation and
enabled again afterwards (even if pacman fails).
EOF
        exit 0
        ;;
    *)
        echo "error: unknown option '$1' (try --help)" >&2
        exit 2
        ;;
    esac
done

if ! command -v curl >/dev/null 2>&1; then
    echo "error: curl is required" >&2
    exit 1
fi
if ! command -v pacman >/dev/null 2>&1; then
    echo "error: this package is built for Arch-based systems (pacman not found)" >&2
    exit 1
fi
arch=$(uname -m)
if [ "$arch" != "x86_64" ]; then
    echo "error: only x86_64 packages are published (this machine is $arch)" >&2
    exit 1
fi

if [ -z "$tag" ]; then
    echo "Looking up the latest release..."
    tag=$(curl -fsSL "https://api.github.com/repos/$repo/releases/latest" | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p' | head -n1)
    if [ -z "$tag" ]; then
        echo "error: could not determine the latest release of $repo" >&2
        exit 1
    fi
fi
echo "Release: $tag"

base="https://github.com/$repo/releases/download/$tag"
tmp=$(mktemp -d)

# SteamOS mounts the system read-only. Remember whether this script disabled it
# so the EXIT trap can always put it back, also when pacman fails.
readonly_disabled=0
reenable_readonly() {
    if [ "$readonly_disabled" = 1 ]; then
        readonly_disabled=0
        echo "Making the system read-only again..."
        sudo steamos-readonly enable ||
            echo "warning: could not re-enable the read-only filesystem, run 'sudo steamos-readonly enable' yourself" >&2
    fi
}

trap 'reenable_readonly; rm -rf "$tmp"' EXIT

# Prefer the checksum file: it names the package and lets us verify it. It also
# lists the -debug package of releases published before that was dropped, hence
# the match on a digit.
file=
if curl -fsSLo "$tmp/SHA256SUMS" "$base/SHA256SUMS" 2>/dev/null; then
    file=$(awk '/^[0-9a-f]+ +uke-plasma-keyboard-custom-[0-9][^ ]*\.pkg\.tar\.zst$/ {print $2}' "$tmp/SHA256SUMS" | head -n1)
    if [ -z "$file" ]; then
        echo "error: $base/SHA256SUMS does not list a package" >&2
        exit 1
    fi
    echo "Downloading $file..."
    curl -fsSLo "$tmp/$file" "$base/$file"
    (cd "$tmp" && grep " ${file}\$" SHA256SUMS > "$file.sums" && sha256sum -c "$file.sums")
else
    # Releases before the checksum asset existed: fall back to the asset list.
    echo "This release has no SHA256SUMS, reading the asset list instead."
    url=$(curl -fsSL "https://api.github.com/repos/$repo/releases/tags/$tag" | grep -o "https://[^\"]*/uke-plasma-keyboard-custom-[0-9][^\"]*\.pkg\.tar\.zst" | head -n1)
    if [ -z "$url" ]; then
        echo "error: $tag has no package to install" >&2
        exit 1
    fi
    file=${url##*/}
    echo "Downloading $file (not verified)..."
    curl -fsSLo "$tmp/$file" "$url"
fi

version=${file#uke-plasma-keyboard-custom-}
version=${version%-x86_64.pkg.tar.zst}

if [ "$dry_run" = 1 ]; then
    echo "Dry run: $file is valid, would install version $version"
    exit 0
fi

if command -v steamos-readonly >/dev/null 2>&1; then
    echo "SteamOS detected: disabling the read-only filesystem for the installation."
    sudo steamos-readonly disable
    readonly_disabled=1
fi

set -- pacman -U --noconfirm
# SteamOS (like any system with a frozen package snapshot) has no libstdc++
# package: the C++ runtime is part of gcc-libs there, and the package cannot be
# downloaded from its repositories at all. Tell pacman that it is installed, the
# library itself is already on the system.
if ! pacman -Si libstdc++ >/dev/null 2>&1; then
    echo "No libstdc++ package in the repositories (SteamOS): treating it as installed, since it comes with gcc-libs."
    set -- "$@" --assume-installed libstdc++
fi
if [ "$overwrite" = 1 ]; then
    set -- "$@" --overwrite "/usr/*"
fi

if ! sudo "$@" "$tmp/$file"; then
    cat >&2 <<'EOF'

pacman could not install the package. If it reported that files "exist in the
filesystem", they were installed without pacman (a manual "cmake --install"):
run this script again with --overwrite to replace them.
EOF
    exit 1
fi

echo "Installed uke-plasma-keyboard-custom $version."

if [ "$restart" = 0 ]; then
    exit 0
fi

# KWin keeps the keyboard running for the whole session, so the new version only
# takes effect once the input method is started again.
if ! pgrep -f "$binary" >/dev/null 2>&1; then
    echo "The keyboard is not running, it will start with the new version."
    exit 0
fi

if command -v kwriteconfig6 >/dev/null 2>&1 && [ -n "${XDG_RUNTIME_DIR:-}" ] && [ -n "${WAYLAND_DISPLAY:-}" ]; then
    input_method=$(kreadconfig6 --file kwinrc --group Wayland --key InputMethod 2>/dev/null || true)
    if [ -n "$input_method" ]; then
        echo "Restarting the keyboard..."
        kwriteconfig6 --notify --file kwinrc --group Wayland --key InputMethod ''
        sleep 1
        kwriteconfig6 --notify --file kwinrc --group Wayland --key InputMethod "$input_method"
    fi
else
    echo "Restart the keyboard (or log out and back in) to use the new version."
fi
