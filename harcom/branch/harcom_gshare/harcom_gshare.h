#ifndef CHAMPSIM_HARCOM_GSHARE_H
#define CHAMPSIM_HARCOM_GSHARE_H

#include <memory>

#include "modules.h"

class harcom_gshare : public champsim::modules::branch_predictor
{
  struct circuit;
  std::unique_ptr<circuit> hardware;

public:
  explicit harcom_gshare(O3_CPU* cpu);
  harcom_gshare(harcom_gshare&&) noexcept;
  ~harcom_gshare();

  void initialize_branch_predictor();
  bool predict_branch(champsim::address ip);
  void last_branch_result(champsim::address ip, champsim::address target, bool taken, uint8_t branch_type);
  [[nodiscard]] champsim::energy_model_snapshot energy_snapshot() const;
};

#endif
