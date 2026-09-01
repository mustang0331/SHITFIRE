#pragma once
#include <optional>
#include <string>
#include <vector>

/* The CAMPAIGN content table — port of src/js/15-campaign-menu.js:10-183
   (oracle-v1): every volume and chapter with its seeds, pars, flags, unlock
   thresholds, difficulty restrictions, asset overrides, scenario options and
   narrative strings, as compiled-in data. Data only; the menu/briefing UI,
   TTS and the tutorial STEP ENGINE (15-campaign-menu.js:338-378) are
   presentation and stay with their own porters.

   Volume unlock thresholds are 0/0/4/9/15/20/Infinity (UX11 — lowered from
   6/14/24/32 by author ruling 2026-08-27, 15-campaign-menu.js:6-9;
   CONTENT_INVENTORY.md §1 agrees). All 26 implemented chapters carry
   impl:true; Volume V has zero chapters (locked tease, :179-180).

   All bodies live in campaign_data.cpp (port/README.md: nothing computational
   inline in a core header). JS numbers stay double throughout. */
namespace shitfire { namespace campaign {

/* One tutorial step row (15-campaign-menu.js:17-23 F.1, :30-43 F.2). The
   step engine that consumes these — event matching, check() closures, the
   dynamic say() of F.2's last step (:43, names tutLandmark()) — is browser
   behavior for the tutorial porter; this struct carries only the DATA of
   each row. */
struct TutStep {
  std::string on;                 /* 'look'|'binos'|'lase'|'map'|'library'|'msg' */
  std::optional<double> amt;      /* :18 — 'look' only (amt: 5) */
  std::string say;                /* static prompt; "" when the source uses a
                                     say() closure (:43) — see sayIsDynamic */
  std::string missSay;            /* :33 — 'msg' only; "" = none */
  bool hasCheck = false;          /* :20/:22/:34 — a check() closure exists */
  bool sayIsDynamic = false;      /* :43 — say is a function of tutLandmark() */
};

/* chapter.tut (15-campaign-menu.js:15, :28). */
struct TutSpec {
  bool noMission = false;         /* suppresses live fire missions (:337) */
  std::string done;               /* completion line (:16, :29) */
  std::vector<TutStep> steps;
};

/* chapter.scn — scenario options consumed by genScenario
   (15-campaign-menu.js: 2.3 mask, 3.5 fClose, 4.5 lanes;
   CONTENT_INVENTORY.md §2 lists effR as engine-live but chapterless). */
struct ScnOptsSpec {
  bool mask = false;
  bool fClose = false;
  bool lanes = false;
  double effR = 0;                /* 0 = unset (no current chapter sets it) */
};

/* One CAMPAIGN chapter row (15-campaign-menu.js:12-177). */
struct ChapterSpec {
  std::string id;                 /* 'F.1' ... 'E.3' */
  std::string title;
  std::string blurb;
  bool impl = true;               /* every shipped row is impl:true */
  std::string type;               /* SCN_TYPES key */
  double seed = 0;                /* CONFIG.SEED.mission for the chapter (:539) */
  std::optional<double> par;      /* seconds; F.1/F.2 none — gradeMission
                                     falls back to 300 (:282) */
  std::string method;             /* '' | 'polar' (2.1) | 'shift' (2.2) */
  bool noLaser = false;           /* 2.1 (:68) */
  std::string tod;                /* '' | 'night'|'dawn'|'dusk' (chapter ToD
                                     overrides volume ToD, :561) */
  bool strict = false;            /* 4.1, 4.4 */
  std::string asset;              /* '' | 'mortar60' (4.2) | 'sunlamp' (E.3) */
  bool reqIllum = false;          /* 2.5 (:90) */
  bool reqImmediate = false;      /* 3.6 (:119) */
  bool reqPlanned = false;        /* 4.5 (:149) */
  std::vector<std::string> diffs; /* empty = all; 4.3/4.4 ['hard'] */
  ScnOptsSpec scn;
  std::string story;              /* briefing text (optionally TTS'd) */
  std::string outro;              /* one-line AAR narrative beat */
  std::vector<std::string> coach; /* GUNNY BOTTLECAP hint lines; empty = none */
  std::optional<TutSpec> tut;     /* F.1/F.2 only */
};

/* One CAMPAIGN volume row (15-campaign-menu.js:11, :53, :67, :97, :129,
   :156, :179). */
struct VolumeSpec {
  std::string id;                 /* 'F','V1','V2','V3','V4','EP','V5' */
  std::string tab;                /* spine label */
  std::string name;
  double need = 0;                /* total stars to unlock; V5 = +Infinity */
  std::optional<double> tseed;    /* per-volume terrain seed; V5 none */
  std::string palette;            /* '' | 'black' (V4, :129) */
  std::string tod;                /* '' | 'overcast' (V4 volume-wide, :129) */
  std::string tease;              /* V5 locked tease (:180); '' otherwise */
  std::vector<ChapterSpec> chapters;
};

/* CAMPAIGN (15-campaign-menu.js:10-181) — insertion order preserved. */
const std::vector<VolumeSpec>& campaignVolumes();

/* ALL_CHAPTERS (15-campaign-menu.js:182) — flatMap over volumes, in order.
   Pointers into campaignVolumes() storage (stable statics). */
const std::vector<const ChapterSpec*>& allChapters();

/* Lookup by chapter id ('1.1'); nullptr when unknown. */
const ChapterSpec* findChapter(const std::string& id);

/* c._vol back-reference (15-campaign-menu.js:183): the volume owning a
   chapter id; nullptr when unknown. */
const VolumeSpec* volumeOf(const std::string& chapterId);

} }
