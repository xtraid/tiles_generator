CC ?= cc
AR ?= ar
UV ?= uv
UV_CACHE_DIR ?= $(CURDIR)/.uv-cache
export UV_CACHE_DIR

CPPFLAGS ?= -Iinclude
CFLAGS ?= -std=c17 -Wall -Wextra -O2
OPENMP_FLAGS ?= -fopenmp

BUILD_DIR := build
LIB_DIR := $(BUILD_DIR)/lib

SERIAL_SOURCES := \
	src/core/tile.c \
	src/core/region.c \
	src/builder/yang_zhang.c \
	src/solver/solver_serial.c \
	src/verify/verify_tiling.c \
	src/io/json.c

OPENMP_SOURCE := src/parallel/solver_openmp.c
SERIAL_OBJECTS := $(SERIAL_SOURCES:%.c=$(BUILD_DIR)/%.o)
OPENMP_OBJECT := $(BUILD_DIR)/$(OPENMP_SOURCE:.c=.o)
PYTHON_TESTS := $(wildcard tests/python/test_*.py)

SERIAL_LIBRARY := $(LIB_DIR)/libwang.a
OPENMP_LIBRARY := $(LIB_DIR)/libwang_openmp.a

.PHONY: all setup serial openmp check python-check clean

all: serial

setup:
	$(UV) sync

serial: $(SERIAL_LIBRARY)

openmp: $(OPENMP_LIBRARY)

$(SERIAL_LIBRARY): $(SERIAL_OBJECTS) | $(LIB_DIR)
	$(AR) rcs $@ $^

$(OPENMP_LIBRARY): $(SERIAL_OBJECTS) $(OPENMP_OBJECT) | $(LIB_DIR)
	$(AR) rcs $@ $^

$(BUILD_DIR)/src/parallel/solver_openmp.o: CFLAGS += $(OPENMP_FLAGS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(LIB_DIR):
	mkdir -p $@

check:
	$(MAKE) serial
	$(MAKE) openmp
	$(MAKE) python-check

python-check:
ifneq ($(strip $(PYTHON_TESTS)),)
	$(UV) run python -m unittest discover -s tests/python -p 'test_*.py'
else
	@echo "No Python tests found; build checks passed."
endif

clean:
	$(RM) -r $(BUILD_DIR)
