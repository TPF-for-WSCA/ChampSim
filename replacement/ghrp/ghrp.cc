#include <algorithm>
#include <iterator>
#include <set>

#include "cache.h"
#include "util.h"

#define NUM_PRED_TABLES 3
#define NUM_COUNTS 4096
#define BYPASS_THRESH 2
#define DEAD_THRESH 2

int cntrsNew[NUM_PRED_TABLES] = {0};
int predTables[NUM_COUNTS][NUM_PRED_TABLES] = {0};
int indices[NUM_PRED_TABLES] = {0};
int hash_functions[NUM_PRED_TABLES] = {0b0111, 0b1110, 0b1101};

uint16_t history_reg = 0;

void update_path_hist(uint64_t PC)
{
  history_reg <<= 4;
  history_reg |= (PC & 0b111);
}

int generate_signature(uint64_t PC)
{
  uint16_t signature = (0xF & PC) xor history_reg;
  return signature;
}

void update_indices(uint16_t signature)
{
  for (int i = 0; i < NUM_PRED_TABLES; i++) {
    int hash_map = hash_functions[i];
    int index = 0;
    int shift_counter = 0;
    for (int j = 0; j < 4; j++) {
      if (hash_map & 0b1) {
        index |= (((signature >> (j * 4)) & 0xFF) << (shift_counter * 4));
        shift_counter++;
      }
      hash_map >>= 1;
    }
    indices[i] = index;
  }
}

void get_counters(int* counters)
{
  for (int i = 0; i < NUM_PRED_TABLES; i++) {
    counters[i] = predTables[indices[i]][i];
  }
}

bool majority_vote(int* counters, int threshold)
{
  int vote = 0;
  for (int i = 0; i < NUM_PRED_TABLES; i++) {
    if (counters[i] > threshold) {
      vote += 1;
    }
  }
  return vote >= (NUM_PRED_TABLES / 2);
}

void updatePredTables(bool isDead)
{
  for (int i = 0; i < NUM_PRED_TABLES; i++) {
    if (isDead) {
      predTables[indices[i]][i]++;
    } else {
      predTables[indices[i]][i]--;
    }
  }
}

void CACHE::initialize_replacement() {}

uint8_t CACHE::get_insert_pos(LruModifier lru_modifier, uint32_t set)
{
  if (lru_modifier >= 100000)
    return NUM_WAY - active_inserts;
  if (lru_modifier >= 10000)
    return NUM_WAY - 4;
  else if (lru_modifier >= 1000)
    return NUM_WAY - 3;
  else if (lru_modifier >= 100)
    return NUM_WAY - 2;
  else if (lru_modifier >= 10)
    return NUM_WAY - 1;
  return 0;
}

uint8_t CACHE::get_lru_offset(LruModifier lru_modifier)
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

bool CACHE::is_default_lru(LruModifier lru_modifier)
{
  if (lru_modifier == DEFAULT or (lru_modifier > 10 and lru_modifier % 10 == 1))
    return true;
  return false;
}

// find replacement victim
uint32_t CACHE::find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{ // TODO: GHRP
  // baseline LRU
  for (int i = 0; i < NUM_WAY; i++) {
    if (current_set[i].dead) {
      return i;
    }
  }
  return std::distance(current_set, std::max_element(current_set, std::next(current_set, NUM_WAY), lru_comparator<BLOCK, BLOCK>()));
}

// called on every cache hit and cache fill
void CACHE::update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type,
                                     uint8_t hit)
{
  if (hit && type == WRITEBACK)
    return;

  auto signature = generate_signature(ip);
  update_indices(signature);
  int counters[NUM_PRED_TABLES];
  get_counters(counters);

  bool ghrp_active = false;
  if (not hit) {
    bool bypass = majority_vote(counters, BYPASS_THRESH);
    if (not bypass) {
      auto begin = std::next(block.begin(), set * NUM_WAY);
      auto end = std::next(begin, NUM_WAY);
      auto victim = block.end();
      for (; begin != end; begin++) {
        if (begin->dead) {
          victim = begin;
          break;
        }
      }
      if (victim != block.end()) {
        update_indices(victim->signature);
        updatePredTables(true);
        bool prediction = majority_vote(counters, DEAD_THRESH);
        victim->dead = prediction;
        victim->signature = signature;
        ghrp_active = true;
      }
    }
  } else {
    auto hit_block = &block[set * NUM_WAY + way];
    update_indices(hit_block->signature);
    updatePredTables(false);
    bool prediction = majority_vote(counters, DEAD_THRESH);
    hit_block->dead = prediction;
    hit_block->signature = signature;
  }

  update_path_hist(ip);

  uint32_t lru_pos = get_insert_pos(lru_modifier, set);
  if (hit)
    lru_pos = 0;
  if (type == FILL && lru_modifier >= 10 && not is_default_lru(lru_modifier)) {
    auto begin = std::next(block.begin(), set * NUM_WAY);
    auto end = std::next(begin, lru_subset.second);
    begin = std::next(begin, lru_subset.first);
    uint32_t waycount = NUM_WAY;
    std::for_each(begin, end, [lru_pos, waycount](BLOCK& x) {
      if (lru_pos >= x.lru && x.lru - 1 < waycount) {
        x.lru++;
      }
    });
    std::next(begin, way)->lru = lru_pos;
  } else {
    auto begin = std::next(block.begin(), set * NUM_WAY);
    auto end = std::next(begin, NUM_WAY);
    uint32_t hit_lru = std::next(begin, way)->lru;
    auto num_ways = NUM_WAY;
    std::for_each(begin, end, [hit_lru, num_ways, lru_pos](BLOCK& x) {
      if (x.lru >= num_ways)
        return;
      if (x.lru <= hit_lru and not lru_pos)
        x.lru++;
      else if (lru_pos and x.lru >= lru_pos)
        x.lru++; // shift upwards if we insert at not mru
    });

    std::next(begin, way)->lru = lru_pos;
  }
}

void CACHE::replacement_final_stats() {}
