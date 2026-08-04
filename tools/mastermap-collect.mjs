/* Master map collector: launch every implemented chapter through the real
   menu, record its scenario geometry; per island (volume terrain seed),
   sample the heightfield + roads + villages + facilities + OP/battery.
   Output: mastermap.json */
import { chromium } from 'playwright-core';
import path from 'path';
import fs from 'fs';
const root = process.argv[2], outJson = process.argv[3];

const browser = await chromium.launch({ channel: 'chrome', headless: true });
const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
await page.route(/^https?:\/\//, r => r.abort());
page.on('pageerror', e => console.log('pageerror:', e.message.split('\n')[0]));
await page.addInitScript(() => {
  try { localStorage.setItem('shitfire_campaign', JSON.stringify({ _devUnlock: true })); } catch (e) {}
});
await page.goto('file:///' + path.join(root, 'SHITFIRE.html').replace(/\\/g, '/'));
await page.waitForFunction(() => window.SHITFIRE && window.SHITFIRE.Scenario, null, { timeout: 30000 });
await page.waitForTimeout(700);

const nvols = await page.evaluate(() => window.SHITFIRE.CAMPAIGN.length);
const out = { islands: {}, chapters: [] };

function sampleTerrain() {
  return page.evaluate(() => {
    const SF = window.SHITFIRE, N = 256, ext = 5000;
    const hs = [];
    for (let j = 0; j < N; j++) {
      const row = new Array(N);
      for (let i = 0; i < N; i++) {
        const x = -ext + (i + 0.5) / N * 2 * ext;
        const z = -ext + (j + 0.5) / N * 2 * ext;
        row[i] = Math.round(SF.H(x, z) * 10) / 10;
      }
      hs.push(row);
    }
    const roads = (SF.WORLD.roads || []).map(r => {
      const step = Math.max(1, Math.floor(r.length / 220));
      const pts = [];
      for (let i = 0; i < r.length; i += step) pts.push([Math.round(r[i].x), Math.round(r[i].z)]);
      const last = r[r.length - 1];
      pts.push([Math.round(last.x), Math.round(last.z)]);
      return pts;
    });
    return {
      h: hs,
      roads,
      villages: (SF.WORLD.villages || []).map(v => ({ x: Math.round(v.x), z: Math.round(v.z), r: Math.round(v.r), name: v.name || '' })),
      facilities: (SF.WORLD.facilities || []).map(f => ({ x: Math.round(f.x), z: Math.round(f.z), name: f.name || '', kind: f.kind || '' })),
      op: { x: Math.round(SF.OP.x), z: Math.round(SF.OP.z) },
      battery: SF.BATTERY ? { x: Math.round(SF.BATTERY.x), z: Math.round(SF.BATTERY.z) } : null,
      map: { originE: SF.CONFIG.MAP.originE, originN: SF.CONFIG.MAP.originN },
    };
  });
}

for (let vi = 0; vi < nvols; vi++) {
  const meta = await page.evaluate(i => {
    const v = window.SHITFIRE.CAMPAIGN[i];
    return { id: v.id, tseed: v.tseed || null, palette: v.palette || null,
             chapters: v.chapters.filter(c => c.impl).map(c => ({ id: c.id, title: c.title, type: c.type, tod: c.tod || (v.tod || 'day'), asset: c.asset || 'battery' })) };
  }, vi);
  if (!meta.chapters.length) continue;
  for (const ch of meta.chapters) {
    await page.keyboard.press('k');
    await page.waitForTimeout(200);
    const clicked = await page.evaluate(([i, chid]) => {
      document.querySelectorAll('#mvols button')[i].click();
      const row = [...document.querySelectorAll('#mchapters .chrow')]
        .find(el => el.querySelector('.chid') && el.querySelector('.chid').textContent.trim() === chid);
      if (!row || row.classList.contains('off')) return false;
      row.click(); return true;
    }, [vi, ch.id]);
    if (!clicked) { console.log('SKIP (locked?)', ch.id); continue; }
    await page.waitForTimeout(1100);
    await page.keyboard.press('Escape');
    const geo = await page.evaluate(() => {
      const S = window.SHITFIRE.Scenario;
      const P = o => o ? [Math.round(o.x), Math.round(o.z)] : null;
      const g = { type: S.type };
      g.enemy = P(S.enemy);
      if (S.compound) g.compound = P(S.compound);
      if (S.fStart) g.fStart = P(S.fStart);
      if (S.village) g.village = P(S.village);
      if (S.bbq) g.bbq = P(S.bbq);
      if (S.fireBase) g.fireBase = P(S.fireBase);
      if (S.avenues) g.avenues = S.avenues.map(P);
      if (S.twinTgt) g.twin = P(S.twinTgt);
      if (S.path) {
        if (S.path.pts) {
          const step = Math.max(1, Math.floor(S.path.pts.length / 60));
          g.route = S.path.pts.filter((_, i) => i % step === 0).map(P);
        } else {
          const p0 = [S.path.sx, S.path.sz];
          const p1 = [S.path.sx + S.path.dx * S.path.len, S.path.sz + S.path.dz * S.path.len];
          g.route = [p0.map(Math.round), p1.map(Math.round)];
        }
        if (S.stop) g.stop = [Math.round(S.path.sx + (S.path.dx || 0) * S.stop.d),
                              Math.round(S.path.sz + (S.path.dz || 0) * S.stop.d)];
      }
      return g;
    });
    const islandKey = String(meta.tseed) + (meta.palette ? '-' + meta.palette : '');
    out.chapters.push({ vol: meta.id, island: islandKey, ...ch, geo });
    if (!out.islands[islandKey]) {
      console.log('sampling island', islandKey, '…');
      out.islands[islandKey] = await sampleTerrain();
      out.islands[islandKey].palette = meta.palette;
    }
    console.log('captured', ch.id, ch.title);
  }
}
fs.writeFileSync(outJson, JSON.stringify(out));
console.log('wrote', outJson, Math.round(fs.statSync(outJson).size / 1024), 'KB;',
  out.chapters.length, 'chapters,', Object.keys(out.islands).length, 'islands');
await browser.close();
