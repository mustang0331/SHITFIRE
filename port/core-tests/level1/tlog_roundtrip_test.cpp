#include <string>
#include <vector>

#include <catch2/catch_amalgamated.hpp>

#include "fixtures.h"
#include "shitfire/review/tlog.h"
#include "tlog_json_util.h"

/* [tlog] — the transcript-log schema gate.

   1) Round-trip: every entry of every level3 transcript parses into
      TlogEntry (STRICT — an unknown key is schema drift and fails) and
      re-serializes to an identical JSON object: no field lost, none
      renamed. `wall` is excluded by design: it is browser Date.now()
      metadata (06-clock-comms.js:27), stripped at oracle capture, and
      documented non-parity in shitfire/review/tlog.h — the test asserts no
      fixture carries it.

   2) TranscriptLog behavior: add() stamping/t-rounding/trim
      (06-clock-comms.js:25-37), persistSlice (:43), and the text() renderer
      (:60-76) line formats. */

using shitfire::review::TlogContext;
using shitfire::review::TlogEntry;
using shitfire::review::TranscriptLog;

TEST_CASE("level3 transcripts round-trip through TlogEntry, no field lost or renamed",
          "[tlog]") {
  std::size_t total = 0;
  std::vector<std::string> failures;
  for (const std::string& name : tlogjson::level3Files()) {
    const auto fx = fixtures::load("level3/" + name);
    const auto& tl = fx.at("tlog");
    REQUIRE(tl.is_array());
    for (std::size_t i = 0; i < tl.size(); i++) {
      total++;
      const auto& je = tl[i];
      if (je.contains("wall"))
        failures.push_back(name + "[" + std::to_string(i) +
                           "]: fixture carries 'wall' (expected stripped)");
      auto pr = tlogjson::fromJson(je);
      for (const auto& err : pr.errors)
        failures.push_back(name + "[" + std::to_string(i) + "]: " + err);
      if (!pr.errors.empty()) continue;
      const auto back = tlogjson::toJson(pr.entry);
      if (back != je) {
        /* name the exact keys that drifted */
        for (auto it = je.begin(); it != je.end(); ++it)
          if (!back.contains(it.key()) || back.at(it.key()) != it.value())
            failures.push_back(name + "[" + std::to_string(i) + "]: field '" +
                               it.key() + "' lost or changed on re-serialize");
        for (auto it = back.begin(); it != back.end(); ++it)
          if (!je.contains(it.key()))
            failures.push_back(name + "[" + std::to_string(i) + "]: field '" +
                               it.key() + "' invented on re-serialize");
      }
    }
  }
  for (const auto& f : failures) { INFO(f); }
  WARN("tlog round-trip: " << (total - failures.size()) << "/" << total
       << " entries clean across " << tlogjson::level3Files().size()
       << " transcripts (wall excluded by design)");
  for (const auto& f : failures) UNSCOPED_INFO(f);
  REQUIRE(failures.empty());
}

TEST_CASE("TranscriptLog add() stamps, rounds t, trims, and slices as the source",
          "[tlog]") {
  TranscriptLog log;
  TlogContext ctx;
  ctx.simNow = 30.14;                 /* Math.round(30.14*10)/10 = 30.1 */
  ctx.state = "OBSERVING";
  ctx.seedMission = 101;
  ctx.chapterId = "1.1";
  log.add(ctx, "sys", "", "hello", TlogEntry{});
  REQUIRE(log.entries().size() == 1);
  CHECK(log.entries()[0].t == 30.1);
  CHECK(log.entries()[0].state == "OBSERVING");
  CHECK(log.entries()[0].seed == 101);
  REQUIRE(log.entries()[0].ch.has_value());
  CHECK(*log.entries()[0].ch == "1.1");
  CHECK_FALSE(log.entries()[0].wall.has_value());   /* non-parity, unset here */

  /* JS Math.round half toward +infinity: 2.25*10 = 22.5 -> 23 -> 2.3 */
  ctx.simNow = 2.25;
  log.add(ctx, "sys", "", "half", TlogEntry{});
  CHECK(log.entries()[1].t == 2.3);

  /* :34-35 — front trim to max */
  log.maxEntries = 3;
  log.add(ctx, "sys", "", "m3", TlogEntry{});
  log.add(ctx, "sys", "", "m4", TlogEntry{});
  log.add(ctx, "sys", "", "m5", TlogEntry{});
  REQUIRE(log.entries().size() == 3);
  CHECK(log.entries()[0].msg == "m3");
  CHECK(log.entries()[2].msg == "m5");

  /* :43 — persistSlice is the last 800 (all, when fewer) */
  CHECK(log.persistSlice().size() == 3);

  /* :56-59 — clear */
  log.clear();
  CHECK(log.entries().empty());

  /* extras merge (Object.assign) */
  TlogEntry extra;
  extra.dTgt = 229;
  ctx.simNow = 68.1;
  ctx.state = "SHOT";
  log.add(ctx, "impact", "", "adjust round 1", extra);
  REQUIRE(log.entries().size() == 1);
  REQUIRE(log.entries()[0].dTgt.has_value());
  CHECK(*log.entries()[0].dTgt == 229);
}

TEST_CASE("TranscriptLog text() renders each entry kind as 06-clock-comms.js:60-76",
          "[tlog]") {
  const std::string DASH = "\xE2\x80\x94";
  const std::string MIDDOT = "\xC2\xB7";
  const std::string STAR = "\xE2\x98\x85";
  TranscriptLog log;
  TlogContext ctx;
  ctx.simNow = 30.1;
  ctx.state = "OBSERVING";
  ctx.seedMission = 101;
  ctx.chapterId = "1.1";

  /* default kind with a callsign (:74) */
  log.add(ctx, "fdc", "HELLHOUND FIRES", "SHOT, OVER.", TlogEntry{});
  /* parse (:63-64) — null warno renders nothing */
  {
    TlogEntry x;
    x.ptype = "cff";
    x.method.present = true;
    x.method.value = "grid";
    x.warno.present = true;
    x.warno.value = "adjust";
    log.add(ctx, "parse", "", "raw text", x);
  }
  /* impact (:65-67) */
  {
    TlogEntry x;
    x.dTgt = 229;
    ctx.state = "SHOT";
    ctx.simNow = 68.1;
    log.add(ctx, "impact", "", "adjust round 1", x);
  }
  /* impact without dTgt — NET8a legacy: no distance clause */
  log.add(ctx, "impact", "", "illumination round", TlogEntry{});
  /* spot (:68-71) — empty form falls back to 'report' */
  {
    TlogEntry x;
    x.form = "salute";
    log.add(ctx, "spot", "", "NORTH OF THE GUN", x);
  }
  /* aar (:72-73) */
  {
    TlogEntry x;
    x.rounds = 2;
    x.dur = 137;
    x.stars = 5;
    ctx.state = "AAR";
    ctx.simNow = 177.1;
    log.add(ctx, "aar", "", "PASS " + DASH + " TARGET NEUTRALIZED", x);
  }
  /* skirmish entry: null ch drops the tag segment */
  ctx.chapterId = std::nullopt;
  ctx.state = "OBSERVING";
  ctx.simNow = 5;
  ctx.seedMission = 2;
  log.add(ctx, "sys", "", "SKIRMISH", TlogEntry{});

  const std::string txt = log.text();
  std::vector<std::string> lines;
  std::size_t p = 0;
  while (true) {
    const std::size_t q = txt.find('\n', p);
    lines.push_back(txt.substr(p, q == std::string::npos ? q : q - p));
    if (q == std::string::npos) break;
    p = q + 1;
  }
  REQUIRE(lines.size() == 7);
  CHECK(lines[0] == "[0:30 OBSERVING ch1.1 s101] HELLHOUND FIRES: SHOT, OVER.");
  CHECK(lines[1] ==
        "[0:30 OBSERVING ch1.1 s101] " + MIDDOT + " parsed as cff/grid/adjust");
  CHECK(lines[2] == "[1:08 SHOT ch1.1 s101] * adjust round 1 " + DASH +
                        " 229 m from target");
  CHECK(lines[3] == "[1:08 SHOT ch1.1 s101] * illumination round");
  CHECK(lines[4] ==
        "[1:08 SHOT ch1.1 s101] ~ spot-report cue (salute): NORTH OF THE GUN");
  CHECK(lines[5] == "[2:57 AAR ch1.1 s101] == AAR: PASS " + DASH +
                        " TARGET NEUTRALIZED (5" + STAR + ") " + DASH +
                        " 2 adj rds, 2:17");
  CHECK(lines[6] == "[0:05 OBSERVING s2] SKIRMISH");
}
