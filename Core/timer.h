#pragma once

#include <chrono>
#include <deque>
#include <numeric>
#include <optional>
#include <type_traits>

using CPUClockPoint =
    std::chrono::time_point<std::chrono::high_resolution_clock>;

template <typename T>
concept DurationConvertableFromNano =
    requires { typename std::common_type<T, std::chrono::nanoseconds>::type; };

class Timer;

class TimerGuard {
private:
  Timer *timer;
  std::optional<CPUClockPoint> start;

public:
  explicit TimerGuard(Timer &timer)
      : timer{&timer}, start{std::chrono::high_resolution_clock::now()} {}
  TimerGuard(TimerGuard &) = delete;
  TimerGuard &operator=(TimerGuard &) = delete;
  TimerGuard(TimerGuard &&) = default;
  TimerGuard &operator=(TimerGuard &&) = default;
  ~TimerGuard();

  void Discard() {
      start = std::nullopt;
  }
};

class Timer {
private:
  static constexpr int MAX_NUM_ELAPSED_TIME = 10;
  std::deque<std::chrono::nanoseconds> elapsedTimes;

public:
  Timer() = default;

  template <typename T = std::chrono::nanoseconds>
    requires DurationConvertableFromNano<T>
  std::optional<T> GetLastElapsed() const {
    if (this->elapsedTimes.empty()) {
      return {};
    }
    return std::chrono::duration_cast<T>(this->elapsedTimes.back());
  }

  template <typename T = std::chrono::nanoseconds>
    requires DurationConvertableFromNano<T>
  std::optional<T> GetElapsed() const {
    if (this->elapsedTimes.empty()) {
      return {};
    }
    auto it = this->elapsedTimes.cbegin();
    auto acc = *it;
    return std::chrono::duration_cast<T>(
        (std::accumulate(++it, this->elapsedTimes.cend(), acc) /
         this->elapsedTimes.size()));
  }

  inline TimerGuard MakeGuard() { return TimerGuard{*this}; }

  inline static CPUClockPoint Now() {
    return std::chrono::high_resolution_clock::now();
  }

  void RecordElapsed(CPUClockPoint start, CPUClockPoint end);
};

#ifdef TIMER_MAIN
Timer GLOBAL_TIMER;
#else
extern Timer GLOBAL_TIMER;
#endif

class TimerView {
private:
  const Timer *timer;
  double timeMultiplier;

public:
  TimerView(const Timer &timer) : TimerView{timer, 1.0} {}
  TimerView(const Timer &timer, double timerMultiplier)
      : timer{&timer}, timeMultiplier{timerMultiplier} {}

  inline void SetTimeMultiplier(double multiplier) {
    this->timeMultiplier = multiplier;
  }

  template <typename T = std::chrono::nanoseconds>
    requires DurationConvertableFromNano<T>
  std::optional<T> GetLastElapsed() const {
    auto elapsed = timer->GetLastElapsed();
    if (!elapsed) {
      return {};
    }
    auto multiplied = (*elapsed) * timeMultiplier;
    return std::chrono::round<T>(multiplied);
  }

  template <typename T = std::chrono::nanoseconds>
    requires DurationConvertableFromNano<T>
  std::optional<T> GetElapsed() const {
    auto elapsed = timer->GetElapsed();
    if (!elapsed) {
      return {};
    }
    auto multiplied = (*elapsed) * timeMultiplier;
    return std::chrono::round<T>(multiplied);
  }
};