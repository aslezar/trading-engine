#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HISTORY_CSV="$ROOT_DIR/results/end_to_end/history.csv"

if [[ ! -f "$HISTORY_CSV" ]]; then
    echo "No history found at $HISTORY_CSV" >&2
    exit 1
fi

if [[ $(wc -l < "$HISTORY_CSV") -lt 3 ]]; then
    echo "Need at least two benchmark runs in history.csv to compare." >&2
    exit 1
fi

old_row="$(tail -n 2 "$HISTORY_CSV" | head -n 1)"
new_row="$(tail -n 1 "$HISTORY_CSV")"

IFS=',' read -r old_ts old_commit old_branch old_ticks old_repeats old_threshold old_tp old_std old_min old_p50 old_p90 old_p95 old_p99 old_p999 old_p9999 old_max old_report old_raw <<< "$old_row"
IFS=',' read -r new_ts new_commit new_branch new_ticks new_repeats new_threshold new_tp new_std new_min new_p50 new_p90 new_p95 new_p99 new_p999 new_p9999 new_max new_report new_raw <<< "$new_row"

pct_change() {
    awk -v n="$1" -v o="$2" 'BEGIN{if (o==0) print "n/a"; else printf "%.2f", ((n-o)/o)*100.0}'
}

abs_change() {
    awk -v n="$1" -v o="$2" 'BEGIN{printf "%.2f", n-o}'
}

echo "================================================"
echo "Benchmark Comparison"
echo "================================================"
echo "Old: $old_ts ($old_commit)"
echo "New: $new_ts ($new_commit)"
echo
echo "Throughput change: $(pct_change "$new_tp" "$old_tp")%"
echo "Stddev change: $(abs_change "$new_std" "$old_std") ticks/sec"
echo "p50 change: $(abs_change "$new_p50" "$old_p50") ns"
echo "p90 change: $(abs_change "$new_p90" "$old_p90") ns"
echo "p95 change: $(abs_change "$new_p95" "$old_p95") ns"
echo "p99 change: $(abs_change "$new_p99" "$old_p99") ns"
echo "p99.9 change: $(abs_change "$new_p999" "$old_p999") ns"
echo "p99.99 change: $(abs_change "$new_p9999" "$old_p9999") ns"
echo "max change: $(abs_change "$new_max" "$old_max") ns"
echo
echo "Old report: $old_report"
echo "New report: $new_report"
echo "================================================"
