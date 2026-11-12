#!/usr/bin/bash
# SPDX-License-Identifier: MIT
# COPR source worker only. No native compilation and no Python.
set -Eeuo pipefail
result_dir=${1:?COPR outdir is required}
mkdir -p "$result_dir"
result_dir=$(realpath "$result_dir")
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
mkdir -p "$work/SOURCES" "$work/SPECS"
version=$(sed -n 's/^Version:[[:space:]]*//p' packaging/uke-plasma-keyboard-custom.spec)
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]
node tools/patch-series.mjs --check
upstream_version=$(jq -r .upstream.version packaging/upstream.json)
upstream_url=$(jq -r .upstream.url packaging/upstream.json)
upstream_sha=$(jq -r .upstream.sha256 packaging/upstream.json)
[[ $upstream_version =~ ^6\.[0-9]+\.[0-9]+$ && $upstream_sha =~ ^[a-f0-9]{64}$ ]]
[[ $upstream_url == "https://download.kde.org/stable/plasma/$upstream_version/plasma-keyboard-$upstream_version.tar.xz" ]]
curl -qfsSL --retry 2 --max-time 300 "$upstream_url" -o "$work/SOURCES/plasma-keyboard-$upstream_version.tar.xz"
printf '%s  %s\n' "$upstream_sha" "$work/SOURCES/plasma-keyboard-$upstream_version.tar.xz" | sha256sum -c -
node tools/patch-series.mjs --verify-base "$work/SOURCES/plasma-keyboard-$upstream_version.tar.xz"
mapfile -t assets < <(jq -r '.assets[].path' packaging/upstream.json)
[[ ${#assets[@]} == 5 ]]
git archive --format=tar HEAD "${assets[@]}" | xz -T2 > "$work/SOURCES/uke-plasma-keyboard-custom-assets-$version.tar.xz"
git archive --format=tar HEAD README.md README.upstream.md README.custom-tools.md README.upstream.ru.md AGENTS.md .copr docs packaging | xz -T2 > "$work/SOURCES/uke-plasma-keyboard-custom-integration-$version.tar.xz"
while IFS= read -r patch; do
 [[ $patch =~ ^[0-9]{4}-[a-z-]+\.patch$ ]]
 cp "packaging/patches/$patch" "$work/SOURCES/"
done < packaging/patches/series
curl -qfsSL --retry 2 --max-time 300 'https://github.com/ggml-org/whisper.cpp/archive/refs/tags/v1.9.3.tar.gz' -o "$work/SOURCES/whisper.cpp-1.9.3.tar.gz"
curl -qfsSL --retry 2 --max-time 300 'https://github.com/handy-computer/transcribe.cpp/archive/5bb2deb2a4afb1fd50534ecb51cfcb521ef94944.tar.gz' -o "$work/SOURCES/transcribe.cpp-5bb2deb2a4afb1fd50534ecb51cfcb521ef94944.tar.gz"
printf '%s\n' \
 '1650f884effba487025143bd8facd2f9fb40a83b3737a732803c67a8d659d9c0  whisper.cpp-1.9.3.tar.gz' \
 'c6491711a6eb3c44217118f20cdf3c9ce877d175e407c7a253a52e5c0c6a824d  transcribe.cpp-5bb2deb2a4afb1fd50534ecb51cfcb521ef94944.tar.gz' > "$work/SOURCES/dependencies.sha256"
(cd "$work/SOURCES" && sha256sum -c dependencies.sha256)
cp packaging/uke-plasma-keyboard-custom.spec "$work/SPECS/"
rpmbuild -bs --define "_topdir $work" --define "_srcrpmdir $result_dir" "$work/SPECS/uke-plasma-keyboard-custom.spec"
