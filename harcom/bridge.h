#ifndef CHAMPSIM_HARCOM_BRIDGE_H
#define CHAMPSIM_HARCOM_BRIDGE_H

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#pragma GCC diagnostic ignored "-Wshadow"
#include "vendor/harcom.hpp"
#pragma GCC diagnostic pop

// HARCOM grants its simulator access to the boundary between traced values and modeled signals.
class harcom_superuser
{
public:
  template <hcm::u64 Width>
  static hcm::val<Width> input(hcm::u64 value, hcm::u64 time)
  {
    return {value, time, hcm::panel.default_location()};
  }

  static bool output(hcm::val<1>& value) { return value.fo1().get(); }

  template <hcm::u64 Width>
  static hcm::u64 ready_time(const hcm::val<Width>& value)
  {
    return value.time();
  }

  static void next_cycle() { hcm::panel.next_cycle(); }
};

#endif
