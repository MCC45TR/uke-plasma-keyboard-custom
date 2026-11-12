#!/usr/bin/bash
# SPDX-License-Identifier: MIT
set -Eeuo pipefail
umask 077
[[ ${PLASMA_KEYBOARD_TEST_ISOLATED_BUS:-} == 1 ]]
task_bus=$(mktemp -d)
trap 'rm -f -- "$task_bus/bus.conf"; rmdir -- "$task_bus"' EXIT
cat > "$task_bus/bus.conf" <<'UKE_BUS'
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN" "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:tmpdir=/tmp</listen>
  <policy context="default">
    <allow user="*"/>
    <allow own="*"/>
    <allow send_type="method_call"/>
    <allow send_type="signal"/>
    <allow send_requested_reply="true" send_type="method_return"/>
    <allow send_requested_reply="true" send_type="error"/>
    <allow receive_sender="*"/>
  </policy>
</busconfig>
UKE_BUS
export UKE_POSTURE_SYSTEM_POLICY=1
dbus-run-session --config-file "$task_bus/bus.conf" -- bash "$(dirname "$0")/private-system-bus.sh" "$@"
