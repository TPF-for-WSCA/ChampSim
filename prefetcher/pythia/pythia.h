#ifndef CHAMPSIM_PREFETCHER_PYTHIA_H
#define CHAMPSIM_PREFETCHER_PYTHIA_H
#include <memory>
#include "modules.h"
struct pythia : champsim::modules::prefetcher {
  struct impl;
  std::unique_ptr<impl> state;
  explicit pythia(CACHE* cache);
  ~pythia();
  pythia(pythia&&) noexcept;
  pythia& operator=(pythia&&) noexcept;
  void prefetcher_initialize();
  uint32_t prefetcher_cache_operate(champsim::address, champsim::address, uint8_t, bool, access_type, uint32_t);
  uint32_t prefetcher_cache_fill(champsim::address, long, long, uint8_t, champsim::address, uint32_t);
  void prefetcher_cycle_operate();
  void prefetcher_final_stats();
};
#endif
