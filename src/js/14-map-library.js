/* ============================================================ MAP [M] */
const mapEl = document.getElementById('map');
const mapCv = document.getElementById('mapcv');
const miniCv = document.createElement('canvas');
function buildMinimap() {
  const N = 280;
  miniCv.width = N; miniCv.height = N;
  const ctx = miniCv.getContext('2d');
  const img = ctx.createImageData(N, N);
  for (let py = 0; py < N; py++) {
    for (let px = 0; px < N; px++) {
      const x = px / N * 10000 - 5000, z = py / N * 10000 - 5000;
      const h = H(x, z);
      let r, g, b;
      if (h < -8)       { r = 30; g = 90; b = 122; }
      else if (h < 0)   { r = 46; g = 125; b = 160; }
      else if (h < 4)   { r = 216; g = 200; b = 154; }
      else if (h > 110) { r = 110; g = 102; b = 92; }
      else {
        const t = Math.min(h / 100, 1);
        r = 78 - 19 * t; g = 122 - 28 * t; b = 61 - 15 * t;
      }
      if (h > 0) {  // crude west-lit hillshade
        const sh = clamp(1 - (H(x + 35, z) - h) * 0.012, 0.78, 1.15);
        r *= sh; g *= sh; b *= sh;
      }
      const o = (py * N + px) * 4;
      img.data[o] = r; img.data[o + 1] = g; img.data[o + 2] = b; img.data[o + 3] = 255;
    }
  }
  ctx.putImageData(img, 0, 0);
}
buildMinimap();
function mapMarkerLabel(ctx, text, x, y) {
  ctx.font = 'bold 11px Consolas, monospace';
  ctx.textAlign = 'left';
  ctx.strokeStyle = 'rgba(255,255,255,0.85)';
  ctx.lineWidth = 2.5;
  ctx.strokeText(text, x, y);
  ctx.fillStyle = '#10150c';
  ctx.fillText(text, x, y);
}

/* ================================================== E5: THE PERMANENT WORLD LAYER
   CLAUDE.md: "Permanent structures and roads must appear on the printed map
   sheets and the [M] map (symbols + legend) so terrain association is a
   practicable skill." That only holds if the paper and the screen agree, so
   both are drawn by THIS function, from THE SAME `WORLD` arrays the 3D world
   was built from, and differ only in a projection and an ink. There is no
   second copy of the symbology to drift out of sync.

   Road classes get genuinely different symbols, per topographic convention:

     METALLED   a CASED double line — a heavy casing stroked first, a light
                core stroked over it, so the route reads as two parallel lines
                with white between. This is how a hard-surface road is drawn on
                a 1:50,000 sheet, and it is what makes "the road bends around
                the headland" a usable resection cue.
     UNIMPROVED a single thin DASHED line. No casing.

   Widths are CONVENTIONAL, not true to scale: the coast road is 14.4 m of
   formation, which at 1:50,000 is 0.29 mm — invisible. Real sheets exaggerate
   roads for exactly this reason, and so does this one.

   Enemy positions are never passed in. This function cannot plot them; it only
   ever reads WORLD, which contains no enemy data at all. */
function facSymbol(ctx, kind, px, py, s, ink, paper, rot) {
  ctx.lineWidth = Math.max(1, s * 0.22);
  ctx.strokeStyle = ink; ctx.fillStyle = ink;
  ctx.setLineDash([]);
  switch (kind) {
    case 'mast':                                  // radio tower: mast + guys + finial
      ctx.beginPath();
      ctx.moveTo(px, py + s); ctx.lineTo(px, py - s);
      ctx.moveTo(px - s * 0.8, py + s); ctx.lineTo(px, py - s * 0.55);
      ctx.lineTo(px + s * 0.8, py + s);
      ctx.stroke();
      ctx.beginPath(); ctx.arc(px, py - s, s * 0.26, 0, Math.PI * 2); ctx.fill();
      break;
    case 'gun': {                                 // coastal battery: emplacement + barrel
      const a = rot || 0, ux = Math.sin(a), uz = -Math.cos(a);
      ctx.beginPath();
      ctx.arc(px, py, s * 0.62, 0, Math.PI * 2);
      ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(px, py); ctx.lineTo(px + ux * s * 1.5, py + uz * s * 1.5);
      ctx.stroke();
      break;
    }
    case 'fuel':                                  // tank farm: circle with a centre dot
      ctx.beginPath(); ctx.arc(px, py, s * 0.72, 0, Math.PI * 2); ctx.stroke();
      ctx.beginPath(); ctx.arc(px, py, s * 0.26, 0, Math.PI * 2); ctx.fill();
      break;
    case 'ammo':                                  // magazine: open square, solid core
      ctx.strokeRect(px - s * 0.8, py - s * 0.8, s * 1.6, s * 1.6);
      ctx.fillRect(px - s * 0.3, py - s * 0.3, s * 0.6, s * 0.6);
      break;
    /* E7 — rock outcrop. Topographic convention for bare rock is an irregular
       closed blob, deliberately NOT a circle or a square, so it cannot be
       confused with the fuel point or a building at a glance. Drawn as a fixed
       five-lobe outline with two scree dots below it; the shape is hard-coded
       rather than random so the same outcrop draws identically every redraw
       and on both surfaces. */
    case 'rock': {
      const lobe = [[-1.0, 0.35], [-0.55, -0.75], [0.15, -1.0], [0.9, -0.35], [1.0, 0.55],
                    [0.3, 0.95], [-0.5, 0.85]];
      ctx.beginPath();
      for (let i = 0; i < lobe.length; i++) {
        const lx = px + lobe[i][0] * s * 0.85, ly = py + lobe[i][1] * s * 0.7;
        if (i) ctx.lineTo(lx, ly); else ctx.moveTo(lx, ly);
      }
      ctx.closePath();
      ctx.stroke();
      ctx.beginPath();
      ctx.arc(px - s * 0.15, py + s * 0.05, s * 0.16, 0, Math.PI * 2);
      ctx.arc(px + s * 0.35, py - s * 0.25, s * 0.13, 0, Math.PI * 2);
      ctx.fill();
      break;
    }
    case 'dock':                                  // jetty: a T off the shore
      ctx.beginPath();
      ctx.moveTo(px - s, py); ctx.lineTo(px + s, py);
      ctx.moveTo(px + s, py - s * 0.7); ctx.lineTo(px + s, py + s * 0.7);
      ctx.stroke();
      break;
    default:                                      // generic permanent building
      ctx.fillRect(px - s * 0.6, py - s * 0.6, s * 1.2, s * 1.2);
      ctx.strokeStyle = paper; ctx.lineWidth = Math.max(0.7, s * 0.14);
      ctx.strokeRect(px - s * 0.6, py - s * 0.6, s * 1.2, s * 1.2);
  }
}
function drawWorldLayer(ctx, X, Y, o) {
  const poly = pts => {
    ctx.beginPath();
    for (let i = 0; i < pts.length; i++) {
      const px = X(pts[i].x), py = Y(pts[i].z);
      if (i) ctx.lineTo(px, py); else ctx.moveTo(px, py);
    }
  };
  const prevJoin = ctx.lineJoin, prevCap = ctx.lineCap;
  ctx.lineJoin = 'round'; ctx.lineCap = 'butt';

  /* --- E6: BUILT-UP AREAS go down first, under everything. A town on a
     1:50,000 sheet is not a hundred building dots — a 10 m building is 1.2 px
     and plotting them individually produced a solid black smear that hid the
     street pattern. It is a generalised block: outline plus diagonal hatch,
     with the streets drawn THROUGH it by the road pass below. That ordering is
     the point; drawn on top, the hatch buried its own streets. */
  for (const v of WORLD.villages) {
    if (v.tier !== 'town' || !v.foot) continue;
    const f = v.foot;
    const a = f.aHalf * o.mPerPx, b = f.bHalf * o.mPerPx;
    ctx.save();
    ctx.translate(X(f.cx), Y(f.cz));
    ctx.rotate(Math.atan2(f.uz, f.ux));
    ctx.beginPath();
    ctx.rect(-a, -b, a * 2, b * 2);
    ctx.save();
    ctx.clip();
    ctx.lineWidth = 0.6;
    ctx.strokeStyle = o.ink;
    for (let k = -(a + b * 2); k < a + b * 2; k += o.hatch) {
      ctx.beginPath();
      ctx.moveTo(k, -b); ctx.lineTo(k + b * 2, b);
      ctx.stroke();
    }
    ctx.restore();
    ctx.lineWidth = Math.max(0.9, o.roadCoreW * 0.7);
    ctx.strokeStyle = o.ink;
    ctx.strokeRect(-a, -b, a * 2, b * 2);
    ctx.restore();
  }

  // --- unimproved tracks first: they run UNDER the metalled net at junctions
  ctx.setLineDash(o.dash);
  ctx.lineWidth = o.dirtW;
  ctx.strokeStyle = o.dirtInk;
  for (const p of WORLD.paths) { poly(p); ctx.stroke(); }
  ctx.setLineDash([]);

  // --- metalled roads: casing, then core. Two passes over the whole network,
  //     not per polyline, so junctions between spurs stay clean.
  ctx.lineWidth = o.roadW;
  ctx.strokeStyle = o.roadInk;
  for (const r of WORLD.roads) { poly(r); ctx.stroke(); }
  ctx.lineWidth = o.roadCoreW;
  ctx.strokeStyle = o.roadCore;
  for (const r of WORLD.roads) { poly(r); ctx.stroke(); }

  // --- settlements. Every building is plotted, at a legibility floor: a 5 m
  //     hut is 0.1 mm at 1:50,000, so the symbol is a minimum size rather than
  //     a scaled footprint. Civilian areas are labelled (CIV) on both surfaces
  //     because putting one round into one of them is an instant mission fail.
  for (const v of WORLD.villages) {
    let labX = X(v.x) + o.sym * 1.6, labY = Y(v.z) + o.sym * 0.5;
    if (v.tier === 'town' && v.foot) {
      const f = v.foot;
      const a = f.aHalf * o.mPerPx, b = f.bHalf * o.mPerPx;
      labX = X(f.cx) + Math.abs(f.ux) * a + Math.abs(f.uz) * b + o.sym;
      labY = Y(f.cz) + o.sym * 0.5;
    } else {
      // villages stay building-by-building at a legibility floor: a 5 m hut is
      // 0.6 px at 1:50,000, so the symbol is a minimum size, not a footprint
      ctx.fillStyle = o.ink;
      for (const hh of (v.huts || [])) {
        const w = Math.max(o.hut, (hh.w || 5) * o.mPerPx);
        const d = Math.max(o.hut, (hh.d || 5) * o.mPerPx);
        ctx.fillRect(X(hh.x) - w / 2, Y(hh.z) - d / 2, w, d);
      }
    }
    o.label(v.name + (v.tier === 'town' ? ' (TOWN, CIV)' : ' (CIV)'), labX, labY);
  }

  /* --- E7: rock outcrops. Drawn after the roads so a boulder field beside a
     track is not overprinted by the track's casing, and BEFORE the facilities
     so a man-made symbol always wins a collision — a fuel point the observer
     misses because a rock symbol sat on it is a worse failure than a rock he
     has to look twice for. Only the named few get a label; fourteen labels
     would bury the sheet and make none of them memorable. */
  for (const rk of WORLD.rocks) {
    const rx = X(rk.x), ry = Y(rk.z);
    facSymbol(ctx, 'rock', rx, ry, o.sym * 0.9, o.ink, o.paper, 0);
    if (rk.name) o.label(rk.name, rx + o.sym * 1.4, ry + o.sym * 1.1);
  }

  // --- named facilities
  for (const f of WORLD.facilities) {
    const fx = X(f.x), fy = Y(f.z);
    if (f.kind === 'airfield') {
      // runway: two casing lines at the true formation width, floored so the
      // strip cannot collapse to a single line at sheet scale
      const ux = Math.sin(f.th), uz = -Math.cos(f.th);
      const l = f.len / 2 * o.mPerPx;
      const hw = Math.max(o.sym * 0.5, 24 * o.mPerPx);
      ctx.strokeStyle = o.ink; ctx.lineWidth = o.roadCoreW + 0.4;
      ctx.setLineDash([]);
      ctx.beginPath();
      ctx.moveTo(fx - ux * l - uz * hw, fy - uz * l + ux * hw);
      ctx.lineTo(fx + ux * l - uz * hw, fy + uz * l + ux * hw);
      ctx.moveTo(fx - ux * l + uz * hw, fy - uz * l - ux * hw);
      ctx.lineTo(fx + ux * l + uz * hw, fy + uz * l - ux * hw);
      ctx.moveTo(fx - ux * l - uz * hw, fy - uz * l + ux * hw);
      ctx.lineTo(fx - ux * l + uz * hw, fy - uz * l - ux * hw);
      ctx.moveTo(fx + ux * l - uz * hw, fy + uz * l + ux * hw);
      ctx.lineTo(fx + ux * l + uz * hw, fy + uz * l - ux * hw);
      ctx.stroke();
    } else {
      facSymbol(ctx, f.kind, fx, fy, o.sym, o.ink, o.paper,
                f.kind === 'gun' ? Math.atan2(f.x, f.z) : 0);
    }
    o.label(f.name, fx + o.sym * 1.5, fy - o.sym * 0.6);
  }
  ctx.lineJoin = prevJoin; ctx.lineCap = prevCap;
  ctx.setLineDash([]);
}

/* Legend rows, shared by both surfaces. Each row draws its own sample symbol so
   the legend can never disagree with the map — same code path, same numbers. */
const LEGEND_ROWS = [
  { t: 'METALLED ROAD (HARD SURFACE)', d: (c, x, y, o) => {
      c.setLineDash([]); c.lineWidth = o.roadW; c.strokeStyle = o.roadInk;
      c.beginPath(); c.moveTo(x, y); c.lineTo(x + o.sw, y); c.stroke();
      c.lineWidth = o.roadCoreW; c.strokeStyle = o.roadCore;
      c.beginPath(); c.moveTo(x, y); c.lineTo(x + o.sw, y); c.stroke(); } },
  { t: 'TRACK / UNIMPROVED (DIRT)', d: (c, x, y, o) => {
      c.setLineDash(o.dash); c.lineWidth = o.dirtW; c.strokeStyle = o.dirtInk;
      c.beginPath(); c.moveTo(x, y); c.lineTo(x + o.sw, y); c.stroke();
      c.setLineDash([]); } },
  { t: 'AIRFIELD / LANDING STRIP', d: (c, x, y, o) => {
      c.setLineDash([]); c.lineWidth = o.roadCoreW + 0.4; c.strokeStyle = o.ink;
      c.strokeRect(x, y - o.sym * 0.55, o.sw, o.sym * 1.1); } },
  { t: 'RADIO MAST', d: (c, x, y, o) => facSymbol(c, 'mast', x + o.sw / 2, y, o.sym, o.ink, o.paper, 0) },
  { t: 'COASTAL GUN BATTERY', d: (c, x, y, o) => facSymbol(c, 'gun', x + o.sw / 2, y, o.sym, o.ink, o.paper, Math.PI / 2) },
  { t: 'FUEL POINT / TANKS', d: (c, x, y, o) => facSymbol(c, 'fuel', x + o.sw / 2, y, o.sym, o.ink, o.paper, 0) },
  { t: 'AMMUNITION DEPOT', d: (c, x, y, o) => facSymbol(c, 'ammo', x + o.sw / 2, y, o.sym, o.ink, o.paper, 0) },
  { t: 'BUILDING (PERMANENT)', d: (c, x, y, o) => facSymbol(c, 'bldg', x + o.sw / 2, y, o.sym, o.ink, o.paper, 0) },
  { t: 'ROCK OUTCROP / BOULDERS', d: (c, x, y, o) => facSymbol(c, 'rock', x + o.sw / 2, y, o.sym * 0.9, o.ink, o.paper, 0) },
  { t: 'VILLAGE — CIVILIAN, NO-STRIKE', d: (c, x, y, o) => {
      c.setLineDash([]); c.fillStyle = o.ink;
      for (const [dx, dy] of [[0, -0.6], [0.9, 0.3], [-0.8, 0.5], [1.9, -0.4]])
        c.fillRect(x + o.sw / 2 + dx * o.sym - o.hut / 2,
                   y + dy * o.sym - o.hut / 2, o.hut, o.hut); } },
  { t: 'BUILT-UP AREA / TOWN — CIVILIAN, NO-STRIKE', d: (c, x, y, o) => {
      c.setLineDash([]);
      const h = o.sym * 0.85;
      c.save(); c.beginPath(); c.rect(x, y - h, o.sw, h * 2); c.clip();
      c.lineWidth = 0.6; c.strokeStyle = o.ink;
      for (let k = x - h * 2; k < x + o.sw; k += o.hatch) {
        c.beginPath(); c.moveTo(k, y + h); c.lineTo(k + h * 2, y - h); c.stroke();
      }
      c.restore();
      c.lineWidth = Math.max(0.9, o.roadCoreW * 0.7); c.strokeStyle = o.ink;
      c.strokeRect(x, y - h, o.sw, h * 2); } },
];

// [M] map: dark ink on the shaded relief, near-white road core so the cased
// double line still reads over jungle green and sand.
function MAPOPT_SCREEN(W) {
  const ctx = mapCv.getContext('2d');
  return {
    mPerPx: W / 10000, sym: 5, hut: 3.4, sw: 22, hatch: 4,
    ink: '#10150c', paper: 'rgba(255,255,255,0.9)',
    roadInk: '#20201C', roadCore: '#E8E2D4', roadW: 4.0, roadCoreW: 1.8,
    dirtInk: 'rgba(84,54,26,0.92)', dirtW: 1.3, dash: [4, 3],
    label: (t, x, y) => mapMarkerLabel(ctx, t, x, y),
  };
}
// Printed sheet: pure line art. Nothing here uses a grey or an alpha that a
// black-and-white laser printer would have to dither.
function MAPOPT_SHEET(ctx, mapPx, sizeM) {
  return {
    mPerPx: mapPx / sizeM, sym: 6, hut: 2.8, sw: 26, hatch: 5,
    ink: '#000', paper: '#fff',
    roadInk: '#000', roadCore: '#fff', roadW: 3.2, roadCoreW: 1.4,
    dirtInk: '#000', dirtW: 0.9, dash: [6, 4],
    label: (t, x, y) => {
      ctx.fillStyle = '#000';
      ctx.font = 'bold 10px Consolas, monospace';
      ctx.textAlign = 'left';
      ctx.fillText(t, x, y);
    },
  };
}
/* Lay the shared rows out in `cols` columns from (x0,y0) and return the bottom
   edge, so the caller can assert the block fits inside its canvas. */
function drawLegend(ctx, rows, x0, y0, colW, rowH, cols, o, font, textInk) {
  const per = Math.ceil(rows.length / cols);
  let maxY = y0;
  rows.forEach((r, i) => {
    const cx = x0 + Math.floor(i / per) * colW;
    const cy = y0 + (i % per) * rowH;
    ctx.save();
    r.d(ctx, cx, cy, o);
    ctx.restore();
    ctx.setLineDash([]);
    ctx.fillStyle = textInk;
    ctx.font = font;
    ctx.textAlign = 'left';
    ctx.fillText(r.t, cx + o.sw + 8, cy + 3.5);
    if (cy > maxY) maxY = cy;
  });
  return maxY + rowH;
}
function drawMap() {
  const W = mapCv.width, ctx = mapCv.getContext('2d');
  const wpx = x => (x + 5000) / 10000 * W;
  const wpz = z => (z + 5000) / 10000 * W;
  ctx.imageSmoothingEnabled = true;
  ctx.drawImage(miniCv, 0, 0, W, W);
  // 1 km grid with principal-digit labels
  ctx.strokeStyle = 'rgba(0,0,0,0.30)';
  ctx.lineWidth = 1;
  ctx.font = '10px Consolas, monospace';
  ctx.textAlign = 'left';
  for (let i = 0; i <= 10; i++) {
    const p = Math.round(i / 10 * W) + 0.5;
    ctx.beginPath();
    ctx.moveTo(p, 0); ctx.lineTo(p, W);
    ctx.moveTo(0, p); ctx.lineTo(W, p);
    ctx.stroke();
    if (i < 10) {
      ctx.fillStyle = 'rgba(0,0,0,0.75)';
      ctx.fillText(String(CONFIG.MAP.originE / 1000 + i), p + 2, W - 4);
      ctx.fillText(String(CONFIG.MAP.originN / 1000 + 10 - i), 3, p + 11);
    }
  }
  // E5 — permanent world features first (terrain association layer), so mission
  // markers draw over them. Same function, same WORLD data, and the same symbol
  // vocabulary the printed sheet uses; only the ink and the projection differ.
  drawWorldLayer(ctx, wpx, wpz, MAPOPT_SCREEN(W));
  // friendly elements (strongpoint compound, or advancing squad centroid)
  if (Scenario.compound) {
    const c = Scenario.compound;
    ctx.fillStyle = '#2E5FA0';
    ctx.fillRect(wpx(c.x) - 5, wpz(c.z) - 5, 10, 10);
    mapMarkerLabel(ctx, 'FRIENDLY', wpx(c.x) + 8, wpz(c.z) + 4);
  } else if (Scenario.type === 'assault') {
    const fp = friendlyPositions();
    if (fp.length) {
      let fx = 0, fz = 0;
      for (const f of fp) { fx += f.x; fz += f.z; }
      fx /= fp.length; fz /= fp.length;
      ctx.fillStyle = '#2E5FA0';
      ctx.fillRect(wpx(fx) - 5, wpz(fz) - 5, 10, 10);
      mapMarkerLabel(ctx, 'FRIENDLY (MOVING)', wpx(fx) + 8, wpz(fz) + 4);
    }
  }
  // firing battery
  ctx.fillStyle = '#2E5FA0';
  ctx.beginPath();
  ctx.arc(wpx(BATTERY.x), wpz(BATTERY.z), 6, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillStyle = '#dfe8c8';
  ctx.beginPath();
  ctx.arc(wpx(BATTERY.x), wpz(BATTERY.z), 2, 0, Math.PI * 2);
  ctx.fill();
  mapMarkerLabel(ctx, 'BTRY 155', wpx(BATTERY.x) + 9, wpz(BATTERY.z) + 4);
  // known points
  for (const kp of (Scenario.kps || [])) {
    const kx = wpx(kp.x), kz = wpz(kp.z);
    ctx.strokeStyle = '#10150c';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(kx - 5, kz - 5); ctx.lineTo(kx + 5, kz + 5);
    ctx.moveTo(kx - 5, kz + 5); ctx.lineTo(kx + 5, kz - 5);
    ctx.stroke();
    mapMarkerLabel(ctx, 'AB' + kp.id + (kp.name ? ' ' + kp.name : ''), kx + 8, kz + 4);
  }
  // OP
  const ox = wpx(OP.x), oz = wpz(OP.z);
  ctx.fillStyle = '#10150c';
  ctx.beginPath();
  ctx.moveTo(ox, oz - 7); ctx.lineTo(ox - 6, oz + 5); ctx.lineTo(ox + 6, oz + 5);
  ctx.closePath(); ctx.fill();
  mapMarkerLabel(ctx, 'OP', ox + 9, oz + 4);
  // north arrow
  ctx.strokeStyle = '#10150c';
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.moveTo(W - 24, 46); ctx.lineTo(W - 24, 16);
  ctx.moveTo(W - 24, 16); ctx.lineTo(W - 29, 25);
  ctx.moveTo(W - 24, 16); ctx.lineTo(W - 19, 25);
  ctx.stroke();
  mapMarkerLabel(ctx, 'N', W - 28, 60);

  /* E5 — symbol legend band, below the map body. The canvas is taller than it
     is wide for exactly this: the map square stays W x W (every projection in
     here is W-based and unaffected), and the band underneath carries the same
     nine legend rows the printed sheet carries, drawn by the same code. A
     symbol the observer learns on paper is the symbol he sees on [M]. */
  const band = mapCv.height - W;
  if (band > 20) {
    ctx.fillStyle = '#12160e';
    ctx.fillRect(0, W, W, band);
    ctx.strokeStyle = '#39442e'; ctx.lineWidth = 1;
    ctx.strokeRect(0.5, W + 0.5, W - 1, band - 1);
    ctx.fillStyle = '#9fb08c';
    ctx.font = '10px Consolas, monospace';
    ctx.textAlign = 'left';
    ctx.fillText('PERMANENT DETAIL — TERRAIN ASSOCIATION', 10, W + 16);
    const lo = MAPOPT_SCREEN(W);
    lo.ink = '#e2e8d2'; lo.paper = '#12160e';
    lo.roadInk = '#e2e8d2'; lo.roadCore = '#12160e';
    lo.dirtInk = '#c8b48a';
    drawLegend(ctx, LEGEND_ROWS, 12, W + 34, 268, 17, 2, lo,
               '9.5px Consolas, monospace', '#9fb08c');
  }
}
function toggleMap(force) {
  const on = force !== undefined ? force : !mapEl.classList.contains('on');
  if (on) {
    drawMap();
    if (document.exitPointerLock) document.exitPointerLock();
    tutEvent('map');
  }
  mapEl.classList.toggle('on', on);
}

/* ============================================================ MAP LIBRARY [P] */
const libEl = document.getElementById('library');
const sheetCv = document.getElementById('sheetcv');
let libSheet = 'full';
const SHEET_DEFS = {
  full: { x0: -5000, z0: -5000, size: 10000, scale: 50000, name: 'FULL SHEET' },
  nw:   { x0: -5000, z0: -5000, size: 5000,  scale: 25000, name: 'NW QUADRANT' },
  ne:   { x0: 0,     z0: -5000, size: 5000,  scale: 25000, name: 'NE QUADRANT' },
  sw:   { x0: -5000, z0: 0,     size: 5000,  scale: 25000, name: 'SW QUADRANT' },
  se:   { x0: 0,     z0: 0,     size: 5000,  scale: 25000, name: 'SE QUADRANT' },
};

// Renders a print-ready topographic sheet from the SAME H(x,z) as the 3D world.
function renderSheet() {
  const def = SHEET_DEFS[libSheet];
  const answer = document.getElementById('libkey').checked;
  const ctx = sheetCv.getContext('2d');
  const W = sheetCv.width, PH = sheetCv.height;
  const dpi = 150;
  ctx.fillStyle = '#fff';
  ctx.fillRect(0, 0, W, PH);
  const mapPx = Math.round(def.size / def.scale / 0.0254 * dpi);   // ≈1181 px
  const mx0 = Math.round((W - mapPx) / 2), my0 = 120;
  const X = wx => mx0 + (wx - def.x0) / def.size * mapPx;
  const Y = wz => my0 + (wz - def.z0) / def.size * mapPx;

  // height field sample
  const N = def.size === 10000 ? 300 : 220;
  const cellW = def.size / N;
  const hs = new Float32Array((N + 1) * (N + 1));
  let maxH = 0;
  for (let j = 0; j <= N; j++)
    for (let i = 0; i <= N; i++) {
      const h = H(def.x0 + i * cellW, def.z0 + j * cellW);
      hs[j * (N + 1) + i] = h;
      if (h > maxH) maxH = h;
    }
  const cellPx = mapPx / N;

  // water tint
  ctx.fillStyle = '#e6e6e6';
  for (let j = 0; j < N; j++)
    for (let i = 0; i < N; i++)
      if (hs[j * (N + 1) + i] < 0.05)
        ctx.fillRect(mx0 + i * cellPx, my0 + j * cellPx, cellPx + 0.6, cellPx + 0.6);

  // marching squares over the sampled field
  function msSegs(level) {
    const segs = [];
    for (let j = 0; j < N; j++) {
      const zT = def.z0 + j * cellW, zB = zT + cellW;
      for (let i = 0; i < N; i++) {
        const xL = def.x0 + i * cellW, xR = xL + cellW;
        const a = hs[j * (N + 1) + i], b = hs[j * (N + 1) + i + 1];
        const c = hs[(j + 1) * (N + 1) + i + 1], d = hs[(j + 1) * (N + 1) + i];
        let idx = 0;
        if (a >= level) idx |= 1;
        if (b >= level) idx |= 2;
        if (c >= level) idx |= 4;
        if (d >= level) idx |= 8;
        if (idx === 0 || idx === 15) continue;
        const pts = [];
        if (((idx & 1) !== 0) !== (((idx >> 1) & 1) !== 0))
          pts.push({ x: xL + cellW * (level - a) / (b - a), z: zT });
        if ((((idx >> 1) & 1) !== 0) !== (((idx >> 2) & 1) !== 0))
          pts.push({ x: xR, z: zT + cellW * (level - b) / (c - b) });
        if ((((idx >> 3) & 1) !== 0) !== (((idx >> 2) & 1) !== 0))
          pts.push({ x: xL + cellW * (level - d) / (c - d), z: zB });
        if (((idx & 1) !== 0) !== (((idx >> 3) & 1) !== 0))
          pts.push({ x: xL, z: zT + cellW * (level - a) / (d - a) });
        if (pts.length >= 2) segs.push([pts[0], pts[1]]);
        if (pts.length === 4) segs.push([pts[2], pts[3]]);
      }
    }
    return segs;
  }

  // contours
  ctx.lineCap = 'round';
  for (let level = 10; level <= maxH; level += 10) {
    const isIndex = level % 50 === 0;
    const segs = msSegs(level);
    ctx.strokeStyle = isIndex ? '#2f2f2f' : '#8a8a8a';
    ctx.lineWidth = isIndex ? 1.2 : 0.55;
    ctx.beginPath();
    for (const s of segs) {
      ctx.moveTo(X(s[0].x), Y(s[0].z));
      ctx.lineTo(X(s[1].x), Y(s[1].z));
    }
    ctx.stroke();
    if (isIndex) {  // sparse elevation labels on index contours
      ctx.font = '10px Consolas, monospace';
      ctx.textAlign = 'center';
      for (let k = 60; k < segs.length; k += 140) {
        const mxp = (X(segs[k][0].x) + X(segs[k][1].x)) / 2;
        const myp = (Y(segs[k][0].z) + Y(segs[k][1].z)) / 2;
        ctx.fillStyle = '#fff';
        ctx.fillRect(mxp - 11, myp - 6, 22, 11);
        ctx.fillStyle = '#2f2f2f';
        ctx.fillText(String(level), mxp, myp + 3);
      }
    }
  }
  // coastline
  {
    const segs = msSegs(0.05);
    ctx.strokeStyle = '#111';
    ctx.lineWidth = 1.4;
    ctx.beginPath();
    for (const s of segs) {
      ctx.moveTo(X(s[0].x), Y(s[0].z));
      ctx.lineTo(X(s[1].x), Y(s[1].z));
    }
    ctx.stroke();
  }

  // permanent world features — plotted like a real sheet so the FO can do
  // terrain association / resection off them. Clipped to the map body.
  ctx.save();
  ctx.beginPath();
  ctx.rect(mx0, my0, mapPx, mapPx);
  ctx.clip();
  // E5: identical symbology to the [M] map — one function, one WORLD, two inks.
  const sheetOpt = MAPOPT_SHEET(ctx, mapPx, def.size);
  drawWorldLayer(ctx, X, Y, sheetOpt);
  ctx.restore();

  // grid + edge labels
  const kmN = def.size / 1000;
  ctx.font = '11px Consolas, monospace';
  for (let k = 0; k <= kmN; k++) {
    const px = mx0 + k / kmN * mapPx, py = my0 + k / kmN * mapPx;
    ctx.strokeStyle = 'rgba(0,0,0,0.45)';
    ctx.lineWidth = 0.7;
    ctx.beginPath();
    ctx.moveTo(px, my0); ctx.lineTo(px, my0 + mapPx);
    ctx.moveTo(mx0, py); ctx.lineTo(mx0 + mapPx, py);
    ctx.stroke();
    ctx.fillStyle = '#000';
    ctx.textAlign = 'center';
    const eKm = Math.round((CONFIG.MAP.originE + def.x0 + 5000) / 1000 + k);
    const nKm = Math.round((CONFIG.MAP.originN + 5000 - def.z0) / 1000 - k);
    ctx.fillText(String(eKm), px, my0 - 5);
    ctx.fillText(String(eKm), px, my0 + mapPx + 14);
    ctx.textAlign = 'right';
    ctx.fillText(String(nKm), mx0 - 4, py + 4);
    ctx.textAlign = 'left';
    ctx.fillText(String(nKm), mx0 + mapPx + 4, py + 4);
  }
  // neatline
  ctx.strokeStyle = '#000';
  ctx.lineWidth = 1.6;
  ctx.strokeRect(mx0, my0, mapPx, mapPx);

  // symbols
  const inExtent = p => p.x >= def.x0 && p.x <= def.x0 + def.size &&
                        p.z >= def.z0 && p.z <= def.z0 + def.size;
  ctx.font = 'bold 11px Consolas, monospace';
  ctx.textAlign = 'left';
  if (inExtent(OP)) {
    const ox = X(OP.x), oz = Y(OP.z);
    ctx.fillStyle = '#000';
    ctx.beginPath();
    ctx.moveTo(ox, oz - 8); ctx.lineTo(ox - 7, oz + 6); ctx.lineTo(ox + 7, oz + 6);
    ctx.closePath(); ctx.fill();
    ctx.fillText(`OP TWR  ${gridOf(OP.x, OP.z)}  ELEV ${Math.round(OP.h)} M`, ox + 10, oz + 4);
  }
  for (const kp of (Scenario.kps || [])) {
    if (!inExtent(kp)) continue;
    const kx = X(kp.x), kz = Y(kp.z);
    ctx.strokeStyle = '#000';
    ctx.lineWidth = 1.8;
    ctx.beginPath();
    ctx.moveTo(kx - 6, kz - 6); ctx.lineTo(kx + 6, kz + 6);
    ctx.moveTo(kx - 6, kz + 6); ctx.lineTo(kx + 6, kz - 6);
    ctx.stroke();
    ctx.fillStyle = '#000';
    ctx.fillText(`AB${kp.id}${kp.name ? ' ' + kp.name : ''}`, kx + 9, kz + 4);
  }
  if (answer && Scenario.enemy && inExtent(Scenario.enemy)) {
    const tx = X(Scenario.enemy.x), tz = Y(Scenario.enemy.z);
    ctx.fillStyle = '#000';
    ctx.beginPath(); ctx.arc(tx, tz, 6, 0, Math.PI * 2); ctx.fill();
    ctx.beginPath(); ctx.arc(tx, tz, 11, 0, Math.PI * 2);
    ctx.lineWidth = 1.4; ctx.stroke();
    ctx.fillText('TGT', tx + 14, tz + 4);
  }

  // title block
  ctx.fillStyle = '#000';
  ctx.textAlign = 'center';
  ctx.font = 'bold 24px Consolas, monospace';
  const islandName = DEM ? DEM.name.toUpperCase() : 'PROCEDURAL ISLAND';
  ctx.fillText(`SHITFIRE — ${islandName} TRAINING SHEET`, W / 2, 46);
  ctx.font = '13px Consolas, monospace';
  ctx.fillText(`${def.name}  ·  SCALE 1:${def.scale.toLocaleString('en-US')}  ·  GRID 1000 M  ·  CONTOUR INTERVAL 10 M (INDEX 50 M)`, W / 2, 72);
  if (answer)
    ctx.fillText(`ANSWER KEY — TARGET: GRID ${gridOf(Scenario.enemy.x, Scenario.enemy.z)} (${SCN_META[Scenario.type].name}${Scenario.type === 'convoy' ? ', POSITION AT PRINT TIME' : ''})`, W / 2, 94);

  // marginal data
  const by = my0 + mapPx + 40;
  ctx.textAlign = 'left';
  ctx.font = '12px Consolas, monospace';
  // scale bar
  const pxPerM = mapPx / def.size;
  const barSeg = 500, barSegs = def.size === 10000 ? 4 : 2;
  const bx0 = mx0, bh = 10;
  for (let s = 0; s < barSegs; s++) {
    ctx.fillStyle = s % 2 ? '#fff' : '#000';
    ctx.fillRect(bx0 + s * barSeg * pxPerM, by, barSeg * pxPerM, bh);
    ctx.strokeStyle = '#000'; ctx.lineWidth = 1;
    ctx.strokeRect(bx0 + s * barSeg * pxPerM, by, barSeg * pxPerM, bh);
  }
  ctx.fillStyle = '#000';
  for (let s = 0; s <= barSegs; s++) {
    ctx.textAlign = 'center';
    ctx.fillText(String(s * barSeg), bx0 + s * barSeg * pxPerM, by + 26);
  }
  ctx.textAlign = 'left';
  ctx.fillText('METERS', bx0 + barSegs * barSeg * pxPerM + 14, by + 10);
  /* G2 — declination diagram, in the marginal data where a real sheet carries it.
     The sheet is GRID; the observer's compass is MAGNETIC; this is the number that
     reconciles them, and without it on the paper the conversion is unlearnable. */
  {
    const dx = bx0 + barSegs * barSeg * pxPerM + 96, dy = by + 4, len = 30;
    const rad = CONFIG.NAV.declEastDeg * Math.PI / 180;
    ctx.strokeStyle = '#000'; ctx.lineWidth = 1.4;
    ctx.beginPath();                                    // grid north, vertical
    ctx.moveTo(dx, dy + len); ctx.lineTo(dx, dy);
    ctx.stroke();
    ctx.beginPath();                                    // magnetic north, east of it
    ctx.moveTo(dx, dy + len);
    ctx.lineTo(dx + Math.sin(rad) * len, dy + len - Math.cos(rad) * len);
    ctx.stroke();
    ctx.font = '10px Consolas, monospace';
    ctx.textAlign = 'left';
    ctx.fillText('GN', dx - 8, dy - 3);
    ctx.fillText('MN', dx + Math.sin(rad) * len + 3, dy - 1);
    ctx.font = '12px Consolas, monospace';
    ctx.fillText(`G-M ANGLE ${CONFIG.NAV.declEastDeg}° E = ${Math.round(declMils())} MILS`,
                 dx + 44, dy + 14);
    ctx.fillText('GRID = MAG + G-M ANGLE', dx + 44, dy + 30);
  }
  ctx.fillText('READ GRIDS RIGHT, THEN UP. 6-DIGIT GRID = 100 M PRECISION (ARTILLERY).', bx0, by + 48);
  ctx.fillText('SHEET GENERATED FROM THE SIM HEIGHTFIELD H(x,z) — PAPER AND WORLD CORRELATE EXACTLY.', bx0, by + 66);
  ctx.fillText('PERMANENT STRUCTURES AND ROADS ARE PLOTTED — ENEMY POSITIONS ARE NOT. FINDING THEM IS THE EXERCISE.', bx0, by + 84);
  // declination diagram
  const dcx = W - 330, dcy = by + 10;
  ctx.strokeStyle = '#000'; ctx.lineWidth = 1.4;
  ctx.beginPath();
  ctx.moveTo(dcx, dcy + 52); ctx.lineTo(dcx, dcy);
  ctx.moveTo(dcx, dcy); ctx.lineTo(dcx - 4, dcy + 9);
  ctx.moveTo(dcx, dcy); ctx.lineTo(dcx + 4, dcy + 9);
  const mnA = 2 * Math.PI / 180;
  ctx.moveTo(dcx, dcy + 52);
  ctx.lineTo(dcx + Math.sin(mnA) * 52, dcy + 52 - Math.cos(mnA) * 52);
  ctx.stroke();
  ctx.font = '11px Consolas, monospace';
  ctx.fillText('GN', dcx - 22, dcy + 8);
  ctx.fillText('MN', dcx + 12, dcy + 8);
  ctx.fillText('GM ANGLE 2°E (≈36 MILS)', dcx - 60, dcy + 74);
  /* E5 — symbol legend. Every symbol on the sheet is in here, and every entry
     draws its own sample through the same code that drew the map body, so a
     legend row and the thing it explains cannot drift apart. Two columns of
     mission symbols on the left, the shared permanent-detail rows to their
     right, all line art — nothing a monochrome laser has to dither. */
  const ly = by + 122;
  ctx.font = '11px Consolas, monospace';
  ctx.fillStyle = '#000';
  ctx.textAlign = 'left';
  ctx.fillText('LEGEND', bx0, ly - 14);
  ctx.beginPath();
  ctx.moveTo(bx0 + 13, ly - 7); ctx.lineTo(bx0 + 6, ly + 5); ctx.lineTo(bx0 + 20, ly + 5);
  ctx.closePath(); ctx.fill();
  ctx.fillText('OBSERVATION POST', bx0 + 34, ly + 4);
  ctx.beginPath();
  ctx.moveTo(bx0 + 7, ly + 12); ctx.lineTo(bx0 + 19, ly + 24);
  ctx.moveTo(bx0 + 7, ly + 24); ctx.lineTo(bx0 + 19, ly + 12);
  ctx.strokeStyle = '#000'; ctx.lineWidth = 1.8; ctx.stroke();
  ctx.fillText('KNOWN / REGISTRATION POINT', bx0 + 34, ly + 22);
  const legEnd = drawLegend(ctx, LEGEND_ROWS, bx0 + 340, ly, 300, 18, 2,
                            sheetOpt, '10px Consolas, monospace', '#000');
  ctx.font = '10px Consolas, monospace';
  ctx.fillStyle = '#000';
  ctx.fillText(`TERRAIN SEED ${CONFIG.SEED.terrain} · MISSION SEED ${CONFIG.SEED.mission} · SHITFIRE FO TRAINER`, mx0, PH - 18);
  return { legEnd, by, my0, mapPx, mx0, X, Y };
}

function toggleLibrary(force) {
  const on = force !== undefined ? force : !libEl.classList.contains('on');
  if (on) {
    renderSheet();
    refreshLibButtons();
    if (document.exitPointerLock) document.exitPointerLock();
    tutEvent('library');
  }
  libEl.classList.toggle('on', on);
}
function refreshLibButtons() {
  document.querySelectorAll('#libctl [data-sheet]').forEach(b =>
    b.classList.toggle('sel', b.dataset.sheet === libSheet));
}
document.querySelectorAll('#libctl [data-sheet]').forEach(b =>
  b.addEventListener('click', () => { libSheet = b.dataset.sheet; refreshLibButtons(); renderSheet(); }));
document.getElementById('libkey').addEventListener('change', renderSheet);
document.getElementById('libsave').addEventListener('click', () => {
  const a = document.createElement('a');
  const answer = document.getElementById('libkey').checked;
  a.download = `SHITFIRE_${libSheet.toUpperCase()}${answer ? '_ANSWERKEY' : ''}.png`;
  a.href = sheetCv.toDataURL('image/png');
  a.click();
});
document.getElementById('libprint').addEventListener('click', () => {
  const img = document.getElementById('printimg');
  img.onload = () => { window.print(); img.onload = null; };
  img.src = sheetCv.toDataURL('image/png');
});

