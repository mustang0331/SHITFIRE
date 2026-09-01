#include "shitfire/review/tlog.h"

#include <cmath>
#include <cstdio>

#include "shitfire/math/jsemantics.h"
#include "shitfire/review/aar_analysis.h"   /* fmtTime (13-aar.js:235) */

/* port of src/js/06-clock-comms.js:14-77 (oracle-v1). */
namespace shitfire { namespace review {

namespace {

/* UTF-8 for the typographic characters text() prints. */
const char* const DASH = "\xE2\x80\x94";   /* em dash */
const char* const MIDDOT = "\xC2\xB7";     /* middle dot */
const char* const STAR = "\xE2\x98\x85";   /* black star */

/* JS number -> string for the values text() interpolates. Every value the
   renderer prints (seed, dTgt, stars, rounds) is integral in practice;
   non-integral doubles fall back to %.17g shortest-ish (JS exponent styling
   is not reproduced — unreachable for recorded data). */
std::string jsNum(double v) {
  if (std::isfinite(v) && std::floor(v) == v && std::fabs(v) < 9.007199254740992e15) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(v));
    return buf;
  }
  char buf[40];
  std::snprintf(buf, sizeof buf, "%.17g", v);
  return buf;
}

/* Copy every extras field of `src` onto `dst` (the Object.assign(e, extra)
   at :32 — no call site's extra carries a base key, so precedence is moot). */
void mergeExtras(TlogEntry& dst, const TlogEntry& src) {
  dst.ptype = src.ptype;
  dst.method = src.method;
  dst.warno = src.warno;
  dst.ffe = src.ffe;
  dst.dTgt = src.dTgt;
  dst.shell = src.shell;
  dst.form = src.form;
  dst.scn = src.scn;
  dst.diff = src.diff;
  dst.selfLocErr = src.selfLocErr;
  dst.noDisp = src.noDisp;
  dst.err = src.err;
  dst.len = src.len;
  dst.rounds = src.rounds;
  dst.dur = src.dur;
  dst.outcome = src.outcome;
  dst.effPct = src.effPct;
  dst.intent = src.intent;
  dst.sheaf = src.sheaf;
  dst.sheafSrc = src.sheafSrc;
  dst.fuze = src.fuze;
  dst.fuzeSrc = src.fuzeSrc;
  dst.bdaClaim = src.bdaClaim;
  dst.firstMiss = src.firstMiss;
  dst.aimErr0 = src.aimErr0;
  dst.tInit = src.tInit;
  dst.corrEff = src.corrEff;
  dst.bracketed = src.bracketed;
  dst.trace = src.trace;
  dst.notes = src.notes;
  dst.stars = src.stars;
}

}  // namespace

/* :25-37 — add(). */
void TranscriptLog::add(const TlogContext& ctx, const std::string& kind,
                        const std::string& call, const std::string& msg,
                        const TlogEntry& extra) {
  TlogEntry e;
  e.t = js::round(ctx.simNow * 10) / 10;              /* :27 */
  e.wall = ctx.wallClock;                             /* :27 — non-parity */
  e.state = ctx.state;                                /* :28 */
  e.seed = ctx.seedMission;                           /* :28 */
  e.ch = ctx.chapterId;                               /* :29 — null outside chapters */
  e.kind = kind;
  e.call = call;                                      /* :30 — call || '' */
  e.msg = msg;
  mergeExtras(e, extra);                              /* :32 — Object.assign */
  entries_.push_back(e);                              /* :33 */
  if (entries_.size() > maxEntries)                   /* :34-35 — front splice */
    entries_.erase(entries_.begin(),
                   entries_.begin() +
                       static_cast<std::ptrdiff_t>(entries_.size() - maxEntries));
  /* :36 — this.save() is debounced browser I/O; the caller persists. */
}

/* :56-59 — clear(). */
void TranscriptLog::clear() { entries_.clear(); }

/* :43 — the persisted window is entries.slice(-800). */
std::vector<TlogEntry> TranscriptLog::persistSlice() const {
  const std::size_t n = entries_.size() > 800 ? entries_.size() - 800 : 0;
  return std::vector<TlogEntry>(entries_.begin() + static_cast<std::ptrdiff_t>(n),
                                entries_.end());
}

/* :60-76 — text(). */
std::string TranscriptLog::text() const {
  std::string out;
  bool first = true;
  for (const TlogEntry& e : entries_) {
    if (!first) out += "\n";                          /* :75 — join('\n') */
    first = false;
    /* :62 — the tag. e.ch is truthy-gated in the JS ('' also skips). */
    std::string tag = "[" + fmtTime(e.t) + " " + e.state;
    if (e.ch && !e.ch->empty()) tag += " ch" + *e.ch;
    tag += " s" + jsNum(e.seed) + "]";
    if (e.kind == "parse") {                          /* :63-64 */
      out += tag + " " + MIDDOT + " parsed as " + (e.ptype ? *e.ptype : "undefined");
      if (e.method.present && e.method.value && !e.method.value->empty())
        out += "/" + *e.method.value;
      if (e.warno.present && e.warno.value && !e.warno.value->empty())
        out += "/" + *e.warno.value;
    } else if (e.kind == "impact") {                  /* :65-67 — NET8a: render
        the distance only when held (legacy illum entries had no dTgt) */
      out += tag + " * " + e.msg;
      if (e.dTgt)
        out += std::string(" ") + DASH + " " + jsNum(*e.dTgt) + " m from target";
    } else if (e.kind == "spot") {                    /* :68-71 — NET8b */
      out += tag + " ~ spot-report cue (" +
             (e.form && !e.form->empty() ? *e.form : "report") + "): " + e.msg;
    } else if (e.kind == "aar") {                     /* :72-73 */
      out += tag + " == AAR: " + e.msg;
      if (e.stars) out += " (" + jsNum(*e.stars) + STAR + ")";
      out += std::string(" ") + DASH + " " + jsNum(e.rounds ? *e.rounds : 0) +
             " adj rds, " + fmtTime(e.dur ? *e.dur : 0);
    } else {                                          /* :74 */
      out += tag + " " + (e.call.empty() ? "" : e.call + ": ") + e.msg;
    }
  }
  return out;
}

} }
