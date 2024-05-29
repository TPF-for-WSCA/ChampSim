
/*
 * This file implements a basic Branch Target Buffer (BTB) structure.
 * It uses a set-associative BTB to predict the targets of non-return branches,
 * and it uses a small Return Address Stack (RAS) to predict the target of
 * returns.
 */

#include "ooo_cpu.h"

#define BASIC_BTB_SETS 4096
#define BASIC_BTB_WAYS 4
#define BASIC_BTB_INDIRECT_SIZE 4096
#define BASIC_BTB_RAS_SIZE 64
#define BASIC_BTB_CALL_INSTR_SIZE_TRACKERS 1024

map<uint32_t, uint64_t> offset_reuse_freq;
uint64_t offset_found_on_BTBmiss;
uint64_t offset_not_found_on_BTBmiss;
uint64_t offset_not_found_on_BTBhit;

struct BTBEntry {
  uint64_t tag;
  uint64_t target_ip;
  uint8_t branch_type;
  uint64_t lru;

  uint32_t offsetBTB_partitionID;
  uint32_t offsetBTB_set;
  uint32_t offsetBTB_way;
};

struct offset_BTBEntry {
  uint64_t target_offset;
  uint64_t lru;
  uint32_t ref_count;
};

struct BTB {
  std::vector<std::vector<BTBEntry>> theBTB;
  uint32_t numSets;
  uint32_t assoc;
  uint64_t indexMask;
  uint32_t numIndexBits;

  BTB() {}

  BTB(int32_t Sets, int32_t Assoc) : numSets(Sets), assoc(Assoc)
  {
    // aBTBSize must be a power of 2
    assert(((Sets - 1) & (Sets)) == 0);
    theBTB.resize(Sets);
    indexMask = Sets - 1;
    numIndexBits = (uint32_t)log2((double)Sets);
  }

  void init_btb(int32_t Sets, int32_t Assoc)
  {
    numSets = Sets;
    assoc = Assoc;
    // aBTBSize must be a power of 2
    assert(((Sets - 1) & (Sets)) == 0);
    theBTB.resize(Sets);
    indexMask = Sets - 1;
    numIndexBits = (uint32_t)log2((double)Sets);
  }

  int32_t index(uint64_t ip) { return ((ip >> 2) & indexMask); }

  uint64_t get_tag(uint64_t ip)
  {
    return ip;
    uint64_t addr = ip;
    addr = addr >> 2;
    addr = addr >> numIndexBits;
    /* We use a 16-bit tag.
     * The lower 8-bits stay the same as in the full tag.
     * The upper 8-bits are the folded X-OR of the remaining bits of the full tag.
     */
    uint64_t tag = addr & 0xFF; // Set the lower 8-bits of the tag
    addr = addr >> 8;
    int tagMSBs = 0;
    /*Get the upper 8-bits (folded X-OR)*/
    for (int i = 0; i < 8; i++) {
      tagMSBs = tagMSBs ^ (addr & 0xFF);
      addr = addr >> 8;
    }
    /*Concatenate the lower and upper 8-bits of tag*/
    tag = tag | (tagMSBs << 8);
    return tag;
  }

  BTBEntry* get_BTBentry(uint64_t ip)
  {
    BTBEntry* entry = NULL;

    int idx = index(ip);
    uint64_t tag = get_tag(ip);
    for (uint32_t i = 0; i < theBTB[idx].size(); i++) {
      if (theBTB[idx][i].tag == tag) {
        return &(theBTB[idx][i]);
      }
    }

    return entry;
  }

  BTBEntry* get_lru_BTBEntry(uint64_t ip)
  {
    int idx = index(ip);

    if (theBTB[idx].size() < assoc) {
      return NULL;
    }

    return &theBTB[idx].front();
  }

  BTBEntry* update_BTB(uint64_t ip, uint8_t b_type, uint64_t target, uint8_t taken, uint64_t lru_counter)
  {
    int idx = index(ip);
    uint64_t tag = get_tag(ip);
    int way = -1;
    for (uint32_t i = 0; i < theBTB[idx].size(); i++) {
      if (theBTB[idx][i].tag == tag) {
        way = i;
        break;
      }
    }

    if (way == -1) {
      if ((target != 0) && taken) {
        BTBEntry entry;
        entry.tag = tag;
        entry.branch_type = b_type;
        entry.target_ip = target;
        entry.lru = lru_counter;

        if (theBTB[idx].size() >= assoc) {
          theBTB[idx].erase(theBTB[idx].begin());
        }
        theBTB[idx].push_back(entry);
      } else {
        assert(0);
      }
    } else {
      BTBEntry entry = theBTB[idx][way];
      entry.branch_type = b_type;
      if (target != 0) {
        entry.target_ip = target;
      }
      entry.lru = lru_counter;

      // Update LRU
      theBTB[idx].erase(theBTB[idx].begin() + way);
      theBTB[idx].push_back(entry);
    }

    return &(theBTB[idx].back());
  }

  uint64_t get_lru_value(uint64_t ip)
  {
    int idx = index(ip);
    uint64_t lru_value;
    if (theBTB[idx].size() < assoc) { // All ways are not yet allocated
      lru_value = 0;
    } else {
      lru_value = theBTB[idx][0].lru;
      for (uint32_t i = 1; i < theBTB[idx].size(); i++) { // We should never enter here because head should be LRU
        if (theBTB[idx][i].lru < lru_value) {
          assert(0);
        }
      }
    }

    return lru_value;
  }
};

struct offsetBTB {
  std::vector<std::vector<offset_BTBEntry>> theOffsetBTB;
  uint32_t numSets;
  uint32_t assoc;
  uint64_t indexMask;
  uint32_t numIndexBits;

  offsetBTB() {}

  offsetBTB(int32_t Sets, int32_t Assoc) : numSets(Sets), assoc(Assoc)
  {
    // aBTBSize must be a power of 2
    assert(((Sets - 1) & (Sets)) == 0);
    theOffsetBTB.resize(Sets);
    indexMask = Sets - 1;
    numIndexBits = (uint32_t)log2((double)Sets);
  }

  void init_offsetbtb(int32_t Sets, int32_t Assoc)
  {
    numSets = Sets;
    assoc = Assoc;
    // aBTBSize must be a power of 2
    assert(((Sets - 1) & (Sets)) == 0);
    theOffsetBTB.resize(Sets);
    indexMask = Sets - 1;
    numIndexBits = (uint32_t)log2((double)Sets);
  }

  int32_t index(uint64_t target) { return ((target >> 2) & indexMask); }

  void inc_ref_count(int set, int way) { theOffsetBTB[set][way].ref_count++; }
  void dec_ref_count(int set, int way)
  {
    if (((size_t)set) >= theOffsetBTB.size() or ((size_t)way) >= theOffsetBTB[set].size())
      return;
    if (theOffsetBTB[set][way].ref_count > 0)
      theOffsetBTB[set][way].ref_count--;
  }

  std::pair<uint32_t, uint32_t> update_OffsetBTB(uint64_t target_offset, uint64_t lru_counter, bool is_new_entry)
  {
    int idx = index(target_offset);
    int way = -1;
    for (uint32_t i = 0; i < theOffsetBTB[idx].size(); i++) {
      if (theOffsetBTB[idx][i].target_offset == target_offset) {
        way = i;
        inc_ref_count(idx, way);
        break;
      }
    }

    // cout << "idx " << idx << " way " << way << " offset " << target_offset << endl;
    if (is_new_entry) {
      if (way == -1) {
        offset_not_found_on_BTBmiss++;
      } else {
        offset_found_on_BTBmiss++;
      }
    } else {
      if (way == -1) {
        offset_not_found_on_BTBhit++;
      }
    }

    if (way == -1) {
      if (/*(target_offset != 0) &&*/ 1) {
        offset_BTBEntry entry;
        entry.target_offset = target_offset;
        entry.lru = lru_counter;
        entry.ref_count = 1;

        // Find and replace LRU entry
        if (theOffsetBTB[idx].size() < assoc) {
          way = theOffsetBTB[idx].size();
          theOffsetBTB[idx].push_back(entry);
        } else {
          way = 0;
          uint64_t lru_count = theOffsetBTB[idx][0].lru;
          for (uint32_t i = 1; i < theOffsetBTB[idx].size(); i++) {
            if (theOffsetBTB[idx][i].lru < lru_count) {
              lru_count = theOffsetBTB[idx][i].lru;
              way = i;
            }
          }

          uint32_t ref_count = theOffsetBTB[idx][way].ref_count;
          map<uint32_t, uint64_t>::iterator it;
          it = offset_reuse_freq.find(ref_count);
          if (it != offset_reuse_freq.end()) {
            it->second++;
          } else {
            offset_reuse_freq[ref_count] = 1;
          }

          theOffsetBTB[idx][way] = entry;
        }
        assert(theOffsetBTB[idx].size() <= assoc);
      }
    } else {
      // Update LRU counter
      theOffsetBTB[idx][way].lru = lru_counter;
    }

    // cout << "idx " << idx << " way " << way << endl;
    return std::make_pair(idx, way);
  }

  uint64_t get_offset_from_offsetBTB(uint32_t idx, uint32_t way)
  {
    assert(idx < numSets);
    assert(way < assoc);
    return theOffsetBTB[idx][way].target_offset;
  }
};

/*BTB BTB_4D(1024, 8);                                   //Storage: (tag:16-bit, branch-type: 2-bit, target-offset: 10-bit) 28*1024*8 = 28KB
BTB BTB_6D(1024, 8);                                   //Storage: (tag:16-bit, branch-type: 2-bit, target-offset: 15-bit) 33*1024*7 = 28.875KB
BTB BTB_8D(1024, 8);                                   //Storage: (tag:16-bit, branch-type: 2-bit, target-offset: 25-bit) 43*1024*8 = 43KB
BTB BTB_12D(512, 8);                                    //Storage: (tag:16-bit, branch-type: 2-bit,   full-target: 64-bit) 82*256*4  = 10.25KB
BTB BTB_18D(512, 8);
BTB BTB_25D(256, 8);
BTB BTB_46D(128, 8);
BTB BTB_Ret(1024, 8);*/

// Way sizes in bits
uint8_t* btb_partition_sizes;

uint8_t* offsetbtb_partition_sizes;

int NUM_BTB_PARTITIONS = -1;
int NUM_NON_INDIRECT_PARTITIONS = -1;
int NUM_OFFSET_BTB_PARTITIONS = -1;
int NUM_SETS = -1;
int LAST_BTB_PARTITION_ID = -1;
BTB** btb_partition;

offsetBTB** offsetBTB_partition;

uint64_t basic_btb_lru_counter[NUM_CPUS];

uint64_t basic_btb_indirect[NUM_CPUS][BASIC_BTB_INDIRECT_SIZE];
uint64_t basic_btb_conditional_history[NUM_CPUS];

uint64_t basic_btb_ras[NUM_CPUS][BASIC_BTB_RAS_SIZE];
int basic_btb_ras_index[NUM_CPUS];
/*
 * The following two variables are used to automatically identify the
 * size of call instructions, in bytes, which tells us the appropriate
 * target for a call's corresponding return.
 * They exist because ChampSim does not model a specific ISA, and
 * different ISAs could use different sizes for call instructions,
 * and even within the same ISA, calls can have different sizes.
 */
uint64_t basic_btb_call_instr_sizes[NUM_CPUS][BASIC_BTB_CALL_INSTR_SIZE_TRACKERS];

uint64_t basic_btb_abs_addr_dist(uint64_t addr1, uint64_t addr2)
{
  if (addr1 > addr2) {
    return addr1 - addr2;
  }

  return addr2 - addr1;
}

void push_basic_btb_ras(uint8_t cpu, uint64_t ip)
{
  basic_btb_ras_index[cpu]++;
  if (basic_btb_ras_index[cpu] == BASIC_BTB_RAS_SIZE) {
    basic_btb_ras_index[cpu] = 0;
  }

  basic_btb_ras[cpu][basic_btb_ras_index[cpu]] = ip;
}

uint64_t peek_basic_btb_ras(uint8_t cpu) { return basic_btb_ras[cpu][basic_btb_ras_index[cpu]]; }

uint64_t pop_basic_btb_ras(uint8_t cpu)
{
  uint64_t target = basic_btb_ras[cpu][basic_btb_ras_index[cpu]];
  basic_btb_ras[cpu][basic_btb_ras_index[cpu]] = 0;

  basic_btb_ras_index[cpu]--;
  if (basic_btb_ras_index[cpu] == -1) {
    basic_btb_ras_index[cpu] += BASIC_BTB_RAS_SIZE;
  }

  return target;
}

uint64_t basic_btb_call_size_tracker_hash(uint64_t ip) { return (ip & (BASIC_BTB_CALL_INSTR_SIZE_TRACKERS - 1)); }

uint64_t basic_btb_get_call_size(uint8_t cpu, uint64_t ip)
{
  uint64_t size = basic_btb_call_instr_sizes[cpu][basic_btb_call_size_tracker_hash(ip)];

  return size;
}

// TODO: CONVERT TO FLEXIBLE SIZE SOLUTION TO SUPPORT BTBX
int convert_offsetBits_to_btb_partitionID(int num_bits)
{
  int j = 0;
  for (int i = 0; i < NUM_BTB_PARTITIONS; i++) {
    int way_size = *(btb_partition_sizes + i);
    if (i <= NUM_NON_INDIRECT_PARTITIONS or i == LAST_BTB_PARTITION_ID)
      j = i;
    if (num_bits <= way_size) {
      break;
    }
  }
  return j;
}

int convert_offsetBits_to_offsetbtb_partitionID(int num_bits)
{
  if (num_bits <= offsetbtb_partition_sizes[0] /* 9 */) {
    return 0;
  } else if (num_bits <= offsetbtb_partition_sizes[1] /* 11 */) {
    return 1;
  } else if (num_bits <= offsetbtb_partition_sizes[2] /* 19 */) {
    return 2;
  } else if (num_bits <= offsetbtb_partition_sizes[3] /* 25 */) {
    return 3;
  } else {
    cout << "Num bits " << dec << num_bits << endl;
    assert(0);
  }
}

int get_lru_partition(int start_partitionID, uint64_t ip)
{
  int lru_partition = start_partitionID;
  uint64_t lru_value = btb_partition[start_partitionID]->get_lru_value(ip);
  for (int i = start_partitionID + 1; i < NUM_BTB_PARTITIONS; i++) {
    uint64_t partition_lru_value = btb_partition[i]->get_lru_value(ip);
    if (partition_lru_value < lru_value) {
      lru_partition = i;
      lru_value = partition_lru_value;
    }
  }
  return lru_partition;
}

void O3_CPU::initialize_btb()
{
  std::cout << "Basic BTB sets: " << BTB_SETS << " ways: " << BTB_WAYS << " indirect buffer size: " << BASIC_BTB_INDIRECT_SIZE
            << " RAS size: " << BASIC_BTB_RAS_SIZE << std::endl;
  btb_partition_sizes = btb_sizes;
  NUM_SETS = BTB_SETS;
  NUM_BTB_PARTITIONS = BTB_WAYS + 1;
  NUM_NON_INDIRECT_PARTITIONS = BTB_NON_INDIRECT;
  offsetbtb_partition_sizes = btb_partition_sizes + NUM_NON_INDIRECT_PARTITIONS;
  NUM_OFFSET_BTB_PARTITIONS = (NUM_BTB_PARTITIONS - NUM_NON_INDIRECT_PARTITIONS - 1);
  LAST_BTB_PARTITION_ID = BTB_WAYS;
  btb_partition = (BTB**)malloc(NUM_BTB_PARTITIONS * sizeof(BTB*));
  offsetBTB_partition = (offsetBTB**)malloc(NUM_NON_INDIRECT_PARTITIONS * sizeof(offsetBTB**));

  for (uint32_t i = 0; i < BASIC_BTB_RAS_SIZE; i++) {
    basic_btb_ras[cpu][i] = 0;
  }
  basic_btb_ras_index[cpu] = 0;
  for (uint32_t i = 0; i < BASIC_BTB_CALL_INSTR_SIZE_TRACKERS; i++) {
    basic_btb_call_instr_sizes[cpu][i] = 4;
  }

  basic_btb_lru_counter[cpu] = 0;

  offset_reuse_freq.clear();
  offset_found_on_BTBmiss = 0;
  offset_not_found_on_BTBmiss = 0;
  offset_not_found_on_BTBhit = 0;

  for (uint32_t i = 0; i < BTB_WAYS; i++) {
    btb_partition[i] = new BTB(BTB_SETS, 1);
  }
  uint32_t small_btb_sets = BTB_SETS / 8;
  if (small_btb_sets == 0)
    small_btb_sets = 1;
  btb_partition[BTB_WAYS] = new BTB(small_btb_sets, 1);

  for (int i = 0; i < NUM_BTB_PARTITIONS; i++)
    offsetBTB_partition[i] = new offsetBTB(1, 2048); // TODO: make sets/ways configurable
}

BTB_outcome O3_CPU::btb_prediction(uint64_t ip, uint8_t branch_type)
{
  BTBEntry* btb_entry;
  int partitionID = -1;

  for (int i = 0; i < NUM_BTB_PARTITIONS; i++) {
    btb_entry = btb_partition[i]->get_BTBentry(ip);
    if (btb_entry) {
      partitionID = i;
      break;
    }
  }

  if (btb_entry == NULL) {
    // no prediction for this IP
    if (branch_type == BRANCH_DIRECT_CALL || branch_type == BRANCH_INDIRECT_CALL) {
      push_basic_btb_ras(cpu, ip);
    }
    BTB_outcome outcome = {0, BRANCH_CONDITIONAL, 2 /*To indicate that it was a BTB miss*/};
    return outcome;
  }

  branch_type = NOT_BRANCH;
  branch_type = btb_entry->branch_type;

  // uint8_t always_taken = false;
  // if (branch_type != BRANCH_CONDITIONAL) {
  // always_taken = true;
  //}

  if ((branch_type == BRANCH_DIRECT_CALL) || (branch_type == BRANCH_INDIRECT_CALL)) {
    // add something to the RAS
    push_basic_btb_ras(cpu, ip);
  }

  if (branch_type == BRANCH_RETURN) {
    // peek at the top of the RAS
    uint64_t target = peek_basic_btb_ras(cpu);
    // and adjust for the size of the call instr
    target += basic_btb_get_call_size(cpu, target);

    BTB_outcome outcome = {target, BRANCH_RETURN, 0};
    return outcome;
  } /*else if ((branch_type == BRANCH_INDIRECT) ||
             (branch_type == BRANCH_INDIRECT_CALL)) {
    return std::make_pair(basic_btb_indirect[cpu][basic_btb_indirect_hash(cpu, ip)], always_taken);
  } */
  else {
    // use BTB for all other branches + direct calls

    uint64_t branch_target = btb_entry->target_ip;
    assert(partitionID != -1 && partitionID < NUM_BTB_PARTITIONS);
    if (partitionID >= NUM_NON_INDIRECT_PARTITIONS && partitionID != LAST_BTB_PARTITION_ID) {
      // cout << "Going to read offset. Partition " << std::hex << partitionID << " ip " << ip << " set " << btb_entry->offsetBTB_set << " way " <<
      // btb_entry->offsetBTB_way << endl;
      uint64_t target_offset =
          offsetBTB_partition[btb_entry->offsetBTB_partitionID]->get_offset_from_offsetBTB(btb_entry->offsetBTB_set, btb_entry->offsetBTB_way);
      int offset_size = offsetbtb_partition_sizes[btb_entry->offsetBTB_partitionID];
      uint64_t temp = ((ip >> offset_size) >> 2) << offset_size;
      branch_target = (temp | target_offset) << 2;
      /*if (btb_entry->target_ip != branch_target) {
        cout << "Partition ID " <<  partitionID << " correct target " << std::hex << btb_entry->target_ip << " computed target " << branch_target << " offset "
      << target_offset << endl; assert(0);
      }*/
    }

    BTB_outcome outcome = {branch_target, branch_type, 0};
    return outcome;
  }

  assert(0);
  // return std::make_pair(0, always_taken);
}

void assert_refcounts()
{
  uint64_t num_idx_entries = 0;
  uint64_t stored_ref_count = 0;
  for (int partition = 0; partition < NUM_OFFSET_BTB_PARTITIONS; partition++) {
    for (auto const& sets : btb_partition[partition + NUM_NON_INDIRECT_PARTITIONS]->theBTB) {
      for (auto const& entry : sets) {
        if (entry.tag != 0 and entry.branch_type != BRANCH_RETURN)
          num_idx_entries += 1;
      }
    }
    for (auto const& sets : offsetBTB_partition[partition]->theOffsetBTB) {
      for (auto const& entry : sets) {
        stored_ref_count += entry.ref_count;
      }
    }
  }
  assert(stored_ref_count == num_idx_entries);
}

void update_offsetbtb(uint64_t branch_target, int num_offset_bits, BTBEntry* btb_entry, uint64_t lru_counter, bool is_new_entry)
{
  int offsetBTB_partitionID = convert_offsetBits_to_offsetbtb_partitionID(num_offset_bits);
  assert(offsetBTB_partitionID < NUM_OFFSET_BTB_PARTITIONS);
  uint64_t target_offset_mask = ((uint64_t)1 << offsetbtb_partition_sizes[offsetBTB_partitionID]) - 1;
  uint64_t target_offset = (branch_target >> 2) & target_offset_mask;
  if (not is_new_entry)
    offsetBTB_partition[btb_entry->offsetBTB_partitionID]->dec_ref_count(btb_entry->offsetBTB_set, btb_entry->offsetBTB_way);
  std::pair<uint32_t, uint32_t> offsetBTB_index = offsetBTB_partition[offsetBTB_partitionID]->update_OffsetBTB(target_offset, lru_counter, is_new_entry);
  assert_refcounts();

  btb_entry->offsetBTB_partitionID = offsetBTB_partitionID;
  btb_entry->offsetBTB_set = offsetBTB_index.first;
  btb_entry->offsetBTB_way = offsetBTB_index.second;
}

bool is_offset_ref(BTBEntry* entry, int partitionID)
{
  return NUM_NON_INDIRECT_PARTITIONS <= partitionID and partitionID != LAST_BTB_PARTITION_ID and entry->branch_type != BRANCH_RETURN;
}

uint64_t get_target_from_offset_btb_entry(BTBEntry* entry, uint64_t ip, int partitionID)
{
  if (not(is_offset_ref(entry, partitionID)))
    return entry->target_ip;
  uint64_t offset = offsetBTB_partition[entry->offsetBTB_partitionID]->get_offset_from_offsetBTB(entry->offsetBTB_set, entry->offsetBTB_way);
  offset <<= 2;
  int num_bits = offsetbtb_partition_sizes[entry->offsetBTB_partitionID];
  uint64_t mask = pow(2, num_bits + 2) - 1;
  auto target = (ip & ~mask) | (offset & mask);
  return target;
}

void print_map(std::map<uint32_t, uint16_t>& pm)
{
  for (auto const& [key, val] : pm) {
    std::cout << key << ": " << val << std::endl;
  }
}

uint64_t last_stats_cycle = 0;

void O3_CPU::update_btb(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  // updates for indirect branches
  /*if ((branch_type == BRANCH_INDIRECT) ||
      (branch_type == BRANCH_INDIRECT_CALL)) {
    basic_btb_indirect[cpu][basic_btb_indirect_hash(cpu, ip)] = branch_target;
  }
  if (branch_type == BRANCH_CONDITIONAL) {
    basic_btb_conditional_history[cpu] <<= 1;
    if (taken) {
      basic_btb_conditional_history[cpu] |= 1;
    }
  }*/

  // TODO: Do fuzzy current cycle (variable containing last measurement timestamp, > 1000 clear)
  if (99997 <= current_cycle - last_stats_cycle) {
    std::cout << "Getting copy statistics - cycle: " << current_cycle << std::endl;
    last_stats_cycle = current_cycle;
    for (int i = 0; i < NUM_OFFSET_BTB_PARTITIONS; ++i) {
      std::map<uint32_t, uint16_t> reuse_frequency;
      auto offsetBTB = offsetBTB_partition[i];
      for (auto& set : offsetBTB->theOffsetBTB)
        for (auto& entry : set) {
          reuse_frequency[entry.ref_count] += 1;
        }
      sharing_in_btb_by_partition[i].push_back(reuse_frequency); // TODO: We are adding this for multiple offset btbs
    }
  }

  if (branch_type == BRANCH_RETURN) {
    // recalibrate call-return offset
    // if our return prediction got us into the right ball park, but not the
    // exactly correct byte target, then adjust our call instr size tracker
    uint64_t call_ip = pop_basic_btb_ras(cpu);
    uint64_t estimated_call_instr_size = basic_btb_abs_addr_dist(call_ip, branch_target);
    if (estimated_call_instr_size <= 10) {
      basic_btb_call_instr_sizes[cpu][basic_btb_call_size_tracker_hash(call_ip)] = estimated_call_instr_size;
    }
  }

  if (taken == false)
    return;

  BTBEntry* btb_entry = NULL;
  int partitionID = -1;
  for (int i = 0; i < NUM_BTB_PARTITIONS; i++) {
    btb_entry = btb_partition[i]->get_BTBentry(ip);
    if (btb_entry) {
      partitionID = i;
      break;
    }
  }

  int num_bits;
  if (branch_type == BRANCH_RETURN) {
    num_bits = 0;
  } else {
    uint64_t diff_bits = (branch_target >> 2) ^ (ip >> 2);
    num_bits = 0;
    while (diff_bits != 0) {
      diff_bits = diff_bits >> 1;
      num_bits++;
    }
  }
  assert(num_bits >= 0 && num_bits < 66);
  uint64_t predicted_target = 0;
  if (btb_entry != NULL)
    predicted_target = get_target_from_offset_btb_entry(btb_entry, ip, partitionID);
  if (btb_entry != NULL && predicted_target != branch_target) {
    if (is_offset_ref(btb_entry, partitionID)) {
      offsetBTB_partition[btb_entry->offsetBTB_partitionID]->dec_ref_count(btb_entry->offsetBTB_set, btb_entry->offsetBTB_way);
    }
    btb_entry->tag = 0;
    btb_entry->lru = 0;
    btb_entry = NULL;
  }

  if (btb_entry == NULL) {

    BTB_writes++;

    int smallest_offset_partition_id = convert_offsetBits_to_btb_partitionID(num_bits);

    int partition = get_lru_partition(smallest_offset_partition_id, ip);
    assert(partition < NUM_BTB_PARTITIONS);

    if (NUM_NON_INDIRECT_PARTITIONS <= partition and partition != NUM_BTB_PARTITIONS - 1) {
      BTBEntry* prev_entry = btb_partition[partition]->get_lru_BTBEntry(ip);
      if (prev_entry and prev_entry->tag and prev_entry->branch_type != BRANCH_RETURN)
        offsetBTB_partition[prev_entry->offsetBTB_partitionID]->dec_ref_count(prev_entry->offsetBTB_set, prev_entry->offsetBTB_way);
    }

    // cout << "IP " << ip << " Target " << branch_target << " mask " << target_offset_mask << " offset " << target_offset << " num bits " << num_bits <<
    // endl;

    BTBEntry* btb_entry = btb_partition[partition]->update_BTB(ip, branch_type, branch_target, taken, basic_btb_lru_counter[cpu]);
    if (is_offset_ref(btb_entry, partition)) {
      update_offsetbtb(branch_target, num_bits, btb_entry, basic_btb_lru_counter[cpu], 1);
    }

    basic_btb_lru_counter[cpu]++;

    // cout << "stored IP " << std::hex << ip << " target " << branch_target << " in partition " << partition << endl;

  } else {
    // update an existing entry
    assert(partitionID != -1);
    BTBEntry* btb_entry = btb_partition[partitionID]->update_BTB(ip, branch_type, branch_target, taken, basic_btb_lru_counter[cpu]);

    if (partitionID >= NUM_NON_INDIRECT_PARTITIONS && partitionID != LAST_BTB_PARTITION_ID && branch_type != BRANCH_RETURN) {
      update_offsetbtb(branch_target, num_bits, btb_entry, basic_btb_lru_counter[cpu], 0);
    }
    basic_btb_lru_counter[cpu]++;
  }
}

void O3_CPU::btb_final_stats()
{
  cout << "XXX offset_found_on_BTBmiss " << offset_found_on_BTBmiss << endl;
  cout << "XXX offset_not_found_on_BTBmiss " << offset_not_found_on_BTBmiss << endl;
  cout << "XXX offset_not_found_on_BTBhit " << offset_not_found_on_BTBhit << endl;
  uint64_t total_offsetBTB_evictions = 0;
  for (map<uint32_t, uint64_t>::iterator it = offset_reuse_freq.begin(); it != offset_reuse_freq.end(); ++it)
    total_offsetBTB_evictions += it->second;

  cout << "XXX total_offsetBTB_evictions " << total_offsetBTB_evictions << endl;
  int i = 0;
  uint64_t offsetBTB_evictions = 0;
  for (map<uint32_t, uint64_t>::iterator it = offset_reuse_freq.begin(); it != offset_reuse_freq.end() && i < 10; ++it, i++) {
    std::cout << "XXX reuse" << it->first << " " << it->second << '\n';
    offsetBTB_evictions += it->second;
  }
  std::cout << "XXX reuseM" << " " << (total_offsetBTB_evictions - offsetBTB_evictions) << '\n';
}

bool O3_CPU::is_not_block_ending(uint64_t ip) { return true; }