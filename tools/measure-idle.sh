#!/usr/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Bounded, read-only process measurement after the keyboard's physical trial.
set -Eeuo pipefail
pid=${1:?Pass the PID of the selected keyboard process}
seconds=${2:-30}
[[ $pid =~ ^[1-9][0-9]*$ && $seconds =~ ^[0-9]+$ && $seconds -ge 5 && $seconds -le 60 ]] || exit 2
exe=$(readlink "/proc/$pid/exe")
[[ $exe == /usr/bin/uke-plasma-keyboard-custom ]] || { printf 'The PID is not the installed Uke keyboard.\n' >&2; exit 2; }
stat_line=$(cat "/proc/$pid/stat")
start_id=$(awk '{sub(/^[0-9]+ \(.*\) /, ""); print $20}' <<<"$stat_line")
cpu_before=$(awk '{sub(/^[0-9]+ \(.*\) /, ""); print $12+$13}' <<<"$stat_line")
context_count(){ awk '/^(voluntary_ctxt_switches|nonvoluntary_ctxt_switches):/ {n+=$2} END {print n+0}' /proc/"$pid"/task/*/status; }
context_before=$(context_count)
tasks_before=$(printf '%s\n' /proc/"$pid"/task/[0-9]*)
t0=$(awk '{print $1}' /proc/uptime)
sleep "$seconds"
[[ $(readlink "/proc/$pid/exe") == "$exe" ]] || exit 3
stat_line=$(cat "/proc/$pid/stat")
[[ $(awk '{sub(/^[0-9]+ \(.*\) /, ""); print $20}' <<<"$stat_line") == "$start_id" ]] || { printf 'The process restarted; discard this sample.\n' >&2; exit 3; }
cpu_after=$(awk '{sub(/^[0-9]+ \(.*\) /, ""); print $12+$13}' <<<"$stat_line")
context_after=$(context_count)
tasks_after=$(printf '%s\n' /proc/"$pid"/task/[0-9]*)
context_delta=$((context_after-context_before))
if [[ $tasks_before != "$tasks_after" ]]; then context_delta=null; fi
t1=$(awk '{print $1}' /proc/uptime)
hz=$(getconf CLK_TCK)
pss=$(awk '$1=="Pss:" {print $2}' "/proc/$pid/smaps_rollup")
rss=$(awk '$1=="VmRSS:" {print $2}' "/proc/$pid/status")
threads=$(awk '$1=="Threads:" {print $2}' "/proc/$pid/status")
jq -n --argjson pid "$pid" --argjson before "$cpu_before" --argjson after "$cpu_after" --argjson hz "$hz" --argjson t0 "$t0" --argjson t1 "$t1" --argjson pss "$pss" --argjson rss "$rss" --argjson threads "$threads" --argjson switches "$context_delta" \
 '{pid:$pid,elapsed_seconds:($t1-$t0),cpu_core_percent:(100*($after-$before)/$hz/($t1-$t0)),pss_kib:$pss,rss_kib:$rss,threads:$threads,context_switch_delta:$switches}'
