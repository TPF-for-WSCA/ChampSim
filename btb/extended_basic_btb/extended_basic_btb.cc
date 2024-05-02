
/*
 * This file implements a basic Branch Target Buffer (BTB) structure.
 * It uses a set-associative BTB to predict the targets of non-return branches,
 * and it uses a small Return Address Stack (RAS) to predict the target of
 * returns.
 */

#include "cache.h"
#include "ooo_cpu.h"

#define idx(cpu, set, way, NUM_SETS, NUM_WAYS) (cpu * NUM_SETS * NUM_WAYS + set * NUM_WAYS + way)
#define BASIC_BTB_INDIRECT_SIZE 131072
#define BASIC_BTB_RAS_SIZE 64
#define BASIC_BTB_CALL_INSTR_SIZE_TRACKERS 1024
#define BASIC_BTB_BRANCH_TABLE 131072

struct BRANCH_TABLE_ENTRY {
  uint8_t branch_type;
  uint8_t size;
  uint8_t BW;
};

std::map<uint64_t, BRANCH_TABLE_ENTRY> branch_table;
uint64_t basic_btb_lru_counter[NUM_CPUS];

// indirect jumps
uint64_t basic_btb_indirect[NUM_CPUS][BASIC_BTB_INDIRECT_SIZE];
uint64_t basic_btb_indirect_ip[NUM_CPUS][BASIC_BTB_INDIRECT_SIZE];
int basic_btb_ras_indirect_index[NUM_CPUS];
uint64_t basic_btb_conditional_history[NUM_CPUS];

// return address stack
uint64_t basic_btb_ras[NUM_CPUS][BASIC_BTB_RAS_SIZE];
int basic_btb_ras_index[NUM_CPUS];
uint64_t basic_btb_ras_ip[NUM_CPUS][BASIC_BTB_RAS_SIZE];
int basic_btb_ras_ip_index[NUM_CPUS];
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

uint64_t basic_btb_set_index(uint64_t ip, size_t num_sets) { return ((ip >> 2) & (num_sets - 1)); }

BASIC_BTB_ENTRY* basic_btb_find_entry(uint8_t cpu, uint64_t ip, size_t num_sets, size_t num_ways, BASIC_BTB_ENTRY* btb)
{
  uint64_t set = basic_btb_set_index(ip, num_sets);
  for (uint32_t i = 0; i < num_ways; i++) {
    if (btb[idx(cpu, set, i, num_sets, num_ways)].ip_tag == ip) {
      return &(btb[idx(cpu, set, i, num_sets, num_ways)]);
    }
  }

  return NULL;
}

BASIC_BTB_ENTRY* basic_btb_get_lru_entry(uint8_t cpu, uint64_t set, size_t num_sets, size_t num_ways, BASIC_BTB_ENTRY* btb)
{
  uint32_t lru_way = 0;
  uint64_t lru_value = btb[idx(cpu, set, lru_way, num_sets, num_ways)].lru;
  for (uint32_t i = 0; i < num_ways; i++) {
    if (btb[idx(cpu, set, i, num_sets, num_ways)].lru < lru_value) {
      lru_way = i;
      lru_value = btb[idx(cpu, set, lru_way, num_sets, num_ways)].lru;
    }
  }

  return &(btb[idx(cpu, set, lru_way, num_sets, num_ways)]);
}

void basic_btb_update_lru(uint8_t cpu, BASIC_BTB_ENTRY* btb_entry)
{
  btb_entry->lru = basic_btb_lru_counter[cpu];
  basic_btb_lru_counter[cpu]++;
}

uint64_t basic_btb_indirect_hash(uint8_t cpu, uint64_t ip)
{
  uint64_t hash = (ip >> 2) ^ (basic_btb_conditional_history[cpu]);
  hash = (hash & (BASIC_BTB_INDIRECT_SIZE - 1));
  if (basic_btb_indirect_ip[cpu][basic_btb_ras_indirect_index[cpu]] != ip) {
    basic_btb_ras_indirect_index[cpu]++;
    if (basic_btb_ras_indirect_index[cpu] == BASIC_BTB_INDIRECT_SIZE)
      basic_btb_ras_indirect_index[cpu] = 0;
    basic_btb_indirect_ip[cpu][basic_btb_ras_indirect_index[cpu]] = ip; // only needs update if we are not
  }

  return hash;
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

uint64_t pop_basic_btb_ras(uint8_t cpu, uint64_t ip)
{
  uint64_t target = basic_btb_ras[cpu][basic_btb_ras_index[cpu]];
  if (basic_btb_ras_ip[cpu][basic_btb_ras_ip_index[cpu]] != ip) {
    basic_btb_ras_ip_index[cpu]++;
    if (basic_btb_ras_ip_index[cpu] == BASIC_BTB_RAS_SIZE)
      basic_btb_ras_ip_index[cpu] = 0;
  }
  basic_btb_ras_ip[cpu][basic_btb_ras_ip_index[cpu]] = ip;
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

void O3_CPU::initialize_btb()
{
  std::cout << "Basic BTB sets: " << BTB_SETS << " ways: " << BTB_WAYS << " indirect buffer size: " << BASIC_BTB_INDIRECT_SIZE
            << " RAS size: " << BASIC_BTB_RAS_SIZE << std::endl;

  for (uint32_t i = 0; i < BTB_SETS; i++) {
    for (uint32_t j = 0; j < BTB_WAYS; j++) {
      basic_btb[idx(cpu, i, j, BTB_SETS, BTB_WAYS)].ip_tag = 0;
      basic_btb[idx(cpu, i, j, BTB_SETS, BTB_WAYS)].target = 0;
      basic_btb[idx(cpu, i, j, BTB_SETS, BTB_WAYS)].always_taken = 0;
      basic_btb[idx(cpu, i, j, BTB_SETS, BTB_WAYS)].lru = 0;
    }
  }
  basic_btb_lru_counter[cpu] = 0;

  for (uint32_t i = 0; i < BASIC_BTB_INDIRECT_SIZE; i++) {
    basic_btb_indirect[cpu][i] = 0;
  }
  basic_btb_conditional_history[cpu] = 0;

  for (uint32_t i = 0; i < BASIC_BTB_RAS_SIZE; i++) {
    basic_btb_ras[cpu][i] = 0;
  }
  basic_btb_ras_index[cpu] = 0;
  for (uint32_t i = 0; i < BASIC_BTB_CALL_INSTR_SIZE_TRACKERS; i++) {
    basic_btb_call_instr_sizes[cpu][i] = 4;
  }
}

BTB_outcome O3_CPU::btb_prediction(uint64_t ip, uint8_t branch_type)
{
  uint8_t always_taken = false;
  if (branch_type != BRANCH_CONDITIONAL) {
    always_taken = true;
  }

  if ((branch_type == BRANCH_DIRECT_CALL) || (branch_type == BRANCH_INDIRECT_CALL)) {
    // add something to the RAS
    push_basic_btb_ras(cpu, ip);
  }

  if (branch_type == BRANCH_RETURN) {
    // peek at the top of the RAS
    uint64_t target = peek_basic_btb_ras(cpu);
    // and adjust for the size of the call instr
    target += basic_btb_get_call_size(cpu, target);

    return BTB_outcome({target, always_taken, branch_type, 0});
  } else if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL)) {
    return BTB_outcome({basic_btb_indirect[cpu][basic_btb_indirect_hash(cpu, ip)], always_taken, branch_type, 0});
  } else {
    // use BTB for all other branches + direct calls
    auto btb_entry = basic_btb_find_entry(cpu, ip, BTB_SETS, BTB_WAYS, basic_btb);

    if (btb_entry == NULL) {
      // no prediction for this IP
      always_taken = true;
      return BTB_outcome({0, always_taken, branch_type, 0});
    }

    always_taken = btb_entry->always_taken;
    basic_btb_update_lru(cpu, btb_entry);

    return BTB_outcome({btb_entry->target, always_taken, branch_type, 0});
  }

  return BTB_outcome({0, always_taken, branch_type, 0});
}

bool O3_CPU::is_not_block_ending(uint64_t ip)
{
  ip = ip >> 2;
  auto BTE = branch_table.find(ip);
  // if end - not a branch thus continuing to next entry
  if (BTE == branch_table.end() || (BTE->second.BW && MIGHT_LOOP_BRANCH(BTE->second.branch_type))) {
    return true;
  }
  return false;
}

void O3_CPU::update_btb(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  assert(ip % 4 == 0 and branch_target % 4 == 0);
  if (MIGHT_LOOP_BRANCH(branch_type))
    branch_table[(ip >> 2)] = {branch_type, 4, (branch_target < ip && (ip - branch_target) < EXTENDED_BTB_MAX_LOOP_BRANCH)};
  if (branch_table.size() > BASIC_BTB_BRANCH_TABLE) {
    std::cerr << "overgrown branch table... " << branch_table.size() << std::endl;
  }

  // updates for indirect branches
  if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL)) {
    basic_btb_indirect[cpu][basic_btb_indirect_hash(cpu, ip)] = branch_target;
  }
  if (branch_type == BRANCH_CONDITIONAL) {
    basic_btb_conditional_history[cpu] <<= 1;
    if (taken) {
      basic_btb_conditional_history[cpu] |= 1;
    }
  }

  if (branch_type == BRANCH_RETURN) {
    // recalibrate call-return offset
    // if our return prediction got us into the right ball park, but not the
    // exactly correct byte target, then adjust our call instr size tracker
    uint64_t call_ip = pop_basic_btb_ras(cpu, ip);
    uint64_t estimated_call_instr_size = basic_btb_abs_addr_dist(call_ip, branch_target);
    if (estimated_call_instr_size <= 10) {
      basic_btb_call_instr_sizes[cpu][basic_btb_call_size_tracker_hash(call_ip)] = estimated_call_instr_size;
    }
  } else if ((branch_type != BRANCH_INDIRECT) && (branch_type != BRANCH_INDIRECT_CALL)) {
    // use BTB
    uint64_t diff_bits = (branch_target >> 2) ^ (ip >> 2);
    int num_bits;
    if (branch_type == BRANCH_RETURN) {
      num_bits = 0;
    } else {
      uint64_t offset = branch_target & (diff_bits - 1);
      num_bits = 0;
      while (diff_bits != 0) {
        diff_bits = diff_bits >> 1;
        num_bits++;
      }
      assert(num_bits <= 64);
      extern uint8_t knob_collect_offsets;
      if (knob_collect_offsets) {
        pc_offset_pairs_by_size.push_back(std::make_pair(ip, offset));
        if (num_bits < 6) {
          assert(offset <= 128);
          offset_counts[offset]++;
          for (uint64_t i = 0; i < 64; i++) {
            uint64_t bitsel = 0x1ull << i;
            if (bitsel & ip)
              pc_bits_offset[offset][i]++;
          }
        }
      }
    }
    assert(num_bits >= 0 && num_bits < 65);

    auto btb_entry = basic_btb_find_entry(cpu, ip, BTB_SETS, BTB_WAYS, basic_btb);

    if (btb_entry == NULL) {
      if ((branch_target != 0) && taken) {
        // no prediction for this entry so far, so allocate one
        uint64_t set = basic_btb_set_index(ip, BTB_SETS);
        auto repl_entry = basic_btb_get_lru_entry(cpu, set, BTB_SETS, BTB_WAYS, basic_btb);

        repl_entry->ip_tag = ip;
        repl_entry->target = branch_target;
        repl_entry->always_taken = 1;
        repl_entry->branch_type = branch_type;
        basic_btb_update_lru(cpu, repl_entry);
      }
    } else {
      // update an existing entry
      btb_entry->target = branch_target;
      if (!taken) {
        btb_entry->always_taken = 0;
      }
    }
  }
}
