#include "shitfire/sim/mission_clock.h"

#include <utility>

namespace shitfire { namespace sim {

// port of src/js/06-clock-comms.js:4-9 (oracle-v1) — the splice-insert scan,
// verbatim: i starts at length, steps left only past strictly-later events,
// insert at i. Equal times therefore keep scheduling order (FIFO).
void EventQueue::schedule(double t, std::function<void()> fn) {
  std::size_t i = events_.size();
  while (i > 0 && events_[i - 1].t > t) i--;
  events_.insert(events_.begin() + static_cast<std::ptrdiff_t>(i),
                 Ev{t, std::move(fn)});
}

// port of src/js/06-clock-comms.js:10-12 (oracle-v1). The front event is
// removed BEFORE its callback runs (events.shift().fn()), and the loop
// condition re-reads both the queue and the clock after every callback, so
// a callback that schedules at t <= now gets drained in this same call.
void EventQueue::run(const SimClock& clock) {
  while (!events_.empty() && events_.front().t <= clock.now) {
    std::function<void()> fn = std::move(events_.front().fn);
    events_.erase(events_.begin());
    fn();
  }
}

// port of src/js/06-clock-comms.js:13 (oracle-v1) — events.length = 0.
void EventQueue::clear() { events_.clear(); }

std::size_t EventQueue::size() const { return events_.size(); }

} }
