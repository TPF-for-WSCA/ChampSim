#include "two_level_geometric_hashed_xbtbx.h"
two_level_geometric_hashed_xbtbx::two_level_geometric_hashed_xbtbx(O3_CPU* cpu) : litebtb_predictor(cpu, litebtb_hash::geometric, true) {}
