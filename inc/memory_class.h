#ifndef MEMORY_CLASS_H
#define MEMORY_CLASS_H

#include <limits>

#include "block.h"

// CACHE ACCESS TYPE
#define LOAD 0
#define RFO 1
#define PREFETCH 2
#define WRITEBACK 3
#define TRANSLATION 4
#define FILL 5
#define NUM_TYPES 6

// CACHE BLOCK
class BLOCK
{
public:
  bool valid = false, prefetch = false, dirty = false, trace = false, dead = false;

  uint16_t signature = 0;

  uint8_t size = 64, offset = 0;

  uint64_t last_modified_access = 0, bytes_accessed_in_predictor[4] = {0};
  uint64_t address = 0, v_address = 0, tag = 0, data = 0, ip = 0, cpu = 0, instr_id = 0, bytes_accessed = 0, prev_present = 0, accesses = 0,
           old_bytes_accessed = 0;
  uint32_t accesses_per_bytes[64] = {0}, time_present = 0;

  // Last byte of likely non-returning branches set to true
  // TODO: If block ending does not end at block-ending branch: probe BTB for next block ending branch
  // On average: 4 probes until we find branch, not guaranteed to be non-returning branch
  bool block_ending_branches[64] = {false};

  // replacement state
  uint32_t lru = std::numeric_limits<uint32_t>::max() >> 1;
  uint32_t max_time = 0;
};

extern std::ostream& operator<<(std::ostream& s, const BLOCK& p);

class MemoryRequestConsumer
{
public:
  /*
   * add_*q() return values:
   *
   * -2 : queue full
   * -1 : packet value forwarded, returned
   * 0  : packet merged
   * >0 : new queue occupancy
   *
   */

  const unsigned fill_level;
  virtual int add_rq(PACKET* packet) = 0;
  virtual int add_wq(PACKET* packet) = 0;
  virtual int add_pq(PACKET* packet) = 0;
  virtual uint32_t get_occupancy(uint8_t queue_type, uint64_t address) = 0;
  virtual uint32_t get_size(uint8_t queue_type, uint64_t address) = 0;

  explicit MemoryRequestConsumer(unsigned fill_level) : fill_level(fill_level) {}
};

class MemoryRequestProducer
{
public:
  MemoryRequestConsumer* lower_level;
  virtual void return_data(PACKET* packet) = 0;

protected:
  MemoryRequestProducer() {}
  explicit MemoryRequestProducer(MemoryRequestConsumer* ll) : lower_level(ll) {}
};

#endif
