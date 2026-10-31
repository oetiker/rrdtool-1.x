#!/usr/bin/env bash
# Run from the build root. Keep each harness's corpus and diagnostics separate.
set -euo pipefail

harness=$1
state=$2
shift 2
mkdir -p "$state/corpus" "$state/reproducers"
cp fuzz/corpus/* "$state/corpus/"

# Aborting lets libFuzzer's signal handler save the triggering input. Merely
# printing a recoverable UBSan diagnostic would leave CI green.
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1
{
    git rev-parse HEAD 2>/dev/null || echo 'Source archive: revision unavailable'
    printf 'ASAN_OPTIONS=%s\nUBSAN_OPTIONS=%s\n' "$ASAN_OPTIONS" "$UBSAN_OPTIONS"
    printf 'Command:'
    printf ' %q' "$harness" "$@" "-artifact_prefix=$state/reproducers/" "$state/corpus"
    printf '\n'
} > "$state/run.log"
status=0
"$harness" "$@" "-artifact_prefix=$state/reproducers/" "$state/corpus" 2>&1 |
    tee -a "$state/run.log" || status=$?
printf 'Exit status: %s\n' "$status" | tee -a "$state/run.log"
exit "$status"
