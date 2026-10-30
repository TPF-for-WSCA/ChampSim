#ifndef CHAMPSIM_PREFETCHER_BERTI_H
#define CHAMPSIM_PREFETCHER_BERTI_H
#include <memory>
#include "modules.h"
struct berti : champsim::modules::prefetcher {
  struct impl;
  std::unique_ptr<impl> state;
  explicit berti(CACHE* cache);
  ~berti();
  berti(berti&&) noexcept;
  berti& operator=(berti&&) noexcept;
  void prefetcher_initialize();
  uint32_t prefetcher_cache_operate(champsim::address, champsim::address, uint8_t, bool, access_type, uint32_t);
  uint32_t prefetcher_cache_fill(champsim::address, long, long, uint8_t, champsim::address, uint32_t);
  void prefetcher_final_stats();
};
#endif
