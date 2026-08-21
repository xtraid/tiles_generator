#!/usr/bin/env python3
"""Compare native and Z3 solvers on versioned CM1-in-3 instances."""

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import platform
import resource
import statistics
import subprocess
import sys
from time import perf_counter_ns
from typing import Any, Final


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT / "python"))

from model.tileset import TILESET  # noqa: E402
from native._lib import library  # noqa: E402
from native.formula_adapter import load_formula  # noqa: E402
from native.reduction_adapter import load_formula_and_region  # noqa: E402
from oracles.boolean_solver import (  # noqa: E402
    BooleanSolveStatus,
    solve_boolean,
)
from oracles.tiling_check import is_valid_tiling  # noqa: E402
from oracles.tiling_solver import TilingSolveStatus, solve_tiling  # noqa: E402
from oracles.witness_check import is_valid_assignment  # noqa: E402
from z3 import get_version_string  # noqa: E402


SCHEMA_VERSION: Final = 1
SUITE_VERSION: Final = 1
ENGINES: Final = (
    "c-reference",
    "c-optimized",
    "python-boolean-z3",
    "python-wang-z3",
)
SCOPES: Final = (
    "wang-solve-verified",
    "file-to-verified-decision",
)


@dataclass(frozen=True, slots=True)
class CaseSpec:
    name: str
    relative_path: str
    expected: str
    variable_count: int
    c_solver_case: str
    c_file_case: str

    @property
    def path(self) -> Path:
        return REPOSITORY_ROOT / self.relative_path


CASES: Final = {
    "pipeline_sat": CaseSpec(
        name="pipeline_sat",
        relative_path="tests/instances/pipeline_sat.cm13",
        expected="SAT",
        variable_count=3,
        c_solver_case="pipeline_sat_solver",
        c_file_case="pipeline_sat_file_to_verified_decision",
    ),
    "pipeline_unsat": CaseSpec(
        name="pipeline_unsat",
        relative_path="tests/instances/pipeline_unsat.cm13",
        expected="UNSAT",
        variable_count=1,
        c_solver_case="pipeline_unsat_solver",
        c_file_case="pipeline_unsat_file_to_verified_decision",
    ),
    "yang_zhang_sat_6": CaseSpec(
        name="yang_zhang_sat_6",
        relative_path="benchmarks/instances/yang_zhang_sat_6.cm13",
        expected="SAT",
        variable_count=6,
        c_solver_case="yang_zhang_sat_6_file_solver",
        c_file_case="yang_zhang_sat_6_file_to_verified_decision",
    ),
    "yang_zhang_unsat_6": CaseSpec(
        name="yang_zhang_unsat_6",
        relative_path="benchmarks/instances/yang_zhang_unsat_6.cm13",
        expected="UNSAT",
        variable_count=6,
        c_solver_case="yang_zhang_unsat_6_file_solver",
        c_file_case="yang_zhang_unsat_6_file_to_verified_decision",
    ),
    "yang_zhang_sat_12": CaseSpec(
        name="yang_zhang_sat_12",
        relative_path="benchmarks/instances/yang_zhang_sat_12.cm13",
        expected="SAT",
        variable_count=12,
        c_solver_case="yang_zhang_sat_12_file_solver",
        c_file_case="yang_zhang_sat_12_file_to_verified_decision",
    ),
    "yang_zhang_unsat_12": CaseSpec(
        name="yang_zhang_unsat_12",
        relative_path="benchmarks/instances/yang_zhang_unsat_12.cm13",
        expected="UNSAT",
        variable_count=12,
        c_solver_case="yang_zhang_unsat_12_file_solver",
        c_file_case="yang_zhang_unsat_12_file_to_verified_decision",
    ),
}

PRESETS: Final = {
    "smoke": ("pipeline_sat", "pipeline_unsat"),
    "standard": (
        "pipeline_sat",
        "pipeline_unsat",
        "yang_zhang_sat_6",
        "yang_zhang_unsat_6",
    ),
    "scaling": tuple(CASES),
}


def _parse_key_value_line(line: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for token in line.split():
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        fields[key] = value
    return fields


def _input_sha256(spec: CaseSpec) -> str:
    return hashlib.sha256(spec.path.read_bytes()).hexdigest()


def _cpu_model() -> str:
    try:
        for line in Path("/proc/cpuinfo").read_text().splitlines():
            if line.startswith("model name"):
                return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return "unknown"


def _git_metadata() -> tuple[str, bool]:
    commit = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=REPOSITORY_ROOT,
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    dirty = bool(
        subprocess.run(
            ["git", "status", "--porcelain"],
            cwd=REPOSITORY_ROOT,
            check=True,
            capture_output=True,
            text=True,
        ).stdout
    )
    return commit, dirty


def _process_peak_rss_kib() -> tuple[int, str]:
    try:
        for line in Path("/proc/self/status").read_text().splitlines():
            if line.startswith("VmHWM:"):
                return int(line.split()[1]), "proc-vmhwm"
    except (OSError, ValueError, IndexError):
        pass
    peak_rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    if sys.platform == "darwin":
        return peak_rss // 1024, "getrusage-macos-normalized"
    return peak_rss, "getrusage"


def _environment_record(
    c_benchmark: Path,
    *,
    c_flags: str,
    cases: list[CaseSpec],
    engines: list[str],
    scopes: list[str],
    samples: int,
    iterations: int,
    timeout_seconds: float,
) -> dict[str, Any]:
    commit, dirty = _git_metadata()
    c_environment = _parse_key_value_line(
        subprocess.run(
            [str(c_benchmark), "--environment"],
            cwd=REPOSITORY_ROOT,
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()
    )
    affinity = (
        sorted(os.sched_getaffinity(0))
        if hasattr(os, "sched_getaffinity")
        else None
    )
    return {
        "schema_version": SCHEMA_VERSION,
        "suite_version": SUITE_VERSION,
        "record": "environment",
        "git_commit": commit,
        "git_dirty": dirty,
        "python_version": platform.python_version(),
        "z3_version": get_version_string(),
        "kernel": platform.release(),
        "machine": platform.machine(),
        "cpu": _cpu_model(),
        "affinity": affinity,
        "c_benchmark_version": int(c_environment["benchmark_version"]),
        "compiler": c_environment["compiler"],
        "c_standard": int(c_environment["c_standard"]),
        "c_flags": c_flags,
        "cases": [spec.name for spec in cases],
        "engines": engines,
        "scopes": scopes,
        "samples": samples,
        "iterations": iterations,
        "timeout_seconds": timeout_seconds,
    }


def _validate_boolean_result(spec: CaseSpec, formula: Any, result: Any) -> None:
    expected = (
        BooleanSolveStatus.SAT
        if spec.expected == "SAT"
        else BooleanSolveStatus.UNSAT
    )
    if result.status is not expected:
        raise RuntimeError(
            f"{spec.name}: Boolean Z3 returned {result.status.value}"
        )
    if expected is BooleanSolveStatus.SAT:
        if result.assignment is None or not is_valid_assignment(
            formula,
            result.assignment,
        ):
            raise RuntimeError(f"{spec.name}: invalid Boolean witness")


def _validate_tiling_result(spec: CaseSpec, region: Any, result: Any) -> None:
    expected = (
        TilingSolveStatus.SAT
        if spec.expected == "SAT"
        else TilingSolveStatus.UNSAT
    )
    if result.status is not expected:
        raise RuntimeError(
            f"{spec.name}: Wang Z3 returned {result.status.value}"
        )
    if expected is TilingSolveStatus.SAT:
        if result.tiling is None or not is_valid_tiling(
            region,
            TILESET,
            result.tiling,
        ):
            raise RuntimeError(f"{spec.name}: invalid Wang witness")


def _run_python_worker(
    spec: CaseSpec,
    engine: str,
    scope: str,
    iterations: int,
) -> dict[str, Any]:
    library()
    cells: int | None = None
    active: int | None = None

    if scope == "wang-solve-verified":
        if engine != "python-wang-z3":
            raise ValueError("only Wang Z3 supports the Region solve scope")
        _, region = load_formula_and_region(spec.path)
        cells = len(region.active)
        active = sum(region.active)

        start = perf_counter_ns()
        for _ in range(iterations):
            result = solve_tiling(region, TILESET)
            _validate_tiling_result(spec, region, result)
        elapsed = perf_counter_ns() - start
        problem = "wang-region"
    elif scope == "file-to-verified-decision":
        start = perf_counter_ns()
        if engine == "python-boolean-z3":
            for _ in range(iterations):
                formula = load_formula(spec.path)
                result = solve_boolean(formula)
                _validate_boolean_result(spec, formula, result)
            problem = "cm13-direct"
        elif engine == "python-wang-z3":
            for _ in range(iterations):
                _, region = load_formula_and_region(spec.path)
                cells = len(region.active)
                active = sum(region.active)
                result = solve_tiling(region, TILESET)
                _validate_tiling_result(spec, region, result)
            problem = "wang-region"
        else:
            raise ValueError(f"unsupported Python engine: {engine}")
        elapsed = perf_counter_ns() - start
    else:
        raise ValueError(f"unsupported scope: {scope}")

    peak_rss_kib, peak_rss_source = _process_peak_rss_kib()
    return {
        "schema_version": SCHEMA_VERSION,
        "suite_version": SUITE_VERSION,
        "record": "worker",
        "case": spec.name,
        "engine": engine,
        "problem": problem,
        "scope": scope,
        "expected": spec.expected,
        "status": spec.expected,
        "iterations": iterations,
        "elapsed_ns": elapsed,
        "ns_per_iteration": elapsed // iterations,
        "process_peak_rss_kib": peak_rss_kib,
        "peak_rss_source": peak_rss_source,
        "variables": spec.variable_count,
        "clauses": spec.variable_count,
        "cells": cells,
        "active": active,
        "input_sha256": _input_sha256(spec),
    }


def _worker_command(
    spec: CaseSpec,
    engine: str,
    scope: str,
    iterations: int,
    c_benchmark: Path,
) -> list[str]:
    if engine.startswith("c-"):
        solver = engine.removeprefix("c-")
        case_name = (
            spec.c_solver_case
            if scope == "wang-solve-verified"
            else spec.c_file_case
        )
        return [
            str(c_benchmark),
            "--case",
            case_name,
            "--solver",
            solver,
            "--iterations",
            str(iterations),
        ]
    return [
        sys.executable,
        str(Path(__file__).resolve()),
        "--worker",
        "--case",
        spec.name,
        "--engine",
        engine,
        "--scope",
        scope,
        "--iterations",
        str(iterations),
    ]


def _run_fresh_worker(
    spec: CaseSpec,
    engine: str,
    scope: str,
    iterations: int,
    timeout_seconds: float,
    c_benchmark: Path,
) -> dict[str, Any]:
    command = _worker_command(spec, engine, scope, iterations, c_benchmark)
    try:
        completed = subprocess.run(
            command,
            cwd=REPOSITORY_ROOT,
            check=True,
            capture_output=True,
            text=True,
            timeout=timeout_seconds,
        )
    except subprocess.TimeoutExpired:
        return {
            "schema_version": SCHEMA_VERSION,
            "suite_version": SUITE_VERSION,
            "record": "sample",
            "case": spec.name,
            "engine": engine,
            "problem": (
                "cm13-direct"
                if engine == "python-boolean-z3"
                else "wang-region"
            ),
            "scope": scope,
            "expected": spec.expected,
            "status": "TIMEOUT",
            "iterations": iterations,
            "elapsed_ns": None,
            "ns_per_iteration": None,
            "process_peak_rss_kib": None,
            "peak_rss_source": None,
            "variables": spec.variable_count,
            "clauses": spec.variable_count,
            "cells": None,
            "active": None,
            "input_sha256": _input_sha256(spec),
        }

    if engine.startswith("c-"):
        fields = _parse_key_value_line(completed.stdout.strip())
        expected_case = (
            spec.c_solver_case
            if scope == "wang-solve-verified"
            else spec.c_file_case
        )
        expected_scope = (
            "solver-only"
            if scope == "wang-solve-verified"
            else "file-to-verified-decision"
        )
        expected_solver = engine.removeprefix("c-")
        if (
            fields.get("case") != expected_case
            or fields.get("solver") != expected_solver
            or fields.get("scope") != expected_scope
            or fields.get("expected") != spec.expected
        ):
            raise RuntimeError(
                f"unexpected native benchmark identity: {fields}"
            )
        record = {
            "schema_version": SCHEMA_VERSION,
            "suite_version": SUITE_VERSION,
            "record": "sample",
            "case": spec.name,
            "engine": engine,
            "problem": "wang-region",
            "scope": scope,
            "expected": spec.expected,
            "status": fields["expected"],
            "iterations": int(fields["iterations"]),
            "elapsed_ns": int(fields["elapsed_ns"]),
            "ns_per_iteration": int(fields["ns_per_iteration"]),
            "process_peak_rss_kib": int(fields["process_peak_rss_kib"]),
            "peak_rss_source": fields["peak_rss_source"],
            "variables": spec.variable_count,
            "clauses": spec.variable_count,
            "cells": int(fields["cells"]),
            "active": int(fields["active"]),
            "input_sha256": _input_sha256(spec),
        }
    else:
        record = json.loads(completed.stdout)
        record["record"] = "sample"
    return record


def _selected_combinations(
    cases: list[CaseSpec],
    engines: list[str],
    scopes: list[str],
) -> list[tuple[CaseSpec, str, str]]:
    combinations = []
    for spec in cases:
        for scope in scopes:
            for engine in engines:
                if (
                    scope == "wang-solve-verified"
                    and engine == "python-boolean-z3"
                ):
                    continue
                combinations.append((spec, engine, scope))
    return combinations


def _summary_records(samples: list[dict[str, Any]]) -> list[dict[str, Any]]:
    grouped: dict[tuple[str, str, str], list[dict[str, Any]]] = {}
    for sample in samples:
        key = (sample["case"], sample["engine"], sample["scope"])
        grouped.setdefault(key, []).append(sample)

    summaries = []
    for (case, engine, scope), records in sorted(grouped.items()):
        completed = [
            record
            for record in records
            if record["status"] == record["expected"]
        ]
        times = [record["ns_per_iteration"] for record in completed]
        rss_values = [record["process_peak_rss_kib"] for record in completed]
        summaries.append(
            {
                "schema_version": SCHEMA_VERSION,
                "suite_version": SUITE_VERSION,
                "record": "summary",
                "case": case,
                "engine": engine,
                "problem": records[0]["problem"],
                "scope": scope,
                "expected": records[0]["expected"],
                "samples": len(records),
                "completed_samples": len(completed),
                "timeouts": sum(
                    record["status"] == "TIMEOUT" for record in records
                ),
                "median_ns_per_iteration": (
                    int(statistics.median(times)) if times else None
                ),
                "min_ns_per_iteration": min(times) if times else None,
                "max_ns_per_iteration": max(times) if times else None,
                "median_process_peak_rss_kib": (
                    int(statistics.median(rss_values))
                    if rss_values
                    else None
                ),
            }
        )
    return summaries


def _positive_int(text: str) -> int:
    value = int(text)
    if value <= 0:
        raise argparse.ArgumentTypeError("value must be positive")
    return value


def _positive_float(text: str) -> float:
    value = float(text)
    if value <= 0:
        raise argparse.ArgumentTypeError("value must be positive")
    return value


def _parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", choices=PRESETS, default="smoke")
    parser.add_argument("--case", choices=CASES, action="append")
    parser.add_argument("--engine", choices=ENGINES, action="append")
    parser.add_argument("--scope", choices=SCOPES, action="append")
    parser.add_argument("--samples", type=_positive_int, default=3)
    parser.add_argument("--iterations", type=_positive_int, default=1)
    parser.add_argument(
        "--timeout-seconds",
        type=_positive_float,
        default=120.0,
    )
    parser.add_argument(
        "--c-benchmark",
        type=Path,
        default=REPOSITORY_ROOT / "build/benchmarks/c/bench_solver",
    )
    parser.add_argument(
        "--c-flags",
        default="unknown (benchmark binary supplied externally)",
        help="C compiler flags recorded as run provenance",
    )
    parser.add_argument("--worker", action="store_true", help=argparse.SUPPRESS)
    return parser.parse_args()


def main() -> int:
    arguments = _parse_arguments()
    case_names = arguments.case or list(PRESETS[arguments.preset])
    engines = arguments.engine or list(ENGINES)
    scopes = arguments.scope or list(SCOPES)
    cases = [CASES[name] for name in case_names]

    if arguments.worker:
        if len(cases) != 1 or len(engines) != 1 or len(scopes) != 1:
            raise SystemExit("worker mode requires one case, engine, and scope")
        if engines[0].startswith("c-"):
            raise SystemExit("C workers execute the native benchmark directly")
        print(
            json.dumps(
                _run_python_worker(
                    cases[0],
                    engines[0],
                    scopes[0],
                    arguments.iterations,
                ),
                sort_keys=True,
            )
        )
        return 0

    c_benchmark = arguments.c_benchmark.resolve()
    if not c_benchmark.is_file():
        raise SystemExit(f"missing C benchmark: {c_benchmark}")

    print(
        json.dumps(
            _environment_record(
                c_benchmark,
                c_flags=arguments.c_flags,
                cases=cases,
                engines=engines,
                scopes=scopes,
                samples=arguments.samples,
                iterations=arguments.iterations,
                timeout_seconds=arguments.timeout_seconds,
            ),
            sort_keys=True,
        )
    )
    combinations = _selected_combinations(cases, engines, scopes)
    if not combinations:
        raise SystemExit("no applicable engine/scope combinations selected")
    samples = []
    for sample_index in range(arguments.samples):
        ordered = (
            combinations
            if sample_index % 2 == 0
            else list(reversed(combinations))
        )
        for spec, engine, scope in ordered:
            record = _run_fresh_worker(
                spec,
                engine,
                scope,
                arguments.iterations,
                arguments.timeout_seconds,
                c_benchmark,
            )
            record["sample_index"] = sample_index
            record["timeout_seconds"] = arguments.timeout_seconds
            samples.append(record)
            print(json.dumps(record, sort_keys=True), flush=True)

    for summary in _summary_records(samples):
        print(json.dumps(summary, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
