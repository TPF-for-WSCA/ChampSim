#undef CHAMPSIM_MODULE
#include "bimodal.h"
#include "ooo_cpu.h"

void bimodal::initialize_branch_predictor()
{
  if (intern_ && !energy)
    energy = intern_->attach_predictor_energy(intern_->make_direction_energy, champsim::predictor_kind::bimodal);
}

bool bimodal::predict_branch(champsim::address ip)
{
  if (energy)
    energy->record(champsim::predictor_event::lookup, 0);
  auto value = bimodal_table[hash(ip)];
  return value.value() > (value.maximum / 2);
}

void bimodal::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
  if (energy)
    energy->record(champsim::predictor_event::resolve, 0);
  bimodal_table[hash(ip)] += taken ? 1 : -1;
}
