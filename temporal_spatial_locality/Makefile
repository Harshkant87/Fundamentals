# ===========================================================================
# Makefile — Cache Locality Performance Demo
#
# Targets:
#   make          — build the benchmark (default, uses -O0)
#   make run      — build and run immediately
#   make opt      — build with -O2 (compiler may partially optimise loops;
#                   volatile g_sink still forces reads to be observable)
#   make clean    — remove compiled binaries
#
# Usage (WSL / Linux / macOS):
#   cd /mnt/c/code_understanding/temporal_spatial_locality
#   make
#   ./cache_demo
# ===========================================================================

CC      = gcc
SRC     = cache_locality_demo.c
TARGET  = cache_demo

# -O0  : no optimisation — safest for a fair memory-access benchmark
# -Wall / -Wextra : catch common mistakes
# -std=gnu99        : require gnu99 (for declarations inside for-loops, etc.)
CFLAGS_DEBUG = -O0 -Wall -Wextra -std=gnu99

# -O2  : standard optimisation; volatile g_sink still prevents dead-code
#        elimination, but the compiler may reorder or vectorise loops
CFLAGS_OPT   = -O2 -Wall -Wextra -std=c99

# Link with the math library (not strictly needed here, but good practice)
LDFLAGS =

# ---------------------------------------------------------------------------
# Default target — build with -O0
# ---------------------------------------------------------------------------
.PHONY: all
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS_DEBUG) -o $@ $< $(LDFLAGS)
	@echo ""
	@echo "  Build complete: ./$(TARGET)"
	@echo "  Run with:       ./$(TARGET)"
	@echo ""

# ---------------------------------------------------------------------------
# run — build (if needed) then execute
# ---------------------------------------------------------------------------
.PHONY: run
run: $(TARGET)
	./$(TARGET)

# ---------------------------------------------------------------------------
# opt — build with -O2 optimisation
# ---------------------------------------------------------------------------
.PHONY: opt
opt: $(SRC)
	$(CC) $(CFLAGS_OPT) -o $(TARGET) $< $(LDFLAGS)
	@echo ""
	@echo "  Optimised build complete: ./$(TARGET)"
	@echo "  Run with:                 ./$(TARGET)"
	@echo ""

# ---------------------------------------------------------------------------
# clean — remove compiled output
# ---------------------------------------------------------------------------
.PHONY: clean
clean:
	rm -f $(TARGET)
	@echo "  Cleaned."