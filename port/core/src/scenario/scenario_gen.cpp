#include "shitfire/scenario/scenario_state.h"
#include "shitfire/math/jsemantics.h"
#include "shitfire/math/mils.h"
#include "shitfire/math/noise.h"
#include "shitfire/terrain/heightfield.h"
#include "shitfire/terrain/los.h"
#include <cmath>
#include <optional>

/* port of src/js/05-units-scenario.js:224 genScenario (oracle-v1), all 15
   types. PRIME DIRECTIVE: every rng draw in source order — retry loops,
   candidate scans, early-exit guards ported verbatim. The stream (05:225)
   mixes the mission seed WITH the terrain seed, so a scenario only reproduces
   on its island. Mesh installation (placeUnits) is engine-side and excluded;
   everything the oracle snapshot serializer keeps is generated here. */
namespace shitfire { namespace scenario {

namespace {

const double PI = 3.14159265358979323846;

/* UTF-8 for the typographic characters the JS briefs carry. */
const char* const DASH = "\xE2\x80\x94";   /* em dash */
const char* const HELLIP = "\xE2\x80\xA6"; /* horizontal ellipsis */

std::string toLowerAscii(std::string s) {
  for (char& c : s)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
  return s;
}

}  // namespace

/* 05:88 — pathPoint */
WorldXZ pathPoint(const PathState& p, double d) {
  if (p.isLine) return WorldXZ{ p.sx + p.dx * d, p.sz + p.dz * d };
  std::size_t i = 1;
  while (i < p.cum.size() - 1 && p.cum[i] < d) i++;
  double seg = p.cum[i] - p.cum[i - 1];
  if (seg == 0) seg = 1;   /* JS `|| 1` */
  const double t = clamp((d - p.cum[i - 1]) / seg, 0, 1);
  return WorldXZ{ lerp(p.pts[i - 1].x, p.pts[i].x, t),
                  lerp(p.pts[i - 1].z, p.pts[i].z, t) };
}

/* 05:97 — pathDir (returned in x/z fields) */
WorldXZ pathDir(const PathState& p, double d) {
  if (p.isLine) return WorldXZ{ p.dx, p.dz };
  std::size_t i = 1;
  while (i < p.cum.size() - 1 && p.cum[i] < d) i++;
  const double vx = p.pts[i].x - p.pts[i - 1].x;
  const double vz = p.pts[i].z - p.pts[i - 1].z;
  double l = std::hypot(vx, vz);
  if (l == 0) l = 1;
  return WorldXZ{ vx / l, vz / l };
}

namespace {

/* 05:176 — roadConvoyRoute (skirmish convoys drive the built roads). */
std::optional<PathState> roadConvoyRoute(Mulberry32& rng, const GenContext& ctx) {
  const double L = 1800;
  struct Cand {
    const Polyline* r;
    std::vector<double> cum;
    double total;
    bool closed;
  };
  std::vector<Cand> cands;
  for (const auto& r : ctx.world->roads) {
    if (r.size() < 2) continue;
    std::vector<double> cum{ 0 };
    for (std::size_t i = 1; i < r.size(); i++)
      cum.push_back(cum[i - 1] +
                    std::hypot(r[i].x - r[i - 1].x, r[i].z - r[i - 1].z));
    const double total = cum.back();
    if (total < L * 0.75) continue;
    const bool closed =
        r[0].x == r[r.size() - 1].x && r[0].z == r[r.size() - 1].z;
    cands.push_back(Cand{ &r, std::move(cum), total, closed });
  }
  if (cands.empty()) return std::nullopt;
  double wsum = 0;
  for (const auto& c : cands) wsum += c.total;
  const auto at = [](const Cand& c, double d) -> WorldXZ {
    std::size_t i = 1;
    while (i < c.cum.size() - 1 && c.cum[i] < d) i++;
    double seg = c.cum[i] - c.cum[i - 1];
    if (seg == 0) seg = 1;
    const double t = clamp((d - c.cum[i - 1]) / seg, 0, 1);
    return WorldXZ{ lerp((*c.r)[i - 1].x, (*c.r)[i].x, t),
                    lerp((*c.r)[i - 1].z, (*c.r)[i].z, t) };
  };
  std::optional<PathState> best;
  double bestVis = -1;
  for (int tries = 0; tries < 40; tries++) {
    double w = rng.next() * wsum;
    const Cand* c = &cands.back();
    for (const auto& cc : cands) {
      if (w < cc.total) { c = &cc; break; }
      w -= cc.total;
    }
    const double span = std::min(L, c->closed ? c->total * 0.9 : c->total);
    const double d0 =
        rng.next() * (c->closed ? c->total : std::max(c->total - span, 1.0));
    PathState route;
    route.isLine = false;
    for (double d = 0; d <= span; d += 30)
      route.pts.push_back(at(*c, c->closed ? std::fmod(d0 + d, c->total)
                                           : std::min(d0 + d, c->total)));
    route.cum.push_back(0);
    for (std::size_t i = 1; i < route.pts.size(); i++)
      route.cum.push_back(route.cum[i - 1] +
                          std::hypot(route.pts[i].x - route.pts[i - 1].x,
                                     route.pts[i].z - route.pts[i - 1].z));
    route.len = route.cum.back();
    double vis = 0, n = 0;
    for (double d = 0; d <= route.len; d += 150) {
      const WorldXZ q = pathPoint(route, d);
      n++;
      if (hasLOS(*ctx.hf, ctx.eye.x, ctx.eye.y, ctx.eye.z, q.x,
                 ctx.hf->H(q.x, q.z) + 2, q.z))
        vis++;
    }
    if (vis >= std::ceil(n * 8 / 13)) return route;
    if (!best || vis > bestVis) { best = std::move(route); bestVis = vis; }
  }
  return best;
}

}  // namespace

ScenarioState genScenario(const std::string& type, double seed,
                          const GenContext& ctx) {
  /* 05:225 — (seed * 2654435761 ^ CONFIG.SEED.terrain) >>> 0. The product
     exceeds 2^32, so ^ applies ECMA ToInt32 to the double first. */
  const std::uint32_t streamSeed = static_cast<std::uint32_t>(
      js::toInt32(seed * 2654435761.0) ^ js::toInt32(ctx.terrainSeed));
  Mulberry32 rng(streamSeed);
  const MissionConfig& M = ctx.mission;
  const OpPos& op = ctx.op;
  const EyePos& eye = ctx.eye;
  const HeightField& hf = *ctx.hf;
  const WorldLayout& world = *ctx.world;
  const auto H = [&](double x, double z) { return hf.H(x, z); };
  const auto los = [&](double x, double y, double z) {
    return hasLOS(hf, eye.x, eye.y, eye.z, x, y, z);
  };
  const auto gof = [&](double x, double z) { return gridOf(x, z, ctx.map); };
  const auto nearVillage = [&](double x, double z) {
    for (const auto& v : world.villages)
      if (dist2(x, z, v.x, v.z) < v.r + 120) return true;
    return false;
  };

  ScenarioState S;
  S.seed = seed;
  S.type = type;
  S.difficulty = ctx.difficulty;
  const ScnOpts& OPT = ctx.opts;   /* (activeChapter && activeChapter.scn) || {} */
  const bool spread = !ctx.chapterActive;   /* 05:247 */

  /* 05:248 — findSpot, shared by most types. Draw counts per try are equal in
     both modes (2), which is what keeps chapter seeds untouched by WORLD1. */
  const auto findSpot = [&](double rMin, double rMax, double hMin, double hMax,
                            bool needLOS) -> std::optional<WorldXZ> {
    const int tryMax = spread ? 1500 : 300;
    for (int tries = 0; tries < tryMax; tries++) {
      double x, z;
      if (spread) {
        const double az = rng.next() * PI * 2;
        const double r = ctx.islandRadius * std::sqrt(rng.next());
        x = std::sin(az) * r;
        z = -std::cos(az) * r;
        const double d = dist2(x, z, op.x, op.z);
        const double bandLo = tries < tryMax * 0.6
                                  ? std::max(rMin, M.skirmishRangeLo)
                                  : M.skirmishRangeLo;
        const double bandHi = tries < tryMax * 0.6
                                  ? std::min(rMax, M.skirmishRangeHi)
                                  : M.skirmishRangeHi;
        if (d < bandLo || d > bandHi) continue;
      } else {
        const double az = rng.next() * PI * 2;
        const double range = lerp(rMin, rMax, rng.next());
        x = op.x + std::sin(az) * range;
        z = op.z - std::cos(az) * range;
      }
      const double h = H(x, z);
      if (h < hMin || h > hMax) continue;
      if (needLOS && !los(x, h + 2, z)) continue;
      if (nearVillage(x, z)) continue;
      return WorldXZ{ x, z };
    }
    return std::nullopt;
  };
  /* 05:286 — brgTo */
  const auto brgTo = [&](const WorldXZ& p) {
    return fmtMils(std::fmod(
        js::round(radToMils(azTo(op.x, op.z, p.x, p.z)) / 100) * 100, 6400.0));
  };

  if (type == "strongpoint") {   /* 05:288 */
    std::optional<WorldXZ> compound, enemy;
    for (int tries = 0; tries < 300 && !enemy; tries++) {
      const auto c = findSpot(M.targetRangeLo, M.targetRangeHi, 4, 999, true);
      if (!c) break;
      const double eAz = rng.next() * PI * 2;
      const double eOff = lerp(M.enemyOffsetLo, M.enemyOffsetHi, rng.next());
      const double ex = c->x + std::sin(eAz) * eOff;
      const double ez = c->z - std::cos(eAz) * eOff;
      if (H(ex, ez) < 3) continue;
      if (!los(ex, H(ex, ez) + 2, ez)) continue;
      if (dist2(ex, ez, op.x, op.z) < M.skirmishRangeLo) continue;   /* WORLD6 */
      compound = c;
      enemy = WorldXZ{ ex, ez };
    }
    if (!enemy) {
      const double az = rng.next() * PI * 2;
      compound = WorldXZ{ op.x + std::sin(az) * 2200, op.z - std::cos(az) * 2200 };
      enemy = WorldXZ{ compound->x + 200, compound->z };
    }
    S.compound = *compound;
    S.enemy = *enemy;
    S.friendlies.push_back({ compound->x, compound->z, M.fratricideRadius });
    S.brief = "Friendly strongpoint, grid " + gof(compound->x, compound->z) +
              " (grid bearing ~" + brgTo(*compound) +
              " mils), is under assault. Locate the attackers pressing it and "
              "destroy them without hitting the friendlies.";
  } else if (type == "troops") {   /* 05:315 */
    if (OPT.mask) {
      std::optional<WorldXZ> spot;
      for (int tries = 0; tries < 400 && !spot; tries++) {
        const double az = rng.next() * PI * 2, range = lerp(1200, 2600, rng.next());
        const double x = op.x + std::sin(az) * range;
        const double z = op.z - std::cos(az) * range;
        const double h = H(x, z);
        if (h < 3 || h > 60) continue;
        if (los(x, h + 2, z)) continue;   /* must NOT be seen */
        if (nearVillage(x, z)) continue;
        spot = WorldXZ{ x, z };
      }
      if (spot) S.enemy = *spot;
      else {
        const auto fb = findSpot(M.targetRangeLo, M.targetRangeHi, 2, 60, false);
        S.enemy = fb ? *fb : WorldXZ{ op.x + 2000, op.z };
      }
      S.brief = "Enemy infantry assembling IN DEFILADE near grid " +
                gof(S.enemy.x, S.enemy.z) + " " + DASH +
                " you will not see them or your bursts from the tower. Fight "
                "it on the map: plot, fire, listen, adjust off the smoke over "
                "the crest.";
    } else {
      const auto e = findSpot(M.targetRangeLo, M.targetRangeHi, 2,
                              spread ? 999 : 14, true);
      S.enemy = e ? *e : WorldXZ{ op.x + 2000, op.z };
      S.brief = "Enemy infantry in the open, reported grid bearing ~" +
                brgTo(S.enemy) +
                " mils from your OP. A gift. Do not waste it.";
    }
  } else if (type == "wreck") {   /* 05:340 */
    const auto e = findSpot(1100, 2100, 1, 6, true);
    S.enemy = e ? *e : WorldXZ{ op.x + 1500, op.z };
    S.tgtClass = "point";
    S.brief = "Training target: a derelict landing craft on the beach, grid "
              "bearing ~" + brgTo(S.enemy) +
              " mils. It cannot shoot back. Neither could the last observer.";
  } else if (type == "raid") {   /* 05:344 */
    const auto nearVillageSpot = [&](const Village& v,
                                     bool needLOS) -> std::optional<WorldXZ> {
      for (int tries = 0; tries < 300; tries++) {
        const double az = rng.next() * PI * 2, r = 130 + rng.next() * 70;
        const double x = v.x + std::sin(az) * r, z = v.z - std::cos(az) * r;
        if (H(x, z) < 2) continue;
        if (needLOS && !los(x, H(x, z) + 2, z)) continue;
        bool nearHut = false;
        for (const auto& hh : v.huts)
          if (dist2(x, z, hh.x, hh.z) < 90) { nearHut = true; break; }
        if (nearHut) continue;
        return WorldXZ{ x, z };
      }
      return std::nullopt;
    };
    std::optional<WorldXZ> enemy;
    const Village* vil = nullptr;
    bool seen = true;
    for (const auto& v : world.villages) {
      enemy = nearVillageSpot(v, true);
      if (enemy) { vil = &v; break; }
    }
    if (!enemy)
      for (const auto& v : world.villages) {
        enemy = nearVillageSpot(v, false);
        if (enemy) { vil = &v; seen = false; break; }
      }
    if (enemy) S.enemy = *enemy;
    else {
      const auto fb = findSpot(1200, 2800, 2, 60, true);
      S.enemy = fb ? *fb : WorldXZ{ op.x + 1800, op.z };
    }
    S.hasVillage = true;   /* S.village = vil (may be null) */
    if (vil) S.village = *vil;
    S.brief = "Raiders are hitting the civilian village" +
              (vil ? " of " + vil->name : std::string()) + " " + DASH +
              " grid " + gof(S.enemy.x, S.enemy.z) + " area." +
              (seen ? ""
                    : " The village is masked from your tower; work it off the "
                      "map and the smoke.") +
              " The huts are NO-STRIKE: one round inside the village fails the "
              "mission, full stop. Cut the raiders down without touching it.";
  } else if (type == "bunker") {   /* 05:374 */
    auto e = findSpot(1200, 3000, 22, 140, true);
    if (!e) e = findSpot(1200, 3000, 8, 999, true);
    S.enemy = e ? *e : WorldXZ{ op.x + 1800, op.z };
    S.tgtClass = "point";
    S.brief = "Dug-in bunker reported on high ground, grid bearing ~" +
              brgTo(S.enemy) + " mils. The effect bands are tight " + DASH +
              " put it on the roof.";
  } else if (type == "convoy") {   /* 05:379 */
    std::optional<PathState> path;
    if (spread) {
      path = roadConvoyRoute(rng, ctx);   /* WORLD3 */
    } else {
      for (int tries = 0; tries < 400 && !path; tries++) {
        const auto s0 = findSpot(1400, 3200, 1, 10, false);
        if (!s0) break;
        const double paz = rng.next() * PI * 2;
        const double dx = std::sin(paz), dz = -std::cos(paz);
        bool ok = true;
        int visible = 0;
        for (double d = 0; d <= 1800; d += 150) {
          const double x = s0->x + dx * d, z = s0->z + dz * d;
          const double h = H(x, z);
          if (h < 0.5 || h > 16) { ok = false; break; }
          if (los(x, h + 2, z)) visible++;
        }
        if (ok && visible >= 8) {
          PathState p;
          p.isLine = true;
          p.sx = s0->x; p.sz = s0->z; p.dx = dx; p.dz = dz; p.len = 1800;
          path = p;
        }
      }
    }
    if (!path) {
      PathState p;
      p.isLine = true;
      p.sx = op.x + 1500; p.sz = op.z; p.dx = 0; p.dz = -1; p.len = 1800;
      path = p;
    }
    S.path = *path;
    const WorldXZ p0 = pathPoint(*S.path, 0);
    S.speed = 3.5;
    std::vector<Veh> veh;
    for (int i = 0; i < 4; i++) {
      Veh v;
      v.off = i * 40; v.dead = false; v.d = 0; v.x = p0.x; v.z = p0.z;
      veh.push_back(v);
    }
    S.veh = veh;
    S.t0 = ctx.now - 220 / *S.speed;   /* column already strung out */
    S.enemy = WorldXZ{ p0.x, p0.z };
    S.escaped = false;
    /* pit stop (05:412) */
    double stopD = lerp(0.35, 0.6, rng.next()) * S.path->len;
    std::string stopName = "a crew halt";
    double bestFD = 320;
    for (const auto& f : world.facilities) {
      if (f.kind != "fuel" && f.kind != "ammo" && f.kind != "airfield") continue;
      if (!S.path->isLine) {
        for (double t = 300; t <= S.path->len - 300; t += 40) {
          const WorldXZ q = pathPoint(*S.path, t);
          const double fd = dist2(f.x, f.z, q.x, q.z);
          if (fd < bestFD) {
            bestFD = fd;
            stopD = t;
            stopName = "the " + toLowerAscii(f.name);
          }
        }
      } else {
        const double t =
            clamp((f.x - S.path->sx) * S.path->dx + (f.z - S.path->sz) * S.path->dz,
                  300, S.path->len - 300);
        const double fd = dist2(f.x, f.z, S.path->sx + S.path->dx * t,
                                S.path->sz + S.path->dz * t);
        if (fd < bestFD) {
          bestFD = fd;
          stopD = t;
          stopName = "the " + toLowerAscii(f.name);
        }
      }
    }
    StopState stop;
    stop.d = std::max(stopD, 320.0);
    stop.dur = lerp(ctx.worldCfg.convoyStopDurLo, ctx.worldCfg.convoyStopDurHi,
                    rng.next());
    stop.resumed = false;
    stop.name = stopName;
    S.stop = stop;
    S.brief = std::string("Enemy convoy, four vehicles, moving on the ") +
              (!S.path->isLine ? "road" : "low ground") + " near grid " +
              gof(p0.x, p0.z) +
              ". Lead the column and time your fire for effect " + DASH +
              " or catch them when they pull in somewhere. If the head of the "
              "column runs off the end of the road, they are gone.";
  } else if (type == "battery") {   /* 05:434 */
    std::optional<WorldXZ> fb;
    for (int tries = 0; tries < 300 && !fb; tries++)
      fb = findSpot(M.targetRangeLo, M.targetRangeHi, 3, 999, true);
    if (!fb) fb = WorldXZ{ op.x + 1600, op.z };
    std::optional<WorldXZ> gun;
    for (int tries = 0; tries < 600 && !gun; tries++) {
      const double az = rng.next() * PI * 2, r = lerp(1500, 3000, rng.next());
      const double x = fb->x + std::sin(az) * r, z = fb->z - std::cos(az) * r;
      if (H(x, z) < 2) continue;
      const double dOP = dist2(x, z, op.x, op.z);
      if (dOP < M.skirmishRangeLo || dOP > M.skirmishRangeHi) continue;
      if (nearVillage(x, z)) continue;
      gun = WorldXZ{ x, z };
    }
    if (!gun) gun = WorldXZ{ op.x + 2400, op.z + 600 };
    S.enemy = *gun;
    S.fireBase = *fb;
    S.friendlies.push_back({ fb->x, fb->z, M.fratricideRadius });
    S.tgtClass = "point";
    S.posture = "dug-in";
    S.btyCas = 0;
    BtyState bty;
    bty.next = 12 + rng.next() * 8;
    bty.period = lerp(24, 38, rng.next());
    bty.tof = lerp(9, 14, rng.next());
    S.bty = bty;
    S.brief = "Friendly position at grid " + gof(fb->x, fb->z) +
              " is taking sustained indirect fire. The gun is somewhere out "
              "there " + DASH +
              " find it by its signature and its fall of shot, and silence it. "
              "Every volley that lands while you work is friendly casualties "
              "on your clock.";
  } else if (type == "defense") {   /* 05:469 */
    std::optional<WorldXZ> en;
    for (int tries = 0; tries < 500 && !en; tries++) {
      const double az = rng.next() * PI * 2, r = lerp(1000, 1600, rng.next());
      const double x = op.x + std::sin(az) * r, z = op.z - std::cos(az) * r;
      const double h = H(x, z);
      if (h < 2) continue;
      if (!los(x, h + 2, z)) continue;
      if (nearVillage(x, z)) continue;
      en = WorldXZ{ x, z };
    }
    if (!en) en = WorldXZ{ op.x + 1200, op.z };
    S.enemy = *en;
    S.friendlies.push_back({ op.x, op.z, M.fratricideRadius });
    S.aSpeed = 0.7;
    S.hasLastT = true;   /* S.lastT = null */
    S.overrun = false;
    S.brief = "You are the objective. Assault force forming up near grid " +
              gof(en->x, en->z) + ", grid bearing ~" + brgTo(*en) +
              " mils, advancing on YOUR TOWER. Your own position is the "
              "friendly element " + DASH +
              " expect DANGER CLOSE, expect creeping corrections, and do not "
              "drop one on yourself. Break the assault before it reaches the "
              "wire.";
  } else if (type == "twin") {   /* 05:493 */
    const auto e1f = findSpot(M.targetRangeLo, M.targetRangeHi, 2,
                              spread ? 999 : 60, true);
    const WorldXZ e1 = e1f ? *e1f : WorldXZ{ op.x + 1800, op.z };
    std::optional<WorldXZ> e2;
    for (int tries = 0; tries < 500 && !e2; tries++) {
      const double az = rng.next() * PI * 2, r = lerp(700, 1400, rng.next());
      const double x = e1.x + std::sin(az) * r, z = e1.z - std::cos(az) * r;
      const double h = H(x, z);
      if (h < 2) continue;
      const double dOP = dist2(x, z, op.x, op.z);
      if (dOP < M.skirmishRangeLo || dOP > M.skirmishRangeHi) continue;
      if (!los(x, h + 2, z)) continue;
      if (nearVillage(x, z)) continue;
      e2 = WorldXZ{ x, z };
    }
    if (!e2) e2 = WorldXZ{ e1.x + 900, e1.z };
    S.enemy = e1;
    TwinTgt tw;
    tw.x = e2->x; tw.z = e2->z;
    S.twinTgt = tw;
    S.brief = "TWO elements, both in the open and both yours: infantry near "
              "grid " + gof(e1.x, e1.z) + " (grid bearing ~" + brgTo(e1) +
              " mils) and a second group near grid " + gof(e2->x, e2->z) +
              " (~" + brgTo(*e2) +
              " mils). The net will hold two missions at once - once the "
              "second is up, LEAD EVERY TRANSMISSION WITH ITS TARGET NUMBER "
              "(\"TARGET AA7001, LEFT 50, ADD 100, OVER\") or the FDC will "
              "bounce it.";
  } else if (type == "qfp") {   /* 05:517 */
    std::optional<WorldXZ> fb;
    for (int tries = 0; tries < 400 && !fb; tries++)
      fb = findSpot(1400, 2600, 3, 999, true);
    if (!fb) fb = WorldXZ{ op.x + 1600, op.z };
    S.fireBase = *fb;
    S.hasAvenues = true;
    if (OPT.lanes) {
      std::optional<double> aAz;
      for (int tries = 0; tries < 400 && !aAz; tries++) {
        const double az = rng.next() * PI * 2;
        const double ox = fb->x + std::sin(az) * 1100;
        const double oz = fb->z - std::cos(az) * 1100;
        if (H(ox, oz) < 2) continue;
        if (nearVillage(ox, oz)) continue;
        if (dist2(ox, oz, op.x, op.z) < M.skirmishRangeLo) continue;   /* WORLD6 */
        aAz = az;
      }
      if (!aAz) aAz = azTo(fb->x, fb->z, op.x, op.z) + PI;
      S.axisAz = *aAz;
      for (const double r : { 850.0, 550.0, 250.0 })
        S.avenues.push_back(WorldXZ{ fb->x + std::sin(*aAz) * r,
                                     fb->z - std::cos(*aAz) * r });
      S.assaultAv = 0;
      S.enemy = WorldXZ{ fb->x + std::sin(*aAz) * 1100,
                         fb->z - std::cos(*aAz) * 1100 };
    } else {
      for (int k = 0; k < 3; k++) {
        std::optional<WorldXZ> av;
        for (int tries = 0; tries < 400 && !av; tries++) {
          const double az = rng.next() * PI * 2, r = lerp(500, 800, rng.next());
          const double x = fb->x + std::sin(az) * r;
          const double z = fb->z - std::cos(az) * r;
          if (H(x, z) < 2) continue;
          if (nearVillage(x, z)) continue;
          bool close = false;
          for (const auto& a : S.avenues)
            if (dist2(a.x, a.z, x, z) < 300) { close = true; break; }
          if (close) continue;
          if (dist2(x, z, op.x, op.z) < M.skirmishRangeLo) continue;   /* WORLD6 */
          av = WorldXZ{ x, z };
        }
        if (!av) {
          const double awayAz = azTo(op.x, op.z, fb->x, fb->z);
          av = WorldXZ{ fb->x + std::sin(awayAz) * (550 + k * 90),
                        fb->z - std::cos(awayAz) * (550 + k * 90) };
        }
        S.avenues.push_back(*av);
      }
      S.assaultAv =
          std::floor(rng.next() * static_cast<double>(S.avenues.size()));
      const auto& a = S.avenues[static_cast<std::size_t>(*S.assaultAv)];
      S.enemy = WorldXZ{ a.x, a.z };
    }
    S.hHour = 240 + rng.next() * 120;   /* 4-6 min planning window */
    S.friendlies.push_back({ fb->x, fb->z, M.fratricideRadius });
    S.hStarted = false;
    S.aSpeed = 0.7;
    S.hasLastT = true;   /* S.lastT = null */
    S.overrun = false;
    if (OPT.lanes) {
      static const char* const dirs[8] = {
        "north", "northeast", "east", "southeast",
        "south", "southwest", "west", "northwest",
      };
      const int di = static_cast<int>(
          std::fmod(js::round(radToMils(*S.axisAz) / 800), 8.0));
      S.brief = "WARNING ORDER. A counterattack on the position at grid " +
                gof(fb->x, fb->z) + " (grid bearing ~" + brgTo(*fb) +
                " mils) steps off at H-HOUR, canalized down the draw from "
                "the " + dirs[di] + ". PHASE LINES on the axis " + DASH +
                " RED near grid " + gof(S.avenues[0].x, S.avenues[0].z) +
                ", WHITE near " + gof(S.avenues[1].x, S.avenues[1].z) +
                ", BLUE near " + gof(S.avenues[2].x, S.avenues[2].z) +
                ". Plan targets in DEPTH while it is quiet; when they cross "
                "RED, two words open the fight.";
    } else {
      static const char* const names[3] = { "ALPHA", "BRAVO", "CHARLIE" };
      std::string list;
      for (std::size_t i = 0; i < S.avenues.size(); i++) {
        if (i) list += ", ";
        list += std::string(names[i]) + " near grid " +
                gof(S.avenues[i].x, S.avenues[i].z);
      }
      S.brief = "WARNING ORDER. An assault on the friendly position at grid " +
                gof(fb->x, fb->z) + " (grid bearing ~" + brgTo(*fb) +
                " mils) is expected at H-HOUR " + DASH +
                " the clock is running. Likely avenues of approach: " + list +
                ". Build your QUICK FIRE PLAN while it is quiet: \"PLAN "
                "TARGET, GRID " + HELLIP +
                "\" files a numbered target the guns hold data for " + DASH +
                " a planned target fires off TWO WORDS when they come. A "
                "target of opportunity still costs the full call.";
    }
  } else if (type == "callin") {   /* 05:590 */
    std::optional<WorldXZ> en;
    for (int tries = 0; tries < 400 && !en; tries++)
      en = findSpot(M.targetRangeLo, M.targetRangeHi, 2, spread ? 999 : 60, true);
    if (!en) en = WorldXZ{ op.x + 2000, op.z };
    std::optional<WorldXZ> team;
    for (int tries = 0; tries < 300 && !team; tries++) {
      const double az = rng.next() * PI * 2, r = lerp(180, 350, rng.next());
      const double x = en->x + std::sin(az) * r, z = en->z - std::cos(az) * r;
      if (H(x, z) < 2) continue;
      if (nearVillage(x, z)) continue;
      team = WorldXZ{ x, z };
    }
    if (!team) team = WorldXZ{ en->x + 250, en->z };
    S.enemy = *en;
    S.team = *team;
    S.friendlies.push_back({ team->x, team->z, M.fratricideRadius });
    const double azTT = azTo(team->x, team->z, en->x, en->z);
    const double dTT = dist2(team->x, team->z, en->x, en->z);
    const double roll = rng.next();
    const std::string wrong =
        roll < 0.4 ? "clean" : roll < 0.7 ? "dirflip" : "dist";
    const double repAz = wrong == "dirflip" ? azTT + PI : azTT;
    double repDist;
    if (wrong == "dist") {
      const double factor = rng.next() < 0.5 ? lerp(0.15, 0.3, rng.next())
                                             : lerp(2.5, 4, rng.next());
      repDist = js::round(dTT * factor / 50) * 50;
    } else {
      repDist = js::round(dTT / 50) * 50;
    }
    CallerState caller;
    caller.i = std::floor(rng.next() * 4);
    caller.wrong = wrong;
    caller.repAz = repAz;
    caller.repDist = repDist;
    S.caller = caller;
    S.brief = std::string("A friendly team is up on your net asking for fire ") +
              DASH +
              " no grid, no format, just a problem. Get their position, weigh "
              "their report against your own glass, and build the call "
              "yourself. The FDC still expects doctrine from YOU.";
  } else if (type == "occupied") {   /* 05:629 */
    const bool facMode = rng.next() < 0.3 && !world.facilities.empty();
    if (facMode) {
      const auto& f = world.facilities[static_cast<std::size_t>(std::floor(
          rng.next() * static_cast<double>(world.facilities.size())))];
      std::optional<WorldXZ> spot;
      for (int tries = 0; tries < 200 && !spot; tries++) {
        const double az = rng.next() * PI * 2, r = 20 + rng.next() * 40;
        const double x = f.x + std::sin(az) * r, z = f.z - std::cos(az) * r;
        if (H(x, z) < 1) continue;
        spot = WorldXZ{ x, z };
      }
      S.enemy = spot ? *spot : WorldXZ{ f.x + 30, f.z };
      OccupiedState oc;
      oc.kind = "facility"; oc.name = f.name; oc.x = f.x; oc.z = f.z;
      S.occupied = oc;
      S.brief = "Enemy element has occupied " + f.name + " " + DASH +
                " it is on your map sheet by name. Confirm the holding "
                "through the glass and take it back with steel.";
    } else {
      /* settlement mode — the pick draw happens even on an empty list */
      const double pickIdx =
          std::floor(rng.next() * static_cast<double>(world.villages.size()));
      Village vil;
      if (!world.villages.empty())
        vil = world.villages[static_cast<std::size_t>(pickIdx)];
      else {
        vil.name = "NOWHERE";
        vil.x = op.x + 2000; vil.z = op.z; vil.r = 70;
      }
      /* WORLD4 — the ring draws from ONE ~70-degree seeded arc */
      std::vector<WorldXZ> ring;
      const double arc0 = rng.next() * PI * 2;
      for (int tries = 0; tries < 600 && ring.size() < 3; tries++) {
        const double az = arc0 + (rng.next() - 0.5) * 1.2;
        const double r = vil.r * (0.9 + rng.next() * 0.5);
        const double x = vil.x + std::sin(az) * r, z = vil.z - std::cos(az) * r;
        if (H(x, z) < 1.5) continue;
        bool nearHut = false;
        for (const auto& hh : vil.huts)
          if (dist2(x, z, hh.x, hh.z) < 42) { nearHut = true; break; }
        if (nearHut) continue;
        ring.push_back(WorldXZ{ x, z });
        if (ring.size() == 3) {
          const double cx = (ring[0].x + ring[1].x + ring[2].x) / 3;
          const double cz = (ring[0].z + ring[1].z + ring[2].z) / 3;
          for (const auto& hh : vil.huts)
            if (dist2(cx, cz, hh.x, hh.z) < 42) { ring.clear(); break; }
        }
      }
      while (ring.size() < 3) {
        const double fx = vil.x + vil.r + 40 + static_cast<double>(ring.size()) * 15;
        const double fz = vil.z;
        bool nearHut = false;
        for (const auto& hh : vil.huts)
          if (dist2(fx, fz, hh.x, hh.z) < 42) { nearHut = true; break; }
        if (nearHut)
          ring.push_back(WorldXZ{
              vil.x, vil.z - vil.r - 40 - static_cast<double>(ring.size()) * 15 });
        else
          ring.push_back(WorldXZ{ fx, fz });
      }
      S.enemy = WorldXZ{ (ring[0].x + ring[1].x + ring[2].x) / 3,
                         (ring[0].z + ring[1].z + ring[2].z) / 3 };
      OccupiedState oc;
      oc.kind = "village"; oc.name = vil.name; oc.x = vil.x; oc.z = vil.z;
      oc.hasRing = true; oc.ring = ring;
      S.occupied = oc;
      S.hasVillage = true;
      S.village = vil;
      /* JS reads the nonexistent key `vil.town` (towns carry `tier`), so the
         brief always says "village" — ported as-is. */
      S.brief = "Enemy element has occupied the village of " + vil.name + " " +
                DASH +
                " find it on your sheet by name. The huts are NO-STRIKE and "
                "the villagers are still in them: one round inside the "
                "settlement fails the mission, full stop. The occupiers hold "
                "the edge. Cut them off it.";
    }
    S.tgtClass = "personnel";
    S.posture = "dug-in";
  } else if (type == "chow") {   /* 05:697 */
    std::optional<WorldXZ> site;
    for (int tries = 0; tries < 300 && !site; tries++) {
      const double az = rng.next() * PI * 2, range = lerp(1200, 2200, rng.next());
      const double x = op.x + std::sin(az) * range;
      const double z = op.z - std::cos(az) * range;
      const double h = H(x, z);
      if (h < 2 || h > 9) continue;
      if (!los(x, h + 2, z)) continue;
      if (nearVillage(x, z)) continue;
      site = WorldXZ{ x, z };
    }
    if (!site) site = WorldXZ{ op.x + 1400, op.z };
    const double fAz = rng.next() * PI * 2, fOff = 150 + rng.next() * 60;
    double ex = site->x + std::sin(fAz) * fOff;
    double ez = site->z - std::cos(fAz) * fOff;
    if (H(ex, ez) < 1) { ex = site->x + 160; ez = site->z; }
    S.enemy = WorldXZ{ ex, ez };
    S.bbq = *site;
    S.friendlies.push_back({ site->x, site->z, 60 });
    S.brief = "The general is grilling at grid " + gof(site->x, site->z) +
              ". A seagull flock, several hundred strong, is assaulting the "
              "pit from the tideline " + DASH + " massing near grid " +
              gof(ex, ez) +
              ". The COOKS are a NO-FIRE line: one round on Private Dombrowski "
              "or his potato salad is FRATRICIDE, same rules, no appeals. This "
              "close to the pit you WILL say DANGER CLOSE. Destroy the flock.";
  } else if (type == "kaiju") {   /* 05:723 */
    Village vil;
    bool haveVil = false;
    if (!world.villages.empty()) {
      vil = world.villages[0];
      haveVil = true;
      for (const auto& v : world.villages)
        if (dist2(v.x, v.z, op.x, op.z) < dist2(vil.x, vil.z, op.x, op.z))
          vil = v;
    } else {
      vil.bare = true;   /* JS fallback { x: 0, z: 0 } */
      vil.x = 0; vil.z = 0;
    }
    const double outAz = std::atan2(vil.x, vil.z);   /* away from island centre */
    double sx = vil.x, sz = vil.z;
    int steps = 0;
    while (H(sx, sz) > -4 && steps++ < 400) {
      sx += std::sin(outAz) * 25;
      sz += std::cos(outAz) * 25;
    }
    sx += std::sin(outAz) * 300;
    sz += std::cos(outAz) * 300;
    const double len = std::max(dist2(sx, sz, vil.x, vil.z) - 60, 400.0);
    const double dxn = (vil.x - sx) / dist2(sx, sz, vil.x, vil.z);
    const double dzn = (vil.z - sz) / dist2(sx, sz, vil.x, vil.z);
    PathState p;
    p.isLine = true;
    p.sx = sx; p.sz = sz; p.dx = dxn; p.dz = dzn; p.len = len;
    S.path = p;
    S.speed = 2.1;
    S.t0 = ctx.now;
    S.enemy = WorldXZ{ sx, sz };
    S.hasVillage = true;
    S.village = vil;
    (void)haveVil;
    S.ashore = false;
    S.tgtClass = "point";
    S.effScale = 2;
    S.armor = 3;
    S.brief = "A crab, dark red, approximately the size of a church, is wading "
              "ashore toward the village of " +
              (vil.name.empty() ? std::string("the coast") : vil.name) +
              " from grid " + gof(sx, sz) + ". It is a MOVING TARGET " + DASH +
              " lead it. It is also a crab. The village is NO-STRIKE as "
              "always. Stop it before landfall; nobody will discuss this "
              "afterwards.";
  } else if (type == "assault") {   /* 05:753 */
    const auto e = findSpot(1600, 3000, 4, 60, true);
    S.enemy = e ? *e : WorldXZ{ op.x + 2000, op.z };
    const double oAz = azTo(S.enemy.x, S.enemy.z, op.x, op.z);
    const double off = OPT.fClose ? 520 : 900;
    S.fStart = WorldXZ{ S.enemy.x + std::sin(oAz) * off,
                        S.enemy.z - std::cos(oAz) * off };
    S.fAdvAz = azTo(S.fStart->x, S.fStart->z, S.enemy.x, S.enemy.z);
    S.fSpeed = 0.7;
    S.fHold = OPT.fClose ? 120 : 180;
    S.fAdvMax = off - *S.fHold;
    S.ft0 = ctx.now;
    S.brief = "Friendly infantry advancing from grid " +
              gof(S.fStart->x, S.fStart->z) +
              " onto an enemy-held objective, grid bearing ~" + brgTo(S.enemy) +
              " mils." +
              (OPT.fClose
                   ? " The lines are TANGLED " + std::string(DASH) +
                         " friendlies are already inside normal safe distances "
                         "and still moving. DANGER CLOSE proword, creeping "
                         "corrections, zero slack."
                   : std::string(
                         " This WILL be danger close: your call must include "
                         "the proword DANGER CLOSE, and a short round lands on "
                         "our own people."));
  }
  /* 05:772 — legacy effR knob */
  if (OPT.effR != 0) S.effScale = OPT.effR / 60;

  /* 05:775 — registration / known points */
  for (int k = 0; k < 2; k++) {
    for (int tries = 0; tries < 200; tries++) {
      const double az = rng.next() * PI * 2;
      const double range = 900 + rng.next() * 2100;
      const double x = op.x + std::sin(az) * range;
      const double z = op.z - std::cos(az) * range;
      if (H(x, z) < 3) continue;
      if (!los(x, H(x, z) + 2, z)) continue;
      if (!S.kps.empty() && dist2(S.kps[0].x, S.kps[0].z, x, z) < 800) continue;
      if (dist2(x, z, S.enemy.x, S.enemy.z) < 400) continue;
      Kp kp;
      kp.id = std::to_string(1001 + k);
      kp.name = KP_NAMES[static_cast<std::size_t>(
          std::fmod(seed + k, static_cast<double>(KP_NAME_COUNT)))];
      kp.x = x;
      kp.z = z;
      S.kps.push_back(kp);
      break;
    }
  }
  S.rng = rng;   /* the live stream travels with the scenario */
  return S;
}

} }
