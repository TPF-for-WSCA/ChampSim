#include "cache.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <set>
#include <string.h>

#include "champsim.h"
#include "champsim_constants.h"
#include "util.h"
#include "vmem.h"

#ifndef SANITY_CHECK
#define NDEBUG
#endif

extern VirtualMemory vmem;
extern uint8_t warmup_complete[NUM_CPUS];
extern uint8_t knob_stall_on_miss;

uint8_t get_lru_offset(LruModifier lru_modifier)
{
  if (lru_modifier >= 10000)
    return lru_modifier / 10000;
  else if (lru_modifier >= 1000)
    return lru_modifier / 1000;
  else if (lru_modifier >= 100)
    return lru_modifier / 100;
  else if (lru_modifier >= 10)
    return lru_modifier / 10;
  return lru_modifier;
}

bool is_default_lru(LruModifier lru_modifier)
{
  if (lru_modifier == DEFAULT or (lru_modifier > 10 and lru_modifier % 10 == 1))
    return true;
  return false;
}

void set_accessed(uint64_t* mask, uint8_t lower, uint8_t upper)
{
  if (upper > 63) {
    upper = 63;
  }

  uint64_t bitmask = 1;
  uint8_t i = 0;
  for (; i < lower; i++) {
    bitmask = bitmask << 1;
  }

  for (; i <= upper; i++) {
    (*mask) |= bitmask;
    bitmask = bitmask << 1;
  }
}

std::vector<std::pair<uint8_t, uint8_t>> get_blockboundaries_from_mask(const uint64_t& mask)
{
  typedef std::pair<uint8_t, uint8_t> Block;
  std::vector<Block> result;
  uint8_t prev_bit = 0, current_bit = 0;
  bool trailing = true; // assume line starts with not accessed bytes

  Block current;
  for (uint8_t byte = 0; byte < BLOCK_SIZE; byte++) {
    current_bit = ((mask >> byte) & 0x1);

    if (current_bit && trailing) {
      trailing = false;
      prev_bit = current_bit;
      current.first = byte;
      continue;
    } else if (current_bit == 0 && trailing) {
      continue;
    }

    if (current_bit == 0 && prev_bit == 1) {
      current.second = byte - 1;
      result.push_back(current);
      current = Block();
      current.first = byte;
    } else if (current_bit == 1 && prev_bit == 0) {
      current.second = byte - 1;
      result.push_back(current);
      current = Block();
      current.first = byte;
    }
    prev_bit = current_bit;
  }

  if (prev_bit == 1 && current_bit == 1) {
    // we have a block that ends at the 63 byte / we need t record that block
    current.second = 63;
    result.push_back(current);
  }
  return result;
}

void record_cacheline_accesses(PACKET& handle_pkt, BLOCK& hit_block, BLOCK& prev_block)
{
  if (handle_pkt.size != 0) {
    // vaddr and ip should be the same for L1I, but lookup happens on address so we also operate on address
    // assert(handle_pkt.address % BLOCK_SIZE == handle_pkt.v_address % BLOCK_SIZE); // not true in case of overlapping blocks
    uint8_t offset = (uint8_t)(handle_pkt.v_address % BLOCK_SIZE);
    uint8_t end = offset + handle_pkt.size - 1;
    set_accessed(&hit_block.bytes_accessed, offset, end);
    if (&prev_block != &hit_block)
      hit_block.accesses++;
    for (int i = offset; i <= end; i++) {
      if (!hit_block.accesses_per_bytes[i])
        hit_block.last_modified_access = hit_block.accesses;
      hit_block.accesses_per_bytes[i]++;
    }
  }
}

void CACHE::handle_fill()
{

  while (writes_available_this_cycle > 0) {
    auto fill_mshr = MSHR.begin();
    if (fill_mshr == std::end(MSHR) || fill_mshr->event_cycle > current_cycle)
      return;

    // VCL Impl: Immediately insert into buffer, search for address if evicted buffer entry has been used
    // find victim
    uint32_t set = get_set(fill_mshr->address);

    auto set_begin = std::next(std::begin(block), set * NUM_WAY);
    auto set_end = std::next(set_begin, NUM_WAY);
    auto first_inv = std::find_if_not(set_begin, set_end, is_valid<BLOCK>());
    uint32_t way = std::distance(set_begin, first_inv);
    if (way == NUM_WAY) {
      way = impl_replacement_find_victim(fill_mshr->cpu, fill_mshr->instr_id, set, &block.data()[set * NUM_WAY], fill_mshr->ip, fill_mshr->address,
                                         fill_mshr->type);
      // TODO: RECORD STATISTICS IF ANY
    }

    bool success = filllike_miss(set, way, *fill_mshr);
    if (!success)
      return;

    if (way != NUM_WAY) {
      // update processed packets
      fill_mshr->data = block[set * NUM_WAY + way].data;

      for (auto ret : fill_mshr->to_return)
        ret->return_data(&(*fill_mshr));
    }

    MSHR.erase(fill_mshr);
    writes_available_this_cycle--;
  }
}

void CACHE::handle_writeback()
{
  while (writes_available_this_cycle > 0) {
    if (!WQ.has_ready())
      return;

    // handle the oldest entry
    PACKET& handle_pkt = WQ.front();

    // access cache
    uint32_t set = get_set(handle_pkt.address);
    uint32_t way = get_way(handle_pkt, set);

    BLOCK& fill_block = block[set * NUM_WAY + way];

    if (way < NUM_WAY || perfect_cache) // HIT
    {
      impl_replacement_update_state(handle_pkt.cpu, set, way, fill_block.address, handle_pkt.ip, 0, handle_pkt.type, 1);

      // COLLECT STATS
      sim_hit[handle_pkt.cpu][handle_pkt.type]++;
      sim_access[handle_pkt.cpu][handle_pkt.type]++;

      // mark dirty
      fill_block.dirty = 1;
      fill_block.accesses++;
    } else // MISS
    {
      bool success;
      if (handle_pkt.type == RFO && handle_pkt.to_return.empty()) {
        success = readlike_miss(handle_pkt);
      } else {
        // find victim
        auto set_begin = std::next(std::begin(block), set * NUM_WAY);
        auto set_end = std::next(set_begin, NUM_WAY);
        auto first_inv = std::find_if_not(set_begin, set_end, is_valid<BLOCK>());
        way = std::distance(set_begin, first_inv);
        if (way == NUM_WAY)
          way = impl_replacement_find_victim(handle_pkt.cpu, handle_pkt.instr_id, set, &block.data()[set * NUM_WAY], handle_pkt.ip, handle_pkt.address,
                                             handle_pkt.type);
        // theory TODO: If we are applying VCL/test things on lower level caches we
        // should implement statistics writing here as well.
        success = filllike_miss(set, way, handle_pkt);
      }

      if (!success)
        return;
    }

    // remove this entry from WQ
    writes_available_this_cycle--;
    WQ.pop_front();
  }
}

void CACHE::handle_read()
{
  while (reads_available_this_cycle > 0) {

    if (!RQ.has_ready())
      return;

    // handle the oldest entry
    PACKET& handle_pkt = RQ.front();

    // A (hopefully temporary) hack to know whether to send the evicted paddr or
    // vaddr to the prefetcher
    ever_seen_data |= (handle_pkt.v_address != handle_pkt.ip);

    uint32_t set = get_set(handle_pkt.address);
    uint32_t way = get_way(handle_pkt, set);

    if (way < NUM_WAY || perfect_cache) // HIT
    {
      way_hits[way]++;
      readlike_hit(set, way, handle_pkt);
    } else {
      bool success = readlike_miss(handle_pkt);

      if (knob_stall_on_miss && 0 == NAME.compare(NAME.length() - 3, 3, "L1I")) {
        ooo_cpu[handle_pkt.cpu]->stall_on_miss = 1;
        reads_available_this_cycle = 1;
      }
      if (!success) {
        reads_available_this_cycle = 0;
        return;
      }
    }

    // remove this entry from RQ
    RQ.pop_front();
    reads_available_this_cycle--;
  }
}

void CACHE::handle_prefetch()
{
  while (reads_available_this_cycle > 0) {
    if (!PQ.has_ready())
      return;

    // handle the oldest entry
    PACKET& handle_pkt = PQ.front();

    uint32_t set = get_set(handle_pkt.address);
    uint32_t way = get_way(handle_pkt, set);

    if (way < NUM_WAY || perfect_cache) // HIT
    {
      readlike_hit(set, way, handle_pkt);
    } else {
      bool success = readlike_miss(handle_pkt);
      if (!success)
        return;
    }

    // remove this entry from PQ
    PQ.pop_front();
    reads_available_this_cycle--;
  }
}

void CACHE::readlike_hit(std::size_t set, std::size_t way, PACKET& handle_pkt)
{
  DP(if (warmup_complete[handle_pkt.cpu]) {
    std::cout << "[" << NAME << "] " << __func__ << " hit";
    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << (handle_pkt.address >> OFFSET_BITS);
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
    std::cout << " type: " << +handle_pkt.type;
    std::cout << " cycle: " << current_cycle << std::endl;
  });

  if (perfect_cache)
    way = 0; // we attribute everything to the first way to ensure no out-of-bounds
  BLOCK& hit_block = block[set * NUM_WAY + way];

  record_cacheline_accesses(handle_pkt, hit_block, *prev_access);
  handle_pkt.data = hit_block.data;

  // update prefetcher on load instruction
  if (should_activate_prefetcher(handle_pkt.type) && handle_pkt.pf_origin_level < fill_level) {
    cpu = handle_pkt.cpu;
    uint64_t pf_base_addr = (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
    handle_pkt.pf_metadata = impl_prefetcher_cache_operate(pf_base_addr, handle_pkt.ip, 1, handle_pkt.type, handle_pkt.pf_metadata);
  }

  // update replacement policy
  impl_replacement_update_state(handle_pkt.cpu, set, way, hit_block.address, handle_pkt.ip, 0, handle_pkt.type, 1);

  // COLLECT STATS
  sim_hit[handle_pkt.cpu][handle_pkt.type]++;
  sim_access[handle_pkt.cpu][handle_pkt.type]++;

  for (auto ret : handle_pkt.to_return)
    ret->return_data(&handle_pkt);

  // update prefetch stats and reset prefetch bit
  if (hit_block.prefetch) {
    pf_useful++;
    hit_block.prefetch = 0;
  }
  prev_access = &hit_block;
}

bool CACHE::readlike_miss(PACKET& handle_pkt)
{
  DP(if (warmup_complete[handle_pkt.cpu]) {
    std::cout << "[" << NAME << "] " << __func__ << " miss";
    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << (handle_pkt.address >> OFFSET_BITS);
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
    std::cout << " type: " << +handle_pkt.type;
    std::cout << " cycle: " << current_cycle << std::endl;
  });

  // check mshr
  auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(handle_pkt.address, OFFSET_BITS));
  bool mshr_full = (MSHR.size() == MSHR_SIZE);

  if (mshr_entry != MSHR.end()) // miss already inflight
  {
    // update fill location
    mshr_entry->fill_level = std::min(mshr_entry->fill_level, handle_pkt.fill_level);

    packet_dep_merge(mshr_entry->lq_index_depend_on_me, handle_pkt.lq_index_depend_on_me);
    packet_dep_merge(mshr_entry->sq_index_depend_on_me, handle_pkt.sq_index_depend_on_me);
    packet_dep_merge(mshr_entry->instr_depend_on_me, handle_pkt.instr_depend_on_me);
    packet_dep_merge(mshr_entry->to_return, handle_pkt.to_return);

    if (handle_pkt.partial) {
      mshr_entry->partial = true;
    }

    if (mshr_entry->type == PREFETCH && handle_pkt.type != PREFETCH) {
      // Mark the prefetch as useful
      if (mshr_entry->pf_origin_level == fill_level)
        pf_useful++;

      uint64_t prior_event_cycle = mshr_entry->event_cycle;
      *mshr_entry = handle_pkt;

      // in case request is already returned, we should keep event_cycle
      mshr_entry->event_cycle = prior_event_cycle;
    }
  } else {
    if (mshr_full)  // not enough MSHR resource
      return false; // TODO should we allow prefetches anyway if they will not
                    // be filled to this level?

    bool is_read = prefetch_as_load || (handle_pkt.type != PREFETCH);

    // check to make sure the lower level queue has room for this read miss
    int queue_type = (is_read) ? 1 : 3;
    if (lower_level->get_occupancy(queue_type, handle_pkt.address) == lower_level->get_size(queue_type, handle_pkt.address))
      return false;

    assert(handle_pkt.size <= 64);
    // Allocate an MSHR
    if (handle_pkt.fill_level <= fill_level) {
      auto it = MSHR.insert(std::end(MSHR), handle_pkt);
      it->cycle_enqueued = current_cycle;
      it->event_cycle = std::numeric_limits<uint64_t>::max();
    }

    if (handle_pkt.fill_level <= fill_level)
      handle_pkt.to_return = {this};
    else
      handle_pkt.to_return.clear();

    if (!is_read)
      lower_level->add_pq(&handle_pkt);
    else
      lower_level->add_rq(&handle_pkt);
  }

  // update prefetcher on load instructions and prefetches from upper levels
  if (should_activate_prefetcher(handle_pkt.type) && handle_pkt.pf_origin_level < fill_level) {
    cpu = handle_pkt.cpu;
    uint64_t pf_base_addr = (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
    handle_pkt.pf_metadata = impl_prefetcher_cache_operate(pf_base_addr, handle_pkt.ip, 0, handle_pkt.type, handle_pkt.pf_metadata);
  }

  return true;
}

void CACHE::record_block_insert_removal(int set, int way, uint64_t address, bool warmup_completed)
{
  BLOCK& repl_block = block[set * NUM_WAY + way];
  if (!repl_block.valid) {
    num_invalid_blocks_in_cache--;
  } else if (warmup_complete) {
    uint8_t covered_percentage = std::floor(100.0 * repl_block.last_modified_access / repl_block.accesses);
    cl_accesses_percentage_of_presence_covered[covered_percentage - 1]++;
    cl_num_accesses_to_complete_profile_buffer.push_back(repl_block.last_modified_access);
  }
  bool newtag_present = false;
  bool oldtag_present = false;
  uint64_t new_tag = (address >> (OFFSET_BITS));
  uint64_t old_tag = repl_block.address >> OFFSET_BITS;
  if (!repl_block.valid)
    oldtag_present = true; // unvalid block is "always" present
  for (int i = 0; i < NUM_WAY; i++) {
    if (i == way) {
      continue;
    }
    if (!block[set * NUM_WAY + i].valid) {
      continue;
    }
    uint64_t incache_tag = (block[set * NUM_WAY + i].address >> (OFFSET_BITS));
    if (incache_tag == new_tag) {
      newtag_present = true;
    }

    // We check for 0 as this is the only case where we should not deduct (invalid is handled as normal, as soon as the last invalid block of a specifice line
    // is filled we adjust)
    if (incache_tag == old_tag) {
      oldtag_present = true;
    }
    if (oldtag_present && newtag_present)
      break;
  }
  if (current_cycle % 10000 < 10) {
    std::set<uint64_t> addresses;
    for (auto& b : block) {
      if (b.valid) {
        addresses.insert((b.address >> OFFSET_BITS));
      }
    }
    assert(addresses.size() == num_blocks_in_cache);
  }
  int net_change = 0;
  if (!oldtag_present) {
    net_change--;
  }
  if (!newtag_present) {
    net_change++;
  }
  num_blocks_in_cache += net_change;
  if (cl_blocks_in_cache_buffer.size() >= WRITE_BUFFER_SIZE) {
    write_buffers_to_disk();
  }
  // assert(num_blocks_in_cache <= (NUM_WAY * NUM_SET));
  if (num_blocks_in_cache > NUM_WAY * NUM_SET) {
    std::set<uint64_t> addresses;
    for (auto const& b : block) {
      if (b.valid) {
        std::cout << b.address << std::endl;
        addresses.insert((b.address >> OFFSET_BITS));
      }
    }
    std::cout << "FOUND " << addresses.size() << " DISTINCT VALID CACHELINES" << std::endl;
    assert(0);
  }
  if (warmup_complete) {
    cl_blocks_in_cache_buffer.push_back(num_blocks_in_cache);
    cl_invalid_blocks_in_cache_buffer.push_back(num_invalid_blocks_in_cache);
  }
}

// TODO: Make more generic for all simple series
// TODO: ONLY TRACK WHEN WARMUP COMPLETE>>>
void CACHE::write_buffers_to_disk()
{
  if (cl_accessmask_buffer.size() == 0 && cl_blocks_in_cache_buffer.size() == 0 && cl_invalid_blocks_in_cache_buffer.size() == 0) {
    return;
  }
  if (!cl_accessmask_file.is_open()) {
    std::filesystem::path result_path = result_dir;
    string filename = this->NAME + "_cl_access_masks.bin";
    result_path /= filename;
    cl_accessmask_file = std::ofstream(result_path.c_str(), std::ios::binary | std::ios::out);
  }
  if (!cl_num_accesses_to_complete_profile_file.is_open()) {
    std::filesystem::path result_path = result_dir;
    string filename = this->NAME + "_cl_num_accesses_to_full_coverage.bin";
    result_path /= filename;
    cl_num_accesses_to_complete_profile_file = std::ofstream(result_path.c_str(), std::ios::binary | std::ios::out);
  }
  if (!cl_num_blocks_in_cache.is_open()) {
    std::filesystem::path result_path = result_dir;
    string filename = this->NAME + "_cl_num_blocks.bin";
    result_path /= filename;
    cl_num_blocks_in_cache = std::ofstream(result_path.c_str(), std::ios::binary | std::ios::out);
  }
  if (!cl_num_invalid_blocks_in_cache.is_open()) {
    std::filesystem::path result_path = result_dir;
    string filename = this->NAME + "_cl_num_invalid_blocks.bin";
    result_path /= filename;
    cl_num_invalid_blocks_in_cache = std::ofstream(result_path.c_str(), std::ios::binary | std::ios::out);
  }
  for (auto& mask : cl_accessmask_buffer) {
    cl_accessmask_file.write(reinterpret_cast<char*>(&mask), sizeof(uint64_t));
  }
  for (auto& access_count : cl_num_accesses_to_complete_profile_buffer) {
    cl_num_accesses_to_complete_profile_file.write(reinterpret_cast<char*>(&access_count), sizeof(uint64_t));
  }
  for (auto& num_blocks : cl_blocks_in_cache_buffer) {
    cl_num_blocks_in_cache.write(reinterpret_cast<char*>(&num_blocks), sizeof(uint64_t));
  }
  for (auto& num_blocks : cl_invalid_blocks_in_cache_buffer) {
    cl_num_invalid_blocks_in_cache.write(reinterpret_cast<char*>(&num_blocks), sizeof(uint32_t));
  }
  cl_accessmask_file.flush();
  cl_num_blocks_in_cache.flush();
  cl_num_invalid_blocks_in_cache.flush();
  cl_accessmask_buffer.clear();
  cl_num_accesses_to_complete_profile_buffer.clear();
  cl_blocks_in_cache_buffer.clear();
  cl_invalid_blocks_in_cache_buffer.clear();
}

void CACHE::record_remainder_cachelines(uint32_t cpu)
{
  if (0 != NAME.compare(NAME.length() - 3, 3, "L1I")) {
    return;
  }

  for (BLOCK b : block) {
    if (!b.valid) {
      continue;
    }
    record_cacheline_stats(cpu, b);
  }

  if (buffer) {
    VCL_CACHE* vc = static_cast<VCL_CACHE*>(this);
    for (BLOCK b : vc->buffer_cache.block) {
      if (!b.valid)
        continue;
      record_cacheline_stats(cpu, b);
    }
  }
}

void CACHE::record_cacheline_stats(uint32_t cpu, BLOCK& handle_block)
{
  if (!warmup_complete[cpu]) {
    return;
  }
  // we onlu write to the file if warmup is complete
  if (cl_accessmask_buffer.size() < WRITE_BUFFER_SIZE) {
    cl_accessmask_buffer.push_back(handle_block.bytes_accessed);
  } else {
    write_buffers_to_disk();
    cl_accessmask_buffer.push_back(handle_block.bytes_accessed);
  }
  auto hitblocks = get_blockboundaries_from_mask(handle_block.bytes_accessed);
  if (hitblocks.size() == 0) {
    // no access? what do we do?
  }
  holecount_hist[cpu][hitblocks.size() / 2]++;
  bool is_hole = false;
  uint8_t total_accessed = 0, first_accessed = 0, last_accessed = 0;
  for (size_t i = 0; i < hitblocks.size(); i++) {
    auto block = hitblocks[i];
    uint8_t size =
        block.second - block.first; // actual size is +1 as we have first and last index - this is useful for indexing with the size (entry 0 = length 1)
    assert(size >= 0 && size < 64);
    if (is_hole) {
      holesize_hist[cpu][size]++;
      is_hole = !is_hole;
      continue;
    }
    if (i == 0) {
      first_accessed = block.first;
    }
    last_accessed = block.second;
    total_accessed += (size + 1); // actual size
    // count how many accesses to this block

    uint64_t count = 0;
    for (int j = block.first; j < block.second; j++) {
      count += handle_block.accesses_per_bytes[j];
    }
    blsize_hist[cpu][size] += 1;
    blsize_hist_accesses[cpu][size] += count;
    is_hole = !is_hole; // alternating block/hole
  }
  cl_bytesaccessed_hist[cpu][total_accessed] += 1;
  cl_bytesaccessed_hist_accesses[cpu][total_accessed] += handle_block.accesses;
  blsize_ignore_holes_hist[cpu][last_accessed - first_accessed] += 1;
}

bool CACHE::filllike_miss(std::size_t set, std::size_t way, PACKET& handle_pkt)
{
  DP(if (warmup_complete[handle_pkt.cpu]) {
    std::cout << "[" << NAME << "] " << __func__ << " miss";
    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << (handle_pkt.address >> OFFSET_BITS);
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
    std::cout << " type: " << +handle_pkt.type;
    std::cout << " cycle: " << current_cycle << std::endl;
  });
  if (0 == NAME.compare(NAME.length() - 3, 3, "L1I")) {
    ooo_cpu[handle_pkt.cpu]->stall_on_miss = 0;
    record_block_insert_removal(set, way, handle_pkt.address, warmup_complete[handle_pkt.cpu]);
  }
  bool bypass = (way == NUM_WAY);
#ifndef LLC_BYPASS
  assert(!bypass);
#endif
  assert(handle_pkt.type != WRITEBACK || !bypass);
  way_hits[way]++;

  BLOCK& fill_block = block[set * NUM_WAY + way];

  // quick and dirty / mainly dirty: only apply if name ends in L1I
  if (0 == NAME.compare(NAME.length() - 3, 3, "L1I") && fill_block.valid)
    record_cacheline_stats(handle_pkt.cpu, fill_block);
  bool evicting_dirty = !bypass && (lower_level != NULL) && fill_block.dirty;
  uint64_t evicting_address = 0;

  if (fill_block.valid && fill_block.accesses == 0) {
    USELESS_CACHELINE++;
  }
  TOTAL_CACHELINES++;

  if (!bypass) {
    if (evicting_dirty) {
      PACKET writeback_packet;

      writeback_packet.fill_level = lower_level->fill_level;
      writeback_packet.cpu = handle_pkt.cpu;
      writeback_packet.address = fill_block.address;
      writeback_packet.data = fill_block.data;
      writeback_packet.instr_id = handle_pkt.instr_id;
      writeback_packet.ip = 0;
      writeback_packet.type = WRITEBACK;

      auto result = lower_level->add_wq(&writeback_packet);
      if (result == -2)
        return false;
    }

    if (ever_seen_data)
      evicting_address = fill_block.address & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
    else
      evicting_address = fill_block.v_address & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);

    if (fill_block.prefetch)
      pf_useless++;

    if (handle_pkt.type == PREFETCH)
      pf_fill++;

    fill_block.bytes_accessed = 0; // newly added to the cache thus no accesses yet
    memset(fill_block.accesses_per_bytes, 0, sizeof(fill_block.accesses_per_bytes));

    fill_block.valid = true;
    fill_block.prefetch = (handle_pkt.type == PREFETCH && handle_pkt.pf_origin_level == fill_level);
    fill_block.dirty = (handle_pkt.type == WRITEBACK || (handle_pkt.type == RFO && handle_pkt.to_return.empty()));
    fill_block.address = handle_pkt.address;
    fill_block.v_address = handle_pkt.v_address;
    fill_block.data = handle_pkt.data;
    fill_block.ip = handle_pkt.ip;
    fill_block.cpu = handle_pkt.cpu;
    fill_block.instr_id = handle_pkt.instr_id;
    fill_block.tag = get_tag(handle_pkt.address);
    fill_block.accesses = 0;
    fill_block.last_modified_access = 0;
    fill_block.time_present = 0;
    record_cacheline_accesses(handle_pkt, fill_block, *prev_access);
  }

  if (warmup_complete[handle_pkt.cpu] && (handle_pkt.cycle_enqueued != 0))
    total_miss_latency += current_cycle - handle_pkt.cycle_enqueued;

  // update prefetcher
  cpu = handle_pkt.cpu;
  handle_pkt.pf_metadata =
      impl_prefetcher_cache_fill((virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS), set, way,
                                 handle_pkt.type == PREFETCH, evicting_address, handle_pkt.pf_metadata);

  // update replacement policy
  impl_replacement_update_state(handle_pkt.cpu, set, way, handle_pkt.address, handle_pkt.ip, 0, FILL, 0);

  // COLLECT STATS
  sim_miss[handle_pkt.cpu][handle_pkt.type]++;
  sim_access[handle_pkt.cpu][handle_pkt.type]++;
  way_hits[way]++;

  prev_access = &fill_block;
  return true;
}

void CACHE::operate()
{
  operate_writes();
  operate_reads();

  record_overlap();

  impl_prefetcher_cycle_operate();
}

void CACHE::operate_writes()
{
  // perform all writes
  writes_available_this_cycle = MAX_WRITE;
  handle_fill();
  handle_writeback();

  WQ.operate();
}

void CACHE::operate_reads()
{
  // perform all reads
  reads_available_this_cycle = MAX_READ;
  handle_read();
  va_translate_prefetches();
  handle_prefetch();

  RQ.operate();
  PQ.operate();
  VAPQ.operate();
}

/// @brief Translate address to tag
/// @param address The adress to be translated
/// @return The tag of the cacheline (without offset and index bits)
uint32_t CACHE::get_tag(uint64_t address) { return ((address >> OFFSET_BITS)); }
uint32_t CACHE::get_set(uint64_t address) { return ((address >> OFFSET_BITS) & bitmask(lg2(NUM_SET))); }

uint32_t CACHE::get_way(PACKET& packet, uint32_t set)
{
  auto begin = std::next(block.begin(), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);
  return std::distance(begin, std::find_if(begin, end, eq_addr<BLOCK>(packet.address, OFFSET_BITS)));
}
// NOTE: As of this commit no-one used this function - needs to be adjusted to use a PACKET instead of a
// uint64_t to support vcl cache
// int CACHE::invalidate_entry(uint64_t inval_addr)
// {
//   uint32_t set = get_set(inval_addr);
//   uint32_t way = get_way(inval_addr, set);
//
//   if (way < NUM_WAY)
//     block[set * NUM_WAY + way].valid = 0;
//
//   return way;
// }

int CACHE::add_rq(PACKET* packet)
{
  assert(packet->address != 0);
  RQ_ACCESS++;

  DP(if (warmup_complete[packet->cpu]) {
    std::cout << "[" << NAME << "_RQ] " << __func__ << " instr_id: " << packet->instr_id << " address: " << std::hex << (packet->address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet->address << " v_address: " << packet->v_address << std::dec << " type: " << +packet->type
              << " occupancy: " << RQ.occupancy();
  })

  // check for the latest writebacks in the write queue
  champsim::delay_queue<PACKET>::iterator found_wq = std::find_if(WQ.begin(), WQ.end(), eq_addr<PACKET>(packet->address, match_offset_bits ? 0 : OFFSET_BITS));

  if (found_wq != WQ.end()) {

    DP(if (warmup_complete[packet->cpu]) std::cout << " MERGED_WQ" << std::endl;)

    packet->data = found_wq->data;
    for (auto ret : packet->to_return)
      ret->return_data(packet);

    WQ_FORWARD++;
    return -1;
  }

  // check for duplicates in the read queue
  if (0 != this->NAME.compare(this->NAME.length() - 3, 3, "L1I")) {

    auto found_rq = std::find_if(RQ.begin(), RQ.end(), eq_addr<PACKET>(packet->address, OFFSET_BITS));
    if (found_rq != RQ.end()) {

      DP(if (warmup_complete[packet->cpu]) std::cout << " MERGED_RQ" << std::endl;)

      packet_dep_merge(found_rq->lq_index_depend_on_me, packet->lq_index_depend_on_me);
      packet_dep_merge(found_rq->sq_index_depend_on_me, packet->sq_index_depend_on_me);
      packet_dep_merge(found_rq->instr_depend_on_me, packet->instr_depend_on_me);
      packet_dep_merge(found_rq->to_return, packet->to_return);

      RQ_MERGED++;

      return 0; // merged index
    }
  }

  // check occupancy
  if (RQ.full()) {
    RQ_FULL++;

    DP(if (warmup_complete[packet->cpu]) std::cout << " FULL" << std::endl;)

    return -2; // cannot handle this request
  }

  // if there is no duplicate, add it to RQ
  assert(packet->size < 64);
  if (warmup_complete[cpu])
    RQ.push_back(*packet);
  else
    RQ.push_back_ready(*packet);

  DP(if (warmup_complete[packet->cpu]) std::cout << " ADDED" << std::endl;)

  RQ_TO_CACHE++;
  return RQ.occupancy();
}

int CACHE::add_wq(PACKET* packet)
{
  WQ_ACCESS++;

  DP(if (warmup_complete[packet->cpu]) {
    std::cout << "[" << NAME << "_WQ] " << __func__ << " instr_id: " << packet->instr_id << " address: " << std::hex << (packet->address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet->address << " v_address: " << packet->v_address << std::dec << " type: " << +packet->type
              << " occupancy: " << RQ.occupancy();
  })

  // check for duplicates in the write queue
  champsim::delay_queue<PACKET>::iterator found_wq = std::find_if(WQ.begin(), WQ.end(), eq_addr<PACKET>(packet->address, match_offset_bits ? 0 : OFFSET_BITS));

  if (found_wq != WQ.end()) {

    DP(if (warmup_complete[packet->cpu]) std::cout << " MERGED" << std::endl;)

    WQ_MERGED++;
    return 0; // merged index
  }

  // Check for room in the queue
  if (WQ.full()) {
    DP(if (warmup_complete[packet->cpu]) std::cout << " FULL" << std::endl;)

    ++WQ_FULL;
    return -2;
  }

  // if there is no duplicate, add it to the write queue
  if (warmup_complete[cpu])
    WQ.push_back(*packet);
  else
    WQ.push_back_ready(*packet);

  DP(if (warmup_complete[packet->cpu]) std::cout << " ADDED" << std::endl;)

  WQ_TO_CACHE++;
  WQ_ACCESS++;

  return WQ.occupancy();
}

int CACHE::prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  pf_requested++;

  PACKET pf_packet;
  pf_packet.type = PREFETCH;
  pf_packet.fill_level = (fill_this_level ? fill_level : lower_level->fill_level);
  pf_packet.pf_origin_level = fill_level;
  pf_packet.pf_metadata = prefetch_metadata;
  pf_packet.cpu = cpu;
  pf_packet.address = pf_addr;
  pf_packet.v_address = virtual_prefetch ? pf_addr : 0;

  if (virtual_prefetch) {
    if (!VAPQ.full()) {
      VAPQ.push_back(pf_packet);
      return 1;
    }
  } else {
    int result = add_pq(&pf_packet);
    if (result != -2) {
      if (result > 0)
        pf_issued++;
      return 1;
    }
  }

  return 0;
}

int CACHE::prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  static bool deprecate_printed = false;
  if (!deprecate_printed) {
    std::cout << "WARNING: The extended signature CACHE::prefetch_line(ip, "
                 "base_addr, pf_addr, fill_this_level, prefetch_metadata) is "
                 "deprecated."
              << std::endl;
    std::cout << "WARNING: Use CACHE::prefetch_line(pf_addr, fill_this_level, "
                 "prefetch_metadata) instead."
              << std::endl;
    deprecate_printed = true;
  }
  return prefetch_line(pf_addr, fill_this_level, prefetch_metadata);
}

void CACHE::va_translate_prefetches()
{
  // TEMPORARY SOLUTION: mark prefetches as translated after a fixed latency
  if (VAPQ.has_ready()) {
    VAPQ.front().address = vmem.va_to_pa(cpu, VAPQ.front().v_address).first;

    // move the translated prefetch over to the regular PQ
    int result = add_pq(&VAPQ.front());

    // remove the prefetch from the VAPQ
    if (result != -2)
      VAPQ.pop_front();

    if (result > 0)
      pf_issued++;
  }
}

int CACHE::add_pq(PACKET* packet)
{
  assert(packet->address != 0);
  PQ_ACCESS++;

  DP(if (warmup_complete[packet->cpu]) {
    std::cout << "[" << NAME << "_WQ] " << __func__ << " instr_id: " << packet->instr_id << " address: " << std::hex << (packet->address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet->address << " v_address: " << packet->v_address << std::dec << " type: " << +packet->type
              << " occupancy: " << RQ.occupancy();
  })

  // check for the latest wirtebacks in the write queue
  champsim::delay_queue<PACKET>::iterator found_wq = std::find_if(WQ.begin(), WQ.end(), eq_addr<PACKET>(packet->address, match_offset_bits ? 0 : OFFSET_BITS));

  if (found_wq != WQ.end()) {

    DP(if (warmup_complete[packet->cpu]) std::cout << " MERGED_WQ" << std::endl;)

    packet->data = found_wq->data;
    for (auto ret : packet->to_return)
      ret->return_data(packet);

    WQ_FORWARD++;
    return -1;
  }

  // check for duplicates in the PQ
  auto found = std::find_if(PQ.begin(), PQ.end(), eq_addr<PACKET>(packet->address, OFFSET_BITS));
  if (found != PQ.end()) {
    DP(if (warmup_complete[packet->cpu]) std::cout << " MERGED_PQ" << std::endl;)

    found->fill_level = std::min(found->fill_level, packet->fill_level);
    packet_dep_merge(found->to_return, packet->to_return);

    PQ_MERGED++;
    return 0;
  }

  // check occupancy
  if (PQ.full()) {

    DP(if (warmup_complete[packet->cpu]) std::cout << " FULL" << std::endl;)

    PQ_FULL++;
    return -2; // cannot handle this request
  }

  // if there is no duplicate, add it to PQ
  if (warmup_complete[cpu])
    PQ.push_back(*packet);
  else
    PQ.push_back_ready(*packet);

  DP(if (warmup_complete[packet->cpu]) std::cout << " ADDED" << std::endl;)

  PQ_TO_CACHE++;
  return PQ.occupancy();
}

void CACHE::return_data(PACKET* packet)
{
  // check MSHR information
  auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(packet->address, OFFSET_BITS));
  auto first_unreturned = std::find_if(MSHR.begin(), MSHR.end(), [](auto x) { return x.event_cycle == std::numeric_limits<uint64_t>::max(); });

  // sanity check
  if (mshr_entry == MSHR.end()) {
    std::cerr << "[" << NAME << "_MSHR] " << __func__ << " instr_id: " << packet->instr_id << " cannot find a matching entry!";
    std::cerr << " address: " << std::hex << packet->address;
    std::cerr << " v_address: " << packet->v_address;
    std::cerr << " address: " << (packet->address >> OFFSET_BITS) << std::dec;
    std::cerr << " event: " << packet->event_cycle << " current: " << current_cycle << std::endl;
    assert(0);
  }

  // MSHR holds the most updated information about this request
  mshr_entry->data = packet->data;
  mshr_entry->pf_metadata = packet->pf_metadata;
  mshr_entry->event_cycle = current_cycle + (warmup_complete[cpu] ? FILL_LATENCY : 0);

  DP(if (warmup_complete[packet->cpu]) {
    std::cout << "[" << NAME << "_MSHR] " << __func__ << " instr_id: " << mshr_entry->instr_id;
    std::cout << " address: " << std::hex << (mshr_entry->address >> OFFSET_BITS) << " full_addr: " << mshr_entry->address;
    std::cout << " data: " << mshr_entry->data << std::dec;
    std::cout << " index: " << std::distance(MSHR.begin(), mshr_entry) << " occupancy: " << get_occupancy(0, 0);
    std::cout << " event: " << mshr_entry->event_cycle << " current: " << current_cycle << std::endl;
  });

  // Order this entry after previously-returned entries, but before non-returned
  // entries
  std::iter_swap(mshr_entry, first_unreturned);
}

uint32_t CACHE::get_occupancy(uint8_t queue_type, uint64_t address)
{
  if (queue_type == 0)
    return std::count_if(MSHR.begin(), MSHR.end(), is_valid<PACKET>());
  else if (queue_type == 1)
    return RQ.occupancy();
  else if (queue_type == 2)
    return WQ.occupancy();
  else if (queue_type == 3)
    return PQ.occupancy();

  return 0;
}

uint32_t CACHE::get_size(uint8_t queue_type, uint64_t address)
{
  if (queue_type == 0)
    return MSHR_SIZE;
  else if (queue_type == 1)
    return RQ.size();
  else if (queue_type == 2)
    return WQ.size();
  else if (queue_type == 3)
    return PQ.size();

  return 0;
}

bool CACHE::should_activate_prefetcher(int type) { return (1 << static_cast<int>(type)) & pref_activate_mask; }

void CACHE::print_deadlock()
{
  if (!std::empty(MSHR)) {
    std::cerr << NAME << " MSHR Entry" << std::endl;
    std::size_t j = 0;
    for (PACKET entry : MSHR) {
      std::cerr << "[" << NAME << " MSHR] entry: " << j++ << " instr_id: " << entry.instr_id;
      std::cerr << " address: " << std::hex << (entry.address >> LOG2_BLOCK_SIZE) << " full_addr: " << entry.address << std::dec << " type: " << +entry.type;
      std::cerr << " fill_level: " << +entry.fill_level << " event_cycle: " << entry.event_cycle << std::endl;
    }
  } else {
    std::cerr << NAME << " MSHR empty" << std::endl;
  }
}

void BUFFER_CACHE::initialize_replacement() {}

// find replacement victim
uint32_t BUFFER_CACHE::find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
  // baseline LRU
  auto max_elem = std::max_element(current_set, std::next(current_set, NUM_WAY), lru_comparator<BLOCK, BLOCK>());
  auto time_max = std::max_element(current_set, std::next(current_set, NUM_WAY), max_time_comparator<BLOCK, BLOCK>());
  if (!fifo && max_elem->valid && time_max->valid && time_max->max_time > 64) {
    max_elem = time_max;
  }
  uint32_t way = std::distance(current_set, max_elem);
  return way;
}

// called on every cache hit and cache fill
void BUFFER_CACHE::update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type,
                                            uint8_t hit)
{
  if (fifo && hit)
    return;

  auto begin = std::next(block.begin(), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);
  if (!fifo)
    if (hit && &block[set * NUM_WAY + way] != prev_access)
      std::next(begin, way)->lru++; // counting accesses
    else {
      std::next(begin, way)->lru = 1;
      std::next(begin, way)->max_time = 0;
    }
  else {
    uint32_t hit_lru = std::next(begin, way)->lru;
    std::for_each(begin, end, [hit_lru](BLOCK& x) {
      if (x.lru <= hit_lru)
        x.lru++;
    });
    std::next(begin, way)->lru = 0; // promote to the MRU position
    std::next(begin, way)->max_time = 0;
  }
  std::for_each(begin, end, [](BLOCK& x) { x.max_time++; });
}

void BUFFER_CACHE::replacement_final_stats() {}

// TODO: statistics
BLOCK* BUFFER_CACHE::probe_buffer(PACKET& packet)
{
  uint32_t set = get_set(packet.address);
  uint32_t way = get_way(packet, set);
  if (way == NUM_WAY) {
    return NULL;
  }
  BLOCK& hitb = block[set * NUM_WAY + way];
  if (!hitb.valid)
    return NULL;

  // COLLECT STATS
  sim_hit[packet.cpu][packet.type]++;
  sim_access[packet.cpu][packet.type]++;
  way_hits[way]++;

  record_cacheline_accesses(packet, hitb, *prev_access);
  (this->*replacement_update_state)(packet.cpu, set, way, packet.address, packet.ip, 0, packet.type, 1);
  prev_access = &hitb;
  return &hitb;
};

// TODO: Statistics
BLOCK* __attribute__((optimize("O0"))) BUFFER_CACHE::probe_merge(PACKET& packet)
{
  if (merge_block.empty()) {
    return NULL;
  }
  uint32_t tag = get_tag(packet.address);
  for (auto it = merge_block.rbegin(); it != merge_block.rend(); it++) {
    if (tag == get_tag(it->address)) {
      sim_hit[packet.cpu][packet.type]++;
      sim_access[packet.cpu][packet.type]++;
      merge_hit++;
      return &(*it);
    }
  }
}

void BUFFER_CACHE::print_private_stats()
{
  CACHE::print_private_stats();
  std::cout << "Time Spent in Buffer by #Cachelines:" << std::endl;
  uint64_t total_time = 0;
  uint64_t total_evictions = 0;
  for (const auto& [time, count] : duration_in_buffer) {
    std::cout << "\t" << std::setw(4) << time << ":\t" << std::setw(10) << count << std::endl;
    total_time += (time * count);
    total_evictions += count;
  }
  std::cout << "AVERAGE Time Spent in Buffer: ";
  std::cout << (total_time / total_evictions) << std::endl;
  std::cout << NAME << " PARTIAL MISSES";
  std::cout << "\tUNDERRUNS: " << std::setw(10) << underruns << "\tOVERRUNS: " << std::setw(10) << overruns << "\tMERGES: " << std::setw(10) << mergeblocks
            << "\tNEW BLOCKS: " << std::setw(10) << newblock << std::endl;
  std::cout << NAME << " UNDERRUN SIZE:" << std::endl;
  for (int i = 1; i < 65; i++) {
    std::cout << i << ":\t" << +underrun_bytes_histogram[i - 1] << std::endl;
  }

  std::cout << NAME << " OVERRUN SIZE:" << std::endl;
  for (int i = 1; i < 65; i++) {
    std::cout << i << ":\t" << +overrun_bytes_histogram[i - 1] << std::endl;
  }

  std::cout << NAME << " NEW BLOCKS SIZE:" << std::endl;
  for (int i = 1; i < 65; i++) {
    std::cout << i << ":\t" << +newblock_bytes_histogram[i - 1] << std::endl;
  }
}

void BUFFER_CACHE::record_duration(BLOCK& block) { duration_in_buffer[block.time_present] += 1; }
void BUFFER_CACHE::update_duration()
{
  for (BLOCK& b : block) {
    if (!b.valid)
      continue;
    b.time_present++;
  }
}

bool BUFFER_CACHE::fill_miss(PACKET& packet, VCL_CACHE& parent)
{

  ooo_cpu[packet.cpu]->stall_on_miss = 0;
  uint32_t set = get_set(packet.address);
  auto set_begin = std::next(std::begin(block), set * NUM_WAY);
  auto set_end = std::next(set_begin, NUM_WAY);
  auto first_inv = std::find_if_not(set_begin, set_end, is_valid<BLOCK>());
  uint32_t way = std::distance(set_begin, first_inv);
  if (way == NUM_WAY) {
    way = (this->*replacement_find_victim)(packet.cpu, packet.instr_id, set, &block.data()[set * NUM_WAY], packet.ip, packet.address, packet.type);
    // TODO: RECORD STATISTICS IF ANY
  }

  BLOCK& fill_block = block[set * NUM_WAY + way];
  if (fill_block.old_bytes_accessed > fill_block.bytes_accessed) {
    assert(0);
  }
  if (merge_block.full() && fill_block.valid) {
    return false;
  }
  if (fill_block.valid && fill_block.accesses == 0) {
    USELESS_CACHELINE++;
    assert(0);
  }
  TOTAL_CACHELINES++;
  (this->*replacement_update_state)(packet.cpu, set, way, packet.address, packet.ip, 0, packet.type, 0);

  if (fill_block.valid && fill_block.old_bytes_accessed > 0 && warmup_complete[packet.cpu]) {
    // Record over/under/new/combined
    uint8_t curr_bp = 0;
    uint8_t prev_new_byte = 0;
    uint8_t prev_old_byte = 0;
    bool overrun_active = false;
    bool new_block_active = false;
    uint8_t new_byte_count = 0;
    while (curr_bp < 64) {
      uint8_t new_byte = (fill_block.bytes_accessed >> curr_bp) & 0b1;
      uint8_t old_byte = (fill_block.old_bytes_accessed >> curr_bp) & 0b1;
      if (new_byte == 1 && old_byte == 0) {
        new_byte_count++;
        if (prev_old_byte == 1) {
          overrun_active = true;
        } else if (!overrun_active) {
          // we are not overrunning and encounter a previously not accessed byte so this must be either an underrun or a new block - we don't know yet
          new_block_active = true;
        }
        // partial miss
      } else if (new_byte_count > 0) {
        // anything to clean up?
        if (new_block_active && old_byte == 0) {
          newblock++;
          newblock_bytes_histogram[new_byte_count]++;
        } else if (new_block_active && old_byte == 1) {
          underruns++;
          underrun_bytes_histogram[new_byte_count]++;
        }
        if (overrun_active && old_byte == 1) {
          mergeblocks++;
          mergeblock_bytes_histogram[new_byte_count]++;
          overrun_active = false;
        } else if (overrun_active && old_byte == 0) {
          overruns++;
          overrun_bytes_histogram[new_byte_count]++;
          overrun_active = false;
        }

        new_byte_count = 0;
      }
      prev_new_byte = new_byte;
      prev_old_byte = old_byte;
      curr_bp++;
    }
  }

  if (fill_block.valid) {
    // Record stats
    record_duration(fill_block);
    merge_block.push_back(fill_block, true);
    record_cacheline_stats(packet.cpu, fill_block);
  }
  update_duration();
  fill_block.bytes_accessed = 0;
  memset(fill_block.accesses_per_bytes, 0, sizeof(fill_block.accesses_per_bytes));
  /////
  uint32_t parent_set = parent.get_set(packet.address); // set of the VCL cache, not the buffer
  uint32_t tag = parent.get_tag(packet.address);        // dito
  auto _in_cache = parent.get_way(tag, parent_set);
  // set all of them to invalid and insert again - not hardware like but easier in sw
  bool invalidated = false;
  for (uint32_t& way : _in_cache) {
    BLOCK& b = parent.block[parent_set * parent.NUM_WAY + way];
    if (!b.valid)
      continue;
    if (history == BufferHistory::FULL)
      set_accessed(&fill_block.bytes_accessed, b.offset, b.offset + b.size - 1); // we only mark them as accessed but not count accesses per bytes
    else if (history == BufferHistory::PARTIAL)
      set_accessed(&fill_block.prev_present, b.offset, b.offset + b.size - 1);
    b.valid = false;
    parent.num_invalid_blocks_in_cache++;

    invalidated = true;
  }
  parent.cl_invalid_blocks_in_cache_buffer.push_back(parent.num_invalid_blocks_in_cache);

  fill_block.old_bytes_accessed = 0;
  if (invalidated) {
    parent.num_blocks_in_cache--; // we just invalidated all blocks of that tag
    fill_block.old_bytes_accessed = fill_block.bytes_accessed;
  } else if (packet.partial) {
    std::cout << "Partial without hit in cache" << std::endl;
  }
  ////
  fill_block.valid = true;
  fill_block.prefetch = (packet.type == PREFETCH && packet.pf_origin_level == fill_level);
  fill_block.dirty = (packet.type == WRITEBACK || (packet.type == RFO && packet.to_return.empty()));
  fill_block.address = packet.address;
  fill_block.v_address = packet.v_address;
  fill_block.data = packet.data;
  fill_block.ip = packet.ip;
  fill_block.cpu = packet.cpu;
  fill_block.tag = parent.get_tag(packet.address);
  fill_block.instr_id = packet.instr_id;
  fill_block.accesses =
      (packet.type != LOAD) ? 0 : 1; // This is a demand load so num accesses is 1, as this wlil be accessed by the frontend as soon as it is filled
  fill_block.last_modified_access = 0;
  fill_block.time_present = 0;
  record_cacheline_accesses(packet, fill_block, *prev_access);
  prev_access = &fill_block;

  if (warmup_complete[packet.cpu] && (packet.cycle_enqueued != 0))
    total_miss_latency += current_cycle - packet.cycle_enqueued;
  sim_miss[packet.cpu][packet.type]++;
  if (packet.partial) {
    sim_partial_miss[packet.cpu][packet.type]++;
  }
  sim_access[packet.cpu][packet.type]++;
  way_hits[way]++;
  return true;
}

uint32_t VCL_CACHE::lru_victim(BLOCK* current_set, uint8_t min_size)
{
  BLOCK* endofset = std::next(current_set, (NUM_WAY - 1));
  BLOCK* begin_of_subset = current_set;
  uint32_t begin_way = 0;
  while (begin_of_subset->size < min_size && begin_of_subset < endofset) {
    begin_way++;
    begin_of_subset++;
  }
  if (is_default_lru(lru_modifier)) {
    uint8_t num_way_bound = get_lru_offset(lru_modifier);
    uint32_t end_way = begin_way;
    int count_sizes = 0;
    int prev_size = 0;
    while (end_way < NUM_WAY && count_sizes < num_way_bound) {
      if (prev_size != way_sizes[end_way]) {
        count_sizes++;
      }
      end_way++;
    }
    endofset = current_set + end_way;
    lru_subset = SUBSET(begin_way, end_way);
  } else {
    lru_subset = SUBSET(0, 0);
  }

  if (begin_of_subset->size < min_size) {
    std::cerr << "Couldn't find way that fits size" << std::endl;
    assert(0);
  }
  uint32_t way = std::distance(
      current_set, std::max_element(begin_of_subset, endofset, [](BLOCK lhs, BLOCK rhs) { return !rhs.valid || (lhs.valid && lhs.lru < rhs.lru); }));
  return way;
}

void VCL_CACHE::operate()
{
  // let buffer cache whatever it needs to do
  if (buffer)
    buffer_cache._operate();
  CACHE::operate();
}

void VCL_CACHE::operate_writes()
{
  if (buffer)
    operate_buffer_evictions();
  CACHE::operate_writes();
}

void VCL_CACHE::operate_buffer_evictions()
{
  while (!buffer_cache.merge_block.empty()) {
    BLOCK& merge_block = buffer_cache.merge_block.front();
    uint32_t set = get_set(merge_block.address); // set of the VCL cache, not the buffer
    auto set_begin = std::next(std::begin(block), set * NUM_WAY);
    auto set_end = std::next(set_begin, NUM_WAY);
    uint64_t bytes_accessed_bitmask = merge_block.bytes_accessed;
    if (buffer_cache.history == BufferHistory::PARTIAL) {
      uint64_t combined_mask = merge_block.bytes_accessed | merge_block.prev_present;
      auto partialblocks = get_blockboundaries_from_mask(combined_mask);
      for (auto b : partialblocks)
        for (uint8_t i = b.first; i < b.second + 1; i++) {
          uint8_t buffer_bit = (merge_block.bytes_accessed >> i) & 0b1;
          uint8_t combined_bit = (combined_mask >> i) & 0b1;
          if (buffer_bit + combined_bit == 2) {
            set_accessed(&bytes_accessed_bitmask, b.first, b.second);
            break;
          }
        }
    }
    auto blocks = get_blockboundaries_from_mask(bytes_accessed_bitmask);
    buffer_cache.merge_block.pop_front();
    if (blocks.size() == 0) {
      continue;
    }
    uint8_t min_start = 0;
    for (int i = 0; i < blocks.size(); i++) {
      if (i % 2 != 0) {
        continue; // Hole, not accessed. // or: already in cache
      }
      if (min_start > blocks[i].second)
        continue; // already in cache: the previous block already contains that block
      uint8_t block_start = std::max(min_start, blocks[i].first);
      size_t block_size = blocks[i].second - block_start + 1;
      auto first_inv = std::find_if_not(set_begin, set_end, is_valid_size<BLOCK>(block_size));
      uint32_t way = std::distance(set_begin, first_inv);
      if (way == NUM_WAY) {
        way = lru_victim(&block.data()[set * NUM_WAY], block_size);
        assert(way < NUM_WAY);
      }

      if (block_start + way_sizes[way] > 64) {
        block_start = 64 - way_sizes[way]; // possible duplicate - can't prevent that TODO: track duplicates here
      }
      uint8_t first_way = 0;
      for (int i = way; i < NUM_WAY; i++) {
        if (way_sizes[i] < block_size)
          continue;
        first_way = i;
        break;
      }
      uint8_t last_way = first_way + get_lru_offset(lru_modifier);
      if (last_way > NUM_WAY)
        last_way = NUM_WAY;
      lru_subset = SUBSET(first_way, last_way);
    }
    if (block_start + way_sizes[way] > 64) {
      block_start = 64 - way_sizes[way]; // possible duplicate - can't prevent that TODO: track duplicates here
    }
    min_start = block_start + way_sizes[way];
    filllike_miss(set, way, block_start, merge_block);
    if (min_start >= 64)
      break; // There is no block left that could be outside as we went until the end
  }
  if (!buffer_cache.merge_block.empty()) {
    std::cout << "did not empty buffer" << std::endl;
  }
}

void VCL_CACHE::handle_fill()
{
  while (writes_available_this_cycle > 0) {
    auto fill_mshr = MSHR.begin();
    if (fill_mshr == std::end(MSHR) || fill_mshr->event_cycle > current_cycle)
      return;
    if ((fill_mshr->address % BLOCK_SIZE) + fill_mshr->size > 64) {
      fill_mshr->size = 64 - fill_mshr->address % BLOCK_SIZE;
    }
    // VCL Impl: Immediately insert into buffer, search for address if evicted buffer entry has been used
    // find victim
    // no buffer impl: insert in invalid big enough block (any way)

    uint32_t set = get_set(fill_mshr->address);
    uint8_t num_blocks_to_write = 1;

    uint64_t orig_addr = fill_mshr->address;
    uint64_t orig_vaddr = fill_mshr->v_address;
    uint64_t orig_size = fill_mshr->size;

    if (buffer) {
      if (!buffer_cache.fill_miss(*fill_mshr, *this))
        return;
      goto inserted;
    }
    while (num_blocks_to_write > 0) {
      // TODO: If buffer go to buffer and insert whatever is in the buffer
      auto set_begin = std::next(std::begin(block), set * NUM_WAY);
      auto set_end = std::next(set_begin, NUM_WAY);
      auto first_inv = std::find_if_not(set_begin, set_end, is_valid<BLOCK>());
      uint32_t way = std::distance(set_begin, first_inv);
      // if aligned, we might have to write two blocks

      if (way == NUM_WAY) {
        way = lru_victim(&block.data()[set * NUM_WAY], 8); // NOTE: THIS IS THE BUFFERLESS IMPLEMENTATION // TODO: CHANGE SIZE ACCORDING TO WAY SIZES
        // TODO: RECORD STATISTICS IF ANY
      }
      uint8_t last_way = way + get_lru_offset(lru_modifier);
      if (last_way > NUM_WAY)
        last_way = NUM_WAY;
      lru_subset = SUBSET(way, last_way);

      bool success = filllike_miss(set, way, *fill_mshr);
      if (!success)
        return;

      uint8_t original_offset = std::min((uint8_t)(BLOCK_SIZE - way_sizes[way]), (uint8_t)(fill_mshr->v_address % BLOCK_SIZE));
      uint8_t aligned_offset = (original_offset - original_offset % way_sizes[way]);
      if (!aligned && way_sizes[way] < fill_mshr->size) {
        num_blocks_to_write++;
        fill_mshr->address = fill_mshr->address + way_sizes[way];
        fill_mshr->v_address = fill_mshr->v_address + way_sizes[way];
        fill_mshr->size = fill_mshr->size - way_sizes[way];
      } else if (aligned && aligned_offset + way_sizes[way] < (fill_mshr->v_address % BLOCK_SIZE) + fill_mshr->size) {
        assert((fill_mshr->v_address % BLOCK_SIZE) + fill_mshr->size <= 64);
        num_blocks_to_write++;
        int8_t diff = aligned_offset + way_sizes[way] - fill_mshr->v_address % BLOCK_SIZE;
        fill_mshr->address += diff;
        fill_mshr->v_address += diff;
        fill_mshr->size = fill_mshr->size - diff;
      }

      num_blocks_to_write--;
    }

  inserted:
    // restore mshr entry
    fill_mshr->address = orig_addr;
    fill_mshr->v_address = orig_vaddr;
    fill_mshr->size = orig_size;
    // data must already be copied in from above serviced
    for (auto ret : fill_mshr->to_return)
      ret->return_data(&(*fill_mshr));
    MSHR.erase(fill_mshr);
    writes_available_this_cycle--;
  }
}

void record_overlap() {}

void VCL_CACHE::handle_writeback()
{
  if (writes_available_this_cycle > 0 && WQ.has_ready()) {
    std::cerr << "Did not expect that we see writebacks in L1I" << std::endl;
    assert(0);
  }
}

uint8_t VCL_CACHE::hit_check(uint32_t& set, uint32_t& way, uint64_t& address, uint64_t& size)
{
  BLOCK b = block[set * NUM_WAY + way];
  uint8_t access_offset = address % BLOCK_SIZE;
  // NON VCL ignores block boundary crossing accesses at this point - so do we
  if ((b.offset <= access_offset && access_offset < b.offset + b.size && (access_offset + size - 1) < b.offset + b.size) || (b.offset + b.size >= 64)) {
    return 0;
  } else if (b.offset <= access_offset && access_offset < b.offset + b.size) {
    return b.offset + b.size; // we hit in the first part, but not in the second part
  }
  return -1;
}

/*BLOCK* VCL_CACHE::probe_buffer(PACKET& packet, uint32_t set)
{
  uint32_t idx = set >> lg2(NUM_SET / buffer_sets);
  if (organisation == BufferOrganisation::DIRECT_MAPPED) {
    return &buffer_cache[idx];
  }
  // Fully associative
  for (BLOCK& b : block) {
    if (b.tag == get_tag(packet.address)) {
      return &b;
    }
  }
  return NULL;
}*/

std::vector<uint32_t> VCL_CACHE::get_way(uint32_t tag, uint32_t set)
{
  auto begin = std::next(block.begin(), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);

  std::vector<uint32_t> ways;
  ways.reserve(NUM_WAY);

  uint32_t way = 0;
  while (begin < end) {
    if (!begin->valid)
      goto next;
    if (get_tag(begin->address) != tag)
      goto next;
    // hit: tag matched
    ways.push_back(way);
  next:
    way++;
    begin = std::next(begin);
  }
  return ways;
}

uint32_t VCL_CACHE::get_way(PACKET& packet, uint32_t set)
{
  auto offset = packet.v_address % BLOCK_SIZE;
  // std::cout << "get_way(TAG: " << std::hex << std::setw(10) << ((packet.v_address >> OFFSET_BITS) >> lg2(NUM_SET)) << std::dec << ", SET: " << std::setw(3)
  //           << set << ", OFFSET: " << std::setw(3) << offset << ") @ " << std::setw(5) << current_cycle << std::endl;
  auto begin = std::next(block.begin(), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);
  uint32_t way = 0;
  bool found_tag = false;
  // expanded loop for easier debugging
  while (begin < end) {
    if (!begin->valid) {
      goto not_found;
    }
    if ((packet.address >> (OFFSET_BITS)) != (begin->address >> (OFFSET_BITS))) {
      goto not_found;
    }
    found_tag = true;
    if (begin->offset <= offset && offset < begin->offset + begin->size) {
      // std::cout << "hit: 1, address:" << packet.v_address << std::endl;
      assert(way_sizes[way] == begin->size);
      return way;
    }
  not_found:
    way++;
    begin = std::next(begin);
  }
  // std::cout << "hit: 0, address:" << packet.v_address << std::endl;
  if (found_tag)
    packet.partial = true;
  return NUM_WAY; // we did not find a way
}

void VCL_CACHE::handle_read()
{
  while (reads_available_this_cycle > 0) {

    if (!RQ.has_ready())
      return;

    // handle the oldest entry
    PACKET& handle_pkt = RQ.front();

    // A (hopefully temporary) hack to know whether to send the evicted paddr or
    // vaddr to the prefetcher
    ever_seen_data |= (handle_pkt.v_address != handle_pkt.ip);

    uint32_t set = get_set(handle_pkt.address);
    uint32_t way = get_way(handle_pkt, set);
    BLOCK* b = NULL;
    if (buffer)
      b = buffer_cache.probe_buffer(handle_pkt);
    if (buffer && !b)
      b = buffer_cache.probe_merge(handle_pkt);

    // HIT IN BUFFER/MERGE REGISTER / always use that first
    if (b) {
      // statistics in the buffer
      RQ.pop_front();
      reads_available_this_cycle--;
      handle_pkt.data = b->data;
      for (auto ret : handle_pkt.to_return)
        ret->return_data(&handle_pkt);
      continue;
    }

    // HIT IN VCL CACHE
    if (way < NUM_WAY) {
      way_hits[way]++;

      // std::cout << "hit in SET: " << std::setw(3) << set << ", way: " << std::setw(3) << way << std::endl;
      readlike_hit(set, way, handle_pkt);
      uint64_t newoffset = hit_check(set, way, handle_pkt.address, handle_pkt.size);
      if (!newoffset) {
        RQ.pop_front();
        reads_available_this_cycle--;
        continue;
      }
      if (newoffset > 63) {
        assert(0);
      }
      assert(handle_pkt.size > 0 && handle_pkt.size < 64);
      uint64_t diff = (newoffset - handle_pkt.v_address % BLOCK_SIZE);
      handle_pkt.size = handle_pkt.size - diff;
      uint64_t mask = ~(BLOCK_SIZE - 1);
      handle_pkt.v_address = (handle_pkt.v_address & mask) + newoffset;
      handle_pkt.address = (handle_pkt.address & mask) + newoffset; // offset matches: all 64 byte aligned
      // not done reading this thing just yet
      continue;
    }

    // MISS
    bool success = readlike_miss(handle_pkt);
    RQ.pop_front();
    if (!success)
      return; // buffer full = try next cycle

    // remove this entry from RQ
    if (knob_stall_on_miss) {
      reads_available_this_cycle = 0;
      ooo_cpu[handle_pkt.cpu]->stall_on_miss = 1;
    } else
      reads_available_this_cycle--;
  }
}

int VCL_CACHE::add_pq(PACKET* packet)
{
  assert(packet->address != 0);
  PQ_ACCESS++;

  DP(if (warmup_complete[packet->cpu]) {
    std::cout << "[" << NAME << "_WQ] " << __func__ << " instr_id: " << packet->instr_id << " address: " << std::hex << (packet->address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet->address << " v_address: " << packet->v_address << std::dec << " type: " << +packet->type
              << " occupancy: " << RQ.occupancy();
  })

  // check for the latest wirtebacks in the write queue
  champsim::delay_queue<PACKET>::iterator found_wq = std::find_if(
      WQ.begin(), WQ.end(), eq_vcl_addr<PACKET>(packet->address, packet->v_address % BLOCK_SIZE, packet->size, match_offset_bits ? 0 : (OFFSET_BITS)));

  if (found_wq != WQ.end()) {
    DP(if (warmup_complete[packet->cpu]) std::cout << " MERGED_WQ" << std::endl;)

    packet->data = found_wq->data;
    for (auto ret : packet->to_return)
      ret->return_data(packet);

    WQ_FORWARD++;
    return -1;
  }

  // check for duplicates in the PQ
  auto found = std::find_if(PQ.begin(), PQ.end(), eq_vcl_addr<PACKET>(packet->address, packet->v_address % BLOCK_SIZE, packet->size, (OFFSET_BITS)));
  if (found != PQ.end()) {
    DP(if (warmup_complete[packet->cpu]) std::cout << " MERGED_PQ" << std::endl;)

    found->fill_level = std::min(found->fill_level, packet->fill_level);
    packet_dep_merge(found->to_return, packet->to_return);

    PQ_MERGED++;
    return 0;
  }

  // check occupancy
  if (PQ.full()) {
    DP(if (warmup_complete[packet->cpu]) std::cout << " FULL" << std::endl;)

    PQ_FULL++;
    return -2; // cannot handle this request
  }

  // if there is no duplicate, add it to PQ
  if (warmup_complete[cpu])
    PQ.push_back(*packet);
  else
    PQ.push_back_ready(*packet);

  DP(if (warmup_complete[packet->cpu]) std::cout << " ADDED" << std::endl;)

  PQ_TO_CACHE++;
  return PQ.occupancy();
}
int VCL_CACHE::add_wq(PACKET* packet)
{
  WQ_ACCESS++;

  DP(if (warmup_complete[packet->cpu]) {
    std::cout << "[" << NAME << "_WQ] " << __func__ << " instr_id: " << packet->instr_id << " address: " << std::hex << (packet->address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet->address << " v_address: " << packet->v_address << std::dec << " type: " << +packet->type
              << " occupancy: " << RQ.occupancy();
  })

  // check for duplicates in the write queue
  champsim::delay_queue<PACKET>::iterator found_wq = std::find_if(
      WQ.begin(), WQ.end(), eq_vcl_addr<PACKET>(packet->address, packet->v_address % BLOCK_SIZE, packet->size, match_offset_bits ? 0 : (OFFSET_BITS)));

  if (found_wq != WQ.end()) {

    DP(if (warmup_complete[packet->cpu]) std::cout << " MERGED" << std::endl;)

    WQ_MERGED++;
    return 0; // merged index
  }

  // Check for room in the queue
  if (WQ.full()) {
    DP(if (warmup_complete[packet->cpu]) std::cout << " FULL" << std::endl;)

    ++WQ_FULL;
    return -2;
  }

  // if there is no duplicate, add it to the write queue
  if (warmup_complete[cpu])
    WQ.push_back(*packet);
  else
    WQ.push_back_ready(*packet);

  DP(if (warmup_complete[packet->cpu]) std::cout << " ADDED" << std::endl;)

  WQ_TO_CACHE++;
  WQ_ACCESS++;

  return WQ.occupancy();
}

int VCL_CACHE::add_rq(PACKET* packet)
{
  assert(packet->address != 0);
  RQ_ACCESS++;

  DP(if (warmup_complete[packet->cpu]) {
    std::cout << "[" << NAME << "_RQ] " << __func__ << " instr_id: " << packet->instr_id << " address: " << std::hex << (packet->address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet->address << " v_address: " << packet->v_address << std::dec << " type: " << +packet->type
              << " occupancy: " << RQ.occupancy();
  })

  // check for the latest writebacks in the write queue
  champsim::delay_queue<PACKET>::iterator found_wq = std::find_if(
      WQ.begin(), WQ.end(), eq_vcl_addr<PACKET>(packet->address, packet->v_address % BLOCK_SIZE, packet->size, match_offset_bits ? 0 : (OFFSET_BITS)));

  if (found_wq != WQ.end()) {

    DP(if (warmup_complete[packet->cpu]) std::cout << " MERGED_WQ" << std::endl;)

    packet->data = found_wq->data;
    for (auto ret : packet->to_return)
      ret->return_data(packet);

    WQ_FORWARD++;
    return -1;
  }
  // check occupancy
  if (RQ.full()) {
    RQ_FULL++;

    DP(if (warmup_complete[packet->cpu]) std::cout << " FULL" << std::endl;)

    return -2; // cannot handle this request
  }

  // if there is no duplicate, add it to RQ
  assert(packet->size < 64);
  if (warmup_complete[cpu])
    RQ.push_back(*packet);
  else
    RQ.push_back_ready(*packet);

  DP(if (warmup_complete[packet->cpu]) std::cout << " ADDED" << std::endl;)

  RQ_TO_CACHE++;
  return RQ.occupancy();
}

void CACHE::print_private_stats()
{
  std::cout << NAME << " LINES HANDLED PER WAY" << std::endl;
  for (uint32_t i = 0; i < NUM_WAY; ++i) {
    std::cout << std::right << std::setw(3) << i << ":\t" << way_hits[i] << std::endl;
  }
  std::cout << NAME << " ALL BYTES ACCESSED COVERAGE " << std::endl;
  for (uint16_t i = 0; i < cl_accesses_percentage_of_presence_covered.size(); i++) {
    std::cout << std::right << std::setw(3) << i << ":\t" << cl_accesses_percentage_of_presence_covered[i] << std::endl;
  }
  if (this->buffer) {
    BUFFER_CACHE& bc = ((VCL_CACHE*)this)->buffer_cache;
    bc.print_private_stats();
    std::cout << std::right << std::setw(3) << "merge register"
              << ":\t" << bc.merge_hit << std::endl;
  }
}

bool VCL_CACHE::filllike_miss(std::size_t set, std::size_t way, size_t offset, BLOCK& handle_block)
{
  BLOCK& fill_block = block[set * NUM_WAY + way];
  way_hits[way]++;
  ooo_cpu[handle_block.cpu]->stall_on_miss = 0;
  record_block_insert_removal(set, way, handle_block.address, warmup_complete[handle_block.cpu]);
  if (fill_block.valid && fill_block.accesses == 0) {
    USELESS_CACHELINE++;
  }
  TOTAL_CACHELINES++;
  bool evicting_dirty = (lower_level != NULL) && fill_block.dirty;
  uint64_t evicting_address = 0;
  if (evicting_dirty) {
    PACKET writeback_packet;

    writeback_packet.fill_level = lower_level->fill_level;
    writeback_packet.cpu = handle_block.cpu;
    writeback_packet.address = fill_block.address;
    writeback_packet.data = fill_block.data;
    writeback_packet.instr_id = handle_block.instr_id;
    writeback_packet.ip = handle_block.ip;
    writeback_packet.type = WRITEBACK;

    auto result = lower_level->add_wq(&writeback_packet);
    if (result == -2)
      return false;
  }

  if (ever_seen_data)
    evicting_address = fill_block.address & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
  else
    evicting_address = fill_block.v_address & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);

  if (fill_block.prefetch)
    pf_useless++;

  fill_block.bytes_accessed = 0;
  memset(fill_block.accesses_per_bytes, 0, sizeof(fill_block.accesses_per_bytes));
  fill_block.valid = true;
  fill_block.prefetch = false; // prefetches would go into the buffer and on merge be useless
  fill_block.dirty = handle_block.dirty;
  fill_block.address = handle_block.address;
  fill_block.v_address = handle_block.v_address;
  fill_block.ip = handle_block.ip;
  fill_block.cpu = handle_block.cpu;
  fill_block.tag = get_tag(handle_block.address);
  fill_block.instr_id = handle_block.instr_id;
  fill_block.offset = std::min((uint8_t)(64 - fill_block.size), (uint8_t)offset);
  fill_block.accesses = 0;
  fill_block.last_modified_access = 0;
  fill_block.time_present = 0;
  auto endidx = 64 - fill_block.offset - fill_block.size;
  fill_block.data = (handle_block.data << offset) >> offset >> endidx << endidx;
  // We already acounted for the evicted block on insert, so what we count here is the insertion of a new block
  impl_replacement_update_state(handle_block.cpu, set, way, handle_block.address, handle_block.ip, 0, FILL, 0);
  return true;
}

bool VCL_CACHE::filllike_miss(std::size_t set, std::size_t way, PACKET& handle_pkt)
{
  DP(if (warmup_complete[handle_pkt.cpu]) {
    std::cout << "[" << NAME << "] " << __func__ << " miss";
    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << (handle_pkt.address >> OFFSET_BITS);
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
    std::cout << " type: " << +handle_pkt.type;
    std::cout << " cycle: " << current_cycle << std::endl;
  });
  way_hits[way]++;

  ooo_cpu[handle_pkt.cpu]->stall_on_miss = 0;
  bool bypass = (way == NUM_WAY);
  record_block_insert_removal(set, way, handle_pkt.address, warmup_complete[handle_pkt.cpu]);

#ifndef LLC_BYPASS
  assert(!bypass);
#endif
  assert(handle_pkt.type != WRITEBACK || !bypass);

  BLOCK& fill_block = block[set * NUM_WAY + way];
  if (fill_block.valid && fill_block.accesses == 0) {
    USELESS_CACHELINE++;
  } else if (fill_block.valid) {
    }
  TOTAL_CACHELINES++;
  // if (0 == NAME.compare(NAME.length() - 3, 3, "L1I") && fill_block.valid) {
  //   record_cacheline_stats(handle_pkt.cpu, fill_block);
  // }

  bool evicting_dirty = !bypass && (lower_level != NULL) && fill_block.dirty;
  uint64_t evicting_address = 0;

  if (!bypass) {
    if (evicting_dirty) {
      PACKET writeback_packet;

      writeback_packet.fill_level = lower_level->fill_level;
      writeback_packet.cpu = handle_pkt.cpu;
      writeback_packet.address = fill_block.address;
      writeback_packet.data = fill_block.data;
      writeback_packet.instr_id = handle_pkt.instr_id;
      writeback_packet.ip = 0;
      writeback_packet.type = WRITEBACK;

      auto result = lower_level->add_wq(&writeback_packet);
      if (result == -2)
        return false;
    }

    if (ever_seen_data)
      evicting_address = fill_block.address & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
    else
      evicting_address = fill_block.v_address & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);

    if (fill_block.prefetch)
      pf_useless++;

    if (handle_pkt.type == PREFETCH)
      pf_fill++;

    fill_block.valid = true;
    fill_block.prefetch = (handle_pkt.type == PREFETCH && handle_pkt.pf_origin_level == fill_level);
    fill_block.dirty = (handle_pkt.type == WRITEBACK || (handle_pkt.type == RFO && handle_pkt.to_return.empty()));
    fill_block.address = handle_pkt.address;
    fill_block.v_address = handle_pkt.v_address;
    fill_block.data = handle_pkt.data;
    fill_block.ip = handle_pkt.ip;
    fill_block.cpu = handle_pkt.cpu;
    fill_block.tag = get_tag(handle_pkt.address);
    fill_block.instr_id = handle_pkt.instr_id;
    record_cacheline_accesses(handle_pkt, fill_block, *prev_access);
    if (aligned) {
      uint8_t original_offset = std::min((uint64_t)64 - fill_block.size, handle_pkt.v_address % BLOCK_SIZE);
      fill_block.offset = (original_offset - original_offset % fill_block.size);
      // CHECK: do we need to adjust the addresses?
      fill_block.address = handle_pkt.address - original_offset % fill_block.size;
      fill_block.v_address = handle_pkt.v_address - original_offset % fill_block.size;
      // This is currently not guaranteed - but does it happen? (overlapping the just inserted block - do we need to insert two blocks?)
      // It also happens in the default implementation: access on byte 63, so we don't need to handle this specially
      // if (fill_block.offset + fill_block.size < (handle_pkt.v_address % BLOCK_SIZE) + handle_pkt.size) {
      //   std::cout << handle_pkt.v_address << std::endl;
      //   assert(0);
      // }
    } else
      fill_block.offset = std::min((uint64_t)64 - fill_block.size, handle_pkt.v_address % BLOCK_SIZE);
    // We already acounted for the evicted block on insert, so what we count here is the insertion of a new block
  }

  if (warmup_complete[handle_pkt.cpu] && (handle_pkt.cycle_enqueued != 0))
    total_miss_latency += current_cycle - handle_pkt.cycle_enqueued;

  // update prefetcher
  cpu = handle_pkt.cpu;
  handle_pkt.pf_metadata =
      impl_prefetcher_cache_fill((virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS), set, way,
                                 handle_pkt.type == PREFETCH, evicting_address, handle_pkt.pf_metadata);

  // update replacement policy
  impl_replacement_update_state(handle_pkt.cpu, set, way, handle_pkt.address, handle_pkt.ip, 0, FILL, 0);

  // COLLECT STATS
  sim_miss[handle_pkt.cpu][handle_pkt.type]++;
  sim_access[handle_pkt.cpu][handle_pkt.type]++;

  prev_access = &fill_block;
  return true;
}