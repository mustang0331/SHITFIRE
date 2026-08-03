/* ============================================================ CAMPAIGN (stage 9 skeleton) */
// Volumes -> chapters per NARRATIVE.md. Chapters are fixed-seed missions;
// impl:false rows are story chapters whose mechanics land in stages 10-11
// (tutorials, strict mode, 60mm, village raid, epilogue) — shown but not
// playable, and skipped by the sequential-unlock chain.
const CAMPAIGN = [
  { id: 'F',  tab: 'FWD', name: 'FOREWORD — THE SCHOOLHOUSE', need: 0, tseed: 1337, chapters: [
    { id: 'F.1', title: 'EYES BEFORE GUNS', blurb: 'optics, mils, laser', impl: true, type: 'wreck', seed: 51,
      story: 'A training beach, stateside. Gunnery Sergeant Bottlecap holds a coffee he clearly hates and looks at you the way a man looks at weather. "The actual FO fell off the boat. Nobody elaborates. That makes you the eyes. Let\'s find out if you have any."',
      outro: 'Gunny initials a box on a clipboard without looking down at it.',
      tut: { noMission: true,
        done: 'Eyes, glass, laser. That is the whole instrument. Tomorrow we find out if the brain is attached.',
        steps: [
          { on: 'look', amt: 5, say: 'First lesson is free: LOOK AROUND. Drag your eyes across the whole horizon — mouse or stick. The heading readout up top is in mils, 6400 to a circle. Go.' },
          { on: 'binos', say: 'Adequate. Now glass up — press [B]. The reticle is graduated in mils; you will measure the world with it.' },
          { on: 'lase', check: h => { TUT.laseSpots.push(h); return true; },
            say: 'Now LASE — put the reticle on any piece of this island and press [L]. Range and grid come straight back. That laser is worth more than you are.' },
          { on: 'lase', check: h => TUT.laseSpots.some(p => dist2(p.x, p.z, h.x, h.z) > 300),
            say: 'Again — a DIFFERENT spot, three hundred meters or more from the first. An observer confirms. He does not assume.' },
        ] } },
    { id: 'F.2', title: 'THE MAP IS NOT THE TERRITORY', blurb: 'mapwork & terrain association', impl: true, type: 'wreck', seed: 52,
      story: '"Paper," Gunny says, producing a map sheet like a priest producing scripture. "The island and this sheet are the same math. Every road, every hut, every tower is plotted. You will learn to stand in one and point at the other."',
      outro: '"The map never lies. It just waits for you to."',
      tut: { noMission: true,
        done: 'That is terrain association. You found a thing in the world on a piece of paper. Some officers never manage it.',
        steps: [
          { on: 'map', say: 'Map up — press [M]. Roads, tracks, villages, facilities: plotted. Enemy: never. Finding HIM is the job.' },
          { on: 'library', say: 'Now the real sheet — press [P] and study the full 1:50,000. Print it if you have a printer; this trade is done on paper. Close it when you have the lay of the island.' },
          { on: 'msg', missSay: 'Negative. Plot it AGAIN. Find it on your map, read right then up, six digits, and send "grid" plus the number.',
            check: p => {
              if (p.type !== 'cff' || p.method !== 'grid' || !p.digits) return false;
              const d = p.digits;
              const w = d.length === 8
                ? enToWorld(parseInt(d.slice(0, 4)) * 10 + 5, parseInt(d.slice(4)) * 10 + 5)
                : enToWorld(parseInt(d.slice(0, 3)) * 100 + 50, parseInt(d.slice(3)) * 100 + 50);
              const lm = tutLandmark();
              return lm && dist2(w.x, w.z, lm.x, lm.z) < 200;
            },
            say: () => `Last rep: TERRAIN ASSOCIATION. Find the ${tutLandmark() ? tutLandmark().name : 'FUEL POINT'} on your map and send me its six-digit grid — say or type "grid" and the number. Do not lase it. Read the paper.` },
        ] } },
    { id: 'F.3', title: 'SAY IT LIKE YOU MEAN IT', blurb: 'your first full call for fire', impl: true, type: 'wreck', seed: 53, par: 420,
      story: 'A derelict landing craft rusts on the beach below. "That wreck has been dead thirty years," Gunny says. "You are going to kill it again — with a complete, correct call for fire. HELLHOUND is live. Try not to embarrass either of us."',
      outro: 'Gunny signs your qualification sheet without looking up. "Congratulations. You are somebody else\'s problem now."',
      coach: [
        'Locate the wreck — binos [B], laser [L]. Then send it: callsigns, ADJUST FIRE, grid, description, OVER.',
        'When the round lands, watch the burst against the target. LEFT/RIGHT so many meters, ADD/DROP so many meters. Then FIRE FOR EFFECT when it is close.',
        'When the wreck is dead: END OF MISSION, target destroyed. Say it like you have done it before.' ] },
  ]},
  { id: 'V1', tab: 'I', name: 'VOLUME I — GREEN AS GRASS', need: 0, tseed: 1337, chapters: [
    { id: '1.1', title: 'TROOPS IN THE OPEN, BRAINS IN THE REAR', blurb: 'grid fundamentals', impl: true, type: 'troops', seed: 101, par: 300,
      story: 'A quiet-sector island, mostly secured — a good place to be bad at your job. Somebody forgot to tell the platoon of enemy infantry sunbathing in the open. First real mission. First real rounds.',
      outro: 'HELLHOUND logs the grid without comment, which from HELLHOUND is a medal.' },
    { id: '1.2', title: 'KNOCK KNOCK', blurb: 'dug-in bunker — bracket it', impl: true, type: 'bunker', seed: 102, par: 360,
      story: 'A bunker with a roof and opinions. Precision matters now: bracket it — over, short, split the difference — and put one through the front door. The book calls it successive bracketing. Gunny called it "haggling."',
      outro: 'The engineers send their thanks and a request that you never again make them identify a bunker by smell.' },
    { id: '1.3', title: 'HOLD THE LINE', blurb: 'near the perimeter, not on it', impl: true, type: 'strongpoint', seed: 103, par: 360,
      story: 'A friendly strongpoint is getting pressed and the perimeter is shrinking by the minute. Your rounds go NEXT to our people, not ON them. There is a village behind the wire, too. Everyone on this island is watching your math, and none of them are being polite about it.',
      outro: 'The strongpoint holds. Somebody inside the wire owes you a beer and will deny it.' },
    { id: '1.4', title: 'DO IT AGAIN, SLOWER', blurb: 'format discipline on the clock', impl: true, type: 'troops', seed: 104, par: 210,
      story: 'HELLHOUND has read your file aloud to the gun crews, twice, doing voices. Today the standard is the FULL format, correct, fast — par is tight and the guns are bored. Prove the schoolhouse took.',
      outro: 'HELLHOUND, grudging: "Log shows a professional worked this net today. I have started an investigation."' },
  ]},
  { id: 'V2', tab: 'II', name: 'VOLUME II — THE RIDGE LINE', need: 6, tseed: 9021, chapters: [
    { id: '2.1', title: 'NUMBERS ON A COMPASS', blurb: 'polar — dead laser, estimate by mil relation', impl: true, type: 'troops', seed: 201, par: 330, method: 'polar', noLaser: true,
      story: 'A new island — coral ridges, dead ground, terrain that hides things. The map is in a crate somewhere and the rangefinder battery died in the surf, so today YOU are the instrument: POLAR, off your own position. Direction from the compass. Distance by MIL RELATION — measure a known object in the reticle, size over mils times a thousand. [R] for the card.',
      outro: 'Direction and distance out of a reticle and a memory. That is the oldest trick in the trade.' },
    /* TEMPO1 — night: shift-from-known-point IS the night mission. Registration
       exists so the guns can shoot accurately in the dark; the observer flies
       it on NVG/thermal [O], the E3 kit's first campaign home. No friendlies
       in this scenario (user rule: night + friendlies waits for TEMPO4). */
    { id: '2.2', title: 'OLD FRIENDS', blurb: 'shift from a known point — after dark', impl: true, type: 'bunker', seed: 202, par: 360, method: 'shift', tod: 'night',
      story: 'The registration points on this island have names — the kind of names positions earn. The guns already know them cold, which matters tonight, because it is DARK out there and the map does not glow. SHIFT from a known point: direction, lateral, range. Ride on data somebody already paid for. Optics [O] — the island looks different through tubes.',
      outro: 'The known points hold, even in the dark. Especially in the dark. That is why we register them before we need them.' },
    { id: '2.3', title: 'DEFILADE BLUES', blurb: 'crest-masked target — fire on what you cannot see', impl: true, type: 'troops', seed: 203, par: 420, scn: { mask: true },
      story: 'Enemy infantry assembling in defilade behind the ridge. You will not see them, and you will not see your bursts — just smoke drifting over the crest and the sound arriving late. Fight this one on the map. The map does not blink. It also does not care that you are scared. Fight it anyway.',
      outro: 'You never saw them. They never saw it coming. Fair trade.' },
    { id: '2.4', title: 'THE PERIMETER', blurb: 'your choice of method, under pressure', impl: true, type: 'strongpoint', seed: 204, par: 330,
      story: 'Another strongpoint, worse ground, less time. Nobody is telling you which location method to use — that is the point. Pick the right tool, first try, while the perimeter shrinks.',
      outro: 'Right method, right rounds. The ridge line is yours now.' },
    /* TEMPO4 — the coordinated-illumination chapter (JFIRE Table 14 / JFO 0103
       §5.d, researched in RESEARCH_NIGHT_RUN.md §2): illum up, walk the light,
       then HE under the flare, re-illuminating as it dies. The first chapter
       deliberately built around a SEQUENCE of fire missions. reqIllum mirrors
       reqImmediate: fighting it dark (thermal is a real tool) still completes,
       capped at 2★ — the chapter's skill is the light. */
    { id: '2.5', title: 'FLARES OUT', blurb: 'night defense — light first, then steel', impl: true, type: 'strongpoint', seed: 205, par: 540, tod: 'night', reqIllum: true,
      story: 'The perimeter again, except somebody turned the island off. A friendly strongpoint is taking fire in the dark and the assault is close enough to hear over the net. You cannot adjust what you cannot see: get ILLUMINATION up — send the call with SHELL ILLUMINATION and walk the light over the attackers, big corrections, the flare does not need ten-meter precision. When they are lit, switch to steel: "SHELL HE, OVER", and fight it like any adjust mission, under your own light. A flare burns a minute, give or take — no two burn alike, so watch the light, not a clock. When it dies, buy another. Registration won the dark for the guns in 2.2 — tonight, light wins it for you.',
      outro: 'The flare settles into the sea and the shooting stops. The strongpoint counts heads twice and comes up even both times. Nobody out there knows the light had a callsign.',
      coach: [
        'Light FIRST, killer. Send a normal call with ILLUMINATION in it. Adjust the flare in big steps — hundreds, not tens — until the attackers are lit.',
        'When you can SEE them: "SHELL HE, OVER" — same mission, new nature — then walk steel onto them under the light. No two flares burn the same, so when the light starts to die, send SHELL ILLUMINATION again and buy more — do not wait for the dark. Doctrine calls the whole dance COORDINATED ILLUMINATION.' ] },
  ]},
  { id: 'V3', tab: 'III', name: 'VOLUME III — THUNDER RUN', need: 14, tseed: 5150, chapters: [
    { id: '3.1', title: 'ROLLING STOCK', blurb: 'convoy — lead it, or catch the pit stop', impl: true, type: 'convoy', seed: 301, par: 420,
      story: 'The big push is on and everything on this island is moving. An enemy column is running the coast road. Intel says they stop for fuel — columns always stop. Lead a moving target, or ambush a sitting one. Choose.',
      outro: 'Burning trucks on a coast road: the war\'s most honest progress report.' },
    { id: '3.2', title: 'CLOSE ENOUGH TO SMELL IT', blurb: 'danger close — say it, creep it', impl: true, type: 'assault', seed: 302, par: 420,
      story: 'Friendlies assaulting an objective with the enemy inside six hundred meters of them. That is DANGER CLOSE — say the proword like you mean it, and creep the fire in from the safe side. A hundred meters at a time. No heroes, no funerals, no phone calls to a general at 0300.',
      outro: 'The assault element walks the objective. Every one of them upright. That was the mission.' },
    /* TEMPO1 — the brief already said "First light."; now the sky agrees. */
    { id: '3.3', title: 'UNINVITED GUESTS', blurb: 'raiders in the village — huts are no-strike', impl: true, type: 'raid', seed: 303, par: 390, tod: 'dawn',
      story: 'First light. Raiders are hitting the fishing village — and the village is full of people whose war this is not. Fire on the raiders. The huts are no-strike. If discrimination were easy, they would not need an observer.',
      outro: 'The raiders are gone. The village still stands. Nobody there will ever know your callsign, which is exactly right.' },
    { id: '3.4', title: "EVERYONE'S MOVING", blurb: 'combined arms, friendlies advancing', impl: true, type: 'assault', seed: 304, par: 390,
      story: 'A full combined-arms push: our infantry advancing, their line breaking, the target shifting while your rounds are in the air. Timing is everything and everything is on the move.',
      outro: 'Fires and maneuver, actually coordinated. Doctrine writers weep with joy.' },
    { id: '3.5', title: 'THE WRONG KIND OF FAMOUS', blurb: 'friendlies interleaved — do not make the news', impl: true, type: 'assault', seed: 305, par: 390, scn: { fClose: true },
      story: 'The lines have collapsed into each other and the map looks like spilled paint. Friendlies are INSIDE your sheaf distances. The chapter title is the threat: observers who guess wrong here get famous in the worst way: a name read aloud at a service nobody wanted to attend.',
      outro: 'Nobody wrote your name in any report today. Stay unfamous.' },
    /* TEMPO2 — the one-transmission chapter. reqImmediate mirrors the method
       requirement: a deliberate three-transmission call still completes the
       mission (the forgiving path is untouched), it just caps at 2★, because
       the skill under test is SPEED — the doctrinal answer to friendlies
       taking effective fire is IMMEDIATE SUPPRESSION, grid, out. */
    { id: '3.6', title: 'SEND IT NOW', blurb: 'immediate suppression — one transmission, rounds now', impl: true, type: 'strongpoint', seed: 306, par: 180, reqImmediate: true,
      story: 'The strongpoint on the low ridge is two minutes from being overrun and their radio is doing that thing where every voice on it is too calm. There is no time for three transmissions and a readback minuet. IMMEDIATE SUPPRESSION, a grid, OUT — rounds now, paperwork later. The book has a page for exactly this day. It is a short page.',
      outro: 'The attack stalls under the fire and breaks. Nobody at the strongpoint knows the format you used. That was the point of the format.',
      coach: [
        'This is the ONE-TRANSMISSION call, killer: "IMMEDIATE SUPPRESSION, GRID, DANGER CLOSE, OUT." No warning order, no description, no MTO. Rounds first, paperwork after.',
        'The attackers are inside six hundred meters of the wire, so DANGER CLOSE rides in the call — even the fast page of the book keeps that word. Lase, send, and keep fire coming: SUPPRESSION only lasts while rounds are landing.' ] },
  ]},
  /* TEMPO1 — the whole volume runs under an overcast: black sand, grey light.
     Overcast is full diffuse daylight (elev 40), so the strict/danger-close
     chapters keep their target discrimination; only the mood changes. */
  { id: 'V4', tab: 'IV', name: 'VOLUME IV — BLACK SAND', need: 24, tseed: 66600, palette: 'black', tod: 'overcast', chapters: [
    { id: '4.1', title: 'STRICT NET', blurb: 'doctrine enforced — full format or silence', impl: true, type: 'strongpoint', seed: 401, par: 360, strict: true,
      story: 'A black-sand fortress island, and a new FDC posture: STRICT NET. Full format or the net stays silent — callsigns, warning order, location, description, OVER, correct rounding, real surveillance. The book, verbatim, under fire.',
      outro: 'The net stayed clean. Doctrine is just courtesy under fire.' },
    /* G18 — the old scn:{effR:30} knob is gone here: the mortar60 asset now
       carries its own FM 7-90 band set (rFull 20 m vs the 155's 30), which IS
       the precision constraint this chapter was faking with a scaled radius. */
    { id: '4.2', title: 'TEN METERS', blurb: '60mm mortars — 8-digit precision', impl: true, type: 'troops', seed: 402, par: 330, asset: 'mortar60',
      story: 'The 155s are displacing, so today you own a 60mm mortar section. Small tubes, tight sheaf, ten-meter grids — EIGHT digits, read right then up. At this precision, "close enough" is a contradiction.',
      outro: 'Ten-meter work with a sixty. The mortar section names a tube after you. It is the ugly one.' },
    { id: '4.3', title: 'NO SECOND CHANCES', blurb: 'hard only. one pass, tight clock.', impl: true, type: 'bunker', seed: 403, par: 240, diffs: ['hard'],
      story: 'One bunker, one clock, hard difficulty, no bracket wide enough to hide in. Everything you have learned, at speed, or nothing.',
      outro: 'On time, on target, no excuses filed. There were none to file.' },
    { id: '4.4', title: 'THE MEAT GRINDER', blurb: 'the final exam — strict, hard, everything at once', impl: true, type: 'assault', seed: 404, par: 330, strict: true, diffs: ['hard'],
      story: 'The last hill on the last island. Strict net, danger close, friendlies advancing, hard difficulty — the whole trade in one mission. Pass this and even HELLHOUND will say something almost kind. Almost.',
      outro: 'HELLHOUND, after a long pause: "…acceptable, MUSTANG." Frame it.' },
    /* SUGG4b — the defense-in-depth chapter: the exam is over, the enemy did
       not read the syllabus. Built on SUGG8's qfp type in its lanes variant
       (phase lines in depth on one canalized axis) with reqPlanned grading —
       a cold full CFF completes at 2★ most; the chapter's skill is the plan. */
    { id: '4.5', title: 'DIG IN', blurb: 'defense in depth — plan the lines, fire in two words', impl: true, type: 'qfp', seed: 405, par: 600, reqPlanned: true, scn: { lanes: true },
      story: 'You took the hill yesterday. Tonight the paperwork says HOLD, and the enemy never reads the paperwork — a counterattack steps off at H-hour, canalized down the one draw that leads up here. This is the fight you get to PLAN. While it is quiet: "PLAN TARGET, GRID …" on each phase line — RED, WHITE, BLUE, stacked in depth down the draw. When they cross RED, you do not build a call for fire. You say two words and the guns already know the rest.',
      outro: 'The draw is a graveyard of a counterattack, one phase line at a time. HELLHOUND: "A plan. An actual plan. I am framing the log, MUSTANG."',
      coach: [
        'Plan while it is QUIET, killer. A target on each phase line — "PLAN TARGET, GRID …" costs nothing and files the data. PRIORITY TARGET the line you expect to fight on.',
        'When they cross a line: "FIRE TARGET AB####" — two words, rounds now. Walk fire down YOUR OWN planned axis as they come. A series or a group of the lines works too. The full call is for people who did not plan.' ] },
  ]},
  { id: 'EP', tab: 'EPI', name: 'EPILOGUE — SUNBURN', need: 32, tseed: 1337, chapters: [
    /* 11a — the Epilogue opens. Humor dial at 11, readback sacred: the FDC
       runs the mission absolutely deadpan; the jokes live in the briefing,
       the coach lines and the fiction — never in place of doctrine. */
    /* TEMPO1 — Epilogue light: the barbecue at golden hour, the crab at first
       light (its own story clock starts at 0400), the space cannon against a
       sunset it will briefly outshine. */
    { id: 'E.1', title: 'THE GREAT CHOW RAID', blurb: 'fire mission on a seagull flock. no, really.', impl: true, type: 'chow', seed: 501, par: 300, tod: 'dusk',
      story: 'The war is won. The paperwork is not. The general is grilling on the beach, and nine hundred seagulls have chosen violence. The flock is assaulting the barbecue pit from the tideline, and the only tubes in range are yours. The cooks are a NO-FIRE line: Private Dombrowski and his potato salad WILL survive this. Fire mission, by the book — the book does not care how stupid the war has become.',
      outro: 'The flock is broken. The potato salad is intact. The general pins nothing on you, because officially none of this happened.',
      coach: ['Birds are TROOPS IN THE OPEN, killer. The field manual does not have a column for wingspan and I am not going to be the one to write it.',
              'Dombrowski is at the grill. If a correction walks one round onto that man or his salad, it is FRATRICIDE, it goes in the report, and I will read the report AT you.'] },
    { id: 'E.2', title: 'CLAWS OUT', blurb: 'a moving target. it is also a crab.', impl: true, type: 'kaiju', seed: 502, par: 420, tod: 'dawn',
      story: 'At 0400 the radar picket reported a contact wading through the surf line. At 0406 the picket stopped filing reports and started filing retirement paperwork. A crab the size of a church is making for the village, and battalion has ruled — in writing — that it is a surface target and therefore yours. It is moving. Lead it. HELLHOUND has been briefed and has elected not to react.',
      outro: 'The crab settles into the shallows. The village never knew. The after-action report lists one (1) hard structure, mobile, destroyed, and no further questions were taken.',
      coach: ['It is a MOVING TARGET, killer. Lead it like the convoy: fire where it is GOING to be, not where it makes you feel things.',
              'Big does not mean soft. That shell is a hard structure — FUZE DELAY pays, and you will need sustained effect. Volley, correct, REPEAT.'] },
    { id: 'E.3', title: 'SUNLAMP ACTUAL', blurb: 'a call for fire to a space cannon. same book.', impl: true, type: 'bunker', seed: 503, par: 300, asset: 'sunlamp', tod: 'dusk',
      story: 'The last thing the war left behind is a bunker complex nobody can reach, so battalion has borrowed something from "a partner force": SUNLAMP, an intergalactic directed-energy space cannon satellite of mass destruction, now holding your net and PLEASED to be here. Same six elements. Same readback. Same corrections in the OT frame. The time of flight is a charging whine from everywhere at once, SHOT is DISCHARGE, SPLASH is SOLAR EVENT, and HELLHOUND — patched in to observe — has never sounded more tired. Send the call.',
      outro: 'END OF MISSION. The bunker is a smooth glass bowl. SUNLAMP thanks you for your business and hopes you will consider orbital fires for your future fire support needs. HELLHOUND says nothing at all, which from HELLHOUND is a standing ovation. The book closes.',
      coach: ['It is a SPACE CANNON, killer, and it still wants the six elements in three transmissions. Doctrine does not care where the tube is parked.',
              'It will ask for a ten-digit grid. It will settle for what you have. Send the call like it is any other gun, because to the book, it is.'] },
  ]},
  { id: 'V5', tab: 'V', name: 'VOLUME V — ON WINGS', need: Infinity,
    tease: 'LOCKED — AWAITING AIRCRAFT. CAS, SOMEDAY.', chapters: [] },
];
const ALL_CHAPTERS = CAMPAIGN.flatMap(v => v.chapters);
CAMPAIGN.forEach(v => v.chapters.forEach(c => { c._vol = v; }));
let activeChapter = null;
// preferred landmark for the F.2 terrain-association drill
function tutLandmark() {
  return WORLD.facilities.find(f => f.kind === 'fuel') ||
         WORLD.facilities.find(f => f.kind === 'mast') ||
         WORLD.facilities[0] ||
         (WORLD.villages[0] ? { name: WORLD.villages[0].name + ' village', x: WORLD.villages[0].x, z: WORLD.villages[0].z } : null);
}

// campaign persistence: best stars per chapter per difficulty
const CAMP = {
  data: {},
  load() {
    try { this.data = JSON.parse(localStorage.getItem('shitfire_campaign') || '{}'); }
    catch (e) { this.data = {}; }
  },
  save() {
    try { localStorage.setItem('shitfire_campaign', JSON.stringify(this.data)); }
    catch (e) { /* in-memory only */ }
  },
  stars(chId) {
    const d = this.data[chId];
    return d ? Math.max(d.easy || 0, d.normal || 0, d.hard || 0) : 0;
  },
  record(chId, diff, stars) {
    if (stars <= 0) return;
    const d = this.data[chId] || (this.data[chId] = {});
    if ((d[diff] || 0) < stars) { d[diff] = stars; this.save(); }
  },
  total() {
    let t = 0;
    for (const ch of ALL_CHAPTERS) t += this.stars(ch.id);
    return t;
  },
};
CAMP.load();

// developer toggle: opens every volume and chapter (progress untouched)
let DEV_UNLOCK = !!CAMP.data._devUnlock;
function volUnlocked(vol) { return DEV_UNLOCK || CAMP.total() >= vol.need; }
function chUnlocked(vol, ch) {
  if (!ch.impl) return false;
  if (DEV_UNLOCK) return true;
  if (!volUnlocked(vol)) return false;
  const impl = vol.chapters.filter(c => c.impl);
  const i = impl.indexOf(ch);
  return i === 0 || CAMP.stars(impl[i - 1].id) > 0;
}
function starStr(n) { return '★'.repeat(n) + '☆'.repeat(5 - n); }

// Public interface: metrics -> 0-5 stars, MW2 style. 1★ = completed;
// bonuses for clean format, <=2 adjusting rounds, initial target location
// within 200 m (the JFO standard), and beating the chapter par time.
// Difficulty caps: easy 3 / normal 4 / hard 5. Any fail = 0.
// Average fractional reduction in miss distance per adjusting round.
// Doctrine's successive bracketing roughly halves it each round (~0.5).
function correctionEfficiency(m) {
  const tr = m.missTrace;
  if (!tr || tr.length < 2) return null;
  let sum = 0;
  for (let i = 1; i < tr.length; i++) sum += (tr[i - 1] - tr[i]) / Math.max(tr[i - 1], 1);
  return sum / (tr.length - 1);
}
function gradeMission(m) {
  const S = Scenario;
  /* G13 — stars gate on the graded outcome. NEUTRALIZED or better earns full
     grading (destruction is not required — neutralization IS mission
     accomplished per FM 6-30, and the competency metrics below decide the
     stars). SUPPRESSED passes only a suppress-intent mission; on a destroy
     mission it is worth a single star — real effect, wrong amount. */
  if (m.failReason) return 0;
  if (m.intent === 'illum') {         // 12h — graded on light provided, then competence
    if (!m.rounds.length) return 0;
  } else if (S.type === 'kaiju') {    // 11b — destroyed or nothing
    if (assessEffect().outcome !== 'destroyed') return 0;
  } else {
    const a = assessEffect();
    if (a.outcome === 'none') return 0;
    if (a.outcome === 'suppressed' && m.intent !== 'suppress') return 1;
  }
  let stars = 1;
  if (m.notes.length === 0) stars++;
  if (m.adjustRounds <= 2) stars++;
  if (m.aimErr0 !== null && m.aimErr0 <= 200) stars++;
  const par = (activeChapter && activeChapter.par) || 300;
  if ((m.tEnd || sim.now) - m.tStart <= par) stars++;
  // Doctrinal deductions (JFO standards): a call that took longer than two
  // minutes to initiate is not five-star work; neither is adjustment that
  // burned rounds without moving the burst. Wasted rounds are the honest
  // signal here — a single large final correction can flatter the average.
  if (m.tInit > 120) stars = Math.min(stars, 4);
  if (m.wasted >= 2) stars = Math.min(stars, 3);
  const eff = correctionEfficiency(m);
  if (eff !== null && eff < 0.25 && m.adjustRounds >= 3) stars = Math.min(stars, 3);
  if (activeChapter && activeChapter.method && m.method &&
      m.method !== activeChapter.method) stars = Math.min(stars, 2);
  // TEMPO2 — a reqImmediate chapter grades the one-transmission call. Same cap
  // shape as the method requirement: deliberate still completes, at 2★ most.
  if (activeChapter && activeChapter.reqImmediate && !m.imm) stars = Math.min(stars, 2);
  // TEMPO4 — a reqIllum chapter demands light before (or during) the HE work.
  // Fighting it dark on thermal still completes the mission — the E3 kit is a
  // real tool — but the chapter's skill is coordinated illumination, so no
  // illum means 2★ at most.
  if (activeChapter && activeChapter.reqIllum && !m.usedIllum) stars = Math.min(stars, 2);
  // SUGG4b — a reqPlanned chapter grades the planned-fires initiation: the
  // graded mission must be born from FIRE/SUPPRESS TARGET, a series, a group,
  // or the FPF. A cold full CFF still completes — at 2★ most, because the
  // chapter's skill is the plan you built while it was quiet.
  if (activeChapter && activeChapter.reqPlanned && !m.planned) stars = Math.min(stars, 2);
  /* ENEMY1 — friendly casualties from ENEMY fire COST the observer, they never
     auto-fail (user decision, 2026-07-30; fratricide/collateral by our own
     rounds remain the only auto-fails). Every two casualty rounds the battery
     put inside the friendly position costs a star, floor 1 — the honest read
     of "score/time": time was already paid hunting the gun, this is the score. */
  if (S.btyCas) stars = Math.max(1, stars - Math.floor(S.btyCas / 2));
  return Math.min(stars, { easy: 3, normal: 4, hard: 5 }[S.difficulty] || 5);
}

/* ============================================================ BRIEFINGS + TUTORIALS (stage 10) */
const briefEl = document.getElementById('brief');
function showBriefing(ch) {
  document.getElementById('brieftitle').textContent = `CHAPTER ${ch.id} — ${ch.title}`;
  document.getElementById('brieftext').textContent = ch.story;
  briefEl.classList.add('on');
  if (document.exitPointerLock) document.exitPointerLock();
  if (CONFIG.VOICE.ttsEnabled && 'speechSynthesis' in window) {
    const u = new SpeechSynthesisUtterance(ch.story);
    u.rate = 1.0; u.pitch = 0.9;
    if (fdcVoice) u.voice = fdcVoice;
    speechSynthesis.speak(u);
  }
}
document.getElementById('briefgo').addEventListener('click', () => {
  briefEl.classList.remove('on');
  if ('speechSynthesis' in window) speechSynthesis.cancel();
});

// Foreword tutorial engine: GUNNY BOTTLECAP walks the observer through steps
// keyed to real events (look / binos / lase / map / library / msg). noMission
// tutorials suppress live fire missions; F.3 rides the normal mission flow.
const TUT = { steps: null, i: 0, noMission: false, yawAcc: 0, lastYaw: null, laseSpots: [] };
function tutStart(ch) {
  TUT.steps = ch.tut.steps; TUT.i = 0; TUT.noMission = !!ch.tut.noMission;
  TUT.yawAcc = 0; TUT.lastYaw = null; TUT.laseSpots.length = 0;
  schedule(sim.now + 2.5, tutSay);
}
function tutStop() { TUT.steps = null; }
function tutSay() {
  if (!TUT.steps || !TUT.steps[TUT.i]) return;
  const s = TUT.steps[TUT.i];
  log('', 'GUNNY BOTTLECAP: ' + (typeof s.say === 'function' ? s.say() : s.say), 'sys');
}
function tutEvent(ev, data) {
  if (!TUT.steps) return false;
  const s = TUT.steps[TUT.i];
  if (!s || s.on !== ev) return false;
  if (s.check && !s.check(data)) {
    if (s.missSay && ev === 'msg') { log('', 'GUNNY BOTTLECAP: ' + s.missSay, 'sys'); return true; }
    return false;
  }
  TUT.i++;
  if (TUT.i >= TUT.steps.length) tutComplete();
  else schedule(sim.now + 1.0, tutSay);
  return true;
}
function tutComplete() {
  const ch = activeChapter;
  TUT.steps = null;
  const cap = { easy: 3, normal: 4, hard: 5 }[DIFFICULTY] || 3;
  CAMP.record(ch.id, DIFFICULTY, cap);
  log('', 'GUNNY BOTTLECAP: ' + ch.tut.done, 'sys');
  aarBox.innerHTML =
    `<h2>TRAINING COMPLETE — ${ch.id}: ${ch.title}</h2>` +
    `<div class="aarstars">${starStr(cap)}</div>` +
    `<div class="verdict pass">QUALIFIED</div>` +
    (ch.outro ? `<div style="font-style:italic;color:#9fb08c;margin:8px 0">${ch.outro}</div>` : '') +
    `<div class="foot">[K] — mission menu &middot; [ESC] — back to the OP</div>`;
  if (document.exitPointerLock) document.exitPointerLock();
  aarEl.classList.add('on');
  setState('AAR');
}

/* ============================================================ MISSION MENU [K] */
const menuEl = document.getElementById('menu');
let menuVol = 'V1';
function renderMenu() {
  const total = CAMP.total();
  document.getElementById('mtotal').textContent = `TOTAL ${total}★`;
  // volume spines
  const vEl = document.getElementById('mvols');
  vEl.innerHTML = '';
  for (const vol of CAMPAIGN) {
    const b = document.createElement('button');
    b.className = 'mbtn' + (vol.id === menuVol ? ' sel' : '');
    b.textContent = vol.tab + (volUnlocked(vol) ? '' : ' \u{1F512}');
    b.onclick = () => { menuVol = vol.id; renderMenu(); };
    vEl.appendChild(b);
  }
  // chapter shelf for the selected volume
  const vol = CAMPAIGN.find(v => v.id === menuVol);
  const unlocked = volUnlocked(vol);
  document.getElementById('mvolname').textContent = vol.name +
    (vol.chapters.length && !unlocked ? ` — LOCKED: ${vol.need}★ NEEDED (YOU HAVE ${total}★)` : '');
  const cEl = document.getElementById('mchapters');
  cEl.innerHTML = '';
  if (!vol.chapters.length) {
    const d = document.createElement('div');
    d.className = 'chrow off';
    d.innerHTML = `<span class="chtitle">${vol.tease}</span>`;
    cEl.appendChild(d);
  }
  for (const ch of vol.chapters) {
    const open = chUnlocked(vol, ch);
    const row = document.createElement('div');
    row.className = 'chrow' + (open ? '' : ' off');
    const best = CAMP.stars(ch.id);
    row.innerHTML =
      `<span class="chid">${ch.id}</span>` +
      `<span class="chtitle">${ch.title}</span>` +
      `<span class="chblurb">${!ch.impl ? ch.blurb :
        (open ? ch.blurb + (ch.diffs ? ' (' + ch.diffs.join('/').toUpperCase() + ')' : '')
              : unlocked ? 'complete the previous chapter' : ch.blurb)}</span>` +
      `<span class="chstars">${ch.impl ? starStr(best) : '&mdash;'}</span>`;
    if (open) row.onclick = () => newMission(false, null, ch);
    cEl.appendChild(row);
  }
  // skirmish scenario buttons
  const tEl = document.getElementById('mtypes');
  tEl.innerHTML = '';
  for (const k of SCN_TYPES) {
    const b = document.createElement('button');
    b.className = 'mbtn' + (k === currentType && !activeChapter ? ' sel' : '');
    b.textContent = SCN_META[k].name;
    b.onclick = () => newMission(false, k);
    tEl.appendChild(b);
  }
  const rb = document.createElement('button');
  rb.className = 'mbtn';
  rb.textContent = 'RANDOM';
  rb.onclick = () => newMission(false);
  tEl.appendChild(rb);
  const dEl = document.getElementById('mdiffs');
  dEl.innerHTML = '';
  for (const d of ['easy', 'normal', 'hard']) {
    const b = document.createElement('button');
    b.className = 'mbtn' + (d === DIFFICULTY ? ' sel' : '');
    b.textContent = d.toUpperCase() + ' (≤' + { easy: 3, normal: 4, hard: 5 }[d] + '★)';
    b.onclick = () => { DIFFICULTY = d; renderMenu(); };
    dEl.appendChild(b);
  }
  const rows = [];
  for (const k of SCN_TYPES) {
    const s = BEST.data[k + ':' + DIFFICULTY];
    if (s) rows.push(`${SCN_META[k].name}: ${s.rounds} adj rds, ${fmtTime(s.time)}`);
  }
  document.getElementById('mbest').textContent = 'SKIRMISH BEST (' + DIFFICULTY.toUpperCase() + '): ' +
    (rows.length ? rows.join(' · ') : 'no passes recorded yet');
  document.getElementById('mtlogn').textContent = TLOG.entries.length + ' entries';
  const devB = document.getElementById('mdevunlock');
  devB.textContent = 'DEV UNLOCK: ' + (DEV_UNLOCK ? 'ON' : 'OFF');
  devB.classList.toggle('sel', DEV_UNLOCK);
  // G3 — the menu face of SHIFT+D. Highlighted when OFF: no-dispersion is the
  // unusual state, and it must be impossible to be in it without noticing.
  const dispB = document.getElementById('mdisp');
  dispB.textContent = CONFIG.BALLISTICS.dispersion
    ? 'IMPACT DISPERSION: ON'
    : 'IMPACT DISPERSION: OFF — rounds land exactly on the aimpoint, not graded';
  dispB.classList.toggle('sel', !CONFIG.BALLISTICS.dispersion);
}
function toggleMenu(force) {
  const on = force !== undefined ? force : !menuEl.classList.contains('on');
  if (on) {
    renderMenu();
    if (document.exitPointerLock) document.exitPointerLock();
  }
  menuEl.classList.toggle('on', on);
}
document.getElementById('mdem').addEventListener('click',
  () => document.getElementById('demfile').click());
document.getElementById('demfile').addEventListener('change', e => {
  if (e.target.files[0]) loadDEMFile(e.target.files[0]);
  e.target.value = '';
});
document.getElementById('mdemreset').addEventListener('click', () => {
  if (DEM) {
    DEM = null;
    rebuildWorld();
    log('', 'Procedural island restored.', 'sys');
  }
});
document.getElementById('mtlogtxt').addEventListener('click', () => {
  const stamp = new Date().toISOString().slice(0, 19).replace(/[T:]/g, '-');
  downloadText(`shitfire_transcript_${stamp}.txt`, TLOG.text(), 'text/plain');
});
document.getElementById('mtlogjson').addEventListener('click', () => {
  const stamp = new Date().toISOString().slice(0, 19).replace(/[T:]/g, '-');
  downloadText(`shitfire_transcript_${stamp}.json`,
    JSON.stringify(TLOG.entries, null, 1), 'application/json');
});
document.getElementById('mtlogclear').addEventListener('click', () => {
  TLOG.clear();
  renderMenu();
});
document.getElementById('mdevunlock').addEventListener('click', () => {
  DEV_UNLOCK = !DEV_UNLOCK;
  CAMP.data._devUnlock = DEV_UNLOCK;
  CAMP.save();
  log('', `Developer unlock ${DEV_UNLOCK ? 'ON — all chapters open (progress untouched)' : 'OFF — normal star-gated progression'}.`, 'sys');
  renderMenu();
});
// G3 — same toggle as SHIFT+D; toggleDispersion owns the flag, the HUD marker,
// the not-graded marking and the TLOG entry, so the two entrances cannot drift
document.getElementById('mdisp').addEventListener('click', () => {
  toggleDispersion();
  renderMenu();
});

/* ============================================================ NEW MISSION */
function newMission(first, type, chapter) {
  aarEl.classList.remove('on');
  menuEl.classList.remove('on');
  clearEvents();
  if ('speechSynthesis' in window) speechSynthesis.cancel();
  FDC.lastT = 0;
  cffqReset();            // G22 — a half-sent call must not survive into a new mission
  FPF = null;             // SUGG1 — an FPF is planned against THIS fight's geometry
  mission = null;
  for (const k in MISSIONS) delete MISSIONS[k];   // SUGG5 - the net clears
  bursts.forEach(b => { b.active = false; b.group.visible = false; });
  activeChapter = chapter || null;
  tutStop();
  if (chapter) {
    // campaign chapter: fixed seed, fixed scenario, difficulty restrictions
    ammoFull();   // SUGG7 — fixed-seed grading must not vary with session history
    CONFIG.SEED.mission = chapter.seed;
    currentType = chapter.type;
    if (chapter.diffs && !chapter.diffs.includes(DIFFICULTY)) {
      DIFFICULTY = chapter.diffs[0];
      log('', `This chapter is ${chapter.diffs.join('/').toUpperCase()} only — difficulty set to ${DIFFICULTY.toUpperCase()}.`, 'sys');
    }
    // per-volume island: rebuild the world when the volume's terrain differs
    // (a user-loaded DEM always wins)
    const vol = chapter._vol;
    if (!DEM && vol && vol.tseed) {
      const wantPal = vol.palette || null;
      if (CONFIG.SEED.terrain !== vol.tseed || TERRAIN_PALETTE !== wantPal) {
        CONFIG.SEED.terrain = vol.tseed;
        TERRAIN_PALETTE = wantPal;
        buildTerrain(); buildMinimap(); placeOP(); placeBattery(); buildWorldFeatures();
        wipeRecordedTargets();   // G21 — same reasoning as rebuildWorld: new island, stale coordinates
        craterClear();           // 13h — same reasoning again: scars belong to the old ground
        log('', 'Deploying to a new island — new map sheets are in the library [P].', 'sys');
      }
    }
    // 13c: chapter TOD overrides volume TOD, both default to day. Only the light and
    // sky change — nothing here touches ballistics, LOS, or the map sheets.
    const wantTOD = chapter.tod || (vol && vol.tod) || 'day';
    if (wantTOD !== TOD) {
      applyTOD(wantTOD);
      if (wantTOD !== 'day') log('', `Light: ${TOD.toUpperCase()}.`, 'sys');
    }
  } else {
    if (!first) CONFIG.SEED.mission++;
    if (type) currentType = type;
    else if (!first) currentType = SCN_TYPES[Math.floor(Math.random() * SCN_TYPES.length)];
    // 13c: Skirmish is always full daylight, so a chapter's dusk can't leak into it.
    if (TOD !== 'day') applyTOD('day');
  }
  Scenario = genScenario(currentType, CONFIG.SEED.mission);
  scenarioT0 = sim.now;
  placeUnits();
  // 13e — move the high-res terrain patch onto the new target area, so burst
  // deviation there is judged against real micro-relief, not 33 m facets
  setTerrainFocus(Scenario.enemy.x, Scenario.enemy.z);
  // 13f — clear vegetation off this mission's elements and corridors: the
  // observer must always be able to see the target and the fall of shot
  vegMissionCull();
  if (chapter) {
    log('', `CHAPTER ${chapter.id} — ${chapter.title} (${DIFFICULTY.toUpperCase()}). ${Scenario.brief}`, 'sys');
    if (chapter.method)
      log('', `Chapter requirement: locate the target by ${chapter.method === 'polar' ? 'POLAR (direction + distance)' : 'SHIFT FROM A KNOWN POINT'} — a grid call caps this chapter at 2★.` +
        // G9 — say the prerequisite up front, where the requirement is stated,
        // so the FDC's refusal is a reminder rather than an ambush
        (chapter.method === 'polar' ? ' POLAR needs your position first: fix yourself on the sheet and send "POSITION GRID …" before your call — the FDC resolves the mission from the position you REPORT.' : ''), 'sys');
    if (chapter.strict)
      log('', 'STRICT NET: full format enforced — callsigns, warning order, location, description, OVER; corrections rounded (deviation in tens, range in hundreds — fifties only entering fire for effect); surveillance term at end of mission.', 'sys');
    if (chapter.asset === 'mortar60')
      log('', `ASSET: 60mm mortar section, callsign ${CONFIG.FDC.fdc60} — address THEM, not HELLHOUND; strict net will bounce the wrong callsign. 8-digit grids (10 m precision) required. Tighter dispersion, tighter target.`, 'sys');
    if (chapter.par)
      log('', `Par time for full stars: ${fmtTime(chapter.par)}.`, 'sys');
    if (chapter.tut) tutStart(chapter);
    if (chapter.coach)
      chapter.coach.forEach((c, i) =>
        schedule(sim.now + 4 + i * 7, () => log('', 'GUNNY BOTTLECAP: ' + c, 'sys')));
    if (chapter.story) showBriefing(chapter);
  } else {
    log('', `SKIRMISH: ${SCN_META[currentType].name} (${DIFFICULTY.toUpperCase()}). ${Scenario.brief}`, 'sys');
  }
  // 12i — the met line: wind is reported the way an observer would get it, and
  // every smoke source on the island (bursts, wisps, screens, illum chutes)
  // drifts to match, so the report can be CHECKED against what the eye sees
  log('', `MET: wind from ${fmtMils(WIND.fromMils)} mils at ${Math.round(WIND.speed)} m/s — read your smoke; it will drift downwind. Rounds are not wind-corrected on this net (the FDC applies met).`, 'sys');
  log('', `Your OP: watchtower at grid ${gridOf(OP.x, OP.z)}, ground elev ${Math.round(OP.h)} m, deck +${Math.round(CONFIG.CAMERA.towerHeight)} m. ` +
    `Use [B] binos and [L] laser to fix the target ([M] map, [K] mission menu), then send your call for fire — grid, polar, or shift from a known point. ` +
    `Hold [SPACE] to transmit by voice, or type below.`, 'sys');
  // G2 — say this once per mission, unprompted. The observer has an instrument
  // that reads one north and a net that runs on another, and finding that out by
  // being 124 mils wrong is a worse lesson than being told.
  log('', `NAVIGATION: your heading readout is MAGNETIC. The grid, the map sheet and the fire net are GRID. ` +
    `Declination is ${CONFIG.NAV.declEastDeg}° EAST, so GRID = MAG + ${Math.round(declMils())} mils. ` +
    `Send OT DIRECTION in GRID — [R] has the conversion on the mil card, and the sheet carries the declination diagram.`, 'sys');
  if (Scenario.kps && Scenario.kps.length)
    log('', 'Known points registered: ' + Scenario.kps.map(kp =>
      `AB${kp.id} "${kp.name}" grid ${gridOf(kp.x, kp.z)} (grid brg ${fmtMils(radToMils(azTo(OP.x, OP.z, kp.x, kp.z)))})`).join(' · ') +
      '. Shift example: "adjust fire, shift known point 1001, right 200, add 400, over" — the KP name works too ("shift BREWERY").', 'sys');
  if (WORLD.villages.length)
    log('', `NO-STRIKE: civilian village${WORLD.villages.length > 1 ? 's' : ''} ` +
      WORLD.villages.map(v => v.name).join(' and ') +
      ' — plotted on your map sheet. Collateral damage fails the mission, same as fratricide. ' +
      'Permanent structures and roads are on the sheet too — use them to orient.', 'sys');
  FDC.say(pick(QUIPS.greet), { delay: 1.5 });
  // continuity: HELLHOUND remembers your record
  const car = CAMP.data._career;
  if (car && Math.random() < 0.4) {
    if (car.frat > 0) FDC.say(pick(QUIPS.careerFrat), { delay: 1.6 });
    else if (car.collat > 0) FDC.say(pick(QUIPS.careerCollat), { delay: 1.6 });
    else if (car.missions >= 5) FDC.say(pick(QUIPS.careerVet), { delay: 1.6 });
  }
  // orient the observer onto a target AREA — never the grid. See sendSpotReport().
  if (Scenario.type === 'callin') callerKickoff();   // NET1 — the caller IS the cue
  else if (SPOT_SCN[Scenario.type]) sendSpotReport('start');
  else FDC.say(pick(QUIPS.spotNone), { delay: CONFIG.SPOT.lead });
  if (first) {
    const c = Scenario.compound || Scenario.enemy;
    yaw = azTo(OP.x, OP.z, c.x, c.z);
    pitch = Math.atan2(H(c.x, c.z) + 2 - eye.y, dist2(OP.x, OP.z, c.x, c.z));
  }
  setVision('day');            // every mission starts on the naked eye
  setState('OBSERVING');
}

