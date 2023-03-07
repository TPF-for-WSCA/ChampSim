#ifndef CACHE_H
#define CACHE_H

#include <filesystem>
#include <fstream>
#include <functional>
#include <list>
#include <string>
#include <vector>

#include "champsim.h"
#include "delay_queue.hpp"
#include "memory_class.h"
#include "ooo_cpu.h"
#include "operable.h"

// virtual address space prefetching
#define VA_PREFETCH_TRANSLATION_LATENCY 2
#define WRITE_BUFFER_SIZE 10000

extern std::array<O3_CPU*, NUM_CPUS> ooo_cpu;
extern std::string result_dir;

/// @brief Set bits (lower, upper) to 1 in the mask. lower and upper are included. Zero indexed
/// @param mask The mask that will be modified
/// @param lower Inclusive lower bound for region
/// @param upper Inclusive upper bound for region
void set_accessed(uint64_t* mask, uint8_t lower, uint8_t upper);

/// @brief Extract start and end offsets for each block and hole. It always
///        starts and ends with a block, separated by a hole. (B H B H B), thus
///        the total count of the vector is always odd.
/// @param mask The bitmask indicating which bytes have been accessed.
/// @return Vector of alternatin block and hole start and end offsets
std::vector<std::pair<uint8_t, uint8_t>> get_blockboundaries_from_mask(const uint64_t& mask);

void record_cacheline_accesses(PACKET& handle_pkt, BLOCK& hit_block);

class CACHE : public champsim::operable, public MemoryRequestConsumer, public MemoryRequestProducer
{

private:
  std::ofstream cl_accessmask_file;

public:
  uint32_t cpu;
  const std::string NAME;
  const uint32_t NUM_SET, NUM_WAY, WQ_SIZE, RQ_SIZE, PQ_SIZE, MSHR_SIZE;
  const uint32_t HIT_LATENCY, FILL_LATENCY, OFFSET_BITS;
  std::vector<BLOCK> block{NUM_SET * NUM_WAY};
  std::vector<uint64_t> cl_accessmask_buffer;
  const uint32_t MAX_READ, MAX_WRITE;
  uint32_t reads_available_this_cycle, writes_available_this_cycle;
  const bool prefetch_as_load;
  const bool match_offset_bits;
  const bool virtual_prefetch;
  bool ever_seen_data = false;
  const unsigned pref_activate_mask = (1 << static_cast<int>(LOAD)) | (1 << static_cast<int>(PREFETCH));

  void write_buffers_to_disk(void);
  // prefetch stats
  uint64_t pf_requested = 0, pf_issued = 0, pf_useful = 0, pf_useless = 0, pf_fill = 0;

  // queues
  champsim::delay_queue<PACKET> RQ{RQ_SIZE, HIT_LATENCY}, // read queue
      PQ{PQ_SIZE, HIT_LATENCY},                           // prefetch queue
      VAPQ{PQ_SIZE, VA_PREFETCH_TRANSLATION_LATENCY},     // virtual address prefetch queue
      WQ{WQ_SIZE, HIT_LATENCY};                           // write queue

  std::list<PACKET> MSHR; // MSHR

  uint64_t sim_access[NUM_CPUS][NUM_TYPES] = {}, sim_hit[NUM_CPUS][NUM_TYPES] = {}, sim_miss[NUM_CPUS][NUM_TYPES] = {}, roi_access[NUM_CPUS][NUM_TYPES] = {},
           roi_hit[NUM_CPUS][NUM_TYPES] = {}, roi_miss[NUM_CPUS][NUM_TYPES] = {}, holecount_hist[NUM_CPUS][BLOCK_SIZE] = {},
           holesize_hist[NUM_CPUS][BLOCK_SIZE] = {}, cl_bytesaccessed_hist[NUM_CPUS][BLOCK_SIZE] = {}, blsize_hist[NUM_CPUS][BLOCK_SIZE] = {},
           blsize_ignore_holes_hist[NUM_CPUS][BLOCK_SIZE] = {};

  uint64_t RQ_ACCESS = 0, RQ_MERGED = 0, RQ_FULL = 0, RQ_TO_CACHE = 0, PQ_ACCESS = 0, PQ_MERGED = 0, PQ_FULL = 0, PQ_TO_CACHE = 0, WQ_ACCESS = 0, WQ_MERGED = 0,
           WQ_FULL = 0, WQ_FORWARD = 0, WQ_TO_CACHE = 0;

  uint64_t total_miss_latency = 0;

  // functions
  int add_rq(PACKET* packet) override;
  int add_wq(PACKET* packet) override;
  int add_pq(PACKET* packet) override;

  void return_data(PACKET* packet) override;
  void operate() override;
  void operate_writes();
  void operate_reads();

  uint32_t get_occupancy(uint8_t queue_type, uint64_t address) override;
  uint32_t get_size(uint8_t queue_type, uint64_t address) override;

  uint32_t get_set(uint64_t address);
  uint32_t get_way(uint64_t address, uint32_t set);

  int invalidate_entry(uint64_t inval_addr);
  int prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata);
  int prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata); // deprecated

  void add_mshr(PACKET* packet);
  void va_translate_prefetches();

  virtual void handle_fill();
  virtual void handle_writeback();
  virtual void handle_read();
  virtual void handle_prefetch();

  void readlike_hit(std::size_t set, std::size_t way, PACKET& handle_pkt);
  bool readlike_miss(PACKET& handle_pkt);
  virtual bool filllike_miss(std::size_t set, std::size_t way, PACKET& handle_pkt);

  bool should_activate_prefetcher(int type);

  void print_deadlock() override;

#include "cache_modules.inc"

  const repl_t repl_type;
  const pref_t pref_type;

  // constructor
  CACHE(std::string v1, double freq_scale, unsigned fill_level, uint32_t v2, int v3, uint32_t v5, uint32_t v6, uint32_t v7, uint32_t v8, uint32_t hit_lat,
        uint32_t fill_lat, uint32_t max_read, uint32_t max_write, std::size_t offset_bits, bool pref_load, bool wq_full_addr, bool va_pref,
        unsigned pref_act_mask, MemoryRequestConsumer* ll, pref_t pref, repl_t repl)
      : champsim::operable(freq_scale), MemoryRequestConsumer(fill_level), MemoryRequestProducer(ll), NAME(v1), NUM_SET(v2), NUM_WAY(v3), WQ_SIZE(v5),
        RQ_SIZE(v6), PQ_SIZE(v7), MSHR_SIZE(v8), HIT_LATENCY(hit_lat), FILL_LATENCY(fill_lat), OFFSET_BITS(offset_bits), MAX_READ(max_read),
        MAX_WRITE(max_write), prefetch_as_load(pref_load), match_offset_bits(wq_full_addr), virtual_prefetch(va_pref), pref_activate_mask(pref_act_mask),
        repl_type(repl), pref_type(pref)
  {
    if (0 == NAME.compare(NAME.length() - 3, 3, "L1I")) {
      cl_accessmask_buffer.reserve(WRITE_BUFFER_SIZE);
    }
  }
  ~CACHE()
  {
    if (cl_accessmask_file.is_open())
      cl_accessmask_file.close();
  };
};

class VCL_CACHE : public CACHE
{
public:
  VCL_CACHE(std::string v1, double freq_scale, unsigned fill_level, uint32_t v2, int v3, uint8_t* way_sizes, uint32_t v5, uint32_t v6, uint32_t v7, uint32_t v8,
            uint32_t hit_lat, uint32_t fill_lat, uint32_t max_read, uint32_t max_write, std::size_t offset_bits, bool pref_load, bool wq_full_addr,
            bool va_pref, unsigned pref_act_mask, MemoryRequestConsumer* ll, pref_t pref, repl_t repl)
      : CACHE(v1, freq_scale, fill_level, v2, v3, v5, v6, v7, v8, hit_lat, fill_lat, max_read, max_write, offset_bits, pref_load, wq_full_addr, va_pref,
              pref_act_mask, ll, pref, repl)
  {
    for (ulong i = 0; i < NUM_SET * NUM_WAY; ++i) {
      block[i].size = way_sizes[i % NUM_WAY];
    }
  }
  // virtual int add_rq(PACKET* packet) override;
  // virtual int add_wq(PACKET* packet) override;
  // virtual int add_pq(PACKET* packet) override;

  virtual void handle_fill() override;
  virtual void handle_read() override;
  // virtual void handle_prefetch() override;
  virtual bool filllike_miss(std::size_t set, std::size_t way, PACKET& handle_pkt) override;
  virtual void handle_writeback() override;
  uint32_t lru_victim(BLOCK* current_set, uint8_t min_size);
  virtual ~VCL_CACHE(){};
  bool hit_check(uint32_t& set, uint32_t& way, uint64_t& address, uint64_t& size);
};

#endif
