#!/usr/bin/env bash
# Exercise the runner with real sanitizer failures, independently of RRDtool.
set -euo pipefail
state=$(mktemp -d "${TMPDIR:-/tmp}/rrd-fuzz-run.XXXXXX")
echo "Runner test evidence: $state"
cat > "$state/probe.c" <<'EOF'
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (!size)
        return 0;
#if MODE == 1
    volatile int value = INT_MAX;
    value += data[0] + 1;
#elif MODE == 2
    volatile char *p = malloc(1);
    p[size] = 1;
    free((void *)p);
#elif MODE == 3
    void *p = malloc(size);
    __asm__ volatile("" : : "r"(p) : "memory");
#endif
    return 0;
}
EOF
for mode in 0 1 2 3; do
    # LeakSanitizer is available on the Linux CI runners, not macOS.
    if test "$mode" = 3 && test "$(uname -s)" != Linux; then
        echo 'Skipping leak probe: requires Linux LeakSanitizer'
        continue
    fi
    "${CC:-clang}" -O1 -g -fsanitize=fuzzer,address,undefined \
        -DMODE="$mode" "$state/probe.c" -o "$state/probe"
    output="$state/mode-$mode"
    mkdir -p "$output/corpus"
    printf 'retained-input' > "$output/corpus/retained"
    status=0
    bash fuzz/run.sh "$state/probe" "$output" -seed=1 -runs=100 -rss_limit_mb=0 \
        > "$state/mode-$mode.console" 2>&1 || status=$?
    test -s "$output/run.log"
    test "$(cat "$output/corpus/retained")" = retained-input
    if test "$mode" = 0; then
        test "$status" = 0
        grep -q DONE "$output/run.log"
    else
        test "$status" != 0
        case "$mode" in
            1) grep -q 'runtime error: signed integer overflow' "$output/run.log" ;;
            2) grep -q 'AddressSanitizer: heap-buffer-overflow' "$output/run.log" ;;
            3) grep -q 'LeakSanitizer: detected memory leaks' "$output/run.log" ;;
        esac
        artifacts=("$output/reproducers/"*)
        test -f "${artifacts[0]}"
    fi
    echo "mode $mode: status=$status, log and corpus preserved"
done
