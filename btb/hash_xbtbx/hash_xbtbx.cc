#include "hash_xbtbx.h"
hash_xbtbx::hash_xbtbx(O3_CPU* cpu) : litebtb_predictor(cpu, litebtb_hash::half_xor) {}
