#define _POSIX_C_SOURCE 200809L

#include "wang/solver.h"

#include "wang/formula.h"
#include "wang/region.h"
#include "wang/yang_zhang.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>

typedef enum {
    BENCH_GENERIC_FORCED_THIN,
    BENCH_GENERIC_UNCONSTRAINED,
    BENCH_GENERIC_BACKTRACKING,
    BENCH_GENERIC_ROOT_UNSAT,
    BENCH_YANG_ZHANG_SAT,
    BENCH_YANG_ZHANG_UNSAT
} BenchmarkKind;

typedef enum {
    BENCH_SOLVER_ONLY,
    BENCH_END_TO_END
} BenchmarkScope;

typedef struct {
    const char *name;
    BenchmarkKind kind;
    BenchmarkScope scope;
    WangSolveStatus expected_status;
    size_t default_iterations;
    uint32_t variable_count;
} BenchmarkSpec;

typedef struct {
    Region region;
    YangZhangReduction reduction;
    Cm13Formula formula;
    Cm13Clause *clauses;
    bool owns_region;
    bool owns_reduction;
} BenchmarkFixture;

static const BenchmarkSpec BENCHMARKS[] = {
    {
        .name = "generic_forced_thin_sat",
        .kind = BENCH_GENERIC_FORCED_THIN,
        .scope = BENCH_SOLVER_ONLY,
        .expected_status = WANG_SOLVE_SAT,
        .default_iterations = 50,
    },
    {
        .name = "generic_unconstrained_sat",
        .kind = BENCH_GENERIC_UNCONSTRAINED,
        .scope = BENCH_SOLVER_ONLY,
        .expected_status = WANG_SOLVE_SAT,
        .default_iterations = 5,
    },
    {
        .name = "generic_backtracking_sat",
        .kind = BENCH_GENERIC_BACKTRACKING,
        .scope = BENCH_SOLVER_ONLY,
        .expected_status = WANG_SOLVE_SAT,
        .default_iterations = 5000,
    },
    {
        .name = "generic_root_unsat",
        .kind = BENCH_GENERIC_ROOT_UNSAT,
        .scope = BENCH_SOLVER_ONLY,
        .expected_status = WANG_SOLVE_UNSAT,
        .default_iterations = 5,
    },
    {
        .name = "yang_zhang_sat_solver",
        .kind = BENCH_YANG_ZHANG_SAT,
        .scope = BENCH_SOLVER_ONLY,
        .expected_status = WANG_SOLVE_SAT,
        .default_iterations = 20,
        .variable_count = 6,
    },
    {
        .name = "yang_zhang_unsat_solver",
        .kind = BENCH_YANG_ZHANG_UNSAT,
        .scope = BENCH_SOLVER_ONLY,
        .expected_status = WANG_SOLVE_UNSAT,
        .default_iterations = 50,
        .variable_count = 6,
    },
    {
        .name = "yang_zhang_sat_end_to_end",
        .kind = BENCH_YANG_ZHANG_SAT,
        .scope = BENCH_END_TO_END,
        .expected_status = WANG_SOLVE_SAT,
        .default_iterations = 20,
        .variable_count = 6,
    },
    {
        .name = "yang_zhang_unsat_end_to_end",
        .kind = BENCH_YANG_ZHANG_UNSAT,
        .scope = BENCH_END_TO_END,
        .expected_status = WANG_SOLVE_UNSAT,
        .default_iterations = 50,
        .variable_count = 6,
    },
    {
        .name = "yang_zhang_sat_large_solver",
        .kind = BENCH_YANG_ZHANG_SAT,
        .scope = BENCH_SOLVER_ONLY,
        .expected_status = WANG_SOLVE_SAT,
        .default_iterations = 5,
        .variable_count = 12,
    },
    {
        .name = "yang_zhang_unsat_large_solver",
        .kind = BENCH_YANG_ZHANG_UNSAT,
        .scope = BENCH_SOLVER_ONLY,
        .expected_status = WANG_SOLVE_UNSAT,
        .default_iterations = 10,
        .variable_count = 12,
    },
    {
        .name = "yang_zhang_sat_large_end_to_end",
        .kind = BENCH_YANG_ZHANG_SAT,
        .scope = BENCH_END_TO_END,
        .expected_status = WANG_SOLVE_SAT,
        .default_iterations = 5,
        .variable_count = 12,
    },
    {
        .name = "yang_zhang_unsat_large_end_to_end",
        .kind = BENCH_YANG_ZHANG_UNSAT,
        .scope = BENCH_END_TO_END,
        .expected_status = WANG_SOLVE_UNSAT,
        .default_iterations = 10,
        .variable_count = 12,
    },
};

static void fixture_destroy(BenchmarkFixture *fixture)
{
    if (fixture == NULL) {
        return;
    }
    if (fixture->owns_reduction) {
        yang_zhang_reduction_destroy(&fixture->reduction);
    }
    if (fixture->owns_region) {
        region_destroy(&fixture->region);
    }
    free(fixture->clauses);
    *fixture = (BenchmarkFixture){0};
}

static bool activate_all(Region *region)
{
    for (int32_t y = 0; y < region->height; ++y) {
        for (int32_t x = 0; x < region->width; ++x) {
            if (!region_set_active(region, x, y, true)) {
                return false;
            }
        }
    }
    return true;
}

static bool build_forced_thin_region(BenchmarkFixture *fixture)
{
    const int32_t width = 32768;
    if (!region_init(&fixture->region, width, 1)) {
        return false;
    }
    fixture->owns_region = true;
    if (!activate_all(&fixture->region)) {
        return false;
    }

    for (int32_t x = 0; x < width; ++x) {
        if (!region_set_boundary(&fixture->region, x, 0, N, COLOR_B) ||
            !region_set_boundary(&fixture->region, x, 0, S, COLOR_B)) {
            return false;
        }
    }
    return region_set_boundary(&fixture->region, 0, 0, W, COLOR_0) &&
        region_set_boundary(&fixture->region, width - 1, 0, E, COLOR_0);
}

static bool build_unconstrained_region(BenchmarkFixture *fixture)
{
    if (!region_init(&fixture->region, 96, 96)) {
        return false;
    }
    fixture->owns_region = true;
    return activate_all(&fixture->region);
}

static bool build_backtracking_region(BenchmarkFixture *fixture)
{
    if (!region_init(&fixture->region, 4, 4)) {
        return false;
    }
    fixture->owns_region = true;
    if (!activate_all(&fixture->region)) {
        return false;
    }

    return region_set_boundary(&fixture->region, 1, 0, N, COLOR_R) &&
        region_set_boundary(&fixture->region, 2, 3, S, COLOR_B) &&
        region_set_boundary(&fixture->region, 0, 3, W, COLOR_1) &&
        region_set_boundary(&fixture->region, 3, 1, E, COLOR_1) &&
        region_set_boundary(&fixture->region, 3, 3, E, COLOR_0);
}

static bool build_root_unsat_region(BenchmarkFixture *fixture)
{
    if (!region_init(&fixture->region, 2048, 1024)) {
        return false;
    }
    fixture->owns_region = true;
    if (!region_set_active(&fixture->region, 0, 0, true)) {
        return false;
    }
    return region_set_boundary(&fixture->region, 0, 0, N, COLOR_V);
}

static bool build_formula(
    BenchmarkFixture *fixture,
    uint32_t variable_count,
    bool satisfiable
)
{
    if (variable_count == 0 ||
        (satisfiable && variable_count % 3u != 0) ||
        (!satisfiable && variable_count % 2u != 0)) {
        return false;
    }

    fixture->clauses = calloc(variable_count, sizeof(*fixture->clauses));
    if (fixture->clauses == NULL) {
        return false;
    }

    if (satisfiable) {
        for (uint32_t group = 0; group < variable_count / 3u; ++group) {
            const uint32_t first = 3u * group;
            for (uint32_t repeat = 0; repeat < 3u; ++repeat) {
                fixture->clauses[3u * group + repeat] = (Cm13Clause){
                    .variable_index = { first, first + 1u, first + 2u },
                };
            }
        }
    } else {
        for (uint32_t pair = 0; pair < variable_count / 2u; ++pair) {
            const uint32_t first = 2u * pair;
            fixture->clauses[2u * pair] = (Cm13Clause){
                .variable_index = { first, first, first + 1u },
            };
            fixture->clauses[2u * pair + 1u] = (Cm13Clause){
                .variable_index = { first, first + 1u, first + 1u },
            };
        }
    }

    fixture->formula = (Cm13Formula){
        .variable_count = variable_count,
        .clauses = fixture->clauses,
        .clause_count = variable_count,
    };
    return true;
}

static bool prepare_fixture(
    const BenchmarkSpec *spec,
    BenchmarkFixture *fixture
)
{
    switch (spec->kind) {
    case BENCH_GENERIC_FORCED_THIN:
        return build_forced_thin_region(fixture);
    case BENCH_GENERIC_UNCONSTRAINED:
        return build_unconstrained_region(fixture);
    case BENCH_GENERIC_BACKTRACKING:
        return build_backtracking_region(fixture);
    case BENCH_GENERIC_ROOT_UNSAT:
        return build_root_unsat_region(fixture);
    case BENCH_YANG_ZHANG_SAT:
    case BENCH_YANG_ZHANG_UNSAT:
        if (!build_formula(
                fixture,
                spec->variable_count,
                spec->kind == BENCH_YANG_ZHANG_SAT
            )) {
            return false;
        }
        if (spec->scope == BENCH_SOLVER_ONLY) {
            if (!yang_zhang_build(&fixture->formula, &fixture->reduction)) {
                return false;
            }
            fixture->owns_reduction = true;
        }
        return true;
    }
    return false;
}

static const Region *prepared_region(const BenchmarkFixture *fixture)
{
    if (fixture->owns_reduction) {
        return &fixture->reduction.region;
    }
    if (fixture->owns_region) {
        return &fixture->region;
    }
    return NULL;
}

static size_t active_cell_count(const Region *region)
{
    size_t count = 0;
    for (size_t i = 0; i < region->cell_count; ++i) {
        if (region->cells[i].active) {
            ++count;
        }
    }
    return count;
}

static bool metrics_equal(
    const WangSolverMetrics *left,
    const WangSolverMetrics *right
)
{
    return left->dfs_nodes == right->dfs_nodes &&
        left->decisions == right->decisions &&
        left->backtracks == right->backtracks &&
        left->failed_leaves == right->failed_leaves &&
        left->domain_reductions == right->domain_reductions &&
        left->propagated_arcs == right->propagated_arcs &&
        left->mrv_cells_scanned == right->mrv_cells_scanned &&
        left->trail_peak == right->trail_peak &&
        left->queue_peak == right->queue_peak &&
        left->max_depth == right->max_depth;
}

static bool result_matches_contract(
    const Region *region,
    WangSolveStatus expected,
    bool capture_unsat,
    const WangSolveResult *result
)
{
    if (region == NULL) {
        return false;
    }

    if (expected == WANG_SOLVE_SAT) {
        return result->domains != NULL &&
            result->domain_count == region->cell_count &&
            result->conflict_cell == SIZE_MAX;
    }

    if (result->conflict_cell >= region->cell_count ||
        !region->cells[result->conflict_cell].active) {
        return false;
    }
    if (capture_unsat) {
        return result->domains != NULL &&
            result->domain_count == region->cell_count &&
            result->domains[result->conflict_cell] == 0;
    }
    return result->domains == NULL && result->domain_count == 0;
}

static bool parse_iterations(const char *text, size_t *out_iterations)
{
    if (text == NULL || text[0] == '\0' || text[0] == '-') {
        return false;
    }
    errno = 0;
    char *end = NULL;
    const uintmax_t value = strtoumax(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        value == 0 || value > SIZE_MAX) {
        return false;
    }
    *out_iterations = (size_t)value;
    return true;
}

static bool elapsed_nanoseconds(
    const struct timespec *start,
    const struct timespec *end,
    uint64_t *out_elapsed
)
{
    if (end->tv_sec < start->tv_sec ||
        (end->tv_sec == start->tv_sec && end->tv_nsec < start->tv_nsec)) {
        return false;
    }

    uint64_t seconds = (uint64_t)(end->tv_sec - start->tv_sec);
    int64_t nanoseconds = end->tv_nsec - start->tv_nsec;
    if (nanoseconds < 0) {
        --seconds;
        nanoseconds += INT64_C(1000000000);
    }
    if (seconds > (UINT64_MAX - (uint64_t)nanoseconds) /
            UINT64_C(1000000000)) {
        return false;
    }
    *out_elapsed = seconds * UINT64_C(1000000000) +
        (uint64_t)nanoseconds;
    return true;
}

static bool solve_once(
    const BenchmarkSpec *spec,
    const Region *region,
    const WangSolverOptions *options,
    bool capture_unsat,
    WangSolverMetrics *out_metrics
)
{
    WangSolveResult result = {0};
    const WangSolveStatus status = wang_solve_serial(
        region,
        options,
        &result
    );
    const bool valid = status == spec->expected_status &&
        result_matches_contract(
            region,
            spec->expected_status,
            capture_unsat,
            &result
        );
    if (valid) {
        *out_metrics = result.metrics;
    }
    wang_solve_result_destroy(&result);
    return valid;
}

static bool run_benchmark(
    const BenchmarkSpec *spec,
    size_t iterations,
    bool collect_metrics,
    bool capture_unsat
)
{
    BenchmarkFixture fixture = {0};
    if (!prepare_fixture(spec, &fixture)) {
        fixture_destroy(&fixture);
        return false;
    }

    WangSolverOptions options = {
        .flags = (collect_metrics ? WANG_SOLVE_COLLECT_METRICS : 0) |
            (capture_unsat ? WANG_SOLVE_CAPTURE_UNSAT_SNAPSHOT : 0),
    };
    WangSolverMetrics reference_metrics = {0};
    size_t cell_count = 0;
    size_t active_count = 0;

    const Region *region = prepared_region(&fixture);
    if (region != NULL) {
        cell_count = region->cell_count;
        active_count = active_cell_count(region);
    }

    struct timespec start;
    struct timespec end;
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        fixture_destroy(&fixture);
        return false;
    }

    for (size_t iteration = 0; iteration < iterations; ++iteration) {
        WangSolverMetrics metrics = {0};
        YangZhangReduction reduction = {0};

        if (spec->scope == BENCH_END_TO_END) {
            if (!yang_zhang_build(&fixture.formula, &reduction)) {
                fixture_destroy(&fixture);
                return false;
            }
            region = &reduction.region;
            if (iteration == 0) {
                cell_count = region->cell_count;
                active_count = active_cell_count(region);
            }
        }

        if (region == NULL) {
            fixture_destroy(&fixture);
            return false;
        }

        const bool solved = solve_once(
            spec,
            region,
            &options,
            capture_unsat,
            &metrics
        );
        if (spec->scope == BENCH_END_TO_END) {
            yang_zhang_reduction_destroy(&reduction);
        }
        if (!solved) {
            fixture_destroy(&fixture);
            return false;
        }

        if (iteration == 0) {
            reference_metrics = metrics;
        } else if (!metrics_equal(&reference_metrics, &metrics)) {
            fixture_destroy(&fixture);
            return false;
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        fixture_destroy(&fixture);
        return false;
    }

    uint64_t elapsed = 0;
    struct rusage usage;
    if (!elapsed_nanoseconds(&start, &end, &elapsed) ||
        getrusage(RUSAGE_SELF, &usage) != 0) {
        fixture_destroy(&fixture);
        return false;
    }

    printf(
        "benchmark_version=1 case=%s scope=%s expected=%s "
        "iterations=%zu metrics=%u capture_unsat=%u "
        "elapsed_ns=%" PRIu64 " ns_per_iteration=%" PRIu64 " "
        "max_rss_kib=%ld cells=%zu active=%zu "
        "dfs_nodes=%" PRIu64 " decisions=%" PRIu64 " "
        "backtracks=%" PRIu64 " failed_leaves=%" PRIu64 " "
        "domain_reductions=%" PRIu64 " propagated_arcs=%" PRIu64 " "
        "mrv_cells_scanned=%" PRIu64 " trail_peak=%zu queue_peak=%zu "
        "max_depth=%zu\n",
        spec->name,
        spec->scope == BENCH_SOLVER_ONLY ? "solver-only" : "end-to-end",
        spec->expected_status == WANG_SOLVE_SAT ? "SAT" : "UNSAT",
        iterations,
        collect_metrics ? 1u : 0u,
        capture_unsat ? 1u : 0u,
        elapsed,
        elapsed / iterations,
        usage.ru_maxrss,
        cell_count,
        active_count,
        reference_metrics.dfs_nodes,
        reference_metrics.decisions,
        reference_metrics.backtracks,
        reference_metrics.failed_leaves,
        reference_metrics.domain_reductions,
        reference_metrics.propagated_arcs,
        reference_metrics.mrv_cells_scanned,
        reference_metrics.trail_peak,
        reference_metrics.queue_peak,
        reference_metrics.max_depth
    );

    fixture_destroy(&fixture);
    return true;
}

static const BenchmarkSpec *find_benchmark(const char *name)
{
    if (name == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < sizeof(BENCHMARKS) / sizeof(BENCHMARKS[0]); ++i) {
        if (strcmp(BENCHMARKS[i].name, name) == 0) {
            return &BENCHMARKS[i];
        }
    }
    return NULL;
}

static void print_usage(const char *program)
{
    fprintf(
        stderr,
        "Usage: %s --case NAME [--iterations N] [--metrics] "
        "[--capture-unsat]\n"
        "       %s --list\n"
        "       %s --environment\n",
        program,
        program,
        program
    );
}

int main(int argc, char **argv)
{
    const char *case_name = NULL;
    size_t iterations = 0;
    bool collect_metrics = false;
    bool capture_unsat = false;
    bool list = false;
    bool environment = false;

    for (int argument = 1; argument < argc; ++argument) {
        if (strcmp(argv[argument], "--case") == 0 && argument + 1 < argc) {
            case_name = argv[++argument];
        } else if (strcmp(argv[argument], "--iterations") == 0 &&
                   argument + 1 < argc) {
            if (!parse_iterations(argv[++argument], &iterations)) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[argument], "--metrics") == 0) {
            collect_metrics = true;
        } else if (strcmp(argv[argument], "--capture-unsat") == 0) {
            capture_unsat = true;
        } else if (strcmp(argv[argument], "--list") == 0) {
            list = true;
        } else if (strcmp(argv[argument], "--environment") == 0) {
            environment = true;
        } else {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (list) {
        if (case_name != NULL || iterations != 0 || collect_metrics ||
            capture_unsat || environment) {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
        for (size_t i = 0;
             i < sizeof(BENCHMARKS) / sizeof(BENCHMARKS[0]);
             ++i) {
            puts(BENCHMARKS[i].name);
        }
        return EXIT_SUCCESS;
    }

    if (environment) {
        if (case_name != NULL || iterations != 0 || collect_metrics ||
            capture_unsat) {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
        printf(
            "benchmark_version=1 compiler=%s c_standard=%ld\n",
            __VERSION__,
            (long)__STDC_VERSION__
        );
        return EXIT_SUCCESS;
    }

    const BenchmarkSpec *spec = find_benchmark(case_name);
    if (spec == NULL) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (iterations == 0) {
        iterations = spec->default_iterations;
    }
    if (!run_benchmark(
            spec,
            iterations,
            collect_metrics,
            capture_unsat
        )) {
        fprintf(stderr, "benchmark failed: %s\n", spec->name);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
