#ifndef BRANCH_PERCEPTRON_H
#define BRANCH_PERCEPTRON_H

#include <array>
#include <bitset>

#include "modules.h"
#include "msl/fwcounter.h"

struct perceptron : champsim::modules::branch_predictor {
  template <std::size_t HISTLEN, std::size_t BITS>
  class internal_perceptron
  {
    using counter_type = champsim::msl::sfwcounter<BITS>;

    counter_type bias{0};
    std::array<counter_type, HISTLEN> weights = {};

  public:
    typename counter_type::value_type predict(std::bitset<HISTLEN> history);

    void update(bool result, std::bitset<HISTLEN> history);
  };

  static constexpr std::size_t PERCEPTRON_HISTORY = 24; // history length for the global history shift register
  static constexpr std::size_t PERCEPTRON_BITS = 8;     // number of bits per weight
  static constexpr std::size_t NUM_PERCEPTRONS = 163;

  std::array<internal_perceptron<PERCEPTRON_HISTORY, PERCEPTRON_BITS>, NUM_PERCEPTRONS> perceptrons; // table of perceptrons
  // ChampSim trains at fetch. Prediction-only lookups (including FDIP and
  // wrong-path replay) must not advance history or retain pending updates.
  std::bitset<PERCEPTRON_HISTORY> global_history;

  using branch_predictor::branch_predictor;

  bool predict_branch(champsim::address ip);
  void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
};

template <std::size_t HISTLEN, std::size_t BITS>
auto perceptron::internal_perceptron<HISTLEN, BITS>::predict(std::bitset<HISTLEN> history) -> typename counter_type::value_type
{
  auto output = bias.value();

  // find the (rest of the) dot product of the history register and the perceptron weights.
  for (std::size_t i = 0; i < std::size(history); i++) {
    if (history[i])
      output += weights[i].value();
    else
      output -= weights[i].value();
  }

  return output;
}

template <std::size_t HISTLEN, std::size_t BITS>
void perceptron::internal_perceptron<HISTLEN, BITS>::update(bool result, std::bitset<HISTLEN> history)
{
  // if the branch was taken, increment the bias weight, else decrement it, with saturating arithmetic
  bias += result ? 1 : -1;

  // for each weight and corresponding bit in the history register...
  auto upd_mask = result ? history : ~history; // if the i'th bit in the history positively
                                               // correlates with this branch outcome,
  for (std::size_t i = 0; i < std::size(upd_mask); i++) {
    // increment the corresponding weight, else decrement it, with saturating arithmetic
    weights[i] += upd_mask[i] ? 1 : -1;
  }
}

#endif
