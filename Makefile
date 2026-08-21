CC ?= cc
AR ?= ar
UV ?= uv
PYTHON ?= python3
VALGRIND ?= valgrind
UV_CACHE_DIR ?= $(CURDIR)/.uv-cache
export UV_CACHE_DIR

CPPFLAGS ?= -Iinclude
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -O2
DEPFLAGS ?= -MMD -MP
OPENMP_FLAGS ?= -fopenmp
STRICT_CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Werror -O2
SANITIZER_CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Werror \
	-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
ANALYZER_CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Werror \
	-O1 -fanalyzer
ASAN_OPTIONS ?= detect_leaks=1:strict_string_checks=1
UBSAN_OPTIONS ?= print_stacktrace=1:halt_on_error=1

BUILD_DIR := build
LIB_DIR := $(BUILD_DIR)/lib
PIC_DIR := $(BUILD_DIR)/pic
PIC_CFLAGS ?= -fPIC

SERIAL_SOURCES := \
	src/core/formula.c \
	src/core/tile.c \
	src/core/region.c \
	src/builder/permutation.c \
	src/builder/yang_zhang.c \
	src/crosscheck/yang_zhang_witness.c \
	src/solver/byte_support_table.c \
	src/solver/failed_leaf_trace.c \
	src/solver/solver_serial.c \
	src/verify/verify_tiling.c \
	src/io/json.c \
	src/io/formula_parser.c

OPENMP_SOURCE := src/parallel/solver_openmp.c

SERIAL_OBJECTS := $(SERIAL_SOURCES:%.c=$(BUILD_DIR)/%.o)
PIC_OBJECTS := $(SERIAL_SOURCES:%.c=$(PIC_DIR)/%.o)
OPENMP_OBJECT := $(BUILD_DIR)/$(OPENMP_SOURCE:.c=.o)

SERIAL_DEPS := $(SERIAL_OBJECTS:.o=.d)
PIC_DEPS := $(PIC_OBJECTS:.o=.d)
OPENMP_DEP := $(OPENMP_OBJECT:.o=.d)

C_TEST_SOURCES := $(wildcard tests/c/test_*.c)
C_TEST_BINS := $(patsubst tests/c/%.c,$(BUILD_DIR)/tests/c/%,$(C_TEST_SOURCES))
C_TEST_DEPS := $(addsuffix .d,$(C_TEST_BINS))
PYTHON_TESTS := $(shell find tests/python -type f -name 'test_*.py' -print)

BENCHMARK_SOURCE := benchmarks/c/bench_solver.c
BENCHMARK_BIN := $(BUILD_DIR)/benchmarks/c/bench_solver
BENCHMARK_DEP := $(BENCHMARK_BIN).d
SOLVER_COMPARISON := benchmarks/python/compare_solvers.py

SERIAL_LIBRARY := $(LIB_DIR)/libwang.a
SHARED_LIBRARY := $(LIB_DIR)/libwang.so
OPENMP_LIBRARY := $(LIB_DIR)/libwang_openmp.a

.PHONY: all setup serial shared openmp check c-check python-check pages-check \
	strict-check sanitizer-check analyzer-check valgrind-check \
	cachegrind-check benchmark benchmark-smoke benchmark-compare \
	benchmark-compare-smoke clean

all: serial shared

setup:
	$(UV) sync --frozen

serial: $(SERIAL_LIBRARY)

shared: $(SHARED_LIBRARY)

openmp: $(OPENMP_LIBRARY)

$(SERIAL_LIBRARY): $(SERIAL_OBJECTS) | $(LIB_DIR)
	$(AR) rcs $@ $^

$(SHARED_LIBRARY): $(PIC_OBJECTS) | $(LIB_DIR)
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(LDLIBS)

$(OPENMP_LIBRARY): $(SERIAL_OBJECTS) $(OPENMP_OBJECT) | $(LIB_DIR)
	$(AR) rcs $@ $^

$(OPENMP_OBJECT): $(OPENMP_SOURCE)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OPENMP_FLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(PIC_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PIC_CFLAGS) $(DEPFLAGS) -c $< -o $@

$(LIB_DIR):
	mkdir -p $@

$(BUILD_DIR)/tests/c/%: tests/c/%.c $(SERIAL_LIBRARY)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) $< $(SERIAL_LIBRARY) -o $@

$(BENCHMARK_BIN): $(BENCHMARK_SOURCE) $(SERIAL_LIBRARY)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) $< $(SERIAL_LIBRARY) -o $@

benchmark: $(BENCHMARK_BIN)
	sh benchmarks/run_reference_profile.sh $(BENCHMARK_BIN)

benchmark-smoke: $(BENCHMARK_BIN)
	$(BENCHMARK_BIN) \
		--case generic_backtracking_sat --iterations 1 --metrics
	$(BENCHMARK_BIN) \
		--case generic_backtracking_sat --solver optimized \
		--iterations 1 --metrics
	$(BENCHMARK_BIN) \
		--case pipeline_unsat_solver --iterations 1
	$(BENCHMARK_BIN) \
		--case pipeline_unsat_file_to_verified_decision \
		--solver optimized --iterations 1

benchmark-compare: $(BENCHMARK_BIN) shared
	$(UV) run --frozen python $(SOLVER_COMPARISON) \
		--preset smoke --samples 7 --iterations 1 \
		--timeout-seconds 30 --c-flags "$(CFLAGS)"

benchmark-compare-smoke: $(BENCHMARK_BIN) shared
	$(UV) run --frozen python $(SOLVER_COMPARISON) \
		--case pipeline_unsat --samples 1 --iterations 1 \
		--timeout-seconds 30 --c-flags "$(CFLAGS)"

check: pages-check c-check openmp python-check benchmark-smoke benchmark-compare-smoke

pages-check:
	$(PYTHON) tools/check_pages.py

c-check: serial $(C_TEST_BINS)
	@set -e; \
	if [ -z "$(strip $(C_TEST_BINS))" ]; then \
		echo "No C tests found."; \
	else \
		for test in $(C_TEST_BINS); do \
			echo "Running $$test"; \
			$$test; \
		done; \
	fi

strict-check:
	$(MAKE) clean
	$(MAKE) c-check shared openmp benchmark-smoke CFLAGS="$(STRICT_CFLAGS)"

sanitizer-check:
	$(MAKE) clean
	ASAN_OPTIONS="$(ASAN_OPTIONS)" UBSAN_OPTIONS="$(UBSAN_OPTIONS)" \
		$(MAKE) c-check openmp benchmark-smoke \
		CFLAGS="$(SANITIZER_CFLAGS)"

analyzer-check:
	$(MAKE) clean
	$(MAKE) serial shared openmp $(BENCHMARK_BIN) \
		CFLAGS="$(ANALYZER_CFLAGS)"

valgrind-check:
	$(MAKE) clean
	$(MAKE) serial openmp $(C_TEST_BINS) $(BENCHMARK_BIN)
	@set -e; \
	for test in $(C_TEST_BINS); do \
		echo "Running $$test under Valgrind"; \
		$(VALGRIND) \
			--error-exitcode=1 \
			--leak-check=full \
			--show-leak-kinds=all \
			--errors-for-leak-kinds=all \
			$$test; \
	done
	$(VALGRIND) \
		--error-exitcode=1 \
		--leak-check=full \
		--show-leak-kinds=all \
		--errors-for-leak-kinds=all \
		$(BENCHMARK_BIN) \
		--case generic_backtracking_sat --iterations 1 --metrics
	$(VALGRIND) \
		--error-exitcode=1 \
		--leak-check=full \
		--show-leak-kinds=all \
		--errors-for-leak-kinds=all \
		$(BENCHMARK_BIN) \
		--case pipeline_unsat_solver --iterations 1
	$(VALGRIND) \
		--error-exitcode=1 \
		--leak-check=full \
		--show-leak-kinds=all \
		--errors-for-leak-kinds=all \
		$(BENCHMARK_BIN) \
		--case pipeline_unsat_file_to_verified_decision \
		--solver optimized --iterations 1

cachegrind-check:
	$(MAKE) clean
	$(MAKE) serial openmp $(C_TEST_BINS) $(BENCHMARK_BIN)
	@mkdir -p $(BUILD_DIR)/cachegrind
	@set -e; \
	for test in $(C_TEST_BINS); do \
		name=$${test##*/}; \
		echo "Running $$test under Cachegrind"; \
		$(VALGRIND) \
			--tool=cachegrind \
			--cache-sim=yes \
			--error-exitcode=1 \
			--cachegrind-out-file=$(BUILD_DIR)/cachegrind/$$name.out \
			$$test; \
	done
	$(VALGRIND) \
		--tool=cachegrind \
		--cache-sim=yes \
		--error-exitcode=1 \
		--cachegrind-out-file=$(BUILD_DIR)/cachegrind/benchmark-smoke.out \
		$(BENCHMARK_BIN) \
		--case generic_backtracking_sat --iterations 1 --metrics

python-check: shared
ifneq ($(strip $(PYTHON_TESTS)),)
	PYTHONPATH="$(CURDIR)/python" \
		$(UV) run --frozen python -m unittest discover \
		-s tests/python -p 'test_*.py'
else
	@echo "No Python tests found; build checks passed."
endif

clean:
	$(RM) -r $(BUILD_DIR)

-include $(SERIAL_DEPS) $(PIC_DEPS) $(OPENMP_DEP) $(C_TEST_DEPS) \
	$(BENCHMARK_DEP)
