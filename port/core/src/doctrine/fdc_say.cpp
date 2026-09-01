#include "shitfire/doctrine/fdc_say.h"

#include <cmath>
#include <cstdio>
#include <regex>

namespace shitfire { namespace doctrine {

// port of src/js/01-math-terrain.js:176-181 (oracle-v1) — fdcCall()
std::string fdcCallFor(const FdcChapterContext& ch, const FdcCallsigns& calls) {
  const bool m60 = ch.active && ch.asset == sim::AssetKind::Mortar60;
  const bool sun = ch.active && ch.asset == sim::AssetKind::Sunlamp;
  return m60 ? calls.fdc60 : sun ? calls.fdcSun : calls.fdc;
}

// port of src/js/01-math-terrain.js:182 (oracle-v1) —
// fdcCall().replace(/\s+(?:FIRES|ACTUAL)$/, '')
std::string fdcShortFor(const FdcChapterContext& ch,
                        const FdcCallsigns& calls) {
  static const std::regex reSuffix(R"(\s+(?:FIRES|ACTUAL)$)");
  return std::regex_replace(fdcCallFor(ch, calls), reSuffix, "");
}

// port of src/js/10-mission.js:67 (oracle-v1) — sunNet()
bool sunNet(const FdcChapterContext& ch) {
  return ch.active && ch.asset == sim::AssetKind::Sunlamp;
}

std::string jsNum(double v) {
  if (std::isfinite(v) && v == std::floor(v) && std::fabs(v) < 1e15) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(v));
    return buf;
  }
  char buf[40];
  std::snprintf(buf, sizeof buf, "%g", v);
  return buf;
}

std::string upperAscii(const std::string& s) {
  std::string out = s;
  for (char& c : out)
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  return out;
}
std::string lowerAscii(const std::string& s) {
  std::string out = s;
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return out;
}

namespace {
/* G17 delivery-time callsign swap — 06-clock-comms.js:218-225. The JS is a
   /\bHELLHOUND(\s+FIRES)?\b/g replace whose replacement depends on whether
   the FIRES suffix matched: full form -> CONFIG.FDC.fdc60/fdcSun, bare ->
   fdcShort(). std::regex_replace cannot branch on a capture, so the global
   replace is walked manually. */
std::string swapCallsign(const std::string& msg, const FdcChapterContext& ch,
                         const FdcCallsigns& calls) {
  const bool m60 = ch.active && ch.asset == sim::AssetKind::Mortar60;
  const bool sun = ch.active && ch.asset == sim::AssetKind::Sunlamp;
  if (!m60 && !sun) return msg;
  static const std::regex re(R"(\bHELLHOUND(\s+FIRES)?\b)");
  const std::string full = m60 ? calls.fdc60 : calls.fdcSun;
  const std::string shortForm = fdcShortFor(ch, calls);
  std::string out;
  out.reserve(msg.size());
  auto it = std::sregex_iterator(msg.begin(), msg.end(), re);
  const auto end = std::sregex_iterator();
  std::size_t pos = 0;
  for (; it != end; ++it) {
    const std::smatch& m = *it;
    out.append(msg, pos, static_cast<std::size_t>(m.position(0)) - pos);
    out += m[1].matched ? full : shortForm;    /* (m0, f) => f ? full : short */
    pos = static_cast<std::size_t>(m.position(0) + m.length(0));
  }
  out.append(msg, pos, std::string::npos);
  return out;
}
}

FdcSpeaker::FdcSpeaker(sim::SimClock& clock, sim::EventQueue& queue,
                       INetSink& net, const FdcChapterContext& chapter,
                       const FdcCallsigns& calls)
    : clock_(clock), queue_(queue), net_(net), chapter_(chapter),
      calls_(calls) {}

double FdcSpeaker::now() const { return clock_.now; }

// port of src/js/06-clock-comms.js:205-232 (oracle-v1) — FDC.say(msg, opts).
// squelch()/speakFDC() are WebAudio/SpeechSynthesis and are not ported.
double FdcSpeaker::say(const std::string& msg, double delay) {
  const double t = std::fmax(clock_.now, lastT_) + delay;   /* :207 */
  lastT_ = t;                                               /* :208 */
  queue_.schedule(t, [this, msg]() {                        /* :209 */
    /* G17/11c — the swap happens HERE, at delivery, reading the chapter
       context as it stands when the line airs (:218-225). */
    const std::string swapped = swapCallsign(msg, chapter_, calls_);
    lastMsg_ = swapped;                                     /* :226 */
    net_.line(NetLineKind::Fdc, fdcCallFor(chapter_, calls_), swapped); /* :228 */
  });
  return t;                                                 /* :231 */
}

// port of src/js/12-cff-handling.js:875 (oracle-v1) — NET18 stale-backlog cap.
void FdcSpeaker::capBacklog() {
  lastT_ = std::fmin(lastT_, clock_.now + 5.0);
}

} }
