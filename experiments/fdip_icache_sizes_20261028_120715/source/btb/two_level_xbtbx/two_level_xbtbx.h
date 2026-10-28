#ifndef BTB_TWO_LEVEL_XBTBX_H
#define BTB_TWO_LEVEL_XBTBX_H
#include "litebtb.h"
class two_level_xbtbx : public litebtb_predictor { public: explicit two_level_xbtbx(O3_CPU* cpu); };
#endif
