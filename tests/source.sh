#!/usr/bin/bash
# SPDX-License-Identifier: MIT
set -Eeuo pipefail
cd "$(dirname "$0")/.."
if find src kwin tools autotests -type f -name '*.py' -print | read -r _; then
 printf 'Project Python code is forbidden\n' >&2; exit 1
fi
if grep -RInE 'find_package\(Python|COMMAND.*python|^[[:space:]]*python[23]?[[:space:]]' CMakeLists.txt src tools autotests; then exit 1; fi
if grep -q '/dev/uinput' src/uinputkeyboard.cpp; then exit 1; fi
grep -q 'name="uke-plasma-keyboard-customrc"' src/plasmakeyboardsettings.kcfg
grep -q 'PluginFactory_iid' kwin/backend.cpp
grep -q 'KEY_POWER' kwin/backend.cpp
grep -q 'QDBusServiceWatcher' kwin/backend.cpp
grep -q 'qrc:/qt/qml/org/uke/plasma/keyboard/main.qml' src/main.cpp
if grep -RIn 'org/kde/plasma/keyboard/custom' src kcm kwin; then exit 1; fi
bash -n packaging/copr/prepare-source.sh
bash autotests/mainpolicysourcetest.sh
printf 'Source and packaging guards passed\n'
