# End-to-End Benchmark Infrastructure

This folder contains the dedicated Stage 5 benchmark tooling.

## Files

- `trading_engine_bench.cpp`: dedicated benchmark binary source (separate from runtime `main.cpp`).
- `run_benchmark.sh`: builds the benchmark binary, runs workload, records full report, raw output, and appends history.
- `compare_results.sh`: compares the last two benchmark runs from history.

## Run

```bash
./benchmarks/end_to_end/run_benchmark.sh --ticks 100000 --repeats 5
```

Optional flags:

- `--ticks <count>`
- `--repeats <count>`
- `--threshold <value>`
- `--cxx <compiler>`
- `--build-flags "-O3 -march=native -std=c++20"`

## Outputs

Generated in `results/end_to_end/`:

- timestamped report: complete metadata + aggregated benchmark numbers
- `raw/` file: full binary output from benchmark run
- `history.csv`: persistent run history for longitudinal comparison
- `latest.txt`: copy of latest report

## Compare Last Two Runs

```bash
./benchmarks/end_to_end/compare_results.sh
```
