#ifndef BTB_BASIC_BTB_H
#define BTB_BASIC_BTB_H

#include "address.h"
#include "direct_predictor.h"
#include "ittage.h"
#include "modules.h"
#include "return_stack.h"
#include <map>

class basic_btb : champsim::modules::btb
{
  return_stack ras{};
  return_stack speculative_ras{};
  std::pair<uint64_t, long long> speculative_block{~uint64_t{0}, -1};
  ittage indirect{};
  direct_predictor direct{};
  // Unlimited static BTB for ideal lookahead; populated only by observed branches.
  std::map<champsim::address, direct_predictor::btb_entry_t> ideal_entries;

public:
  explicit basic_btb(O3_CPU* cpu);
  basic_btb() : basic_btb(nullptr) {}

  void initialize_btb();
  std::pair<champsim::address, bool> btb_prediction(champsim::address ip);
  void update_btb(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
};

#endif
