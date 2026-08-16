#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RESULTS_DIR="$ROOT_DIR/results/end_to_end"
RAW_DIR="$RESULTS_DIR/raw"
BUILD_DIR="$ROOT_DIR/build"
BIN_PATH="$BUILD_DIR/trading_engine_bench"
HISTORY_CSV="$RESULTS_DIR/history.csv"

CXX_BIN="${CXX:-g++}"
BUILD_FLAGS="${BUILD_FLAGS:--O3 -march=native -std=c++20}"
TICKS="${TICKS:-100000}"
REPEATS="${REPEATS:-5}"
THRESHOLD="${THRESHOLD:-5}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --ticks)
            TICKS="$2"
            shift 2
            ;;
        --repeats)
            REPEATS="$2"
            shift 2
            ;;
        --threshold)
            THRESHOLD="$2"
            shift 2
            ;;
        --cxx)
            CXX_BIN="$2"
            shift 2
            ;;
        --build-flags)
            BUILD_FLAGS="$2"
            shift 2
            ;;
        *)
            echo "Unknown argument: $1" >&2
            exit 1
            ;;
    esac
done

mkdir -p "$RESULTS_DIR" "$RAW_DIR" "$BUILD_DIR"

timestamp_id="$(date -u +%Y%m%d_%H%M%S)"
run_time_utc="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
commit_hash="$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || echo unknown)"
git_branch="$(git -C "$ROOT_DIR" rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"

cpu_model="$(sysctl -n machdep.cpu.brand_string 2>/dev/null || uname -p)"
physical_cores="$(sysctl -n hw.physicalcpu 2>/dev/null || echo unknown)"
logical_cores="$(sysctl -n hw.logicalcpu 2>/dev/null || echo unknown)"
os_name="$(sw_vers -productName 2>/dev/null || uname -s)"
os_version="$(sw_vers -productVersion 2>/dev/null || uname -r)"
compiler_version="$($CXX_BIN --version | head -n 1)"

read -r -a build_flags_array <<< "$BUILD_FLAGS"

echo "Building benchmark binary..."
"$CXX_BIN" "${build_flags_array[@]}" "$ROOT_DIR/benchmarks/end_to_end/trading_engine_bench.cpp" -o "$BIN_PATH"

echo "Running benchmark workload..."
raw_output="$($BIN_PATH --ticks "$TICKS" --repeats "$REPEATS" --threshold "$THRESHOLD")"

raw_file="$RAW_DIR/${timestamp_id}_${commit_hash}.raw.txt"
report_file="$RESULTS_DIR/${timestamp_id}_${commit_hash}.txt"
printf "%s\n" "$raw_output" > "$raw_file"

mean_throughput="$(printf "%s\n" "$raw_output" | awk '/^Mean throughput:/{v=$3} END{print v+0}')"
stddev_throughput="$(printf "%s\n" "$raw_output" | awk '/^Stddev throughput:/{v=$3} END{print v+0}')"
mean_min="$(printf "%s\n" "$raw_output" | awk '/^  min: /{v=$2} END{print v+0}')"
mean_p50="$(printf "%s\n" "$raw_output" | awk '/^  p50: /{v=$2} END{print v+0}')"
mean_p90="$(printf "%s\n" "$raw_output" | awk '/^  p90: /{v=$2} END{print v+0}')"
mean_p95="$(printf "%s\n" "$raw_output" | awk '/^  p95: /{v=$2} END{print v+0}')"
mean_p99="$(printf "%s\n" "$raw_output" | awk '/^  p99: /{v=$2} END{print v+0}')"
mean_p999="$(printf "%s\n" "$raw_output" | awk '/^  p99.9: /{v=$2} END{print v+0}')"
mean_p9999="$(printf "%s\n" "$raw_output" | awk '/^  p99.99: /{v=$2} END{print v+0}')"
mean_max="$(printf "%s\n" "$raw_output" | awk '/^  max: /{v=$2} END{print v+0}')"

if [[ ! -f "$HISTORY_CSV" ]]; then
    echo "timestamp,commit,branch,ticks,repeats,threshold,throughput,stddev,min,p50,p90,p95,p99,p999,p9999,max,report_file,raw_file" > "$HISTORY_CSV"
fi

printf "%s,%s,%s,%s,%s,%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%s,%s\n" \
    "$run_time_utc" "$commit_hash" "$git_branch" "$TICKS" "$REPEATS" "$THRESHOLD" \
    "$mean_throughput" "$stddev_throughput" "$mean_min" "$mean_p50" "$mean_p90" "$mean_p95" "$mean_p99" "$mean_p999" "$mean_p9999" "$mean_max" \
    "$report_file" "$raw_file" >> "$HISTORY_CSV"

comparison_lines="No previous result available."
if [[ $(wc -l < "$HISTORY_CSV") -ge 3 ]]; then
    previous_row="$(tail -n 2 "$HISTORY_CSV" | head -n 1)"
    current_row="$(tail -n 1 "$HISTORY_CSV")"

    IFS=',' read -r prev_ts prev_commit prev_branch prev_ticks prev_repeats prev_threshold prev_tp prev_std prev_min prev_p50 prev_p90 prev_p95 prev_p99 prev_p999 prev_p9999 prev_max _ <<< "$previous_row"
    IFS=',' read -r cur_ts cur_commit cur_branch cur_ticks cur_repeats cur_threshold cur_tp cur_std cur_min cur_p50 cur_p90 cur_p95 cur_p99 cur_p999 cur_p9999 cur_max _ <<< "$current_row"

    tp_delta_pct="$(awk -v c="$cur_tp" -v p="$prev_tp" 'BEGIN{if (p==0) print "n/a"; else printf "%.2f", ((c-p)/p)*100.0}')"
    p99_delta_ns="$(awk -v c="$cur_p99" -v p="$prev_p99" 'BEGIN{printf "%.2f", c-p}')"
    p999_delta_ns="$(awk -v c="$cur_p999" -v p="$prev_p999" 'BEGIN{printf "%.2f", c-p}')"
    p9999_delta_ns="$(awk -v c="$cur_p9999" -v p="$prev_p9999" 'BEGIN{printf "%.2f", c-p}')"

    comparison_lines="Compared to previous run\n"
    comparison_lines+="Previous: ${prev_ts} (${prev_commit})\n"
    comparison_lines+="Throughput delta: ${tp_delta_pct}%\n"
    comparison_lines+="p99 delta: ${p99_delta_ns} ns\n"
    comparison_lines+="p99.9 delta: ${p999_delta_ns} ns\n"
    comparison_lines+="p99.99 delta: ${p9999_delta_ns} ns"
fi

{
    echo "================================================"
    echo "Trading Engine Benchmark"
    echo "================================================"
    echo "Version: ${commit_hash}"
    echo "Branch: ${git_branch}"
    echo "Timestamp (UTC): ${run_time_utc}"
    echo
    echo "Hardware"
    echo "CPU: ${cpu_model}"
    echo "Physical cores: ${physical_cores}"
    echo "Logical cores: ${logical_cores}"
    echo
    echo "Software"
    echo "OS: ${os_name} ${os_version}"
    echo "Compiler: ${compiler_version}"
    echo "Build flags: ${BUILD_FLAGS}"
    echo
    echo "Workload"
    echo "Messages: ${TICKS}"
    echo "Repeats: ${REPEATS}"
    echo "Threshold: ${THRESHOLD}"
    echo
    echo "Aggregated Results"
    echo "Throughput (mean): ${mean_throughput} ticks/sec"
    echo "Throughput (stddev): ${stddev_throughput} ticks/sec"
    echo "Latency (mean percentiles, ns):"
    echo "p50: ${mean_p50}"
    echo "p90: ${mean_p90}"
    echo "p95: ${mean_p95}"
    echo "p99: ${mean_p99}"
    echo "p99.9: ${mean_p999}"
    echo "p99.99: ${mean_p9999}"
    echo "max: ${mean_max}"
    echo
    echo "Comparison"
    printf "%b\n" "$comparison_lines"
    echo
    echo "Artifacts"
    echo "Raw output: ${raw_file}"
    echo "History: ${HISTORY_CSV}"
    echo "================================================"
} > "$report_file"

cp "$report_file" "$RESULTS_DIR/latest.txt"

echo "Saved report: $report_file"
echo "Saved raw output: $raw_file"
echo "Updated history: $HISTORY_CSV"
echo
echo "--- Report ---"
cat "$report_file"
