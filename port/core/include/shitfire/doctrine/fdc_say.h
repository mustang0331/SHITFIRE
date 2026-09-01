#pragma once
#include <string>

#include "shitfire/doctrine/parser_message.h"
#include "shitfire/sim/mission_ballistics.h"   /* AssetKind */
#include "shitfire/sim/mission_clock.h"

/* The FDC say-queue — port of src/js/06-clock-comms.js:200-233 (oracle-v1) —
   plus the net-output seam the wave-2 FDC layer speaks through.

   The timing chain is the load-bearing piece (events.h sink contract):
     t = max(sim.now, lastT) + delay;  lastT = t;  deliver at t;  RETURN t.
   fireAdjustRound/fireForEffect schedule impacts at tShot + tof off that
   return value, so lastT lives HERE and is shared by every line the FDC
   speaks — machine-emitted lines and cff-rule lines alike.

   Everything textual (delivery-time callsign swap, lastMsg for SAY AGAIN)
   is 06-clock-comms.js:205-232 verbatim; the audio calls (squelch, speakFDC/
   hiss) are WebAudio/SpeechSynthesis and are not ported (assignment scope).
   Bodies in fdc_say.cpp (port/README.md: nothing computational inline). */
namespace shitfire { namespace doctrine {

/* port of src/js/00-config.js:169-171 (oracle-v1) — CONFIG.FDC callsign
   strings (the timing keys live in sim::FdcTunables; tunables.h left the
   strings to this layer on purpose). */
struct FdcCallsigns {
  std::string obs = "MUSTANG 12";
  std::string fdc = "HELLHOUND FIRES";
  std::string fdc60 = "HACKSAW FIRES";
  std::string fdcSun = "SUNLAMP ACTUAL";
};

/* The chapter state the FDC layer reads (the browser reads the activeChapter
   global). The campaign layer mutates one instance in place; the speaker
   reads it at DELIVERY time (G17: a line queued during a chapter transition
   names whoever owns the net when it airs, 06-clock-comms.js:217-225). */
struct FdcChapterContext {
  bool active = false;                              /* activeChapter truthy */
  sim::AssetKind asset = sim::AssetKind::Battery155; /* activeChapter.asset */
  bool strict = false;                              /* activeChapter.strict */
  std::string requiredMethod;                       /* activeChapter.method, "" = none */
  bool reqImmediate = false;                        /* activeChapter.reqImmediate */
};

/* fdcCall() / fdcShort() / sunNet() — 01-math-terrain.js:176-182,
   10-mission.js:67. */
std::string fdcCallFor(const FdcChapterContext& ch, const FdcCallsigns& calls);
std::string fdcShortFor(const FdcChapterContext& ch, const FdcCallsigns& calls);
bool sunNet(const FdcChapterContext& ch);

/* JS Number-to-string for the template literals in this layer: integral
   values print without a decimal point (the way every number that reaches
   these strings prints in the browser); non-integral values print shortest-
   round-trip-ish via %g. Text-only — nothing scored reads it. */
std::string jsNum(double v);

/* ASCII-only toUpperCase()/toLowerCase() — the net vocabulary is ASCII. */
std::string upperAscii(const std::string& s);
std::string lowerAscii(const std::string& s);

/* Where delivered net traffic goes (the browser's log()/TLOG.add sites).
   The campaign porter's TLOG implements this; a headless harness records it.
   Implementations must not throw (core compiles exceptions-off). */
enum class NetLineKind {
  Fdc,      /* log(fdcCall(), msg, 'fdc') — delivered FDC speech */
  Sys,      /* log('', msg, 'sys') — synchronous system/coach line */
  Obs,      /* log(CONFIG.FDC.obs, raw.toUpperCase(), 'obs') — observer echo */
  Caller,   /* callerLine(cs, msg) — NET1 caller-net traffic (07:526-529) */
  Note,     /* TLOG.add('note', '', text) — NET7 verbatim tester note */
};
class INetSink {
public:
  virtual ~INetSink() = default;
  virtual void line(NetLineKind kind, const std::string& call,
                    const std::string& text) = 0;
  /* TLOG.add('parse', '', raw, {...}) — 12-cff-handling.js:907. */
  virtual void parseClass(const std::string& raw, const TlogParseClass& pc) = 0;
  /* TLOG.add('spot', '', loc, { form }) — 07-voice-spot-audio.js:456/:476.
     diff/scn metadata is scenario state the TLOG owner reads directly. */
  virtual void spotMeta(const std::string& loc, const std::string& form) = 0;
  /* TLOG.add('syserr', '', msg, { len }) — 12-cff-handling.js:892. */
  virtual void sysErr(const std::string& msg, double num) = 0;
  /* TLOG.add('sys', '', `posrep ...`, { selfLocErr }) — 10-mission.js:279. */
  virtual void posRepMeta(const std::string& digits, double selfLocErr) = 0;
};

/* port of src/js/06-clock-comms.js:200-233 (oracle-v1) — const FDC = {...}.
   One instance per net; the FdcMissionSink and the FdcEngine share it. */
class FdcSpeaker {
public:
  FdcSpeaker(sim::SimClock& clock, sim::EventQueue& queue, INetSink& net,
             const FdcChapterContext& chapter, const FdcCallsigns& calls);

  /* FDC.say(msg, {delay}) — queue at max(now, lastT) + delay, advance lastT,
     schedule delivery (callsign swap + lastMsg update + net line at t),
     return t. Default delay 0.9 (06:206). */
  double say(const std::string& msg, double delay = 0.9);

  /* NET18 backlog cap — 12-cff-handling.js:875:
     FDC.lastT = Math.min(FDC.lastT, sim.now + 5). */
  void capBacklog();

  double lastT() const { return lastT_; }
  const std::string& lastMsg() const { return lastMsg_; }
  double now() const;

  /* Test seam only: preset the chain state. */
  void setLastT(double t) { lastT_ = t; }

private:
  sim::SimClock& clock_;
  sim::EventQueue& queue_;
  INetSink& net_;
  const FdcChapterContext& chapter_;
  FdcCallsigns calls_;
  double lastT_ = 0;        /* FDC.lastT (06:201) */
  std::string lastMsg_;     /* FDC.lastMsg (06:202), set at delivery (06:226) */
};

} }
