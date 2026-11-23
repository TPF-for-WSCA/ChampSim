#ifndef CHAMPSIM_HARCOM_TAGE_SC_L_H
#define CHAMPSIM_HARCOM_TAGE_SC_L_H

#include "../../../branch/tage_sc_l/tage_sc_l.h"

class harcom_tage_sc_l : public tage_sc_l
{
public:
  explicit harcom_tage_sc_l(O3_CPU* cpu);
  [[nodiscard]] champsim::energy_model_snapshot energy_snapshot() const;
};

#endif
