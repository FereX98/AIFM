#pragma once
#include <atomic>
#include <cstdio>

//#define ENABLE_PROFILER

struct overhead_profiler {
    std::atomic<uint64_t> accumulated_cycles;
    std::atomic<uint64_t> accumulated_count;
};

enum overhead_profiler_type {
    FASTPATH,
	BARRIER_SWAP_IN,
	BARRIER_MIGRATION,
	SWAP_IN_PREP,
	SWAP_IN_READ,
	SWAP_IN_INIT,
	SERVER_DF_VECTOR_READ,
	SERVER_PTR_READ,
    NUM_OVERHEAD_TYPES
};

#ifdef ENABLE_PROFILER
// time utils
// reference cycles.
// #1, Fix the clock cycles of CPU.
// #2, Divided by CPU frequency to calculate the wall time.
// 500 cycles/ 4.0GHz * 10^9 ns = 500/4.0 ns = xx ns.
// Use "__asm__" in header files (".h") and "asm" in source files (".c")
static inline uint64_t get_cycles_start(void)
{
	uint32_t cycles_high, cycles_low;
	__asm__ __volatile__("xorl %%eax, %%eax\n\t"
			     "CPUID\n\t"
			     "RDTSC\n\t"
			     "mov %%edx, %0\n\t"
			     "mov %%eax, %1\n\t"
			     : "=r"(cycles_high), "=r"(cycles_low)::"%rax",
			       "%rbx", "%rcx", "%rdx");
	return ((uint64_t)cycles_high << 32) + (uint64_t)cycles_low;
}

// More strict than get_cycles_start since "RDTSCP; read registers; CPUID"
// gurantee all instructions before are executed and all instructions after
// are not speculativly executed
// Refer to https://www.intel.com/content/dam/www/public/us/en/documents/white-papers/ia-32-ia-64-benchmark-code-execution-paper.pdf
static inline uint64_t get_cycles_end(void)
{
	uint32_t cycles_high, cycles_low;
	__asm__ __volatile__("RDTSCP\n\t"
			     "mov %%edx, %0\n\t"
			     "mov %%eax, %1\n\t"
			     "xorl %%eax, %%eax\n\t"
			     "CPUID\n\t"
			     : "=r"(cycles_high), "=r"(cycles_low)::"%rax",
			       "%rbx", "%rcx", "%rdx");
	return ((uint64_t)cycles_high << 32) + (uint64_t)cycles_low;
}

extern struct overhead_profiler profilers[NUM_OVERHEAD_TYPES];

static inline void reset_profilers(void)
{
    for (int i = 0; i < NUM_OVERHEAD_TYPES; i++) {
        profilers[i].accumulated_cycles = 0;
        profilers[i].accumulated_count = 0;
    }
}

static inline void record_overhead(enum overhead_profiler_type type, uint64_t cycles)
{
    profilers[type].accumulated_cycles += cycles;
    profilers[type].accumulated_count++;
}

static inline void record_counter(enum overhead_profiler_type type)
{
    profilers[type].accumulated_count++;
}

static inline void report_stats(void)
{
    for (int i = 0; i < NUM_OVERHEAD_TYPES; i++) {
        std::printf("Profiler %d: %lu cycles, %lu count\n", i, profilers[i].accumulated_cycles.load(), profilers[i].accumulated_count.load());
    }
}

static inline void report_on_count(enum overhead_profiler_type type, uint64_t times)
{
    if (profilers[type].accumulated_count % times == 0) {
        report_stats();
        std::fflush(stdout);
    }
}
#else // ENABLE_PROFILER
static inline uint64_t get_cycles_start(void)
{
	return 0;
}
static inline uint64_t get_cycles_end(void)
{
	return 0;
}
static inline void reset_profilers(void)
{
}

static inline void record_overhead(enum overhead_profiler_type type, uint64_t cycles)
{
}

static inline void record_counter(enum overhead_profiler_type type)
{
}

static inline void report_stats(void)
{
}

static inline void report_on_count(enum overhead_profiler_type type, uint64_t times)
{
}
#endif // ENABLE_PROFILER