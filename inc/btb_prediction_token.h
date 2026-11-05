#ifndef CHAMPSIM_BTB_PREDICTION_TOKEN_H
#define CHAMPSIM_BTB_PREDICTION_TOKEN_H

#include <cstddef>
#include <cstdint>

namespace champsim
{
// Simulator bookkeeping: identifies the occupant observed at prediction time.
// This is not part of modeled BTB storage.
struct btb_prediction_token {
  std::size_t slot{};
  uint64_t generation{};
  const void* owner{};
};
} // namespace champsim

#endif
