#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
#
# Development helper for the landing-page screenshots.
#
# The workflow is deliberately split: this script changes the keyboard
# settings and takes the picture, while the keyboard panel itself is opened by
# the person at the machine (a tap on a text field or the "Show Virtual
# Keyboard" shortcut). The panel is an on-screen keyboard, so letting a human
# open it keeps the frames honest and avoids driving the session from a script.
#
# Usage:
#   tools/screenshots.sh backup                  save the current settings
#   tools/screenshots.sh restore                 put the saved settings back
#   tools/screenshots.sh set KEY VALUE [KEY VALUE ...]
#   tools/screenshots.sh restart                 restart the keyboard (needed for themes)
#   tools/screenshots.sh wait [SECONDS]          wait until the panel is visible
#   tools/screenshots.sh shot OUT [--crop WxH+X+Y]
#   tools/screenshots.sh click X Y
#   tools/screenshots.sh type TEXT
#   tools/screenshots.sh key KEYS
#   tools/screenshots.sh kcm OUT [--click X,Y]... [--crop WxH+X+Y]
#
# Environment: SHOTS_OUT (output directory), KB_BIN (keyboard binary),
# SCREEN_WIDTH/SCREEN_HEIGHT (screen size).

set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(dirname "$script_dir")

out_dir=${SHOTS_OUT:-$repo_root/.dsh/tmp/landing/raw}
kb_bin=${KB_BIN:-$HOME/.local/bin/uke-plasma-keyboard-custom}
kcm_plugin_path=${KCM_PLUGIN_PATH:-$HOME/.local/lib/plugins}
screen_width=${SCREEN_WIDTH:-1920}
screen_height=${SCREEN_HEIGHT:-1200}
runtime_dir=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}
settings_file=${KB_SETTINGS_FILE:-$HOME/.config/uke-plasma-keyboard-customrc}
settings_backup=$out_dir/settings-backup

export DBUS_SESSION_BUS_ADDRESS=${DBUS_SESSION_BUS_ADDRESS:-unix:path=$runtime_dir/bus}
export YDOTOOL_SOCKET=${YDOTOOL_SOCKET:-$runtime_dir/.ydotool_socket}
export WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-wayland-0}

log() { printf '%s\n' "$*" >&2; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

kwin_prop() {
    qdbus6 org.kde.KWin /VirtualKeyboard \
        org.freedesktop.DBus.Properties.Get org.kde.kwin.VirtualKeyboard "$1" 2>/dev/null
}

panel_visible() { [ "$(kwin_prop visible)" = "true" ]; }

# --- settings -------------------------------------------------------------

cmd_backup() {
    mkdir -p "$out_dir"
    cp "$settings_file" "$settings_backup"
    log "settings saved to $settings_backup"
}

cmd_restore() {
    [ -f "$settings_backup" ] || die "no saved settings at $settings_backup"
    cp "$settings_backup" "$settings_file"
    log "settings restored from $settings_backup"
}

cmd_set() {
    [ $# -ge 2 ] || die "set needs KEY VALUE pairs"
    while [ $# -gt 0 ]; do
        kwriteconfig6 --file uke-plasma-keyboard-customrc --group General --key "$1" "$2"
        log "General/$1 = $2"
        shift 2
    done
    sleep 1
}

cmd_restart() {
    "$kb_bin" --restart-input-method >/dev/null 2>&1 || true
    sleep 3
    log "keyboard restarted; open the panel again"
}

cmd_wait() {
    local seconds=${1:-120} waited=0
    while [ "$waited" -lt "$seconds" ]; do
        if panel_visible; then
            log "panel is visible"
            return 0
        fi
        sleep 1
        waited=$((waited + 1))
    done
    die "the panel did not appear within ${seconds}s"
}

# --- capture and input ----------------------------------------------------

crop() {
    local in=$1 out=$2 geometry=$3
    if [ -z "$geometry" ] || [ "$geometry" = "full" ]; then
        magick "$in" -strip "$out"
    else
        magick "$in" -crop "$geometry" +repage -strip "$out"
    fi
}

cmd_shot() {
    local out=$1 crop_geometry=${3:-full}
    [ -n "$out" ] || die "shot needs an output path"
    mkdir -p "$(dirname "$out")"
    local raw=$out_dir/.raw.png
    spectacle -b -n -f -o "$raw" >/dev/null 2>&1
    [ -s "$raw" ] || die "spectacle produced no image"
    crop "$raw" "$out" "$crop_geometry"
    log "wrote $out"
}

cmd_click() {
    [ $# -eq 2 ] || die "click needs X Y"
    ydotool mousemove --absolute -x "$1" -y "$2" >/dev/null 2>&1
    ydotool click 0xC0 >/dev/null 2>&1
}

cmd_type() { ydotool type "$1" >/dev/null 2>&1; }
cmd_key() { ydotool key "$1" >/dev/null 2>&1; }

cmd_kcm() {
    local out= crop_geometry= clicks=()
    while [ $# -gt 0 ]; do
        case "$1" in
            --crop) crop_geometry=$2; shift 2 ;;
            --click) clicks+=("$2"); shift 2 ;;
            -*) die "unknown option $1" ;;
            *) out=$1; shift ;;
        esac
    done
    [ -n "$out" ] || die "kcm needs an output path"

    QT_PLUGIN_PATH="$kcm_plugin_path" setsid kcmshell6 kcm_uke_plasma_keyboard_custom \
        >"$out_dir/kcm.log" 2>&1 </dev/null &
    sleep 9
    local point
    for point in "${clicks[@]}"; do
        cmd_click "${point%%,*}" "${point##*,}"
        sleep 1
    done
    cmd_shot "$out" --crop "${crop_geometry:-full}"
    pkill -f 'kcmshell6 kcm_uke_plasma_keyboard_custom' >/dev/null 2>&1 || true
}

usage() { sed -n '2,26p' "$0" | sed 's/^# \{0,1\}//'; }

main() {
    local command=${1:-}
    [ $# -gt 0 ] && shift
    case "$command" in
        backup) cmd_backup ;;
        restore) cmd_restore ;;
        set) cmd_set "$@" ;;
        restart) cmd_restart ;;
        wait) cmd_wait "$@" ;;
        shot) cmd_shot "$@" ;;
        click) cmd_click "$@" ;;
        type) cmd_type "$@" ;;
        key) cmd_key "$@" ;;
        kcm) cmd_kcm "$@" ;;
        ''|-h|--help|help) usage ;;
        *) die "unknown command $command" ;;
    esac
}

main "$@"
