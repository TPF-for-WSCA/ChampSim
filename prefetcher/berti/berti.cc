#include "berti.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include "cache.h"
using namespace std;
struct berti::impl {
  berti* owner{};
  uint64_t cycle() const { return static_cast<uint64_t>(owner->intern_->current_time.time_since_epoch() / owner->intern_->clock_period); }
  bool queue_available() const { return owner->intern_->get_pq_occupancy().back() < owner->intern_->PQ_SIZE; }
  bool prefetch_line(uint64_t ip, uint64_t, uint64_t addr, int, uint32_t metadata) {
    return owner->intern_->prefetch_line(champsim::address{addr}, true, metadata, champsim::address{ip});
  }
  static constexpr int FILL_L1 = 1;
#include "algorithm.inc"
};
berti::berti(CACHE* cache) : prefetcher(cache), state(std::make_unique<impl>()) { state->owner = this; }
berti::~berti() = default;
berti::berti(berti&& other) noexcept : prefetcher(other.intern_), state(std::move(other.state)) { if (state) state->owner = this; }
berti& berti::operator=(berti&& other) noexcept {
  intern_ = other.intern_; state = std::move(other.state); if (state) state->owner = this; return *this;
}
void berti::prefetcher_initialize() {
  if (BLOCK_SIZE != 64 || PAGE_SIZE != 4096 || !intern_->virtual_prefetch)
    throw std::invalid_argument("Berti requires 64-byte blocks, 4-KiB pages, and virtual_prefetch=true");
  state->initialize();
}
uint32_t berti::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t hit, bool, access_type type, uint32_t metadata) {
  if (type == access_type::LOAD || type == access_type::RFO)
    state->operate(addr.to<uint64_t>(), ip.to<uint64_t>(), hit, type == access_type::LOAD ? 0 : 1, 0);
  return metadata;
}
uint32_t berti::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t pf, champsim::address evicted, uint32_t metadata) {
  state->cache_fill(addr.to<uint64_t>(), addr.to<uint64_t>(), static_cast<uint32_t>(set), static_cast<uint32_t>(way), pf,
                    evicted.to<uint64_t>(), evicted.to<uint64_t>(), metadata);
  return metadata;
}
void berti::prefetcher_final_stats() { state->final_stats(); }
