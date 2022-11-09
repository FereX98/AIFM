#include "profiler.hpp"

#ifdef ENABLE_PROFILER
struct overhead_profiler profilers[NUM_OVERHEAD_TYPES];
#endif