#!/usr/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -Eeuo pipefail
cd "$(dirname "$0")/.."
grep -q 'setExclusiveZone(sessionLocked ? 0 : panelHeight)' src/main.cpp
grep -A2 'const bool sessionLocked' src/main.cpp | grep -q 'isKnown()'
grep -q 'LaptopController::instance()->active()' src/main.cpp
! grep -q 'setEnabled(true)' src/laptop/laptopcontroller.cpp
