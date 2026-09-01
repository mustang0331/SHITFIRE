#include "shitfire/scenario/world_layout.h"
#include "shitfire/math/jsemantics.h"
#include "shitfire/math/mils.h"
#include "shitfire/math/noise.h"
#include "shitfire/math/prng.h"
#include "shitfire/terrain/heightfield.h"
#include <algorithm>
#include <cmath>
#include <optional>

/* port of the placement half of src/js/04-world.js (oracle-v1).
   PRIME DIRECTIVE: PRNG call order is source order. Streams:
     rng   04:358  wSeed                                    (coast facilities, villages, civilians)
     rng2  04:364  (imul(wSeed,2246822519)+13)>>>0          (towns, connector bows)
     rng3  04:370  (imul(wSeed,3266489917)+0x9E3779B9)>>>0  (rocks/boulders)
     wind  04:929  (wSeed^0x517CC1B7)>>>0
     veg   04:1004 (wSeed^0x9E3779B9)>>>0
   Conditional draws are preserved exactly: a skipped hut consumes 2 draws not
   4; a two-storey town building skips the material draw; boulder i==0 skips
   the radius draw; linkTo draws its bow only after the distance gates pass. */
namespace shitfire { namespace scenario {

const char* const KP_NAMES[KP_NAME_COUNT] = {
  "BREWERY", "LATRINE", "SAWMILL", "CHAPEL", "DERBY", "ICEBOX",
};
const char* const ROCK_NAMES[ROCK_NAME_COUNT] = {
  "THE TEETH", "ANVIL RK", "HOGBACK", "THE KNUCKLES",
  "SPLIT RK", "WIDOW RK", "THE MOLARS", "GRAVEYARD",
};
const char* const SCN_TYPES[SCN_TYPE_COUNT] = {
  "strongpoint", "troops", "bunker", "convoy", "assault", "raid", "wreck",
  "battery", "defense", "callin", "occupied", "qfp", "twin", "chow", "kaiju",
};

/* 04-world.js:10 — strHash. charCodeAt over UTF-16 code units; fixture DEM
   names are ASCII, where code unit == byte. */
std::uint32_t strHash(const std::string& s) {
  std::int32_t h = js::toInt32(2166136261.0);
  for (std::size_t i = 0; i < s.size(); i++) {
    h ^= static_cast<std::int32_t>(static_cast<unsigned char>(s[i]));
    h = js::imul(h, 16777619);
  }
  return static_cast<std::uint32_t>(h);
}

namespace {

const double PI = 3.14159265358979323846;

/* 04-world.js:337 — isleName */
std::string isleName(Mulberry32& rng) {
  static const char* const syl[12] = {
    "ta", "va", "mo", "ra", "hi", "no", "ke", "lu", "pa", "si", "ngo", "fa",
  };
  std::string n;
  const int c = 2 + static_cast<int>(std::floor(rng.next() * 2));
  for (int i = 0; i < c; i++)
    n += syl[static_cast<int>(std::floor(rng.next() * 12))];
  for (char& ch : n) ch = static_cast<char>(ch - ('a' - 'A'));
  return n;
}

/* 04-world.js:928 — rollWind (independent stream; 2 draws) */
Wind rollWind(std::uint32_t wSeed) {
  Mulberry32 r(static_cast<std::uint32_t>(
      static_cast<std::int32_t>(wSeed) ^ js::toInt32(1367065015.0 /*0x517CC1B7*/)));
  Wind w;
  const double from = r.next() * PI * 2;
  w.speed = 1.5 + r.next() * 4.5;
  w.fromMils = std::fmod(js::round(radToMils(from) / 100) * 100, 6400.0);
  w.dx = -std::sin(from) * w.speed;
  w.dz = std::cos(from) * w.speed;
  return w;
}

struct NearRoad { const WorldXZ* p = nullptr; double d = 0; };

/* generic {x,z} target for the dirt-path pairing (04:856) */
struct PathTarget { double x, z; };

/* 04-world.js:988 — buildVegetation, placement loop only. */
void buildVegetation(WorldLayout& out, const HeightField& hf, const OpPos& op,
                     const BatteryPos& battery, const WorldConfig& W,
                     const GfxVegConfig& gfx, std::uint32_t wSeed,
                     bool blackPalette) {
  out.veg.clear();
  if (!gfx.veg) return;
  const bool blk = blackPalette;
  const double budget = js::round(gfx.vegBudget * (blk ? 0.3 : 1));
  Mulberry32 rng(static_cast<std::uint32_t>(
      static_cast<std::int32_t>(wSeed) ^ js::toInt32(2654435769.0 /*0x9E3779B9*/)));

  struct Line { const Polyline* pts; double r; };
  std::vector<Line> lines;
  for (const auto& rd : out.roads) lines.push_back({ &rd, W.roadWidth / 2 + 10 });
  for (const auto& p : out.paths) lines.push_back({ &p, 8 });
  const auto segDist2 = [](double x, double z, double ax, double az,
                           double bx, double bz) {
    const double dx = bx - ax, dz = bz - az;
    double denom = dx * dx + dz * dz;
    if (denom == 0) denom = 1;   /* JS `|| 1` */
    const double t = clamp(((x - ax) * dx + (z - az) * dz) / denom, 0, 1);
    const double px = ax + dx * t - x, pz = az + dz * t - z;
    return px * px + pz * pz;
  };
  const auto onLine = [&](double x, double z) {
    for (const auto& L : lines) {
      const double r2 = L.r * L.r;
      const Polyline& P = *L.pts;
      for (std::size_t i = 1; i < P.size(); i++)
        if (segDist2(x, z, P[i - 1].x, P[i - 1].z, P[i].x, P[i].z) < r2)
          return true;
    }
    return false;
  };
  struct Disc { double x, z, r; };
  std::vector<Disc> discs;
  for (const auto& v : out.villages)
    discs.push_back({ v.x, v.z, (v.r != 0 ? v.r : 120) + 30 });
  for (const auto& f : out.facilities)
    discs.push_back({ f.x, f.z, f.kind == "airfield" ? 320.0 : 100.0 });
  for (const auto& rk : out.rocks)
    discs.push_back({ rk.x, rk.z, (rk.r != 0 ? rk.r : 30) + 20 });
  discs.push_back({ op.x, op.z, 70 });
  discs.push_back({ battery.x, battery.z, 90 });
  const auto inDisc = [&](double x, double z) {
    for (const auto& d : discs)
      if (dist2(x, z, d.x, d.z) < d.r) return true;
    return false;
  };

  const double R = gfx.vegRadius;
  for (double tries = budget * 5;
       tries > 0 && static_cast<double>(out.veg.size()) < budget; tries--) {
    const double az = rng.next() * PI * 2, rr = std::sqrt(rng.next()) * R;
    const double x = op.x + std::sin(az) * rr, z = op.z - std::cos(az) * rr;
    const double h = hf.H(x, z);
    if (h < 2 || h > 130) continue;
    const double sl = std::hypot(hf.H(x + 9, z) - h, hf.H(x, z + 9) - h) / 9;
    if (sl > 0.62) continue;
    const double clump = vnoise(x * 0.012, z * 0.012, 999);
    if (rng.next() > clump * 1.5 - 0.12) continue;
    if (rng.next() > 1 - (rr / R) * (rr / R) * 0.6) continue;
    if (onLine(x, z) || inDisc(x, z)) continue;
    VegPlacement p;
    p.x = x; p.z = z; p.h = h;
    p.type = rng.next() < 0.68 ? 0 : 1;   /* draw order = object-literal order */
    p.s = 0.55 + rng.next() * 0.45;
    p.rot = rng.next() * PI * 2;
    p.t = rng.next();
    out.veg.push_back(p);
  }
}

}  // namespace

WorldLayout buildWorldFeatures(const HeightField& hf, const OpPos& op,
                               const BatteryPos& battery, const WorldConfig& W,
                               const GfxVegConfig& gfx, double terrainSeed,
                               const std::string* demName, bool blackPalette) {
  WorldLayout out;
  const auto H = [&](double x, double z) { return hf.H(x, z); };

  /* 04:357 — wSeed = (terrain ^ (DEM ? strHash(DEM.name) : 0)) >>> 0 */
  const std::uint32_t wSeed = static_cast<std::uint32_t>(
      js::toInt32(terrainSeed) ^
      static_cast<std::int32_t>(demName ? strHash(*demName) : 0u));
  Mulberry32 rng(wSeed);
  out.wind = rollWind(wSeed);   /* 04:359 — independent stream */
  /* 04:364 */
  Mulberry32 rng2(js::toUint32(
      static_cast<double>(js::imul(static_cast<std::int32_t>(wSeed),
                                   static_cast<std::int32_t>(2246822519u))) + 13.0));
  /* 04:370 */
  Mulberry32 rng3(js::toUint32(
      static_cast<double>(js::imul(static_cast<std::int32_t>(wSeed),
                                   static_cast<std::int32_t>(3266489917u))) +
      2654435769.0 /*0x9E3779B9*/));

  /* --- coast road (04:376) — no draws in this whole block ---------------- */
  const int NA = 128;
  std::vector<std::optional<WorldXZ>> ring(NA);
  for (int a = 0; a < NA; a++) {
    const double th = static_cast<double>(a) / NA * PI * 2;
    const double ux = std::cos(th), uz = std::sin(th);
    std::optional<WorldXZ> pt;
    for (double r = 4700; r >= 400; r -= 40) {
      const double h = H(ux * r, uz * r);
      if (h >= 2 && h <= 9) { pt = WorldXZ{ ux * r, uz * r }; break; }
    }
    ring[a] = pt;
  }
  for (int a = 0; a < NA; a++) {   /* gap fill — in place, like the JS */
    if (ring[a]) continue;
    const auto& p = ring[(a + NA - 1) % NA];
    const auto& q = ring[(a + 1) % NA];
    if (p && q) ring[a] = WorldXZ{ (p->x + q->x) / 2, (p->z + q->z) / 2 };
  }
  Polyline road;
  for (const auto& pt : ring)
    if (pt) road.push_back(*pt);
  for (int pass = 0; pass < 2; pass++) {   /* smooth — map() reads the OLD array */
    Polyline next(road.size());
    const std::size_t len = road.size();
    for (std::size_t i = 0; i < len; i++) {
      const WorldXZ& a = road[(i + len - 1) % len];
      const WorldXZ& p = road[i];
      const WorldXZ& b = road[(i + 1) % len];
      next[i] = WorldXZ{ (a.x + p.x * 2 + b.x) / 4, (a.z + p.z * 2 + b.z) / 4 };
    }
    road = std::move(next);
  }
  for (std::size_t i = 0; i < road.size(); i++) {   /* de-water (04:406) */
    const WorldXZ p = road[i];
    if (H(p.x, p.z) >= 1.0) continue;
    double l = std::hypot(p.x, p.z);
    if (l == 0) l = 1;
    const double ux = p.x / l, uz = p.z / l;
    for (double d = 20; d <= 260; d += 20) {
      const double nx = p.x - ux * d, nz = p.z - uz * d;
      if (H(nx, nz) >= 1.0) { road[i] = WorldXZ{ nx, nz }; break; }
    }
  }
  if (road.size() > 8) {
    road.push_back(road[0]);      /* close the loop */
    out.roads.push_back(road);    /* the coast road is the metalled route */
  }
  const auto roadAt = [&](double f) -> WorldXZ {
    const std::size_t len = road.size();
    if (len == 0) return WorldXZ{ 0, 0 };
    const double idx = std::floor(f * static_cast<double>(len - 1));
    if (idx < 0 || idx >= static_cast<double>(len)) return WorldXZ{ 0, 0 };
    return road[static_cast<std::size_t>(idx)];
  };
  const auto inland = [&](const WorldXZ& p, double d) -> WorldXZ {
    double l = std::hypot(p.x, p.z);
    if (l == 0) l = 1;
    return WorldXZ{ p.x - p.x / l * d, p.z - p.z / l * d };
  };
  const auto nearestOnRoad = [&](double tx, double tz) -> std::optional<NearRoad> {
    const WorldXZ* best = nullptr;
    double bd = 1e308;   /* Infinity stand-in; strict < keeps first best */
    for (const auto& p : road) {
      const double d = dist2(p.x, p.z, tx, tz);
      if (d < bd) { bd = d; best = &p; }
    }
    if (!best) return std::nullopt;
    NearRoad nr; nr.p = best; nr.d = bd;
    return nr;
  };
  /* 04:439 — linkTo. The bow draw (rng2) happens AFTER the distance gates and
     BEFORE the water test: a water-rejected route still consumed its draw. */
  const auto linkTo = [&](double tx, double tz,
                          double maxLen) -> std::optional<Polyline> {
    if (road.size() < 9) return std::nullopt;
    const auto a = nearestOnRoad(tx, tz);
    if (!a || a->d > maxLen || a->d < 8) return std::nullopt;
    const double n = clamp(js::round(a->d / 45), 4, 40);
    const double px = -(tz - a->p->z) / a->d, pz = (tx - a->p->x) / a->d;
    const double bow = (rng2.next() - 0.5) * std::min(a->d * 0.16, 70.0);
    Polyline pts;
    for (double s = 0; s <= n; s++) {
      const double t = s / n, w = std::sin(t * PI) * bow;
      const double x = lerp(a->p->x, tx, t) + px * w;
      const double z = lerp(a->p->z, tz, t) + pz * w;
      if (s != 0 && H(x, z) < 0.5) return std::nullopt;   /* no bridges, no fords */
      pts.push_back(WorldXZ{ x, z });
    }
    return pts;
  };
  const auto addRoute = [&](const std::optional<Polyline>& pts, bool asphalt) {
    if (!pts) return false;
    (asphalt ? out.roads : out.paths).push_back(*pts);
    return true;
  };

  /* --- facilities (04:466) — scanBest keeps the FIRST strict-best --------- */
  const auto scanBest =
      [&](const auto& score) -> std::optional<WorldXZ> {
    std::optional<WorldXZ> best;
    double bs = -1e308;
    for (double x = -3600; x <= 3600; x += 180)
      for (double z = -3600; z <= 3600; z += 180) {
        const std::optional<double> s = score(x, z);
        if (s && *s > bs) { bs = *s; best = WorldXZ{ x, z }; }
      }
    return best;
  };

  /* airfield (04:489) — adaptive strip length, winning heading travels with
     the winning site. No draws. */
  {
    struct Af { double x, z, th, half; };
    std::optional<Af> af;
    for (const double half : { 310.0, 240.0, 170.0 }) {
      double bs = -1e308;
      for (double x = -3600; x <= 3600; x += 90)
        for (double z = -3600; z <= 3600; z += 90) {
          const double h = H(x, z);
          if (h < 2 || h > 14) continue;
          struct Bd { double dev, th; };
          std::optional<Bd> bd;
          for (int d = 0; d < 12; d++) {
            const double th = d * PI / 12;
            const double ux = std::sin(th), uz = -std::cos(th);
            const double px = -uz, pz = ux;
            double dev = 0;
            bool wet = false;
            for (double s = -half; s <= half && !wet; s += 60)
              for (double q = -20; q <= 20; q += 20) {
                const double hh = H(x + ux * s + px * q, z + uz * s + pz * q);
                if (hh < 1.0) { wet = true; break; }
                dev = std::max(dev, std::fabs(hh - h));
              }
            if (wet) continue;
            if (!bd || dev < bd->dev) bd = Bd{ dev, th };
          }
          if (!bd || bd->dev > 7.0) continue;
          const double sc = -bd->dev - std::fabs(h - 5) * 0.2;
          if (sc > bs) { bs = sc; af = Af{ x, z, bd->th, half }; }
        }
      if (af) break;   /* longest strip this island holds */
    }
    if (af) {
      const double th = af->th, ux = std::sin(th), uz = -std::cos(th);
      const double perpX = -uz, perpZ = ux;
      /* strip ribbon + buildings are mesh-only; the apron/taxiway POLYLINE is
         plotted road data (04:534-537). */
      Polyline apron;
      for (double s = 0; s <= 4; s++)
        apron.push_back(WorldXZ{ af->x + perpX * (12 + s * 14),
                                 af->z + perpZ * (12 + s * 14) });
      addRoute(apron, true);
      Facility f;
      f.kind = "airfield"; f.name = "AIRFIELD";
      f.x = af->x; f.z = af->z; f.th = th; f.len = af->half * 2;
      out.facilities.push_back(f);
    }
  }
  /* radio mast (04:558) */
  {
    const auto mp = scanBest([&](double x, double z) -> std::optional<double> {
      const double h = H(x, z);
      if (h < 35) return std::nullopt;
      if (dist2(x, z, op.x, op.z) < 900) return std::nullopt;
      return h;
    });
    if (mp) {
      Facility f;
      f.kind = "mast"; f.name = "RDO MAST"; f.x = mp->x; f.z = mp->z;
      out.facilities.push_back(f);
    }
  }
  /* coastal gun (04:572) */
  {
    const auto cg = scanBest([&](double x, double z) -> std::optional<double> {
      const double h = H(x, z);
      if (h < 8 || h > 30) return std::nullopt;
      double l = std::hypot(x, z);
      if (l == 0) l = 1;
      if (H(x + x / l * 240, z + z / l * 240) > 0) return std::nullopt;
      return -std::fabs(h - 16);
    });
    if (cg) {
      Facility f;
      f.kind = "gun"; f.name = "CSTL GUN"; f.x = cg->x; f.z = cg->z;
      out.facilities.push_back(f);
    }
  }
  /* fuel point + ammo depot (04:587) — draws 1 and 2 of world stream 1 */
  if (road.size() > 8) {
    const double fFrac = 0.1 + rng.next() * 0.3;
    const double aFrac = std::fmod(fFrac + 0.4 + rng.next() * 0.2, 1.0);
    const WorldXZ fp = inland(roadAt(fFrac), 30);
    const WorldXZ ap = inland(roadAt(aFrac), 34);
    if (H(fp.x, fp.z) > 1) {
      Facility f;
      f.kind = "fuel"; f.name = "FUEL PT"; f.x = fp.x; f.z = fp.z;
      out.facilities.push_back(f);
    }
    if (H(ap.x, ap.z) > 1) {
      Facility f;
      f.kind = "ammo"; f.name = "AMMO DEPOT"; f.x = ap.x; f.z = ap.z;
      out.facilities.push_back(f);
    }
  }

  /* --- civilian villages (04:604) ---------------------------------------- */
  std::vector<double> usedFracs;
  for (double v = 0; v < W.villageCount && road.size() > 8; v++) {
    double frac = 0;
    int tries = 0;
    bool reject;
    do {
      frac = rng.next();
      tries++;
      reject = false;
      for (const double u : usedFracs)
        if (std::fabs(u - frac) < 0.18 || std::fabs(u - frac) > 0.82) {
          reject = true;
          break;
        }
      if (!reject) {
        const WorldXZ rp = roadAt(frac);
        for (const auto& f : out.facilities)
          if (dist2(f.x, f.z, rp.x, rp.z) < 500) { reject = true; break; }
      }
    } while (tries < 20 && reject);
    usedFracs.push_back(frac);
    const WorldXZ c = inland(roadAt(frac), 55);
    if (H(c.x, c.z) < 1.5) continue;
    Village vil;
    vil.name = isleName(rng);
    vil.x = c.x; vil.z = c.z; vil.r = 70;
    const int nH = 5 + static_cast<int>(std::floor(rng.next() * 3));
    for (int i = 0; i < nH; i++) {
      const double a = rng.next() * PI * 2, r = 12 + rng.next() * 42;
      const double hx = c.x + std::cos(a) * r, hz = c.z + std::sin(a) * r;
      if (H(hx, hz) < 1) continue;   /* skipped hut: only 2 draws consumed */
      (void)rng.next();               /* wBox material pick (04:622) */
      (void)rng.next();               /* wBox rotation */
      Hut hut; hut.x = hx; hut.z = hz;
      vil.huts.push_back(hut);
    }
    if (vil.huts.empty()) continue;
    for (double i = 0; i < W.civsPerVillage; i++) {
      Civ cv;
      cv.bx = c.x + (rng.next() - 0.5) * 70;   /* draw order = literal order */
      cv.bz = c.z + (rng.next() - 0.5) * 70;
      cv.wr = 5 + rng.next() * 9;
      cv.phase = rng.next() * 20;
      cv.x = c.x; cv.z = c.z;
      out.civs.push_back(cv);
    }
    out.villages.push_back(vil);
  }

  /* --- E6: towns (04:650) — rng2 ----------------------------------------- */
  for (double t = 0; t < W.townCount && road.size() > 8; t++) {
    const auto site = scanBest([&](double x, double z) -> std::optional<double> {
      const double h = H(x, z);
      if (h < 3 || h > 45) return std::nullopt;
      const auto a = nearestOnRoad(x, z);
      if (!a || a->d > 340 || a->d < 70) return std::nullopt;
      double dev = 0;
      static const double offs[6][2] = {
        { -120, 0 }, { 120, 0 }, { 0, -120 }, { 0, 120 }, { -85, -85 }, { 85, 85 },
      };
      for (const auto& o : offs)
        dev = std::max(dev, std::fabs(H(x + o[0], z + o[1]) - h));
      if (dev > 24) return std::nullopt;
      for (const auto& vv : out.villages)
        if (dist2(x, z, vv.x, vv.z) < 900) return std::nullopt;
      for (const auto& f : out.facilities)
        if (dist2(x, z, f.x, f.z) < 700) return std::nullopt;
      return -dev - std::fabs(a->d - 170) * 0.02;
    });
    if (!site) continue;
    const auto anchor = nearestOnRoad(site->x, site->z);
    const double ax = (site->x - anchor->p->x) / anchor->d;
    const double az = (site->z - anchor->p->z) / anchor->d;
    const double mx = -az, mz = ax;   /* main-street unit vector */
    Village town;
    town.name = isleName(rng2);       /* draws BEFORE the street water test */
    town.x = site->x; town.z = site->z; town.r = W.townR; town.isTown = true;
    struct Street { Polyline pts; double ux, uz, half; };
    std::vector<Street> streets;
    const auto mkStreet = [&](double ux, double uz,
                              double half) -> std::optional<Polyline> {
      Polyline pts;
      for (double s = -half; s <= half; s += half / 4) {
        const double x = site->x + ux * s, z = site->z + uz * s;
        if (H(x, z) < 1.2) return std::nullopt;
        pts.push_back(WorldXZ{ x, z });
      }
      return pts;
    };
    const auto main = mkStreet(mx, mz, W.townStreetHalf);
    if (!main) continue;
    streets.push_back({ *main, mx, mz, W.townStreetHalf });
    const auto cross = mkStreet(ax, az, W.townCrossHalf);
    if (cross) streets.push_back({ *cross, ax, az, W.townCrossHalf });
    const double want =
        W.townBuildingsMin +
        std::floor(rng2.next() * (W.townBuildingsMax - W.townBuildingsMin));
    double placed = 0;
    for (const auto& st : streets) {
      const double px = -st.uz, pz = st.ux;
      for (double side = -1; side <= 1 && placed < want; side += 2) {
        double s = -st.half + 14;
        while (s < st.half - 14 && placed < want) {
          const double w = 6.5 + rng2.next() * 7.5, d = 6 + rng2.next() * 5.5;
          const bool twoStorey = rng2.next() < 0.3;
          const double hgt = twoStorey ? 6.4 + rng2.next() * 2.2
                                       : 3.2 + rng2.next() * 1.4;
          const double off = W.townSetback + rng2.next() * 5;
          (void)hgt;
          const double bx = site->x + st.ux * s + px * off * side;
          const double bz = site->z + st.uz * s + pz * off * side;
          const double g = H(bx, bz);
          if (g >= 1.2) {
            (void)rng2.next();                    /* rot jitter (04:703) */
            if (!twoStorey) (void)rng2.next();    /* material pick (04:704) */
            Hut b; b.x = bx; b.z = bz; b.w = w; b.d = d;
            town.huts.push_back(b);
            placed++;
          }
          s += 20 + rng2.next() * 12;             /* always drawn */
        }
      }
    }
    if (placed < 6) continue;   /* not a town; drop it */
    {
      const double nx = -mz, nz = mx;
      double aMin = 1e9, aMax = -1e9, bMin = 1e9, bMax = -1e9;
      for (const auto& b : town.huts) {
        const double dx = b.x - site->x, dz = b.z - site->z;
        const double a = dx * mx + dz * mz, n = dx * nx + dz * nz;
        if (a < aMin) aMin = a;
        if (a > aMax) aMax = a;
        if (n < bMin) bMin = n;
        if (n > bMax) bMax = n;
      }
      const double pad = 22;
      TownFoot foot;
      foot.ux = mx; foot.uz = mz;
      foot.aHalf = (aMax - aMin) / 2 + pad;
      foot.bHalf = (bMax - bMin) / 2 + pad;
      foot.cx = site->x + mx * (aMax + aMin) / 2 + nx * (bMax + bMin) / 2;
      foot.cz = site->z + mz * (aMax + aMin) / 2 + nz * (bMax + bMin) / 2;
      town.foot = foot;
    }
    for (const auto& st : streets) out.roads.push_back(st.pts);
    for (double i = 0; i < W.civsPerTown; i++) {
      Civ cv;
      cv.bx = site->x + (rng2.next() - 0.5) * W.townR * 1.4;
      cv.bz = site->z + (rng2.next() - 0.5) * W.townR * 1.4;
      cv.wr = 6 + rng2.next() * 11;
      cv.phase = rng2.next() * 20;
      cv.x = site->x; cv.z = site->z;
      out.civs.push_back(cv);
    }
    out.villages.push_back(town);
  }

  /* --- E7: rock outcrops (04:766) — rng3 --------------------------------- */
  {
    const RockConfig& RK = W.rock;
    const auto rockScore = [&](double x, double z) -> std::optional<double> {
      const double h = H(x, z);
      if (h < 18 || h > 150) return std::nullopt;
      const double d = 55;
      const double hE = H(x + d, z), hW = H(x - d, z);
      const double hN = H(x, z - d), hS = H(x, z + d);
      const double slope = std::hypot(hE - hW, hN - hS) / (2 * d);
      const double conv = h - (hE + hW + hN + hS) / 4;
      if (slope < 0.045 || conv < 0.4) return std::nullopt;
      return slope * 26 + conv * 1.5;
    };
    struct Cand { double x, z, s; };
    std::vector<Cand> cands;
    for (double x = -3400; x <= 3400; x += 100)
      for (double z = -3400; z <= 3400; z += 100) {
        const auto s = rockScore(x, z);
        if (s) cands.push_back({ x, z, *s + rng3.next() * 0.9 });
      }
    std::stable_sort(cands.begin(), cands.end(),
                     [](const Cand& a, const Cand& b) { return b.s < a.s; });
    const auto tooClose = [&](double x, double z) {
      if (dist2(x, z, op.x, op.z) < 300) return true;
      for (const auto& r : out.rocks)
        if (dist2(x, z, r.x, r.z) < RK.sep) return true;
      for (const auto& f : out.facilities)
        if (dist2(x, z, f.x, f.z) < 200) return true;
      for (const auto& vv : out.villages)
        if (dist2(x, z, vv.x, vv.z) < 220) return true;
      return false;
    };
    const auto take = [&](const Cand& c) {
      if (tooClose(c.x, c.z)) return false;   /* refused: no draw consumed */
      Rock r;
      r.x = c.x; r.z = c.z; r.s = c.s; r.h = H(c.x, c.z);
      r.r = RK.rMin + rng3.next() * (RK.rMax - RK.rMin);
      out.rocks.push_back(r);
      return true;
    };
    const int SECT = 8;
    const double sMil = 6400.0 / SECT;
    for (int s0 = 0; s0 < SECT && static_cast<double>(out.rocks.size()) < RK.count;
         s0++)
      for (const auto& c : cands) {
        const double mil = std::fmod(
            std::atan2(c.x - op.x, op.z - c.z) * MILS_PER_RAD + 6400, 6400.0);
        if (std::floor(mil / sMil) != static_cast<double>(s0)) continue;
        if (take(c)) break;
      }
    for (const auto& c : cands) {
      if (static_cast<double>(out.rocks.size()) >= RK.count) break;
      take(c);
    }
    /* naming (04:814) — stable sort desc by score, splice from the pool */
    std::vector<std::size_t> byScore(out.rocks.size());
    for (std::size_t i = 0; i < byScore.size(); i++) byScore[i] = i;
    std::stable_sort(byScore.begin(), byScore.end(),
                     [&](std::size_t a, std::size_t b) {
                       return out.rocks[b].s < out.rocks[a].s;
                     });
    std::vector<std::string> pool(ROCK_NAMES, ROCK_NAMES + ROCK_NAME_COUNT);
    for (double i = 0; i < RK.named &&
                       i < static_cast<double>(byScore.size()) && !pool.empty();
         i++) {
      const std::size_t pick =
          static_cast<std::size_t>(std::floor(rng3.next() * pool.size()));
      out.rocks[byScore[static_cast<std::size_t>(i)]].name = pool[pick];
      pool.erase(pool.begin() + static_cast<std::ptrdiff_t>(pick));
    }
    /* boulder detail (04:820) — mesh-only OUTPUT, but the draws advance rng3
       in source order, so they are taken and discarded. i==0 (the dominant
       block) skips the radius draw — the JS ternary short-circuits. */
    for (const auto& rk : out.rocks) {
      const int n = static_cast<int>(RK.nMin) +
                    static_cast<int>(std::floor(rng3.next() * (RK.nMax - RK.nMin + 1)));
      for (int i = 0; i < n; i++) {
        (void)rng3.next();                       /* ang */
        if (i) (void)rng3.next();                /* rad (only i>0) */
        (void)rng3.next();                       /* sz0 (one draw either branch) */
        (void)rng3.next(); (void)rng3.next(); (void)rng3.next();   /* sx sy szz */
        (void)rng3.next(); (void)rng3.next(); (void)rng3.next();   /* rx ry rz */
        (void)rng3.next();                       /* palette pick */
        (void)rng3.next();                       /* shade */
      }
      (void)rk;
    }
  }

  /* --- dirt paths (04:856) — pairs, no draws ----------------------------- */
  {
    std::vector<PathTarget> pathTargets;
    for (const auto& vv : out.villages)
      if (!vv.isTown) pathTargets.push_back({ vv.x, vv.z });
    for (const auto& f : out.facilities)
      if (f.kind == "mast" || f.kind == "gun")
        pathTargets.push_back({ f.x, f.z });
    for (std::size_t i = 0; i + 1 < pathTargets.size(); i += 2) {
      const PathTarget& a = pathTargets[i];
      const PathTarget& b = pathTargets[i + 1];
      Polyline pts;
      const double n = 26;
      for (double s = 0; s <= n; s++) {
        const double t = s / n;
        const double x = lerp(a.x, b.x, t) +
                         std::sin(t * 9 + static_cast<double>(i)) * 26;
        const double z = lerp(a.z, b.z, t) +
                         std::cos(t * 7 - static_cast<double>(i)) * 26;
        if (H(x, z) < 0.5) { pts.clear(); break; }   /* refuse water crossings */
        pts.push_back(WorldXZ{ x, z });
      }
      if (!pts.empty()) out.paths.push_back(pts);
    }
  }

  /* --- E4 connectors (04:879) — rng2 bow draws in this exact order ------- */
  for (const auto& f : out.facilities) {
    if (f.kind == "airfield") addRoute(linkTo(f.x, f.z, 3000), true);
    else if (f.kind == "fuel" || f.kind == "ammo") addRoute(linkTo(f.x, f.z, 260), true);
    else if (f.kind == "gun" || f.kind == "mast") addRoute(linkTo(f.x, f.z, 2200), false);
  }
  for (const auto& vv : out.villages)
    addRoute(linkTo(vv.x, vv.z, vv.isTown ? 600 : 700), vv.isTown);
  addRoute(linkTo(op.x, op.z, 6000), false);   /* the track up to the OP */

  /* mergedRibbonMesh / flushBoxes are mesh-only: skipped. */
  buildVegetation(out, hf, op, battery, W, gfx, wSeed, blackPalette);   /* 04:897 */
  return out;
}

} }
