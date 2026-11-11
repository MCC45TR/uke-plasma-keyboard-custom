#!/usr/bin/bash
# SPDX-License-Identifier: MIT
set -Eeuo pipefail
[[ ${PLASMA_KEYBOARD_TEST_ISOLATED_BUS:-} == 1 ]]
export DBUS_SYSTEM_BUS_ADDRESS=${DBUS_SESSION_BUS_ADDRESS:?private session bus required}
exec "$@"
