#include "cache.h"

#include <algorithm>
#include <iterator>
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

void record_cacheline_accesses(PACKET& handle_pkt, BLOCK& hit_block)
{
  if (handle_pkt.size != 0) {
    // vaddr and ip should be the same for L1I, but lookup happens on address so we also operate on address
    // assert(handle_pkt.address % BLOCK_SIZE == handle_pkt.v_address % BLOCK_SIZE); // not true in case of overlapping blocks
    uint8_t offset = (uint8_t)(handle_pkt.v_address % BLOCK_SIZE);
    uint8_t end = offset + handle_pkt.size - 1;
    set_accessed(&hit_block.bytes_accessed, offset, end);
    for (int i = offset; i <= end; i++) {
      hit_block.accesses_per_bytes[i]++;
    }
    hit_block.accesses++;
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
      readlike_hit(set, way, handle_pkt);
    } else {
      bool success = readlike_miss(handle_pkt);
      if (!success)
        return;
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

  record_cacheline_accesses(handle_pkt, hit_block);
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

void CACHE::record_block_insert_removal(int set, int way, uint64_t address)
{
  BLOCK& repl_block = block[set * NUM_WAY + way];
  bool newtag_present = false;
  bool oldtag_present = false;
  for (int i = 0; i < NUM_WAY; i++) {
    if (i == way)
      continue;
    if (block[set * NUM_WAY + i].address >> (OFFSET_BITS) == address >> (OFFSET_BITS))
      newtag_present = true;
    if (block[set * NUM_WAY + i].address >> OFFSET_BITS == repl_block.address >> OFFSET_BITS)
      oldtag_present = true;
  }
  if (!oldtag_present)
    num_blocks_in_cache--;
  if (!newtag_present)
    num_blocks_in_cache++;
  if (cl_blocks_in_cache_buffer.size() >= WRITE_BUFFER_SIZE) {
    write_buffers_to_disk();
  }
  assert(num_blocks_in_cache <= NUM_WAY * NUM_SET);
  cl_blocks_in_cache_buffer.push_back(num_blocks_in_cache);
}

// TODO: Make more generic for all simple series
// TODO: ONLY TRACK WHEN WARMUP COMPLETE>>>
void CACHE::write_buffers_to_disk()
{
  if (cl_accessmask_buffer.size() == 0 && cl_blocks_in_cache_buffer.size() == 0) {
    return;
  }
  if (!cl_accessmask_file.is_open()) {
    std::filesystem::path result_path = result_dir;
    result_path /= "cl_access_masks.bin";
    cl_accessmask_file = std::ofstream(result_path.c_str(), std::ios::binary | std::ios::out);
  }
  if (!cl_num_blocks_in_cache.is_open()) {
    std::filesystem::path result_path = result_dir;
    result_path /= "cl_num_blocks.bin";
    cl_num_blocks_in_cache = std::ofstream(result_path.c_str(), std::ios::binary | std::ios::out);
  }
  for (auto& mask : cl_accessmask_buffer) {
    cl_accessmask_file.write(reinterpret_cast<char*>(&mask), sizeof(uint64_t));
  }
  for (auto& num_blocks : cl_blocks_in_cache_buffer) {
    cl_num_blocks_in_cache.write(reinterpret_cast<char*>(&num_blocks), sizeof(uint64_t));
  }
  cl_accessmask_file.flush();
  cl_num_blocks_in_cache.flush();
  cl_accessmask_buffer.clear();
  cl_blocks_in_cache_buffer.clear();
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
    uint8_t size = block.second - block.first; // actual size is +1 as we have first and last index
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
    total_accessed += size;
    // count how many accesses to this block

    uint64_t count = 1;
    if (count_method == CountBlockMethod::SUM_ACCESSES) {
      count = 0;
      for (int j = block.first; j < block.second; j++) {
        count += handle_block.accesses_per_bytes[j];
      }
    }
    blsize_hist[cpu][size] += count;
    is_hole = !is_hole; // alternating block/hole
  }
  uint64_t count = 1;
  if (count_method == CountBlockMethod::SUM_ACCESSES) {
    count = handle_block.accesses;
  }
  cl_bytesaccessed_hist[cpu][total_accessed] += count;
  blsize_ignore_holes_hist[cpu][last_accessed - first_accessed] += count;
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

  bool bypass = (way == NUM_WAY);
#ifndef LLC_BYPASS
  assert(!bypass);
#endif
  assert(handle_pkt.type != WRITEBACK || !bypass);

  BLOCK& fill_block = block[set * NUM_WAY + way];

  // quick and dirty / mainly dirty: only apply if name ends in L1I
  if (0 == NAME.compare(NAME.length() - 3, 3, "L1I") && fill_block.valid) {
    record_cacheline_stats(handle_pkt.cpu, fill_block);
    record_block_insert_removal(set, way, handle_pkt.address);
  }
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

    fill_block.bytes_accessed = 0; // newly added to the cache thus no accesses yet
    memset(fill_block.accesses_per_bytes, 0, sizeof(fill_block.accesses_per_bytes));

    record_cacheline_accesses(handle_pkt, fill_block);

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
    record_cacheline_accesses(handle_pkt, fill_block);
  }

  if (warmup_complete[handle_pkt.cpu] && (handle_pkt.cycle_enqueued != 0))
    total_miss_latency += current_cycle - handle_pkt.cycle_enqueued;

  // update prefetcher
  cpu = handle_pkt.cpu;
  handle_pkt.pf_metadata =
      impl_prefetcher_cache_fill((virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS), set, way,
                                 handle_pkt.type == PREFETCH, evicting_address, handle_pkt.pf_metadata);

  // update replacement policy
  impl_replacement_update_state(handle_pkt.cpu, set, way, handle_pkt.address, handle_pkt.ip, 0, handle_pkt.type, 0);

  // COLLECT STATS
  sim_miss[handle_pkt.cpu][handle_pkt.type]++;
  sim_access[handle_pkt.cpu][handle_pkt.type]++;
  way_hits[way]++;

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

  // check occupancy
  if (RQ.full()) {
    RQ_FULL++;

    DP(if (warmup_complete[packet->cpu]) std::cout << " FULL" << std::endl;)

    return -2; // cannot handle this request
  }

  // if there is no duplicate, add it to RQ
  assert(packet->size <= 64);
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
  impl_replacement_update_state(packet.cpu, set, way, packet.address, packet.ip, 0, packet.type, 1);

  // COLLECT STATS
  sim_hit[packet.cpu][packet.type]++;
  sim_access[packet.cpu][packet.type]++;

  record_cacheline_accesses(packet, hitb);
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
      return &(*it);
    }
  }
}

bool BUFFER_CACHE::fill_miss(PACKET& packet)
{
  uint32_t set = get_set(packet.address);
  auto set_begin = std::next(std::begin(block), set * NUM_WAY);
  auto set_end = std::next(set_begin, NUM_WAY);
  auto first_inv = std::find_if_not(set_begin, set_end, is_valid<BLOCK>());
  uint32_t way = std::distance(set_begin, first_inv);
  if (way == NUM_WAY) {
    way = impl_replacement_find_victim(packet.cpu, packet.instr_id, set, &block.data()[set * NUM_WAY], packet.ip, packet.address, packet.type);
    // TODO: RECORD STATISTICS IF ANY
  }

  if (merge_block.full()) {
    return false;
  }
  BLOCK& fill_block = block[set * NUM_WAY + way];

  if (fill_block.valid) {
    merge_block.push_back(fill_block, true);
  }

  fill_block.bytes_accessed = 0;
  memset(fill_block.accesses_per_bytes, 0, sizeof(fill_block.accesses_per_bytes));
  fill_block.valid = true;
  fill_block.prefetch = (packet.type == PREFETCH && packet.pf_origin_level == fill_level);
  fill_block.dirty = (packet.type == WRITEBACK || (packet.type == RFO && packet.to_return.empty()));
  fill_block.address = packet.address;
  fill_block.v_address = packet.v_address;
  fill_block.data = packet.data;
  fill_block.ip = packet.ip;
  fill_block.cpu = packet.cpu;
  fill_block.tag = get_tag(packet.address);
  fill_block.instr_id = packet.instr_id;
  fill_block.accesses = 0;
  record_cacheline_accesses(packet, fill_block);

  if (warmup_complete[packet.cpu] && (packet.cycle_enqueued != 0))
    total_miss_latency += current_cycle - packet.cycle_enqueued;
  sim_miss[packet.cpu][packet.type]++;
  sim_access[packet.cpu][packet.type]++;
  return true;
}

uint32_t VCL_CACHE::lru_victim(BLOCK* current_set, uint8_t min_size)
{
  BLOCK* endofset = std::next(current_set, NUM_WAY);
  BLOCK* begin_of_subset = current_set;
  while (begin_of_subset->size < min_size && begin_of_subset < endofset) {
    begin_of_subset++;
  }
  if (begin_of_subset->size < min_size) {
    std::cerr << "Couldn't find way that fits size" << std::endl;
    assert(0);
  }
  return std::distance(begin_of_subset,
                       std::max_element(begin_of_subset, endofset, [](BLOCK lhs, BLOCK rhs) { return !rhs.valid || (lhs.valid && lhs.lru < rhs.lru); }));
}

void VCL_CACHE::operate()
{
  // let buffer cache whatever it needs to do
  buffer_cache._operate();
  CACHE::operate();
}

void VCL_CACHE::operate_writes()
{
  operate_buffer_merges();
  CACHE::operate_writes();
}

void VCL_CACHE::operate_buffer_merges()
{
  while (!buffer_cache.merge_block.empty()) {
    BLOCK& merge_block = buffer_cache.merge_block.front();
    uint32_t set = get_set(merge_block.address); // set of the VCL cache, not the buffer
    auto set_begin = std::next(std::begin(block), set * NUM_WAY);
    auto set_end = std::next(set_begin, NUM_WAY);
    uint32_t tag = get_tag(merge_block.address); // dito
    auto _in_cache = get_way(tag, set);
    // set all of them to invalid and insert again - not hardware like but easier in sw
    for (uint32_t& way : _in_cache) {
      BLOCK& b = block[set * NUM_WAY + way];
      set_accessed(&merge_block.bytes_accessed, b.offset, b.offset + b.size - 1);
      b.valid = false;
    }
    auto blocks = get_blockboundaries_from_mask(merge_block.bytes_accessed);
    for (int i = 0; i < blocks.size(); i++) {
      if (i % 2 != 0) {
        continue; // Hole, not accessed.
      }
      size_t block_size = blocks[i].second - blocks[i].first + 1;
      auto first_inv = std::find_if_not(set_begin, set_end, is_valid_size<BLOCK>(block_size));
      uint32_t way = std::distance(set_begin, first_inv);
      if (way == NUM_WAY) {
        way = lru_victim(&block.data()[set * NUM_WAY], block_size);
      }
      filllike_miss(set, way, blocks[i].first, merge_block);
    }
    buffer_cache.merge_block.pop_front();
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
      if (!buffer_cache.fill_miss(*fill_mshr))
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
        way = lru_victim(&block.data()[set * NUM_WAY], 8); // TODO: FIX FOR SIZE once we have buffer
        // TODO: RECORD STATISTICS IF ANY
      }

      bool success = filllike_miss(set, way, *fill_mshr);
      if (!success)
        return;

      uint8_t original_offset = std::min(BLOCK_SIZE - way_sizes[way], fill_mshr->v_address % BLOCK_SIZE);
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
  if ((b.offset <= access_offset && access_offset < b.offset + b.size && access_offset + size < b.offset + b.size) || (b.offset + b.size >= 64)) {
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
  // expanded loop for easier debugging
  while (begin < end) {
    if (!begin->valid) {
      goto not_found;
    }
    if ((packet.address >> (OFFSET_BITS)) != (begin->address >> (OFFSET_BITS))) {
      goto not_found;
    }
    if (begin->offset <= offset && offset < begin->offset + begin->size) {
      // std::cout << "hit: 1, address:" << packet.v_address << std::endl;
      return way;
    }
  not_found:
    way++;
    begin = std::next(begin);
  }
  // std::cout << "hit: 0, address:" << packet.v_address << std::endl;
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
    BLOCK* b = buffer_cache.probe_buffer(handle_pkt);
    if (!b)
      b = buffer_cache.probe_merge(handle_pkt);
    // TODO: WE MIGHT NEED ALSO MULTIPLE WAYS
    // TODO: We might need multiple accesses
    if (b) {
      // statistics in the buffer
      RQ.pop_front();
      reads_available_this_cycle--;
      handle_pkt.data = b->data;
      for (auto ret : handle_pkt.to_return)
        ret->return_data(&handle_pkt);
      continue;
    } else if (way < NUM_WAY) // HIT
    {
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
      handle_pkt.size = handle_pkt.size - (newoffset - handle_pkt.v_address % BLOCK_SIZE);
      uint64_t mask = ~(BLOCK_SIZE - 1);
      handle_pkt.v_address = (handle_pkt.v_address & mask) + newoffset;
      handle_pkt.address = (handle_pkt.address & mask) + newoffset; // offset matches: all 64 byte aligned
      // not done reading this thing just yet
      continue;
    } else {
      bool success = readlike_miss(handle_pkt);
      RQ.pop_front();
      if (!success)
        return; // buffer full = try next cycle
    }

    // remove this entry from RQ
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

  // check for duplicates in the read queue
  auto found_rq = std::find_if(RQ.begin(), RQ.end(), eq_vcl_addr<PACKET>(packet->address, packet->v_address % BLOCK_SIZE, packet->size, (OFFSET_BITS)));
  if (found_rq != RQ.end()) {

    DP(if (warmup_complete[packet->cpu]) std::cout << " MERGED_RQ" << std::endl;)

    packet_dep_merge(found_rq->lq_index_depend_on_me, packet->lq_index_depend_on_me);
    packet_dep_merge(found_rq->sq_index_depend_on_me, packet->sq_index_depend_on_me);
    packet_dep_merge(found_rq->instr_depend_on_me, packet->instr_depend_on_me);
    packet_dep_merge(found_rq->to_return, packet->to_return);

    RQ_MERGED++;

    return 0; // merged index
  }

  // check occupancy
  if (RQ.full()) {
    RQ_FULL++;

    DP(if (warmup_complete[packet->cpu]) std::cout << " FULL" << std::endl;)

    return -2; // cannot handle this request
  }

  // if there is no duplicate, add it to RQ
  assert(packet->size <= 64);
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
}

bool VCL_CACHE::filllike_miss(std::size_t set, std::size_t way, size_t offset, BLOCK& handle_block)
{
  record_block_insert_removal(set, way, handle_block.address);
  BLOCK& fill_block = block[set * NUM_WAY + way];

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

  fill_block.valid = true;
  fill_block.prefetch = false; // prefetches would go into the buffer and on merge be useless
  fill_block.dirty = handle_block.dirty;
  fill_block.address = handle_block.address;
  fill_block.v_address = handle_block.v_address;
  fill_block.ip = handle_block.ip;
  fill_block.cpu = handle_block.cpu;
  fill_block.tag = get_tag(handle_block.address);
  fill_block.instr_id = handle_block.instr_id;
  fill_block.offset = std::min((uint64_t)64 - fill_block.size, offset);
  auto endidx = 64 - fill_block.offset - fill_block.size;
  fill_block.data = (handle_block.data << offset) >> offset >> endidx << endidx;
  // We already acounted for the evicted block on insert, so what we count here is the insertion of a new block
  way_hits[way]++;
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

  bool bypass = (way == NUM_WAY);
  record_block_insert_removal(set, way, handle_pkt.address);

#ifndef LLC_BYPASS
  assert(!bypass);
#endif
  assert(handle_pkt.type != WRITEBACK || !bypass);

  BLOCK& fill_block = block[set * NUM_WAY + way];

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
    way_hits[way]++;
  }

  if (warmup_complete[handle_pkt.cpu] && (handle_pkt.cycle_enqueued != 0))
    total_miss_latency += current_cycle - handle_pkt.cycle_enqueued;

  // update prefetcher
  cpu = handle_pkt.cpu;
  handle_pkt.pf_metadata =
      impl_prefetcher_cache_fill((virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS), set, way,
                                 handle_pkt.type == PREFETCH, evicting_address, handle_pkt.pf_metadata);

  // update replacement policy
  impl_replacement_update_state(handle_pkt.cpu, set, way, handle_pkt.address, handle_pkt.ip, 0, handle_pkt.type, 0);

  // COLLECT STATS
  sim_miss[handle_pkt.cpu][handle_pkt.type]++;
  sim_access[handle_pkt.cpu][handle_pkt.type]++;

  return true;
}