#ifndef PYTHIA_UPSTREAM_COMPAT_H
#define PYTHIA_UPSTREAM_COMPAT_H
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <strings.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "champsim.h"
namespace pythia_impl {
using namespace std;
inline constexpr unsigned DRAM_BW_LEVELS = 4;
inline constexpr unsigned CACHE_ACC_LEVELS = 10;
}
#endif
