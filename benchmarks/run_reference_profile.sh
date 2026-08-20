#!/bin/sh

set -eu

benchmark_bin=${1:-build/benchmarks/c/bench_solver}

cases='generic_forced_thin_sat
generic_result_copy_sat
generic_unconstrained_sat
generic_backtracking_sat
generic_root_unsat
yang_zhang_sat_solver
yang_zhang_unsat_solver
yang_zhang_sat_end_to_end
yang_zhang_unsat_end_to_end
yang_zhang_sat_large_solver
yang_zhang_unsat_large_solver
yang_zhang_sat_large_end_to_end
yang_zhang_unsat_large_end_to_end'

"$benchmark_bin" --environment
uname -srmo
getconf _NPROCESSORS_ONLN
git rev-parse HEAD

for benchmark_case in $cases; do
    "$benchmark_bin" --case "$benchmark_case"
done

for benchmark_case in $cases; do
    "$benchmark_bin" --case "$benchmark_case" --iterations 1
done

for benchmark_case in $cases; do
    "$benchmark_bin" --case "$benchmark_case" --iterations 1 --metrics
done

"$benchmark_bin" \
    --case generic_root_unsat \
    --capture-unsat

"$benchmark_bin" \
    --case generic_root_unsat \
    --iterations 1 \
    --capture-unsat

"$benchmark_bin" \
    --case generic_root_unsat \
    --iterations 1 \
    --metrics \
    --capture-unsat
