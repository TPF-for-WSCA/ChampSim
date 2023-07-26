#ifndef CACHE_H
#define CACHE_H

#include <filesystem>
#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "champsim.h"
#include "circular_buffer.hpp"
#include "delay_queue.hpp"
#include "memory_class.h"
#include "ooo_cpu.h"
#include "operable.h"

// virtual address space prefetching
#define VA_PREFETCH_TRANSLATION_LATENCY 2
#define WRITE_BUFFER_SIZE 10

typedef unsigned long ulong;
typedef std::pair<uint8_t, uint8_t> SUBSET;

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

void record_cacheline_accesses(PACKET& handle_pkt, BLOCK& hit_block, BLOCK& prev_block);

enum class CountBlockMethod { EVICTION, SUM_ACCESSES };
enum class BufferHistory { NONE, PARTIAL, FULL };
enum LruModifier {
  DEFAULT = 0,
  PRECISE = 1,
  BOUND2 = 2,
  BOUND3 = 3,
  BOUND4 = 4,
  LRU1DEFAULT = 21,
  LRU1BOUND2 = 20,
  LRU1BOUND3 = 30,
  LRU1BOUND4 = 40,
  LRU2DEFAULT = 201,
  LRU2BOUND2 = 200,
  LRU2BOUND3 = 300,
  LRU2BOUND4 = 400,
  LRU3DEFAULT = 2001,
  LRU3BOUND2 = 2000,
  LRU3BOUND3 = 3000,
  LRU3BOUND4 = 4000,
  LRU4DEFAULT = 20001,
  LRU4BOUND2 = 20000,
  LRU4BOUND3 = 30000,
  LRU4BOUND4 = 40000
};
enum BufferOrganisation { FULLY_ASSOCIATIVE = 0, DIRECT_MAPPED = 1, SET2_ASSOCIATIVE = 2, SET4_ASSOCIATIVE = 4, SET8_ASSOCIATIVE = 8 };

bool is_default_lru(LruModifier);
class CACHE : public champsim::operable, public MemoryRequestConsumer, public MemoryRequestProducer
{

private:
  std::ofstream cl_accessmask_file;
  std::ofstream cl_num_blocks_in_cache;
  std::ofstream cl_num_invalid_blocks_in_cache;
  std::ofstream cl_num_accesses_to_complete_profile_file;
  CountBlockMethod count_method;

protected:
  uint64_t* way_hits;
  BLOCK* prev_access = NULL;
  uint8_t get_insert_pos(LruModifier lru_modifier);

public:
  LruModifier lru_modifier = LruModifier::DEFAULT;
  SUBSET lru_subset{0, 0};
  bool buffer = false;
  uint32_t cpu;
  const std::string NAME;
  const uint32_t NUM_SET, NUM_WAY, WQ_SIZE, RQ_SIZE, PQ_SIZE, MSHR_SIZE;
  uint32_t HIT_LATENCY, FILL_LATENCY, OFFSET_BITS;
  std::vector<BLOCK> block{NUM_SET * NUM_WAY};
  std::vector<uint64_t> cl_accessmask_buffer;
  std::vector<uint64_t> cl_blocks_in_cache_buffer;
  std::vector<uint32_t> cl_invalid_blocks_in_cache_buffer;
  std::vector<uint64_t> cl_accesses_percentage_of_presence_covered;
  std::vector<uint64_t> cl_num_accesses_to_complete_profile_buffer;
  const uint32_t MAX_READ, MAX_WRITE;
  uint32_t reads_available_this_cycle, writes_available_this_cycle, num_blocks_in_cache, num_invalid_blocks_in_cache;
  const bool prefetch_as_load;
  const bool match_offset_bits;
  const bool virtual_prefetch;
  bool ever_seen_data = false;
  const unsigned pref_activate_mask = (1 << static_cast<int>(LOAD)) | (1 << static_cast<int>(PREFETCH));

  virtual void write_buffers_to_disk(void);
  // prefetch stats
  uint64_t pf_requested = 0, pf_issued = 0, pf_useful = 0, pf_useless = 0, pf_fill = 0;

  // queues
  champsim::delay_queue<PACKET> RQ{RQ_SIZE, HIT_LATENCY}, // read queue
      PQ{PQ_SIZE, HIT_LATENCY},                           // prefetch queue
      VAPQ{PQ_SIZE, VA_PREFETCH_TRANSLATION_LATENCY},     // virtual address prefetch queue
      WQ{WQ_SIZE, HIT_LATENCY};                           // write queue

  std::list<PACKET> MSHR; // MSHR

  uint64_t sim_access[NUM_CPUS][NUM_TYPES] = {}, sim_hit[NUM_CPUS][NUM_TYPES] = {}, sim_miss[NUM_CPUS][NUM_TYPES] = {},
           sim_partial_miss[NUM_CPUS][NUM_TYPES] = {}, roi_access[NUM_CPUS][NUM_TYPES] = {}, roi_hit[NUM_CPUS][NUM_TYPES] = {},
           roi_miss[NUM_CPUS][NUM_TYPES] = {}, roi_partial_miss[NUM_CPUS][NUM_TYPES] = {}, holecount_hist[NUM_CPUS][BLOCK_SIZE] = {},
           holesize_hist[NUM_CPUS][BLOCK_SIZE] = {}, cl_bytesaccessed_hist[NUM_CPUS][BLOCK_SIZE + 1] = {},
           cl_bytesaccessed_hist_accesses[NUM_CPUS][BLOCK_SIZE + 1] = {}, blsize_hist[NUM_CPUS][BLOCK_SIZE] = {},
           blsize_hist_accesses[NUM_CPUS][BLOCK_SIZE] = {}, blsize_ignore_holes_hist[NUM_CPUS][BLOCK_SIZE] = {},
           accesses_before_eviction[NUM_CPUS][BUCKET_SIZE] = {};

  uint64_t RQ_ACCESS = 0, RQ_MERGED = 0, RQ_FULL = 0, RQ_TO_CACHE = 0, PQ_ACCESS = 0, PQ_MERGED = 0, PQ_FULL = 0, PQ_TO_CACHE = 0, WQ_ACCESS = 0, WQ_MERGED = 0,
           WQ_FULL = 0, WQ_FORWARD = 0, WQ_TO_CACHE = 0, USELESS_CACHELINE = 0, TOTAL_CACHELINES = 0;

  uint8_t perfect_cache = 0;

  uint64_t total_miss_latency = 0;

  // functions
  virtual int add_rq(PACKET* packet) override;
  virtual int add_wq(PACKET* packet) override;
  virtual int add_pq(PACKET* packet) override;

  void return_data(PACKET* packet) override;
  virtual void operate() override;
  virtual void operate_writes();
  void operate_reads();

  uint32_t get_occupancy(uint8_t queue_type, uint64_t address) override;
  uint32_t get_size(uint8_t queue_type, uint64_t address) override;

  uint32_t get_tag(uint64_t address);
  uint32_t get_set(uint64_t address);
  virtual uint32_t get_way(PACKET& packet, uint32_t set);
  virtual std::vector<uint32_t> get_way(uint32_t tag, uint32_t set);

  // int invalidate_entry(uint64_t inval_addr);  // NOTE: As of this commit no-one used this function - needs to be adjusted to use a PACKET instead of a
  // uint64_t to support vcl cache
  int prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata);
  int prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata); // deprecated

  void add_mshr(PACKET* packet);
  void va_translate_prefetches();

  virtual void handle_fill();
  virtual void handle_writeback();
  virtual void handle_read();
  void record_remainder_cachelines(uint32_t cpu);
  virtual void handle_prefetch();
  virtual void record_block_insert_removal(int set, int way, uint64_t newtag, bool warmup_completed);

  void record_cacheline_stats(uint32_t cpu, BLOCK& handle_block);
  virtual void record_overlap(void){};

  virtual void readlike_hit(std::size_t set, std::size_t way, PACKET& handle_pkt);
  virtual bool readlike_miss(PACKET& handle_pkt);
  virtual bool filllike_miss(std::size_t set, std::size_t way, PACKET& handle_pkt);

  bool should_activate_prefetcher(int type);

  void print_deadlock() override;

  virtual void print_private_stats(void);

#include "cache_modules.inc"

  const repl_t repl_type;
  const pref_t pref_type;

  // constructor
  CACHE(std::string v1, double freq_scale, unsigned fill_level, uint32_t v2, int v3, uint8_t perfect_cache, uint32_t v5, uint32_t v6, uint32_t v7, uint32_t v8,
        uint32_t hit_lat, uint32_t fill_lat, uint32_t max_read, uint32_t max_write, std::size_t offset_bits, bool pref_load, bool wq_full_addr, bool va_pref,
        unsigned pref_act_mask, MemoryRequestConsumer* ll, pref_t pref, repl_t repl)
      : champsim::operable(freq_scale), MemoryRequestConsumer(fill_level), MemoryRequestProducer(ll), NAME(v1), NUM_SET(v2), NUM_WAY(v3),
        perfect_cache(perfect_cache), WQ_SIZE(v5), RQ_SIZE(v6), PQ_SIZE(v7), MSHR_SIZE(v8), HIT_LATENCY(hit_lat), FILL_LATENCY(fill_lat),
        OFFSET_BITS(offset_bits), MAX_READ(max_read), MAX_WRITE(max_write), prefetch_as_load(pref_load), match_offset_bits(wq_full_addr),
        virtual_prefetch(va_pref), pref_activate_mask(pref_act_mask), repl_type(repl), pref_type(pref), count_method(CountBlockMethod::EVICTION)
  {
    if (0 == NAME.compare(NAME.length() - 3, 3, "L1I")) {
      cl_accessmask_buffer.reserve(WRITE_BUFFER_SIZE);
      cl_num_accesses_to_complete_profile_buffer.reserve(WRITE_BUFFER_SIZE);
    }
    cl_accesses_percentage_of_presence_covered.resize(100);
    num_invalid_blocks_in_cache = NUM_SET * NUM_WAY;
    cl_blocks_in_cache_buffer.reserve(WRITE_BUFFER_SIZE);
    cl_invalid_blocks_in_cache_buffer.reserve(WRITE_BUFFER_SIZE);
    way_hits = (uint64_t*)malloc(NUM_WAY * sizeof(uint64_t));
    if (way_hits == NULL)
      std::cerr << "COULD NOT ALLOCATE WAY_HIT ARRAY" << std::endl;
  }
  CACHE(std::string v1, double freq_scale, unsigned fill_level, uint32_t v2, int v3, uint8_t perfect_cache, uint32_t v5, uint32_t v6, uint32_t v7, uint32_t v8,
        uint32_t hit_lat, uint32_t fill_lat, uint32_t max_read, uint32_t max_write, std::size_t offset_bits, bool pref_load, bool wq_full_addr, bool va_pref,
        unsigned pref_act_mask, MemoryRequestConsumer* ll, pref_t pref, repl_t repl, CountBlockMethod method, LruModifier lru_modifier)
      : champsim::operable(freq_scale), MemoryRequestConsumer(fill_level), MemoryRequestProducer(ll), NAME(v1), NUM_SET(v2), NUM_WAY(v3),
        perfect_cache(perfect_cache), WQ_SIZE(v5), RQ_SIZE(v6), PQ_SIZE(v7), MSHR_SIZE(v8), HIT_LATENCY(hit_lat), FILL_LATENCY(fill_lat),
        OFFSET_BITS(offset_bits), MAX_READ(max_read), MAX_WRITE(max_write), prefetch_as_load(pref_load), match_offset_bits(wq_full_addr),
        virtual_prefetch(va_pref), pref_activate_mask(pref_act_mask), repl_type(repl), pref_type(pref), count_method(method), lru_modifier(lru_modifier)
  {
    if (0 == NAME.compare(NAME.length() - 3, 3, "L1I")) {
      cl_accessmask_buffer.reserve(WRITE_BUFFER_SIZE);
    }
    cl_accesses_percentage_of_presence_covered.resize(100);
    cl_blocks_in_cache_buffer.reserve(WRITE_BUFFER_SIZE);
    cl_invalid_blocks_in_cache_buffer.reserve(WRITE_BUFFER_SIZE);
    way_hits = (uint64_t*)malloc(NUM_WAY * sizeof(uint64_t));
    if (way_hits == NULL)
      std::cerr << "COULD NOT ALLOCATE WAY_HIT ARRAY" << std::endl;
  }
  ~CACHE()
  {
    free((void*)way_hits);
    if (cl_accessmask_file.is_open())
      cl_accessmask_file.close();
    if (cl_num_accesses_to_complete_profile_file.is_open())
      cl_num_accesses_to_complete_profile_file.close();
  };
};
class VCL_CACHE; // forward declaration so we can pass parent VCL cache to the Buffer Cache
class BUFFER_CACHE : public CACHE
{
protected:
  void insert_merge(BLOCK b);
  bool fifo;
  template <typename U>
  using buffer_t = champsim::circular_buffer<U>;
  void (BUFFER_CACHE::*replacement_update_state)(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type,
                                                 uint8_t hit);
  uint32_t (BUFFER_CACHE::*replacement_find_victim)(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr,
                                                    uint32_t type);
  uint32_t underruns = 0, overruns = 0, newblock = 0, mergeblocks = 0;
  std::vector<uint32_t> underrun_bytes_histogram;
  std::vector<uint32_t> overrun_bytes_histogram;
  std::vector<uint32_t> newblock_bytes_histogram;
  std::vector<uint32_t> mergeblock_bytes_histogram;

public:
  buffer_t<BLOCK> merge_block{MAX_WRITE};
  uint64_t total_accesses;
  uint64_t hits;
  uint64_t merge_hit;
  BufferHistory history;
  std::map<uint32_t, uint32_t> duration_in_buffer;

  BUFFER_CACHE(std::string v1, double freq_scale, unsigned fill_level, uint32_t v2, int v3, uint8_t perfect_cache, uint32_t v5, uint32_t v6, uint32_t v7,
               uint32_t v8, uint32_t hit_lat, uint32_t fill_lat, uint32_t max_read, uint32_t max_write, std::size_t offset_bits, bool pref_load,
               bool wq_full_addr, bool va_pref, unsigned pref_act_mask, MemoryRequestConsumer* ll, pref_t pref, repl_t repl, CountBlockMethod method,
               bool FIFO = false, BufferHistory history = BufferHistory::FULL)
      : CACHE(v1, freq_scale, fill_level, v2, v3, 0, v5, v6, v7, v8, hit_lat, fill_lat, max_read, max_write, offset_bits, pref_load, wq_full_addr, va_pref,
              pref_act_mask, ll, pref, repl, method, lru_modifier),
        fifo(FIFO), underrun_bytes_histogram(64), overrun_bytes_histogram(64), newblock_bytes_histogram(64), mergeblock_bytes_histogram(64), history(history)
  {
    replacement_update_state = &BUFFER_CACHE::update_replacement_state;
    replacement_find_victim = &BUFFER_CACHE::find_victim;

    merge_hit = 0;
  };

  ~BUFFER_CACHE(){};

  void initialize_replacement();

  void update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit);
  uint32_t find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type);
  void replacement_final_stats();

  /// @brief Check if packet is in cache. If it is, rv is the block, if not it is null. If hit, accessed bytes and statistics will be recorded
  /// @param packet The request packet to be handled
  /// @return The block if it is found.
  BLOCK* probe_buffer(PACKET& packet);
  void record_duration(BLOCK& block);
  void update_duration(void);
  void print_private_stats(void) override;

  BLOCK* __attribute__((optimize("O0"))) probe_merge(PACKET& packet);

  /// @brief Insert a serviced read from lower level. If an entry is evicted, it is entered into the merge register
  /// @param packet The packet to be inserting
  /// @return Returns true if successful and false if not. Reasons for being not successful could be full merge register;
  bool fill_miss(PACKET& packet, CACHE& parent);
};

class VCL_CACHE : public CACHE
{

protected:
  bool aligned = false; // should the blocks be aligned to the way size?
  uint8_t* way_sizes;
  uint32_t buffer_sets = 0;
  uint32_t buffer_ways = 0;
  BufferOrganisation organisation;

public:
  BUFFER_CACHE buffer_cache;
  VCL_CACHE(std::string v1, double freq_scale, unsigned fill_level, uint32_t v2, int v3, uint8_t* way_sizes, bool buffer, uint32_t buffer_sets,
            bool buffer_fifo, bool aligned, uint32_t v5, uint32_t v6, uint32_t v7, uint32_t v8, uint32_t hit_lat, uint32_t fill_lat, uint32_t max_read,
            uint32_t max_write, std::size_t offset_bits, bool pref_load, bool wq_full_addr, bool va_pref, unsigned pref_act_mask, MemoryRequestConsumer* ll,
            pref_t pref, repl_t repl, BufferOrganisation buffer_organisation, LruModifier lru_modifier, CountBlockMethod method, BufferHistory history)
      : CACHE(v1, freq_scale, fill_level, v2, v3, 0, v5, v6, v7, v8, hit_lat, fill_lat, max_read, max_write, offset_bits, pref_load, wq_full_addr, va_pref,
              pref_act_mask, ll, pref, repl, method, lru_modifier),
        aligned(aligned), buffer_sets(buffer_sets), way_sizes(way_sizes), organisation(buffer_organisation),
        buffer_cache(BUFFER_CACHE(
            (v1 + "_buffer"), freq_scale, fill_level, (buffer_organisation == BufferOrganisation::FULLY_ASSOCIATIVE) ? 1 : buffer_sets / buffer_organisation,
            (buffer_organisation == BufferOrganisation::FULLY_ASSOCIATIVE) ? buffer_sets : buffer_organisation, 0, std::min(buffer_sets, v5),
            std::min(v6, buffer_sets), std::min(buffer_sets, v7), std::min(v8, buffer_sets), 0, 0, max_read, max_write / 2, offset_bits, false, true, false, 0,
            ll, pref_t::CPU_REDIRECT_pprefetcherDno_instr_, repl_t::rreplacementDlru, method, buffer_fifo, history))
  {
    CACHE::buffer = buffer;
    for (ulong i = 0; i < NUM_SET * NUM_WAY; ++i) {
      block[i].size = way_sizes[i % NUM_WAY];
    }
    if ((buffer_sets % NUM_SET != 0 && NUM_SET % buffer_sets != 0) && organisation == BufferOrganisation::DIRECT_MAPPED) {
      std::cerr << "can't directmap if num sets and buffer size are not divisible in either direction" << std::endl;
      assert(0);
    }
    if (buffer) {
      uint32_t buffer_hit_latency = organisation == BufferOrganisation::DIRECT_MAPPED ? 0 : 1;
      buffer_cache.HIT_LATENCY = buffer_hit_latency;
    }
    CACHE::lru_modifier = lru_modifier;
    overlap_bytes_history.reserve(WRITE_BUFFER_SIZE);
    current_overlap = 0;
  }
  virtual int add_rq(PACKET* packet) override;
  virtual int add_wq(PACKET* packet) override;
  virtual int add_pq(PACKET* packet) override;
  // void write_buffers_to_disk(void) override;
  // virtual void record_overlap(void) override;

  virtual void operate() override;
  virtual void operate_writes() override;
  virtual void operate_buffer_evictions();

  virtual void handle_fill() override;
  virtual void handle_read() override;
  // virtual void handle_prefetch() override;
  // virtual bool readlike_miss(PACKET& handle_pkt) override;
  void buffer_hit(BLOCK& b, PACKET& handle_pkt);
  virtual bool filllike_miss(std::size_t set, std::size_t way, PACKET& handle_pkt) override;
  virtual bool filllike_miss(std::size_t set, std::size_t way, size_t offset, BLOCK& handle_block);
  virtual void handle_writeback() override;
  // virtual void return_data(PACKET* packet) override;
  uint32_t lru_victim(BLOCK* current_set, uint8_t min_size);
  uint32_t get_way(PACKET& packet, uint32_t set);
  /// @brief Get all ways that match the tag of packet
  /// @param tag The tag to look up
  /// @param set The set that we search for the tag
  /// @return Vector of way indexes
  std::vector<uint32_t> get_way(uint32_t tag, uint32_t set);
  BLOCK* probe_buffer(PACKET& packet, uint32_t set);
  void handle_fill_from_buffer(BLOCK& b, uint32_t set);

  uint8_t hit_check(uint32_t& set, uint32_t& way, uint64_t& address, uint64_t& size);

private:
  std::vector<uint32_t> overlap_bytes_history;
  uint32_t current_overlap;
};

class AMOEBA_CACHE : public CACHE
{
public:
  BUFFER_CACHE buffer_cache;
  typedef std::vector<BLOCK> SET;
  BLOCK perfect_cache_block;
  std::vector<SET> storage_array;
  std::vector<uint16_t> freespace_per_set;
  // TODO: reuse some functions from VCL as template functions
  AMOEBA_CACHE(std::string v1, double freq_scale, unsigned fill_level, uint32_t v2, int v3, bool buffer, uint32_t buffer_sets, bool buffer_fifo, bool aligned,
               uint32_t v5, uint32_t v6, uint32_t v7, uint32_t v8, uint32_t hit_lat, uint32_t fill_lat, uint32_t max_read, uint32_t max_write,
               std::size_t offset_bits, bool pref_load, bool wq_full_addr, bool va_pref, unsigned pref_act_mask, MemoryRequestConsumer* ll, pref_t pref,
               repl_t repl, BufferOrganisation buffer_organisation, LruModifier lru_modifier, CountBlockMethod method, BufferHistory history)
      : CACHE(v1, freq_scale, fill_level, v2, v3, 0, v5, v6, v7, v8, hit_lat, fill_lat, max_read, max_write, offset_bits, pref_load, wq_full_addr, va_pref,
              pref_act_mask, ll, pref, repl, method, lru_modifier),
        freespace_per_set(v2, 512) // TODO: set size needs to be calculated depending on number of ways and block size
  {
    // TODO: If buffer -> our buffer, else: Default Amoeba Cache predictor (probably separate class)
    if (perfect_cache) {
      perfect_cache_block = BLOCK();
    }
  }

  // we cant pass ways around, we always need to pass the block ref due to the variability of the cache
  BLOCK* get_block(PACKET& packet, uint32_t set);
  // set and tag should stay relatively constant, we can store offsets as given.
  virtual int add_rq(PACKET* packet) override;
  virtual int add_wq(PACKET* packet) override;
  virtual int add_pq(PACKET* packet) override;

  virtual void operate_writes() override;

  void operate_buffer_evictions();

  virtual void handle_fill() override;
  virtual void handle_read() override;

  virtual void readlike_hit(std::size_t set, BLOCK& b, PACKET& handle_pkt);
  virtual bool filllike_miss(std::size_t set, std::size_t way, PACKET& handle_pkt) override;
  virtual bool filllike_miss(std::size_t set, std::size_t way, size_t offset, BLOCK& handle_block);

  uint32_t lru_victim(BLOCK* current_set, uint8_t min_size);
  uint8_t hit_check(uint32_t& set, uint32_t& way, uint64_t& address, uint64_t& size);

  void initialize_replacement();

  void update_replacement_state(uint32_t set, uint32_t replaced_lru, BLOCK* inserted);
  // returns the index in the current set
  uint32_t find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const SET* current_set, uint64_t ip, uint64_t full_addr, uint32_t type);
};

#endif
