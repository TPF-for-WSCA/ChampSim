#ifndef BRANCH_TAGE_SC_L_H
#define BRANCH_TAGE_SC_L_H

#include <memory>

#include "modules.h"

// Seznec's 64 KiB TAGE-SC-L, from Daniel A. Jimenez's ChampSim fork.
class tage_sc_l : public champsim::modules::branch_predictor
{
  struct state;
  std::unique_ptr<state> predictor;

public:
  explicit tage_sc_l(O3_CPU* cpu);
  ~tage_sc_l();
  tage_sc_l(tage_sc_l&&) noexcept;
  tage_sc_l& operator=(tage_sc_l&&) noexcept;

  bool predict_branch(champsim::address ip);
  void last_branch_result(champsim::address ip, champsim::address target, bool taken, uint8_t branch_type);
};

#endif
