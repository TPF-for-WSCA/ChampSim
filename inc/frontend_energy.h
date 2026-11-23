#ifndef CHAMPSIM_FRONTEND_ENERGY_H
#define CHAMPSIM_FRONTEND_ENERGY_H

#include <cstddef>
#include <string>
#include <vector>

#include "energy_stats.h"

namespace champsim
{
// Events refer to hardware actions. The simulator remains responsible for their results.
enum class frontend_event {
  dib_lookup, dib_hit, dib_fill, ftq_read, ftq_write, ftq_push, ftq_consume, ftq_pop, ftq_redirect, fdip_slot, btb_lookup, btb_promote, btb_fill,
  ittage_lookup, ittage_train, ittage_tagged_write, ittage_base_write, ittage_useful_read, ittage_useful_write, ittage_useful_reset,
  ittage_chooser_write, ittage_pressure_write, ittage_history_mix, ittage_history_bit,
  ras_push, ras_pop, ras_calibration_pop, ras_return, ras_speculative_return, ras_calibration_write, ras_checkpoint
};

class frontend_energy_model
{
public:
  virtual ~frontend_energy_model() = default;
  // Return-stack reads and calibration writes carry the call-size index. Other
  // events carry the number of affected slots, bits, or operations.
  virtual void record(frontend_event event, std::size_t argument) = 0;
  [[nodiscard]] virtual std::vector<energy_model_snapshot> energy_snapshots() const = 0;
  [[nodiscard]] virtual std::vector<std::string> coverage_gaps() const = 0;
};
} // namespace champsim

#endif
