#ifndef BRANCH_TAGE_SC_L_H
#define BRANCH_TAGE_SC_L_H

#include <array>
#include <memory>
#include <optional>

#include "modules.h"

// Seznec's 64 KiB TAGE-SC-L, from Daniel A. Jimenez's ChampSim fork.
class tage_sc_l : public champsim::modules::branch_predictor
{
  struct state;
  std::unique_ptr<state> predictor;
  std::optional<champsim::address> prediction_ip;
  bool prediction_result = false;

protected:
  enum class table {
    bimodal_prediction, bimodal_hysteresis, short_tagged, long_tagged, loop,
    sc_128, sc_256, sc_512, sc_1024, sc_weight, sc_threshold, sc_local1, sc_local2, sc_local3, sc_imli, alternate_chooser, count
  };
  struct table_accesses {
    uint64_t reads = 0;
    uint64_t writes = 0;
  };
  using table_activity = std::array<table_accesses, static_cast<std::size_t>(table::count)>;

  void track_table_activity();
  [[nodiscard]] const table_activity& observed_table_activity() const;

public:
  explicit tage_sc_l(O3_CPU* cpu);
  ~tage_sc_l();
  tage_sc_l(tage_sc_l&&) noexcept;
  tage_sc_l& operator=(tage_sc_l&&) noexcept;

  bool predict_branch(champsim::address ip);
  void last_branch_result(champsim::address ip, champsim::address target, bool taken, uint8_t branch_type);
};

#endif
