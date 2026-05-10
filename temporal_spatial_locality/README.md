# Cache Locality Performance Demo

A self-contained C program that benchmarks three memory-access patterns to
show the measurable performance difference between:

| Test | Temporal locality | Spatial locality | Expected speed |
|------|:-----------------:|:----------------:|:--------------:|
| 1    | No                | No               | Slowest        |
| 2    | Yes               | No               | Medium         |
| 3    | Yes               | Yes              | Fastest        |

---

## Key Concepts

**Spatial locality** — accessing memory addresses that are *close together*.  
CPUs load data in **cache lines** (~64 bytes = 16 `int`s at a time).  
Sequential access uses every element in a loaded cache line.  
Random access loads 64 bytes but uses only 4 — a **16x bandwidth waste**.

**Temporal locality** — accessing the *same* memory locations repeatedly.  
If the working set fits in cache, repeated accesses are served from fast
on-chip cache instead of slow main memory.

---

## How Each Test Works

    Test 1 — No temporal, No spatial locality
      Array  : 64 MB  (exceeds L3 cache — data is always evicted)
      Pattern: 4 M random reads, single pass
      Effect : ~every access is a main-memory miss (~100 ns each)

    Test 2 — Temporal locality only (no spatial locality)
      Array  : 256 KB (fits in L2 cache)
      Pattern: random permutation x 64 passes  (= 4 M total reads)
      Effect : pass 1 is cold; passes 2-64 hit L2/L3 cache
               but random order prevents hardware prefetching

    Test 3 — Both temporal and spatial locality
      Array  : 256 KB (fits in L2 cache)
      Pattern: sequential scan x 64 passes  (= 4 M total reads)
      Effect : hardware prefetcher loads lines ahead of time;
               nearly all accesses hit L1/L2 cache (~1-5 ns each)

All three tests perform the **same total number of reads (4,194,304)**,
making the timing comparison fair.

---

## Running on WSL (Windows Subsystem for Linux)

The program uses `clock_gettime(CLOCK_MONOTONIC)` on Linux/WSL for
nanosecond-resolution timing — no code changes are needed.

### Step 1 — Open a WSL terminal

Press **Win + S**, type `wsl` or `Ubuntu`, and open the WSL shell.  
Alternatively, open Windows Terminal and select your Linux distro from the
tab dropdown.

### Step 2 — Install GCC (if not already installed)

```bash
sudo apt update
sudo apt install -y gcc
```

Verify the installation:

```bash
gcc --version
```

You should see output like `gcc (Ubuntu ...) 11.x.x`.

### Step 3 — Navigate to the source file

Windows drives are mounted under `/mnt/` in WSL.  
The file lives at `C:\code_understanding\temporal_spatial_locality\`, which
maps to:

```bash
cd /mnt/c/code_understanding/temporal_spatial_locality
```

Confirm the file is there:

```bash
ls -lh cache_locality_demo.c
```

### Step 4 — Compile

```bash
make
```

This invokes `gcc -O0 -Wall -Wextra -std=c99`.  
`-O0` is used so the compiler cannot eliminate the array reads as dead code
(the `volatile` sink variable also guards against this at higher opt levels).

To build **and** run in one command:

```bash
make run
```

To build with `-O2` optimisation instead:

```bash
make opt
```

### Step 5 — Run

```bash
./cache_demo
```

To remove the compiled binary:

```bash
make clean
```

---

## Expected Output

    =================================================================
      Cache Locality Performance Demo
    =================================================================
      Large array  :   16777216 ints  =   64 MB  (exceeds L3 cache)
      Small array  :      65536 ints  =  256 KB  (fits in L2 cache)
      Total accesses per test : 4194304
      Passes over small array : 64
    =================================================================

    Running benchmarks -- please wait...

    -----------------------------------------------------------------
      Test                                        Time (ms)    Speedup
    -----------------------------------------------------------------
      1. No temporal,  No spatial locality          850.00     1.00x
      2. Temporal only (no spatial locality)         45.00    18.89x
      3. Both temporal & spatial locality             8.00   106.25x
    -----------------------------------------------------------------

    Explanation
    -----------

    Test 1 -- No temporal, No spatial locality
      Working set : 64 MB (exceeds all cache levels)
      Pattern     : 4194304 random accesses, single pass
      Cache lines : 64 bytes loaded per miss, only 4 bytes used (16x waste)
      Result      : nearly every access is a main-memory miss (~100 ns each)

    Test 2 -- Temporal locality only (no spatial locality)
      Working set : 256 KB (fits in L2 cache)
      Pattern     : random permutation repeated 64 times
      Pass 1      : cold misses load the working set into L2/L3 cache
      Passes 2-64 : same locations hit cache (temporal locality)
      Prefetcher  : cannot predict random addresses -- no spatial benefit
      Result      : much faster than Test 1; slower than Test 3

    Test 3 -- Both temporal and spatial locality
      Working set : 256 KB (fits in L2 cache)
      Pattern     : sequential scan repeated 64 times
      Spatial     : hardware prefetcher loads the next cache line early
      Temporal    : warm working set served from L1/L2 on every pass
      Result      : fastest -- theoretical best case for cache utilisation

*(Exact numbers vary by CPU, RAM speed, and OS scheduler. The ordering
Test 1 > Test 2 > Test 3 in elapsed time is consistent across all
modern x86-64 systems.)*

---

## Tuning

Edit the macros near the top of `cache_locality_demo.c` if needed:

| Macro            | Default            | Purpose                                       |
|------------------|--------------------|-----------------------------------------------|
| `LARGE_N`        | 16 M ints (64 MB)  | Must exceed your L3 cache size                |
| `SMALL_N`        | 64 K ints (256 KB) | Must fit in your L2 cache                     |
| `TOTAL_ACCESSES` | 4 M                | Total reads per test (increase for precision) |

---

## Other Build Options

**Native Linux / macOS / Clang:**

```bash
gcc -O0 -o cache_demo cache_locality_demo.c
# or
clang -O0 -o cache_demo cache_locality_demo.c
```

**MSVC (Windows Developer Command Prompt — no WSL needed):**

```cmd
cl /Od /Fe:cache_demo.exe cache_locality_demo.c
cache_demo.exe
```

---

## Files

- `cache_locality_demo.c` — single-file C program (C99, no external dependencies)
- `Makefile` — build rules (`make`, `make run`, `make opt`, `make clean`)
- `README.md` — this file
