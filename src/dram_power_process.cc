#include "dram_power_process.h"

#include <array>
#include <cerrno>
#include <cmath>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>
#include <spawn.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace champsim
{
namespace
{
struct process_pipe {
  int read_end;
  int write_end;

  process_pipe()
  {
    int endpoints[2];
    if (pipe2(endpoints, O_CLOEXEC) != 0)
      throw std::system_error(errno, std::generic_category(), "DRAMPower pipe");
    read_end = endpoints[0];
    write_end = endpoints[1];
  }

  ~process_pipe()
  {
    if (read_end >= 0)
      close(read_end);
    if (write_end >= 0)
      close(write_end);
  }

  process_pipe(const process_pipe&) = delete;
  process_pipe& operator=(const process_pipe&) = delete;
};

// Turn a closed child input into a diagnostic without changing the
// application's signal disposition or consuming a pre-existing signal.
ssize_t write_event(int fd, std::string_view remaining)
{
  sigset_t blocked, previous, pending;
  sigemptyset(&blocked);
  sigaddset(&blocked, SIGPIPE);
  if (sigprocmask(SIG_BLOCK, &blocked, &previous) != 0)
    throw std::system_error(errno, std::generic_category(), "block SIGPIPE");
  const bool preserve_pending = sigpending(&pending) != 0 || sigismember(&pending, SIGPIPE) == 1;
  const auto count = write(fd, remaining.data(), remaining.size());
  const int error = errno;
  if (count < 0 && error == EPIPE && !preserve_pending) {
    const timespec immediate{};
    while (sigtimedwait(&blocked, nullptr, &immediate) < 0 && errno == EINTR) {
    }
  }
  sigprocmask(SIG_SETMASK, &previous, nullptr);
  errno = error;
  return count;
}
} // namespace

dram_power_process::dram_power_process(const std::string& adapter, const std::string& cli, const std::string& memspec, double toggle)
{
  std::array<std::string, 10> arguments{"python3", adapter, "--events", "-", "--drampower", cli, "--memspec", memspec,
                                        "--summary-stdout", "--toggle=" + nlohmann::json(toggle).dump()};
  std::array<char*, arguments.size() + 1> argv{};
  for (std::size_t i = 0; i < arguments.size(); ++i)
    argv[i] = arguments[i].data();

  process_pipe events;
  process_pipe summary;
  posix_spawn_file_actions_t actions;
  int error = posix_spawn_file_actions_init(&actions);
  if (error == 0) {
    error = posix_spawn_file_actions_adddup2(&actions, events.read_end, STDIN_FILENO);
    if (error == 0)
      error = posix_spawn_file_actions_adddup2(&actions, summary.write_end, STDOUT_FILENO);
    if (error == 0)
      error = posix_spawnp(&child, argv.front(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
  }
  if (error != 0)
    throw std::system_error(error, std::generic_category(), "launch DRAMPower adapter");
  input = std::exchange(events.write_end, -1);
  output = std::exchange(summary.read_end, -1);
}

int dram_power_process::wait() noexcept
{
  int status = 0;
  int result;
  do {
    result = waitpid(child, &status, 0);
  } while (result < 0 && errno == EINTR);
  child = -1;
  return result < 0 || !WIFEXITED(status) ? -1 : WEXITSTATUS(status);
}

dram_power_process::~dram_power_process()
{
  if (input >= 0)
    close(input);
  if (output >= 0)
    close(output);
  if (child >= 0)
    wait();
}

void dram_power_process::flush()
{
  std::string_view remaining{pending};
  while (!remaining.empty()) {
    const auto count = write_event(input, remaining);
    if (count < 0 && errno == EINTR)
      continue;
    if (count <= 0)
      throw std::system_error(count < 0 ? errno : EPIPE, std::generic_category(), "send DRAM events");
    remaining.remove_prefix(static_cast<std::size_t>(count));
  }
  pending.clear();
}

void dram_power_process::send(std::string_view event)
{
  pending.append(event);
  if (pending.size() >= 16384)
    flush();
}

dram_replay_energy_stats dram_power_process::finish()
{
  flush();
  close(std::exchange(input, -1));

  std::string response;
  std::array<char, 4096> buffer{};
  for (;;) {
    const auto count = read(output, buffer.data(), buffer.size());
    if (count < 0 && errno == EINTR)
      continue;
    if (count < 0)
      throw std::system_error(errno, std::generic_category(), "receive DRAMPower summary");
    if (count == 0)
      break;
    response.append(buffer.data(), static_cast<std::size_t>(count));
  }

  const int status = wait();
  if (status != 0)
    throw std::runtime_error("DRAMPower adapter failed with status " + std::to_string(status));

  const auto summary = nlohmann::json::parse(response);
  dram_replay_energy_stats result{summary.at("native_duration_ps").get<double>(), summary.at("replay_duration_ps").get<double>(),
                                  summary.at("replay_energy_joules").get<double>(), {}, summary.at("memspec_sha256").get<std::string>(),
                                  summary.at("drampower_cli_sha256").get<std::string>(), summary.at("dq_toggle_rate_and_duty_cycle").get<double>()};
  if (!std::isfinite(result.native_duration_ps) || result.native_duration_ps <= 0 || !std::isfinite(result.replay_duration_ps)
      || result.replay_duration_ps < result.native_duration_ps || !std::isfinite(result.energy_joules) || result.energy_joules < 0)
    throw std::runtime_error("invalid DRAMPower replay duration or energy");
  for (const auto& channel : summary.at("channels"))
    result.channel_energy_joules.push_back(channel.at("drampower_energy_joules").get<double>());
  return result;
}
} // namespace champsim
