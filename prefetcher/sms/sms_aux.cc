//=======================================================================================//
// File             : sms/sms_aux.cc
// Author           : Rahul Bera, SAFARI Research Group (write2bera@gmail.com)
// Date             : 19/AUG/2025
// Description      : Implements Spatial Memory Streaming prefetcher, ISCA'06
//=======================================================================================//

#include "cache.h"
#include "sms.h"

/* Functions for Filter table */
std::deque<FTEntry*>::iterator sms::search_filter_table(uint64_t page)
{
  return find_if(filter_table.begin(), filter_table.end(), [page](FTEntry* ftentry) { return (ftentry->page == page); });
}

void sms::insert_filter_table(uint64_t pc, uint64_t page, uint32_t offset)
{
  //   stats.ft.insert++;
  FTEntry* ftentry = NULL;
  if (filter_table.size() >= sms::FT_SIZE) {
    auto victim = search_victim_filter_table();
    evict_filter_table(victim);
  }

  ftentry = new FTEntry();
  ftentry->page = page;
  ftentry->pc = pc;
  ftentry->trigger_offset = offset;
  filter_table.push_back(ftentry);
}

std::deque<FTEntry*>::iterator sms::search_victim_filter_table() { return filter_table.begin(); }

void sms::evict_filter_table(std::deque<FTEntry*>::iterator victim)
{
  //   stats.ft.evict++;
  FTEntry* ftentry = (*victim);
  filter_table.erase(victim);
  delete ftentry;
}

/* Functions for Accumulation Table */
std::deque<ATEntry*>::iterator sms::search_acc_table(uint64_t page)
{
  return find_if(acc_table.begin(), acc_table.end(), [page](ATEntry* atentry) { return (atentry->page == page); });
}

void sms::insert_acc_table(FTEntry* ftentry, uint32_t offset)
{
  //   stats.at.insert++;
  ATEntry* atentry = NULL;
  if (acc_table.size() >= sms::AT_SIZE) {
    auto victim = search_victim_acc_table();
    evict_acc_table(victim);
  }

  atentry = new ATEntry();
  atentry->pc = ftentry->pc;
  atentry->page = ftentry->page;
  atentry->trigger_offset = ftentry->trigger_offset;
  atentry->pattern[ftentry->trigger_offset] = 1;
  atentry->pattern[offset] = 1;
  atentry->age = 0;
  for (uint32_t index = 0; index < acc_table.size(); ++index)
    acc_table[index]->age++;
  acc_table.push_back(atentry);
}

std::deque<ATEntry*>::iterator sms::search_victim_acc_table()
{
  uint32_t max_age = 0;
  std::deque<ATEntry*>::iterator it, victim;
  for (it = acc_table.begin(); it != acc_table.end(); ++it) {
    if ((*it)->age >= max_age) {
      max_age = (*it)->age;
      victim = it;
    }
  }
  return victim;
}

void sms::evict_acc_table(std::deque<ATEntry*>::iterator victim)
{
  //   stats.at.evict++;
  ATEntry* atentry = (*victim);
  insert_pht_table(atentry);
  acc_table.erase(victim);

  // cout << "[PHT_INSERT] pc " << hex << setw(10) << atentry->pc
  // 	<< " page " << hex << setw(10) << atentry->page
  // 	<< " offset " << dec << setw(3) << atentry->trigger_offset
  // 	<< " pattern " << BitmapHelper::to_string(atentry->pattern)
  // 	<< endl;

  delete (atentry);
}

void sms::update_age_acc_table(std::deque<ATEntry*>::iterator current)
{
  for (auto it = acc_table.begin(); it != acc_table.end(); ++it) {
    (*it)->age++;
  }
  (*current)->age = 0;
}

/* Functions for Pattern History Table */
void sms::insert_pht_table(ATEntry* atentry)
{
  //   stats.pht.lookup++;
  uint64_t signature = create_signature(atentry->pc, atentry->trigger_offset);

  // cout << "signature " << hex << setw(20) << signature << dec
  // 	<< " pattern " << BitmapHelper::to_string(atentry->pattern)
  // 	<< endl;

  uint32_t set = 0;
  auto pht_index = search_pht(signature, set);
  if (pht_index != pht[set].end()) {
    /* PHT hit */
    // stats.pht.hit++;
    (*pht_index)->pattern = atentry->pattern;
    update_age_pht(set, pht_index);
  } else {
    /* PHT miss */
    if (pht[set].size() >= sms::PHT_ASSOC) {
      auto victim = search_victim_pht(set);
      evcit_pht(set, victim);
    }

    // stats.pht.insert++;
    PHTEntry* phtentry = new PHTEntry();
    phtentry->signature = signature;
    phtentry->pattern = atentry->pattern;
    phtentry->age = 0;
    for (uint32_t index = 0; index < pht[set].size(); ++index)
      pht[set][index]->age = 0;
    pht[set].push_back(phtentry);
  }
}

std::deque<PHTEntry*>::iterator sms::search_pht(uint64_t signature, uint32_t& set)
{
  set = (uint32_t)(signature % sms::PHT_SETS);
  return find_if(pht[set].begin(), pht[set].end(), [signature](PHTEntry* phtentry) { return (phtentry->signature == signature); });
}

std::deque<PHTEntry*>::iterator sms::search_victim_pht(int32_t set)
{
  uint32_t max_age = 0;
  std::deque<PHTEntry*>::iterator it, victim;
  for (it = pht[set].begin(); it != pht[set].end(); ++it) {
    if ((*it)->age >= max_age) {
      max_age = (*it)->age;
      victim = it;
    }
  }
  return victim;
}

void sms::update_age_pht(int32_t set, std::deque<PHTEntry*>::iterator current)
{
  for (auto it = pht[set].begin(); it != pht[set].end(); ++it) {
    (*it)->age++;
  }
  (*current)->age = 0;
}

void sms::evcit_pht(int32_t set, std::deque<PHTEntry*>::iterator victim)
{
  //   stats.pht.evict++;
  PHTEntry* phtentry = (*victim);
  pht[set].erase(victim);
  delete phtentry;
}

uint64_t sms::create_signature(uint64_t pc, uint32_t offset)
{
  uint64_t signature = pc;
  signature = (signature << (sms::REGION_SIZE_LOG - LOG2_BLOCK_SIZE));
  signature += (uint64_t)offset;
  return signature;
}

std::size_t sms::generate_prefetch(uint64_t pc, uint64_t address, uint64_t page, uint32_t offset, std::vector<uint64_t>& pref_addr)
{
  //   stats.generate_prefetch.called++;
  uint64_t signature = create_signature(pc, offset);
  uint32_t set = 0;
  auto pht_index = search_pht(signature, set);
  if (pht_index == pht[set].end()) {
    // stats.generate_prefetch.pht_miss++;
    return 0;
  }

  PHTEntry* phtentry = (*pht_index);
  for (uint32_t index = 0; index < BITMAP_MAX_SIZE; ++index) {
    if (phtentry->pattern[index] && offset != index) {
      uint64_t addr = (page << sms::REGION_SIZE_LOG) + (index << LOG2_BLOCK_SIZE);
      pref_addr.push_back(addr);
    }
  }
  update_age_pht(set, pht_index);
  //   stats.generate_prefetch.pref_generated += pref_addr.size();
  return pref_addr.size();
}

void sms::buffer_prefetch(std::vector<uint64_t> pref_addr)
{
  // cout << "buffering " << pref_addr.size() << " already present " << pref_buffer.size() << endl;
  uint32_t count = 0;
  for (uint32_t index = 0; index < pref_addr.size(); ++index) {
    if (pref_buffer.size() >= sms::PREF_BUFFER_SIZE) {
      break;
    }
    pref_buffer.push_back(pref_addr[index]);
    count++;
  }
  //   stats.pref_buffer.buffered += count;
  //   stats.pref_buffer.spilled += (pref_addr.size() - count);
}

void sms::issue_prefetch()
{
  uint32_t count = 0;
  while (!pref_buffer.empty() && count < sms::PREF_DEGREE) {
    champsim::address pf_addr{pref_buffer.front()};
    const bool success = prefetch_line(pf_addr, true, 0);
    if (!success)
      break;
    else {
      pref_buffer.pop_front();
      count++;
    }
  }
  //   stats.pref_buffer.issued += pref_addr.size();
}