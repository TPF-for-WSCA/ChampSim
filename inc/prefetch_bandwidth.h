#ifndef CHAMPSIM_PREFETCH_BANDWIDTH_H
#define CHAMPSIM_PREFETCH_BANDWIDTH_H
#include <cstdint>
namespace champsim::prefetch_bandwidth {
// Simulator-wide DRAM telemetry, sampled by system-aware prefetchers. The
// memory controller resets it at initialization; warmup bypasses are excluded.
inline uint64_t read_enqueue_bytes = 0;
inline double peak_bytes_per_picosecond = 0;
}
#endif
