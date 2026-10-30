#include "pythia.h"
#include <algorithm>
#include <stdexcept>
#include "cache.h"
#include "prefetch_bandwidth.h"
#include "upstream/scooby.hpp"

struct pythia::impl {
  pythia_impl::Scooby engine{"pythia"};
  uint64_t last_bytes = champsim::prefetch_bandwidth::read_enqueue_bytes;
  champsim::chrono::clock::time_point last_sample{};
  uint64_t ticks = 0;
  uint64_t smoothed_bytes = 0;
  uint64_t useful = 0, fills = 0;
  unsigned accuracy = 0;
};
pythia::pythia(CACHE* cache) : prefetcher(cache) {}
pythia::~pythia() = default;
pythia::pythia(pythia&&) noexcept = default;
pythia& pythia::operator=(pythia&&) noexcept = default;
void pythia::prefetcher_initialize() {
  if (BLOCK_SIZE != 64 || PAGE_SIZE != 4096)
    throw std::invalid_argument("Pythia requires 64-byte blocks and 4-KiB pages");
  state = std::make_unique<impl>();
  state->last_sample = intern_->current_time;
}
uint32_t pythia::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t hit, bool useful, access_type type, uint32_t metadata) {
  if (type == access_type::PREFETCH) {
    if (hit) state->engine.register_prefetch_hit(addr.to<uint64_t>());
  }
  if (type != access_type::LOAD && type != access_type::RFO && type != access_type::PREFETCH) return metadata;
  if (useful) {
    ++state->useful;
  }
  std::vector<uint64_t> predictions;
  state->engine.invoke_prefetcher(ip.to<uint64_t>(), addr.to<uint64_t>(), hit, static_cast<uint8_t>(type), predictions);
  // As in the artifact's multi.l2c_pref, all predictions target this cache;
  // the normal PQ accepts/rejects them. Training tracks predictions, not just
  // successfully enqueued requests, matching the original integration.
  for (auto prediction : predictions) intern_->prefetch_line(champsim::address{prediction}, true, 0, ip);
  return metadata;
}
uint32_t pythia::prefetcher_cache_fill(champsim::address addr, long, long, uint8_t pf, champsim::address, uint32_t metadata) {
  state->engine.register_fill(addr.to<uint64_t>());
  if (pf) ++state->fills;
  return metadata;
}
void pythia::prefetcher_cycle_operate() {
  if (++state->ticks % 256 == 0) {
    const auto bytes = champsim::prefetch_bandwidth::read_enqueue_bytes;
    const auto elapsed = (intern_->current_time - state->last_sample).count();
    const auto peak = champsim::prefetch_bandwidth::peak_bytes_per_picosecond;
    const auto delta = bytes >= state->last_bytes ? bytes - state->last_bytes : 0;
    state->smoothed_bytes = state->smoothed_bytes / 2 + delta;
    const double utilization = elapsed > 0 && peak > 0
        ? static_cast<double>(state->smoothed_bytes) / (static_cast<double>(elapsed) * peak) : 0;
    const uint8_t level = utilization <= .25 ? 0 : utilization <= .50 ? 1 : utilization <= .75 ? 2 : 3;
    state->engine.update_bw(level);
    state->last_bytes = bytes;
    state->last_sample = intern_->current_time;
  }
  if (state->ticks % 1024 == 0) {
    const unsigned epoch_accuracy = state->fills ? static_cast<unsigned>(std::min(uint64_t{100}, 100 * state->useful / state->fills)) : 0;
    state->accuracy = (state->accuracy + epoch_accuracy) / 2;
    state->engine.update_acc(std::min(9u, state->accuracy / 10));
    state->useful = state->fills = 0;
  }
}
void pythia::prefetcher_final_stats() { state->engine.dump_stats(); }
