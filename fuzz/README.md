# Running and replaying fuzzers

Build with Clang and `./configure --enable-fuzzing --disable-shared` (plus
the optional-feature switches needed on your system), then `make`.
From the build root, run:

```sh
bash fuzz/run.sh fuzz/rrd_open_fuzzer fuzz-state/rrd_open_fuzzer -max_total_time=120
bash fuzz/run.sh fuzz/rrd_restore_fuzzer fuzz-state/rrd_restore_fuzzer -max_total_time=120
```

The runner enables leak detection and aborts on sanitizer errors. Each state
directory holds `run.log` (revision, command, sanitizer options and engine
output), `reproducers/`, and the growing `corpus/`. Existing corpus inputs
survive later runs. Run `bash fuzz/test-run.sh` to check success, sanitizer
failure propagation, reproducer creation and corpus preservation. The leak
probe requires Linux.

The mutation workflow restores a separate cache for each harness and saves a
new cache after successful or failed runs. GitHub cache scope and eviction
rules apply; checked-in seeds remain the fallback. Its artifacts retain logs,
reproducers and corpora for 30 days. The coverage workflow also retains these
diagnostics, raw coverage profiles and regression logs after failures, while
starting from checked-in seeds for repeatability. Cancellation or runner loss
can interrupt preservation.

Download and extract the matching harness artifact from the failed Actions
run. Build the recorded revision with the same sanitizer configuration, then
replay an individual input:

```sh
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1 \
  fuzz/rrd_open_fuzzer -runs=1 path/to/reproducers/crash-HASH
```

Use `rrd_restore_fuzzer` for its artifacts. Engine logs record the random seed
and mutation limits. A failed fuzz run is an outstanding finding even if
other coverage or regression jobs pass.
