CC ?= cc
AR ?= ar
UV ?= uv
UV_CACHE_DIR ?= $(CURDIR)/.uv-cache
export UV_CACHE_DIR

CPPFLAGS ?= -Iinclude
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -O2
DEPFLAGS ?= -MMD -MP
OPENMP_FLAGS ?= -fopenmp

BUILD_DIR := build
LIB_DIR := $(BUILD_DIR)/lib

SERIAL_SOURCES := \
	src/core/tile.c \
	src/core/region.c \
	src/builder/permutation.c \
	src/builder/yang_zhang.c \
	src/solver/solver_serial.c \
	src/verify/verify_tiling.c \
	src/io/json.c

OPENMP_SOURCE := src/parallel/solver_openmp.c

SERIAL_OBJECTS := $(SERIAL_SOURCES:%.c=$(BUILD_DIR)/%.o)
OPENMP_OBJECT := $(BUILD_DIR)/$(OPENMP_SOURCE:.c=.o)

SERIAL_DEPS := $(SERIAL_OBJECTS:.o=.d)
OPENMP_DEP := $(OPENMP_OBJECT:.o=.d)

PYTHON_TESTS := $(wildcard tests/python/test_*.py)
C_TEST_SOURCES := $(wildcard tests/c/test_*.c)
C_TEST_BINS := $(patsubst tests/c/%.c,$(BUILD_DIR)/tests/c/%,$(C_TEST_SOURCES))

SERIAL_LIBRARY := $(LIB_DIR)/libwang.a
OPENMP_LIBRARY := $(LIB_DIR)/libwang_openmp.a

.PHONY: all setup serial openmp check c-check python-check clean

all: serial

setup:
	$(UV) sync --frozen

serial: $(SERIAL_LIBRARY)

openmp: $(OPENMP_LIBRARY)

$(SERIAL_LIBRARY): $(SERIAL_OBJECTS) | $(LIB_DIR)
	$(AR) rcs $@ $^

$(OPENMP_LIBRARY): $(SERIAL_OBJECTS) $(OPENMP_OBJECT) | $(LIB_DIR)
	$(AR) rcs $@ $^

$(BUILD_DIR)/src/parallel/solver_openmp.o: CFLAGS += $(OPENMP_FLAGS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(LIB_DIR):
	mkdir -p $@

$(BUILD_DIR)/tests/c/%: tests/c/%.c $(SERIAL_LIBRARY)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(SERIAL_LIBRARY) -o $@

check: c-check openmp python-check

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

python-check:
ifneq ($(strip $(PYTHON_TESTS)),)
	$(UV) run --frozen python -m unittest discover -s tests/python -p 'test_*.py'
else
	@echo "No Python tests found; build checks passed."
endif

clean:
	$(RM) -r $(BUILD_DIR)

-include $(SERIAL_DEPS) $(OPENMP_DEP)
