#ifndef CHAMPSIM_DRAM_POWER_SCHEDULER_H
#define CHAMPSIM_DRAM_POWER_SCHEDULER_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace champsim::drampower
{
using cycle = std::uint64_t;
enum class operation { activate, precharge, precharge_all, read, write, refresh };
struct target {
  std::size_t rank, group, bank, row, column;
};
struct burst {
  cycle arrival, data_at;
  target address;
  bool write;
};
struct timing {
  cycle command_gap, rcd, ras, rc, rp, ppd, rrd_short, rrd_long, rrd_rank, faw;
  cycle ccd_short, ccd_long, ccd_rank, rtp, wr, wtr_short, wtr_long, rank_turn;
  cycle read_latency, write_latency, beats, preamble, postamble, rfc, refi;
};
struct burst_delays {
  std::uint64_t count = 0;
  long double sum = 0;
  cycle max = 0;
};
struct command {
  cycle when;
  operation op;
  target address;
};

class scheduler
{
  struct bank_state {
    std::optional<std::size_t> row;
    cycle next_activate = 0, next_precharge = 0, next_access = 0;
    std::vector<burst> bursts;
    std::size_t front = 0;
  };

  struct rank_state {
    std::vector<cycle> activate_by_group;
    std::vector<cycle> column_by_group;
    std::vector<cycle> write_to_read;
    std::vector<cycle> recent_activates;
    cycle next_precharge = 0;
  };

  struct candidate {
    cycle when;
    operation op;
    std::size_t flat_bank;
    cycle data_at;
    bool access() const { return op == operation::read || op == operation::write; }
  };

  const timing t;
  const std::size_t groups, banks_per_group;
  std::vector<rank_state> ranks;
  std::vector<bank_state> banks;
  std::vector<command> commands;
  std::vector<std::size_t> busy_banks;
  cycle cursor = 0, data_end = 0, next_column = 0, deadline;
  std::optional<target> previous_data;
  bool previous_write = false;
  bool finished = false;
  burst_delays delays;

  [[nodiscard]] cycle refresh_slot() const { return std::max({t.rp, t.ppd, t.command_gap}) + t.rfc; }
  [[nodiscard]] cycle refresh_start() const { return deadline - ranks.size() * refresh_slot(); }

  void emit(cycle when, operation op, target address)
  {
    commands.push_back({when, op, address});
    cursor = when + t.command_gap;
  }

  void refresh()
  {
    const auto start = refresh_start();
    for (std::size_t rank = 0; rank < ranks.size(); ++rank) {
      const auto slot = start + rank * refresh_slot();
      bool open = false;
      const auto first = rank * groups * banks_per_group;
      for (std::size_t i = first; i < first + groups * banks_per_group; ++i) {
        open |= banks[i].row.has_value();
        banks[i].row.reset();
      }
      if (open)
        emit(slot, operation::precharge_all, {rank, 0, 0, 0, 0});
      emit(slot + refresh_slot() - t.rfc, operation::refresh, {rank, 0, 0, 0, 0});
    }
    cursor = deadline;
    deadline += t.refi;
  }

  [[nodiscard]] cycle latency(const burst& request) const { return request.write ? t.write_latency : t.read_latency; }

  [[nodiscard]] candidate next(std::size_t i) const
  {
    const auto& bank = banks[i];
    const auto& request = bank.bursts[bank.front];
    const auto& addr = request.address;
    const auto& rank = ranks[addr.rank];
    candidate result{std::max(cursor, request.arrival), operation::activate, i, request.data_at};
    if (!bank.row) {
      result.when = std::max(result.when, std::max(bank.next_activate, rank.activate_by_group[addr.group]));
      if (rank.recent_activates.size() == 4)
        result.when = std::max(result.when, rank.recent_activates.front() + t.faw);
    } else if (*bank.row != addr.row) {
      result.op = operation::precharge;
      result.when = std::max({result.when, bank.next_precharge, rank.next_precharge});
    } else {
      result.op = request.write ? operation::write : operation::read;
      const auto delay = latency(request);
      // Admission, bank, command bus and real data bus timing are independent.
      auto earliest_data = request.data_at;
      if (previous_data) {
        const bool change_rank = previous_data->rank != addr.rank;
        const auto turnaround = change_rank || previous_write != request.write ? t.rank_turn + t.preamble + t.postamble : 0;
        earliest_data = std::max(earliest_data, data_end + turnaround);
      }
      result.when = std::max({result.when, bank.next_access, rank.column_by_group[addr.group], next_column,
                              earliest_data > delay ? earliest_data - delay : 0});
      if (!request.write)
        result.when = std::max(result.when, rank.write_to_read[addr.group]);
    }
    return result;
  }

  [[nodiscard]] bool fits(const candidate& action) const
  {
    const auto start = refresh_start();
    if (action.when >= start || action.when + t.command_gap > start)
      return false;
    if (action.op == operation::activate)
      return action.when + std::max(t.ras, t.rc) <= start;
    if (action.op == operation::precharge)
      return action.when + std::max(t.rp, t.ppd) <= start;
    const auto& bank = banks[action.flat_bank];
    const auto& request = bank.bursts[bank.front];
    const auto completion = action.when + latency(request) + t.beats + t.postamble;
    return std::max(completion + (request.write ? t.wr : 0), action.when + t.rtp) <= start;
  }

  void issue(const candidate& action)
  {
    auto& bank = banks[action.flat_bank];
    const auto& request = bank.bursts[bank.front];
    const auto& addr = request.address;
    auto& rank = ranks[addr.rank];
    const auto now = action.when;
    emit(now, action.op, addr);
    switch (action.op) {
    case operation::activate:
      bank.row = addr.row;
      bank.next_activate = now + t.rc;
      bank.next_precharge = now + t.ras;
      bank.next_access = now + t.rcd;
      for (std::size_t i = 0; i < groups; ++i)
        rank.activate_by_group[i] = std::max(rank.activate_by_group[i], now + (i == addr.group ? t.rrd_long : t.rrd_short));
      for (std::size_t r = 0; r < ranks.size(); ++r)
        if (r != addr.rank)
          for (auto& ready : ranks[r].activate_by_group)
            ready = std::max(ready, now + t.rrd_rank);
      if (rank.recent_activates.size() == 4)
        rank.recent_activates.erase(rank.recent_activates.begin());
      rank.recent_activates.push_back(now);
      break;
    case operation::precharge:
      bank.row.reset();
      bank.next_activate = std::max(bank.next_activate, now + t.rp);
      rank.next_precharge = now + t.ppd;
      break;
    case operation::read:
    case operation::write: {
      const auto delay = now + latency(request) - request.data_at;
      ++delays.count;
      delays.sum += delay;
      delays.max = std::max(delays.max, delay);
      data_end = now + latency(request) + t.beats;
      previous_data = addr;
      previous_write = request.write;
      // DRAMPower v6 also places DQ activity at command timestamps.
      next_column = now + t.beats;
      for (std::size_t i = 0; i < groups; ++i) {
        rank.column_by_group[i] = std::max(rank.column_by_group[i], now + (i == addr.group ? t.ccd_long : t.ccd_short));
        if (request.write)
          rank.write_to_read[i] = std::max(rank.write_to_read[i], data_end + (i == addr.group ? t.wtr_long : t.wtr_short));
      }
      for (std::size_t r = 0; r < ranks.size(); ++r)
        if (r != addr.rank) {
          for (auto& ready : ranks[r].column_by_group)
            ready = std::max(ready, now + t.ccd_rank);
          if (request.write)
            for (auto& ready : ranks[r].write_to_read)
              ready = std::max(ready, data_end + t.wtr_long);
        }
      bank.next_precharge = std::max(bank.next_precharge, std::max(data_end + t.postamble + (request.write ? t.wr : 0), now + t.rtp));
      ++bank.front;
      break;
    }
    case operation::precharge_all:
    case operation::refresh:
      break;
    }
  }

public:
  scheduler(timing spec, std::size_t nranks, std::size_t ngroups, std::size_t nbanks)
      : t(spec), groups(ngroups), banks_per_group(nbanks),
        ranks(nranks, rank_state{std::vector<cycle>(groups), std::vector<cycle>(groups), std::vector<cycle>(groups), {}}),
        banks(nranks * ngroups * nbanks), deadline(t.refi)
  {
    const auto access = std::max(t.read_latency, t.write_latency) + t.beats + t.postamble + std::max(t.rtp, t.wr);
    if (!t.rfc || nranks * refresh_slot() + std::max(t.ras, t.rc) + t.rcd + access >= t.refi)
      throw std::invalid_argument("device refresh interval cannot sustain replay");
  }

  void add(burst request)
  {
    const auto& a = request.address;
    const auto i = (a.rank * groups + a.group) * banks_per_group + a.bank;
    auto& pending = banks.at(i).bursts;
    if (pending.empty())
      busy_banks.push_back(i);
    pending.push_back(request);
  }

  cycle schedule(cycle native_end)
  {
    // A bank exposes only its oldest burst. Select the earliest legal
    // command among banks, preferring a data command on a tie.
    while (!busy_banks.empty()) {
      std::optional<candidate> selected;
      for (auto bank : busy_banks) {
        auto action = next(bank);
        if (!fits(action))
          continue;
        if (!selected || std::tuple{action.when, !action.access(), action.data_at} <
                             std::tuple{selected->when, !selected->access(), selected->data_at})
          selected = action;
      }
      if (selected) {
        issue(*selected);
        const auto i = selected->flat_bank;
        if (banks[i].front == banks[i].bursts.size())
          busy_banks.erase(std::find(busy_banks.begin(), busy_banks.end(), i));
      } else {
        refresh();
      }
    }
    return std::max({native_end, cursor, data_end + (previous_data ? t.postamble : 0)});
  }

  [[nodiscard]] const burst_delays& burst_start_delays() const { return delays; }

  const std::vector<command>& finish(cycle end)
  {
    if (finished)
      throw std::logic_error("DRAM command replay was already finalized");
    while (deadline <= end)
      refresh();
    finished = true;
    return commands;
  }
};
} // namespace champsim::drampower

#endif
