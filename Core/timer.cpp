#define TIMER_MAIN

#include "timer.h"
#include <cassert>

using namespace std::chrono_literals;

constexpr auto ELAPSED_UPPER_LIMIT = 1s;
constexpr auto FORCED_ELAPSED_TIME = 16ms; // 1 / 60 sec

TimerGuard::~TimerGuard() {
  if (this->start) {
    auto now = std::chrono::high_resolution_clock::now();
    timer->RecordElapsed(*start, now);
  }
}

void Timer::RecordElapsed(CPUClockPoint start, CPUClockPoint end) {
  if (this->elapsedTimes.size() == MAX_NUM_ELAPSED_TIME) {
    this->elapsedTimes.pop_front();
  }
  auto elapsed = end - start;
  if (ELAPSED_UPPER_LIMIT < elapsed) {
      elapsed = FORCED_ELAPSED_TIME;
  }
  this->elapsedTimes.emplace_back(elapsed);
  assert(this->elapsedTimes.size() <= MAX_NUM_ELAPSED_TIME);
}
