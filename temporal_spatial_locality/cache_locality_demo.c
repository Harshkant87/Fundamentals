/*
 * cache_locality_demo.c
 *
 * Demonstrates the performance impact of CPU cache locality by benchmarking
 * three distinct memory-access patterns with an equal number of read operations.
 *
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │  Test 1 — No temporal locality, No spatial locality  (worst case)       │
 * │    Working set : 64 MB  (exceeds L3 cache)                              │
 * │    Pattern     : random access, single pass                             │
 * │    Effect      : nearly every access is a main-memory miss (~100 ns)    │
 * │                                                                         │
 * │  Test 2 — Temporal locality only, No spatial locality                   │
 * │    Working set : 256 KB (fits in L2 cache)                              │
 * │    Pattern     : random access, 64 repeated passes                      │
 * │    Effect      : data is warm in cache after pass 1; random order       │
 * │                  prevents hardware prefetching                          │
 * │                                                                         │
 * │  Test 3 — Both temporal and spatial locality         (best case)        │
 * │    Working set : 256 KB (fits in L2 cache)                              │
 * │    Pattern     : sequential access, 64 repeated passes                  │
 * │    Effect      : hardware prefetcher loads lines ahead of time;         │
 * │                  nearly all accesses hit L1/L2 cache (~1–5 ns)          │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * Key concepts
 * ────────────
 *  Spatial locality  – Accessing memory addresses that are close together.
 *    CPUs load data in cache lines (~64 bytes = 16 ints).  Sequential access
 *    uses every element in a loaded cache line.  Random access loads a full
 *    cache line but uses only one element, wasting 60 of 64 bytes.
 *
 *  Temporal locality – Accessing the same memory locations repeatedly.
 *    If the working set fits in cache, repeated accesses are served from
 *    fast cache rather than slow main memory.
 *
 * Build
 * ─────
 *  GCC / Clang  :  gcc  -O0 -o cache_demo cache_locality_demo.c
 *  MSVC         :  cl   /Od /Fe:cache_demo.exe cache_locality_demo.c
 *
 *  Use -O0 / /Od to prevent the compiler from eliminating memory accesses.
 *
 * Typical cache sizes (adjust LARGE_N / SMALL_N for your CPU if needed)
 * ──────────────────────────────────────────────────────────────────────
 *  L1 data cache : 32–64 KB
 *  L2 cache      : 256 KB – 1 MB
 *  L3 cache      : 8–32 MB
 */
#define _POSIX_C_SOURCE 199309L  
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* =========================================================================
 * Platform-specific high-resolution timer
 * ========================================================================= */

#ifdef _WIN32
#  include <windows.h>
   typedef LARGE_INTEGER hrtimer_t;
   static LARGE_INTEGER g_perf_freq;

   static void timer_init(void) {
       QueryPerformanceFrequency(&g_perf_freq);
   }
   static void timer_now(hrtimer_t *t) {
       QueryPerformanceCounter(t);
   }
   static double timer_elapsed_ms(const hrtimer_t *start,
                                   const hrtimer_t *end) {
       return (double)(end->QuadPart - start->QuadPart)
              * 1000.0 / (double)g_perf_freq.QuadPart;
   }
#else
#  include <time.h>
   typedef struct timespec hrtimer_t;

   static void timer_init(void) { /* nothing needed on POSIX */ }
   static void timer_now(hrtimer_t *t) {
       clock_gettime(CLOCK_MONOTONIC, t);
   }
   static double timer_elapsed_ms(const hrtimer_t *start,
                                   const hrtimer_t *end) {
       return (double)(end->tv_sec  - start->tv_sec)  * 1000.0
            + (double)(end->tv_nsec - start->tv_nsec) / 1.0e6;
   }
#endif

/* =========================================================================
 * Configuration
 * =========================================================================
 *
 * Increase LARGE_N if your CPU has an unusually large L3 cache (> 64 MB).
 * Decrease SMALL_N if your CPU has an unusually small L2 cache (< 256 KB).
 */

/* 16 M ints = 64 MB — larger than a typical L3 cache (8–32 MB) */
#define LARGE_N         (16 * 1024 * 1024)

/* 64 K ints = 256 KB — fits comfortably in a typical L2 cache */
#define SMALL_N         (64 * 1024)

/* Total read operations performed in every test (keeps comparisons fair) */
#define TOTAL_ACCESSES  (4 * 1024 * 1024)

/* Number of full passes over the small array  (= TOTAL_ACCESSES / SMALL_N) */ 
#define PASSES          (TOTAL_ACCESSES / SMALL_N)

/* =========================================================================
 * Utility helpers
 * ========================================================================= */

/*
 * Global accumulator — forces the compiler to treat every array read as
 * observable, preventing dead-code elimination even at -O2 / -O3.
 */
static volatile long long g_sink = 0;

/*
 * Fisher-Yates in-place shuffle.
 * Produces a uniformly random permutation of arr[0..n-1].
 */
static void shuffle(int *arr, int n)
{
    for (int i = n - 1; i > 0; --i) {
        int j   = rand() % (i + 1);
        int tmp = arr[i];
        arr[i]  = arr[j];
        arr[j]  = tmp;
    }
}

/* =========================================================================
 * Test 1 — No temporal locality, No spatial locality
 * =========================================================================
 *
 * Access TOTAL_ACCESSES random positions scattered across a 64 MB array.
 *
 *  • No spatial locality  : random positions mean adjacent elements in a
 *    loaded cache line (64 bytes = 16 ints) are never used.  The CPU loads
 *    64 bytes but uses only 4 — a 16× waste of memory bandwidth.
 *
 *  • No temporal locality : each position is visited at most once, so
 *    previously loaded cache lines are never reused before eviction.
 *
 * Expected result: slowest — almost every access goes to main memory.
 */
static double test_no_locality(const int *data, const int *rand_idx)
{
    hrtimer_t t0, t1;
    long long sum = 0;

    timer_now(&t0);
    for (int i = 0; i < TOTAL_ACCESSES; ++i) {
        sum += data[rand_idx[i]];
    }
    timer_now(&t1);

    g_sink += sum;
    return timer_elapsed_ms(&t0, &t1);
}

/* =========================================================================
 * Test 2 — Temporal locality only (no spatial locality)
 * =========================================================================
 *
 * Access a random permutation of a 256 KB array, repeated PASSES times.
 *
 *  • No spatial locality  : the permutation is random, so the hardware
 *    prefetcher cannot predict the next address.  Each cache-line load
 *    still yields only one useful element.
 *
 *  • Temporal locality    : the 256 KB working set fits in L2 cache.
 *    After the first pass every element is cached; the remaining
 *    (PASSES - 1) passes are served from L2/L3 rather than main memory.
 *
 * Expected result: faster than Test 1 (cache hits after pass 1),
 *                  slower than Test 3 (no prefetching benefit).
 */
static double test_temporal_only(const int *data, const int *rand_perm)
{
    hrtimer_t t0, t1;
    long long sum = 0;

    timer_now(&t0);
    for (int pass = 0; pass < PASSES; ++pass) {
        for (int i = 0; i < SMALL_N; ++i) {
            sum += data[rand_perm[i]];
        }
    }
    timer_now(&t1);

    g_sink += sum;
    return timer_elapsed_ms(&t0, &t1);
}

/* =========================================================================
 * Test 3 — Both temporal and spatial locality
 * =========================================================================
 *
 * Access a 256 KB array sequentially, repeated PASSES times.
 *
 *  • Spatial locality     : sequential access lets the hardware prefetcher
 *    load the next cache line before it is needed.  Every byte of every
 *    loaded cache line is eventually used — zero bandwidth waste.
 *
 *  • Temporal locality    : the 256 KB working set fits in L2 cache.
 *    After the first pass all data is warm; subsequent passes hit L1/L2.
 *
 * Expected result: fastest — nearly all accesses hit L1/L2 cache.
 */
static double test_both_locality(const int *data)
{
    hrtimer_t t0, t1;
    long long sum = 0;

    timer_now(&t0);
    for (int pass = 0; pass < PASSES; ++pass) {
        for (int i = 0; i < SMALL_N; ++i) {
            sum += data[i];
        }
    }
    timer_now(&t1);

    g_sink += sum;
    return timer_elapsed_ms(&t0, &t1);
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void)
{
    timer_init();
    srand((unsigned int)time(NULL));

    /* ── Print configuration ─────────────────────────────────────────── */
    printf("=================================================================\n");
    printf("  Cache Locality Performance Demo\n");
    printf("=================================================================\n");
    printf("  Large array  : %10d ints  = %4d MB  (exceeds L3 cache)\n",
           LARGE_N,
           (int)((size_t)LARGE_N * sizeof(int) / (1024u * 1024u)));
    printf("  Small array  : %10d ints  = %4d KB  (fits in L2 cache)\n",
           SMALL_N,
           (int)((size_t)SMALL_N * sizeof(int) / 1024u));
    printf("  Total accesses per test : %d\n", TOTAL_ACCESSES);
    printf("  Passes over small array : %d\n", PASSES);
    printf("=================================================================\n\n");

    /* ── Allocate memory ─────────────────────────────────────────────── */
    int *large_data = (int *)malloc((size_t)LARGE_N        * sizeof(int));
    int *small_data = (int *)malloc((size_t)SMALL_N        * sizeof(int));
    int *large_rand = (int *)malloc((size_t)TOTAL_ACCESSES * sizeof(int));
    int *small_perm = (int *)malloc((size_t)SMALL_N        * sizeof(int));

    if (!large_data || !small_data || !large_rand || !small_perm) {
        fprintf(stderr, "ERROR: Memory allocation failed.\n");
        free(large_data);
        free(small_data);
        free(large_rand);
        free(small_perm);
        return EXIT_FAILURE;
    }

    /* ── Initialise data arrays ──────────────────────────────────────── */
    /*
     * Writing to every page now avoids page-fault overhead during timing.
     * The values themselves are unimportant; we just need valid reads.
     */
    for (int i = 0; i < LARGE_N; ++i) large_data[i] = i;
    for (int i = 0; i < SMALL_N; ++i) {
        small_data[i] = i;
        small_perm[i] = i;   /* will be shuffled below */
    }

    /* ── Build random index arrays ───────────────────────────────────── */

    /* Test 1: TOTAL_ACCESSES independent random indices into the large array */
    for (int i = 0; i < TOTAL_ACCESSES; ++i) {
        large_rand[i] = rand() % LARGE_N;
    }

    /* Tests 2 & 3: a single random permutation of [0, SMALL_N) */
    shuffle(small_perm, SMALL_N);

    /* ── Warm up the CPU pipeline (avoids cold-start measurement noise) ─ */
    {
        volatile long long dummy = 0;
        for (int i = 0; i < 1000000; ++i) dummy += i;
        g_sink += dummy;
    }

    /* ── Run benchmarks ──────────────────────────────────────────────── */
    printf("Running benchmarks — please wait...\n\n");

    double ms1 = test_no_locality   (large_data, large_rand);
    double ms2 = test_temporal_only (small_data, small_perm);
    double ms3 = test_both_locality (small_data);

    /* ── Print results ───────────────────────────────────────────────── */
    printf("-----------------------------------------------------------------\n");
    printf("  %-42s  %9s  %9s\n", "Test", "Time (ms)", "Speedup");
    printf("-----------------------------------------------------------------\n");
    printf("  %-42s  %9.2f  %9s\n",
           "1. No temporal,  No spatial locality",
           ms1, "1.00x");
    printf("  %-42s  %9.2f  %8.2fx\n",
           "2. Temporal only (no spatial locality)",
           ms2, ms1 / ms2);
    printf("  %-42s  %9.2f  %8.2fx\n",
           "3. Both temporal & spatial locality",
           ms3, ms1 / ms3);
    printf("-----------------------------------------------------------------\n\n");

    /* ── Detailed explanation ────────────────────────────────────────── */
    printf("Explanation\n");
    printf("-----------\n\n");

    printf("Test 1 — No temporal, No spatial locality\n");
    printf("  Working set : 64 MB (exceeds all cache levels)\n");
    printf("  Pattern     : %d random accesses, single pass\n", TOTAL_ACCESSES);
    printf("  Cache lines : 64 bytes loaded per miss, only 4 bytes used (16x waste)\n");
    printf("  Result      : nearly every access is a main-memory miss (~100 ns each)\n\n");

    printf("Test 2 — Temporal locality only (no spatial locality)\n");
    printf("  Working set : 256 KB (fits in L2 cache)\n");
    printf("  Pattern     : random permutation repeated %d times\n", PASSES);
    printf("  Pass 1      : cold misses load the working set into L2/L3 cache\n");
    printf("  Passes 2-%d : same locations hit cache (temporal locality)\n", PASSES);
    printf("  Prefetcher  : cannot predict random addresses — no spatial benefit\n");
    printf("  Result      : much faster than Test 1; slower than Test 3\n\n");

    printf("Test 3 — Both temporal and spatial locality\n");
    printf("  Working set : 256 KB (fits in L2 cache)\n");
    printf("  Pattern     : sequential scan repeated %d times\n", PASSES);
    printf("  Spatial     : hardware prefetcher loads the next cache line early\n");
    printf("  Temporal    : warm working set served from L1/L2 on every pass\n");
    printf("  Result      : fastest — theoretical best case for cache utilisation\n\n");

    printf("Summary of cache latencies (approximate)\n");
    printf("  L1 cache hit  :   ~1–4   ns\n");
    printf("  L2 cache hit  :   ~5–12  ns\n");
    printf("  L3 cache hit  :  ~20–40  ns\n");
    printf("  Main memory   : ~60–100  ns\n\n");

    /* ── Cleanup ─────────────────────────────────────────────────────── */
    free(large_data);
    free(small_data);
    free(large_rand);
    free(small_perm);

    return EXIT_SUCCESS;
}
