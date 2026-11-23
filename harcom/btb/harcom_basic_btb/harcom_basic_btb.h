#ifndef CHAMPSIM_HARCOM_BASIC_BTB_H
#define CHAMPSIM_HARCOM_BASIC_BTB_H

#include <memory>

#include "../../../btb/basic_btb/basic_btb.h"

class harcom_basic_btb : public basic_btb
{
  class frontend_model;
  O3_CPU* cpu;
  std::unique_ptr<frontend_model> model;

public:
  explicit harcom_basic_btb(O3_CPU* owner);
  harcom_basic_btb(harcom_basic_btb&&) noexcept;
  ~harcom_basic_btb();
  void initialize_btb();
};

#endif
