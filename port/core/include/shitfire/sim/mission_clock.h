#pragma once
#include <functional>
#include <vector>

/* Sim clock + event scheduler — port of src/js/06-clock-comms.js:2-13
   (oracle-v1). The TLOG and FDC.say live with the wave-2 campaign/FDC
   porters; only the clock and the sorted event queue are here.

   The splice-insert scan and the equal-time tie-break are LOAD-BEARING
   (P2 brief): an event scheduled at time t is inserted AFTER every event
   with t' <= t (the scan only steps left past strictly-greater times), so
   equal-time events run in scheduling order, and the drain re-checks the
   front after every callback — a callback scheduling at t <= now runs in
   the same drain, exactly as the JS while loop does. */
namespace shitfire { namespace sim {

/* port of src/js/06-clock-comms.js:2 (oracle-v1) — const sim = { now: 0 }.
   The single injection point: the harness (17-loop-boot.js:73 in the
   browser; the replay driver here) writes now, nothing else does. */
struct SimClock { double now = 0; };

class EventQueue {
public:
  /* port of src/js/06-clock-comms.js:4-9 (oracle-v1) — schedule(t, fn):
     backwards scan while events[i-1].t > t, then splice-insert at i
     (std::vector::insert at the same index). */
  void schedule(double t, std::function<void()> fn);

  /* port of src/js/06-clock-comms.js:10-12 (oracle-v1) — runEvents():
     while (events.length && events[0].t <= sim.now) events.shift().fn().
     shift-before-call preserved: the callback sees itself already removed
     and may schedule freely. clock.now is re-read every iteration. */
  void run(const SimClock& clock);

  /* port of src/js/06-clock-comms.js:13 (oracle-v1) — clearEvents(). */
  void clear();

  std::size_t size() const;

private:
  struct Ev { double t; std::function<void()> fn; };
  std::vector<Ev> events_;
};

} }
