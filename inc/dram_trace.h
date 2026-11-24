#ifndef CHAMPSIM_DRAM_TRACE_H
#define CHAMPSIM_DRAM_TRACE_H

#include <fstream>
#include <functional>
#include <string>
#include <string_view>

#include "chrono.h"

class MEMORY_CONTROLLER;

namespace champsim
{
class dram_trace
{
  std::ofstream output;
  std::function<void(std::string_view)> sink;
  void write(std::string_view line);
  void write_header(const MEMORY_CONTROLLER& controller);
  bool enabled = false;

public:
  dram_trace(const std::string& path, const MEMORY_CONTROLLER& controller);
  dram_trace(std::function<void(std::string_view)> stream, const MEMORY_CONTROLLER& controller);
  [[nodiscard]] bool streaming() const { return static_cast<bool>(sink); }
  void begin(chrono::clock::time_point time);
  void end(chrono::clock::time_point time);
  void event(std::string_view kind, chrono::clock::time_point time, std::size_t channel, std::size_t rank = 0,
             std::size_t bankgroup = 0, std::size_t bank = 0, std::size_t row = 0, std::size_t column = 0);
};
} // namespace champsim

#endif
