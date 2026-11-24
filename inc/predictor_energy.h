#ifndef CHAMPSIM_PREDICTOR_ENERGY_H
#define CHAMPSIM_PREDICTOR_ENERGY_H

#include <vector>

#include "frontend_energy.h"

class O3_CPU;

enum class litebtb_hash { identity, half_xor, full_xor, geometric, pearson, rabin, sha3, zobrist };

namespace champsim
{
enum class predictor_kind { bimodal, perceptron, hashed_perceptron };

enum class predictor_event {
  lookup, resolve, train, threshold_step,
  l1_read, l1_select, l1_promote, l1_write, l1_fill_select,
  l2_read, l2_select, l2_promote, l2_write, l2_fill_select, l2_recheck,
  target_width, decode_l1, decode_l2, invalidate_l1, invalidate_l2,
  utb_read, utb_promote, utb_replace, utb_owner, utb_reference,
  ras_empty
};

struct litebtb_energy_geometry {
  struct way {
    unsigned tag;
    unsigned target;
    bool utb_pointer;
    bool hashed;
    bool reconstruct_from_lookup_pc;
  };
  litebtb_hash hash;
  bool two_level;
  std::size_t sets;
  std::vector<way> ways;
  std::size_t l1_sets;
  std::size_t l1_ways;
  std::size_t utb_entries;
  std::size_t utb_ways;
  unsigned utb_tag_bits;
  bool utb_reference_count;
  std::size_t ras_entries;
};

class predictor_energy_model : public frontend_energy_model
{
public:
  void record(frontend_event, std::size_t) override {}
  virtual void record(predictor_event event, std::size_t argument) = 0;
  [[nodiscard]] std::vector<std::string> coverage_gaps() const override { return {}; }
};

void configure_harcom_predictors(O3_CPU& cpu);
} // namespace champsim

#endif
