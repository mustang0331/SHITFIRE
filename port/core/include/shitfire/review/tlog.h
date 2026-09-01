#pragma once
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

/* The transcript log — port of TLOG (src/js/06-clock-comms.js:14-77,
   oracle-v1): the structured session transcript every net transmission,
   parse result, impact and AAR outcome is appended to, with sim-time and
   mission context. STABLE interface: field names below are the recorded
   JSON keys, verbatim (the level3 fixtures are dumps of TLOG.entries).

   Persistence (localStorage 'shitfire_transcript', debounced save :38-46,
   BUILD3-guarded load :47-55) is browser I/O and stays with the platform
   layer — (de)serialization of entries is the caller's; persistSlice()
   hands back the exact last-800 window the JS persists.

   The `wall` field (Date.now(), :27) exists in the browser but is
   NON-PARITY metadata: the oracle capture strips it, no fixture carries it,
   and nothing scored reads it. It is kept as an optional stamp so a UE
   platform layer can fill it.

   All bodies live in tlog.cpp. JS numbers stay double. */
namespace shitfire { namespace review {

/* JS object-field tri-state: absent from the entry, present as null, or
   present with a value. Needed because e.g. a parse entry ALWAYS carries
   `method`, possibly null (12-cff-handling.js:907), while a sys entry
   carries no `method` key at all. */
template <typename T>
struct JsProp {
  bool present = false;
  std::optional<T> value;   /* nullopt while present == recorded JSON null */
};

/* One TLOG entry (06-clock-comms.js:26-31 base + Object.assign extras).
   The extras block is the union of every TLOG.add call site at oracle-v1:
     parse  — ptype/method/warno/ffe        (12-cff-handling.js:907-908)
     impact — dTgt, shell when not 'he'     (10-mission.js:1130-1132,:1154-1156)
     spot   — form/diff/scn                 (07-voice-spot-audio.js:456,:476)
     caller — no extras                     (07-voice-spot-audio.js:528)
     sys    — selfLocErr (posrep, 10-mission.js:279), noDisp (dispersion
              toggle, 16-input.js:207), none (09-camera.js:253, log() lines)
     syserr — err (05-units-scenario.js:1039/:1053, 10-mission.js:401/:1165,
              12-cff-handling.js:904), len (oversize refusal, 12-cff:892)
     note   — no extras                     (12-cff-handling.js:855)
     aar    — the graded-outcome payload    (13-aar.js:364-379)
   Field names must never be renamed — they are the recorded schema. */
struct TlogEntry {
  /* base, stamped by add() */
  double t = 0;                       /* Math.round(sim.now * 10) / 10 */
  std::optional<double> wall;         /* non-parity (see header note) */
  std::string state;                  /* the setState() vocabulary string */
  double seed = 0;                    /* CONFIG.SEED.mission */
  std::optional<std::string> ch;      /* chapter id; nullopt = recorded null */
  std::string kind;
  std::string call;                   /* '' where the JS passes falsy */
  std::string msg;
  /* extras — parse */
  std::optional<std::string> ptype;
  JsProp<std::string> method;
  JsProp<std::string> warno;
  std::optional<bool> ffe;
  /* extras — impact / aar shell */
  std::optional<double> dTgt;
  std::optional<std::string> shell;
  /* extras — spot */
  std::optional<std::string> form;
  std::optional<std::string> scn;
  std::optional<std::string> diff;
  /* extras — sys / syserr */
  std::optional<double> selfLocErr;
  std::optional<bool> noDisp;         /* also on aar when dispersion was off */
  std::optional<std::string> err;
  std::optional<double> len;
  /* extras — aar (13-aar.js:364-379) */
  std::optional<double> rounds;       /* m.adjustRounds */
  std::optional<double> dur;          /* Math.round(dur) */
  std::optional<std::string> outcome;
  std::optional<double> effPct;
  std::optional<std::string> intent;
  std::optional<std::string> sheaf;
  std::optional<std::string> sheafSrc;
  std::optional<std::string> fuze;
  std::optional<std::string> fuzeSrc;
  std::optional<std::string> bdaClaim;
  JsProp<double> firstMiss;           /* rounded, or recorded null */
  JsProp<double> aimErr0;             /* rounded, or recorded null */
  std::optional<double> tInit;        /* Math.round(m.tInit) */
  JsProp<double> corrEff;             /* Math.round(corrEff*100), or null */
  std::optional<bool> bracketed;
  std::optional<std::vector<double>> trace;
  std::optional<double> notes;        /* m.notes.length */
  std::optional<double> stars;        /* absent when noDisp (13-aar.js:379) */
};

/* The sim state add() stamps onto every entry (the JS reads globals:
   sim.now, state, CONFIG.SEED.mission, activeChapter — :27-29). */
struct TlogContext {
  double simNow = 0;
  std::string state;
  double seedMission = 0;
  std::optional<std::string> chapterId;
  std::optional<double> wallClock;    /* non-parity; browser Date.now() */
};

class TranscriptLog {
public:
  std::size_t maxEntries = 2000;      /* TLOG.max (:23) */

  /* :25-37 — add(kind, call, msg, extra): stamp the base fields, merge the
     extras (only the extras fields of `extra` are read; its base fields are
     ignored — the JS spreads extra over a fresh base the same way), append,
     trim the front to maxEntries. The debounced save (:36) is I/O and is
     the caller's. */
  void add(const TlogContext& ctx, const std::string& kind,
           const std::string& call, const std::string& msg,
           const TlogEntry& extra);

  /* :56-59 — clear(). */
  void clear();

  /* The persisted window: entries.slice(-800) (:43). */
  std::vector<TlogEntry> persistSlice() const;

  /* :60-76 — text(): the human-readable render, one line per entry, joined
     with '\n'. Pure over the entries. */
  std::string text() const;

  std::vector<TlogEntry>& entries() { return entries_; }
  const std::vector<TlogEntry>& entries() const { return entries_; }

private:
  std::vector<TlogEntry> entries_;
};

} }
