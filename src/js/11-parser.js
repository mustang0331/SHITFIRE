/* ============================================================ PARSER */
// Voice transcripts arrive as words ("two four five five two three"), so
// normalization converts number words to digits and merges digit runs.
const DIGIT_WORDS = { zero: '0', oh: '0', o: '0', one: '1', two: '2', three: '3',
  four: '4', five: '5', six: '6', seven: '7', eight: '8', nine: '9', niner: '9' };
const TENS_WORDS = { ten: '10', twenty: '20', thirty: '30', forty: '40', fifty: '50',
  sixty: '60', seventy: '70', eighty: '80', ninety: '90' };
/* F5 — speech-recognition repairs for the CORRECTION prowords, from transcript
   evidence. The browser recogniser renders "right" as WRITE and "drop" as DROPPED
   or DRAW, and the correction regexes match none of those. The failure mode was
   the bad one: `"WRITE 50 ADD 50"` matched only the range half, so the FDC read
   back "ADD 50, OUT" and the deviation was **silently discarded** — no error, no
   notice, the observer believing he had sent a correction he had not. A mangled
   range word alone parsed as full `unknown`, which cost a player over a minute of
   retries in one recorded session before he landed on the exact word "drop".

   Repaired in normalize() so there is one place it happens and every consumer —
   corrections, shifts, refinements at end of mission — inherits it.

   Chosen conservatively. Every entry is a word with no other meaning on a fire
   net, so a repair cannot destroy a legitimate transmission. NOT included: "at"
   (it is the proword "at my command"), "ad" (too short to be safe), and "fire"
   variants (already handled structurally by G24's ordering). */
const STT_FIX = {
  write: 'right', wright: 'right', rite: 'right', writ: 'right',
  dropped: 'drop', draw: 'drop', drawn: 'drop', drops: 'drop', dropp: 'drop',
  lft: 'left', laft: 'left',
  ads: 'add', adds: 'add', added: 'add',
  danjer: 'danger',
  /* NET3 — "FIRE FOR AFFECT" (transcript 20-01 t=2111.8) silently fired an
     adjust round instead of the FFE the observer called for. Same class of
     repair as the rest of this table; "affect" has no other meaning on a
     fire net. */
  affect: 'effect',
};
function normalize(s) {
  let toks = s.toLowerCase().replace(/[^a-z0-9]+/g, ' ').trim().split(' ').filter(Boolean);
  toks = toks.map(w => STT_FIX[w] || w);
  toks = toks.map(w => DIGIT_WORDS[w] !== undefined ? DIGIT_WORDS[w] : (TENS_WORDS[w] || w));
  // "four hundred" -> 400, "two thousand" -> 2000
  const out = [];
  for (const w of toks) {
    if ((w === 'hundred' || w === 'thousand') && out.length && /^\d+$/.test(out[out.length - 1]))
      out[out.length - 1] = String(parseInt(out[out.length - 1]) * (w === 'hundred' ? 100 : 1000));
    else out.push(w);
  }
  // merge runs of single digits: "2 4 5 5 2 3" -> "245523", "5 0" -> "50"
  const merged = [];
  for (let i = 0; i < out.length; i++) {
    if (/^\d$/.test(out[i])) {
      let num = out[i];
      while (i + 1 < out.length && /^\d$/.test(out[i + 1])) num += out[++i];
      merged.push(num);
    } else merged.push(out[i]);
  }
  return merged.join(' ');
}

function parseMessage(raw) {
  const t = normalize(raw);
  const toks = t.split(' ').filter(Boolean);
  if (!toks.length) return { type: 'empty' };
  if (t.includes('say again')) return { type: 'sayagain' };
  /* G25 — the two SAFETY prowords (DOCTRINE.md §67). Both used to fall through to
     `unknown` and be answered with the reply reserved for gibberish. These are the
     calls that stop guns; being mocked for one, in the moment somebody uses one,
     is the worst response the trainer could give. Matched EARLY and before
     anything else so no future keyword can shadow them. */
  if (t.includes('check firing')) return { type: 'checkfire', raw: t, toks };
  if (t.includes('cease loading')) return { type: 'ceaseload', raw: t, toks };
  /* G24 — method of fire and control (DOCTRINE.md §34), all previously `unknown`.
     Order matters twice here: CANCEL AT MY COMMAND must be tested before AT MY
     COMMAND (it contains it), and both must be tested before the bare FIRE below,
     since every one of these transmissions contains the word "fire". */
  if (t.includes('cancel at my command')) return { type: 'amc', on: false, raw: t, toks };
  if (t.includes('do not load')) return { type: 'donotload', raw: t, toks };
  if (t.includes('cannot observe')) return { type: 'cannotobserve', raw: t, toks };
  const mTot = t.match(/\btime on target\s+(\d+)\b/);
  if (mTot) return { type: 'tot', sec: parseInt(mTot[1]), raw: t, toks };
  if (t.includes('time on target')) return { type: 'tot', sec: 0, raw: t, toks };
  /* G15 — sheaf, sent standalone mid-mission ("CONVERGED SHEAF, OVER") or
     cancelled ("CANCEL CONVERGED SHEAF" — ATP 3-09.30 §5-30). A sheaf named
     inside a call for fire rides in that call's raw text instead and is
     extracted by handleCFF; this branch is only for the bare transmission,
     which previously parsed as gibberish. */
  /* 12h — shell, sent standalone mid-mission ("SHELL SMOKE, OVER"): switches
     what the tubes are loading for the rest of the mission. Same bare-
     transmission discipline as fuze/sheaf below; a shell named inside a call
     rides in the call's raw text and is extracted by handleCFF. */
  const mShell = t.match(/\bshell\s+(he|smoke|wp|white phosphorus|illum\w*)\b/);
  if (mShell && !t.includes('grid') && !/\b(?:left|right|add|drop)\b/.test(t)) {
    const rest = t.replace(mShell[0], ' ')
      .replace(/\b(?:over|out|break|hellhound|hacksaw|fires|this|is|mustang|\d+)\b/g, ' ')
      .split(/\s+/).filter(Boolean);
    if (rest.length <= 2) {
      const k = mShell[1];
      return { type: 'shell',
               kind: k === 'wp' || k === 'white phosphorus' ? 'smoke'
                   : k.startsWith('illum') ? 'illum' : k, raw: t, toks };
    }
  }
  /* G16 — fuze, sent standalone mid-adjustment ("FUZE VT, OVER" — legal after
     range/deviation and before fire for effect, ATP 3-09.30 §5-55). A fuze
     named inside a call rides in that call's raw text and is extracted by
     handleCFF, same split as sheaf below. */
  const mFuze = t.match(/\bfuze\s+(vt|victor tango|variable time|time|delay|quick|pd|point detonating)\b/);
  if (mFuze && !t.includes('grid') && !/\b(?:left|right|add|drop)\b/.test(t)) {
    const rest = t.replace(mFuze[0], ' ')
      .replace(/\b(?:over|out|break|hellhound|hacksaw|fires|this|is|mustang|shell|he|fire|for|effect|\d+)\b/g, ' ')
      .split(/\s+/).filter(Boolean);
    if (rest.length <= 2) {
      const k = mFuze[1];
      const kind = (k === 'victor tango' || k === 'variable time') ? 'vt'
                 : (k === 'point detonating' || k === 'quick') ? 'pd' : k;
      return { type: 'fuze', kind, raw: t, toks };
    }
  }
  /* G10 — FIRE UNIT STATUS, requested by the observer before the first
     mission (NGF shape, FM 6-30 §8-12 — no land doctrine has one). Contains
     the word "fire", so it must sit before the bare-FIRE branch; tested here
     with its method-of-engagement kin. Tolerant of the STT variants. */
  if (/\b(?:fire|firing) unit'?s? status\b/.test(t) || /\brequest (?:your )?status\b/.test(t))
    return { type: 'unitstatus', raw: t, toks };
  const mSheaf = t.match(/\b(?:(cancel)\s+)?(converged|open|parallel|linear|circular|special)\s+sheaf\b/);
  if (mSheaf && !t.includes('fire') && !t.includes('grid')) {
    // claim only a BARE sheaf transmission — if real description text rides
    // along ("converged sheaf, troops in trenches"), let it flow to the CFF
    // queue as a description carrying a sheaf, not vanish into this branch
    const rest = t.replace(mSheaf[0], ' ')
      .replace(/\b(?:over|out|break|hellhound|hacksaw|fires|this|is|mustang|\d+)\b/g, ' ')
      .split(/\s+/).filter(Boolean);
    if (rest.length <= 2)
      return { type: 'sheaf', kind: mSheaf[2], cancel: !!mSheaf[1], raw: t, toks };
  }
  if (t.includes('end of mission')) {
    // RREMS: refinement and "record as target" may ride with end of mission
    const ref = { right: 0, add: 0, any: false };
    const rLR = t.match(/\b(left|right)\s+(\d+)/);
    if (rLR) { ref.right = (rLR[1] === 'right' ? 1 : -1) * parseInt(rLR[2]); ref.any = true; }
    const rAD = t.match(/\b(add|drop)\s+(\d+)/);
    if (rAD) { ref.add = (rAD[1] === 'add' ? 1 : -1) * parseInt(rAD[2]); ref.any = true; }
    // surveillance text only — strip the refinement and record prowords
    const bda = t.replace(/.*end of mission/, '')
                 .replace(/\b(left|right|add|drop|up|down)\s+\d+/g, '')
                 .replace(/record as target/g, '')
                 .replace(/\bover\b\s*$/, '')
                 .replace(/\s{2,}/g, ' ').replace(/^[\s,]+|[\s,]+$/g, '');
    return { type: 'eom', refine: ref, record: /record as target/.test(t), bda };
  }
  const ffe = t.includes('fire for effect');
  /* G9 — POSITION REPORT. Must be tested BEFORE grid extraction: "position grid
     245 523" contains the word "grid" and a digit run, so the grid-CFF branch
     would otherwise claim it as a fire mission at the observer's own position —
     which is not a parse error, it is a fire mission at the observer's own
     position. The pattern requires a position word IMMEDIATELY before "grid" so
     that a target description like "enemy position on the hill" in a real CFF
     can never trip it. */
  const mPos = t.match(/\b(?:pos\s*rep|my\s+position|observer\s+position|position)\s+(?:is\s+)?grid\s+((?:\d+\s*)+)/);
  if (mPos && !t.includes('adjust fire') && !t.includes('fire for effect')) {
    const pd = mPos[1].replace(/\s+/g, '');
    return { type: 'posrep', digits: pd, raw: t, toks };
  }
  /* G14/G26 — the IMMEDIATE mission types (DOCTRINE.md §22) are sent as ONE
     transmission: ID + warning order + location, and the FDC sends no MTO. Before
     this, "immediate suppression, grid …" parsed as an ordinary HE grid mission and
     the mission type was silently lost, which is worse than being unrecognised. */
  const imm = t.includes('immediate suppression') ? 'suppress'
            : t.includes('immediate smoke') ? 'smoke' : null;
  /* G27 — SUPPRESS on a previously recorded target, by number and duration.
     Tested before location parsing because it carries a target number, not a grid. */
  const mSup = t.match(/\bsuppress\s+(?:target\s+)?([a-z]{2})\s*(\d{4})\b/);
  if (mSup && !imm) {
    const mMin = t.match(/(\d+)\s*(?:minute|minutes|min)\b/);
    return { type: 'suppresstgt', tgtNum: (mSup[1] + mSup[2]).toUpperCase(),
             minutes: mMin ? parseInt(mMin[1]) : null, raw: t, toks };
  }
  /* SUGG2 — priority target (ATP 3-09.30 §1-29): designate a RECORDED target
     as the one the guns stay laid on. Same target-number shape as suppress,
     tested before location parsing for the same reason. */
  const mPri = t.match(/\bpriority\s+target\s+([a-z]{2})\s*(\d{4})\b/);
  if (mPri && !imm)
    return { type: 'prioritytgt', tgtNum: (mPri[1] + mPri[2]).toUpperCase(), raw: t, toks };
  /* SUGG2 — fire a recorded target by number ("FIRE TARGET AB7101"). On the
     priority target the guns are already laid; on any other filed target the
     FDC re-lays first and the reaction time shows it. */
  const mFire = t.match(/\bfire\s+target\s+([a-z]{2})\s*(\d{4})\b/);
  if (mFire && !imm)
    return { type: 'firetgt', tgtNum: (mFire[1] + mFire[2]).toUpperCase(), raw: t, toks };
  /* SUGG1 — the final protective fire (ATP 3-09.30 §1-30–1-32, §7-16–7-23).
     Three shapes, all tested before location parsing:
       "FIRE THE FPF" / "REPEAT THE FPF"      — execute at max rate
       "PLAN FPF, GRID nnnnnn, DIRECTION dddd" — establish the line
       "NUMBER n, RIGHT/LEFT x, ADD/DROP y" / "NUMBER n, REPEAT"
                                               — flank-piece-in adjustment
     The gun-number regex demands a single digit at a word boundary, so an MTO
     readback's "TARGET NUMBER 7001" can never be mistaken for a piece. */
  if (/\b(?:fire|repeat)\s+(?:the\s+)?fpf\b/.test(t))
    return { type: 'firefpf', raw: t, toks };
  if (/\bfpf\b/.test(t) && toks.includes('grid')) {
    const gi2 = toks.indexOf('grid');
    let d2 = '';
    for (let i = gi2 + 1; i < toks.length && /^\d+$/.test(toks[i]); i++) d2 += toks[i];
    const mDir = t.match(/\bdirection\s+(\d{3,4})\b/);
    return { type: 'planfpf', digits: d2, dirMils: mDir ? parseInt(mDir[1], 10) : null,
             raw: t, toks };
  }
  const mGun = t.match(/\bnumber\s+(\d)\b/);
  if (mGun) {
    const gR = t.match(/\bright\s+(\d+)\b/), gL = t.match(/\bleft\s+(\d+)\b/);
    const gA = t.match(/\badd\s+(\d+)\b/), gD = t.match(/\bdrop\s+(\d+)\b/);
    const rep = /\brepeat\b/.test(t);
    if (rep || gR || gL || gA || gD)
      return { type: 'fpfadj', gun: parseInt(mGun[1], 10), repeat: rep,
               right: gR ? parseInt(gR[1], 10) : gL ? -parseInt(gL[1], 10) : 0,
               add: gA ? parseInt(gA[1], 10) : gD ? -parseInt(gD[1], 10) : 0,
               raw: t, toks };
  }
  // grid extraction
  const gi = toks.indexOf('grid');
  let digits = '';
  if (gi >= 0) {
    for (let i = gi + 1; i < toks.length && /^\d+$/.test(toks[i]); i++) digits += toks[i];
  }
  /* An immediate call is shouted under fire, and the observer routinely drops the
     word "grid" — a real transcript has "IMMEDIATE SUPPRESSION 253535 OUT", which
     used to parse as nothing at all. Accept a bare 6/8-digit run when the
     transmission is already unambiguously an immediate mission. */
  if (!digits && imm)
    for (const w of toks)
      if (/^\d+$/.test(w) && (w.length === 6 || w.length === 8)) { digits = w; break; }
  if (digits) {
    const warno = imm ? 'ffe' : (ffe ? 'ffe' : (t.includes('adjust fire') ? 'adjust' : 'none'));
    // description: words after the grid digit run, up to a trailing proword
    let di = gi + 1;
    while (di < toks.length && /^\d+$/.test(toks[di])) di++;
    const stop = new Set(['over', 'out', 'break']);
    const descToks = [];
    for (let i = di; i < toks.length && !stop.has(toks[i]); i++) descToks.push(toks[i]);
    return { type: 'cff', method: 'grid', warno, digits, imm,
             desc: descClean(descToks.join(' ')), raw: t, toks };   // F7
  }
  // shift from known point: "shift known point 1001, right 200, add 400" —
  // named KPs also accepted ("shift BREWERY"), normalized to their id first
  let tS = t;
  for (const k of ((typeof Scenario !== 'undefined' && Scenario && Scenario.kps) || []))
    if (k.name)
      tS = tS.replace(new RegExp('\\b(?:known point\\s+|kp\\s+)?' + k.name.toLowerCase() + '\\b'),
                      'known point ' + k.id);
  const mShift = tS.match(/\b(?:from|shift)\s+(?:known point\s+|kp\s+|a\s?b\s+)?(\d{4})\b/);
  if (mShift) {
    const warno = ffe ? 'ffe' : (t.includes('adjust fire') ? 'adjust' : 'none');
    let sRight = 0, sAdd = 0;
    const sLR = t.match(/\b(left|right)\s+(\d+)/);
    if (sLR) sRight = (sLR[1] === 'right' ? 1 : -1) * parseInt(sLR[2]);
    const sAD = t.match(/\b(add|drop)\s+(\d+)/);
    if (sAD) sAdd = (sAD[1] === 'add' ? 1 : -1) * parseInt(sAD[2]);
    const sDir = t.match(/\bdirection\s+(\d+)\b/);
    const kw = new Set(['hellhound', 'hacksaw', 'fires', 'this', 'is', 'mustang', 'adjust',
      'fire', 'for', 'effect', 'shift', 'from', 'known', 'point', 'kp', 'a', 'b',
      'ab', 'left', 'right', 'add', 'drop', 'direction', 'over', 'out', 'break',
      'danger', 'close',
      'up', 'down']);          // G23 — or the vertical becomes the target description
    const desc = toks.filter(w => !kw.has(w) && !/^\d+$/.test(w)).join(' ');
    // G23 — the shift vertical ("up 55"), same rule as polar: sent only when the
    // known-point-to-target height difference is >= 35 m.
    const sUD = t.match(/\b(up|down)\s+(\d+)/);
    return { type: 'cff', method: 'shift', warno, kpId: mShift[1],
             sRight, sAdd, dirMils: sDir ? parseInt(sDir[1]) : null,
             vertM: sUD ? (sUD[1] === 'up' ? 1 : -1) * parseInt(sUD[2]) : null,
             desc, raw: t, toks };
  }
  /* polar: "direction 4400, distance 2100"
     NET3 — tolerance, from transcript evidence (a real player lost ~17 minutes
     to this): RANGE is accepted as a synonym for DISTANCE, and a natural
     connective word between the proword and the number ("distance TO 600",
     "direction FOR 500") no longer breaks the match. A human FDC hears
     "direction 4500, range 600" and knows exactly what is being attempted. */
  const mDir = t.match(/\bdirection\s+(?:for\s+|of\s+)?(\d+)\b/);
  const mDist = t.match(/\b(?:distance|range)\s+(?:to\s+|of\s+|at\s+)?(\d+)\b/);
  if (mDir && mDist) {
    const warno = ffe ? 'ffe' : (t.includes('adjust fire') ? 'adjust' : 'none');
    let di = toks.indexOf('distance');
    if (di < 0) di = toks.indexOf('range');
    di++;
    while (di < toks.length && /^(?:to|of|at)$/.test(toks[di])) di++;
    while (di < toks.length && /^\d+$/.test(toks[di])) di++;
    const stop = new Set(['over', 'out', 'break']);
    const descToks = [];
    for (let i = di; i < toks.length && !stop.has(toks[i]); i++) descToks.push(toks[i]);
    // G23 — the polar vertical ("down 40"). Doctrine sends it only when the
    // observer-to-target height difference is >= 35 m; captured so that can be
    // checked rather than ignored.
    const pUD = t.match(/\b(up|down)\s+(\d+)/);
    return { type: 'cff', method: 'polar', warno, dirMils: parseInt(mDir[1]),
             distM: parseInt(mDist[1]),
             vertM: pUD ? (pUD[1] === 'up' ? 1 : -1) * parseInt(pUD[2]) : null,
             // strip the vertical out of the description — it follows the distance,
             // so "distance 2100, down 40, troops" would read as "down 40 troops"
             desc: descClean(descToks.join(' ').replace(/\b(up|down)\s+\d+\s*/g, '')),  // F7
             raw: t, toks };
  }
  // standalone OT direction ("direction 5920, over") — doctrine requires it
  // before or with the first correction on a grid mission (NET3: same
  // connective tolerance as the polar branch)
  const mDirOnly = t.match(/\bdirection\s+(?:for\s+|of\s+)?(\d+)\b/);
  if (mDirOnly && !/\b(left|right|add|drop)\b/.test(t))
    return { type: 'direction', dirMils: parseInt(mDirOnly[1]) };
  /* G23 — the correction now carries its VERTICAL. `corr` had exactly two fields,
     right and add, so every `up`/`down` the observer sent was matched by nothing
     and silently discarded, and `"up 20, over"` on its own fell through to
     `unknown` and got mocked. Height of burst is a real doctrinal correction
     (DOCTRINE.md §54) and the third element of the correction sequence
     deviation → range → height of burst. */
  const corr = { right: 0, add: 0, vert: 0 };
  let any = false;
  const mLR = t.match(/\b(left|right)\s+(\d+)/);
  if (mLR) { corr.right = (mLR[1] === 'right' ? 1 : -1) * parseInt(mLR[2]); any = true; }
  const mAD = t.match(/\b(add|drop)\s+(\d+)/);
  if (mAD) { corr.add = (mAD[1] === 'add' ? 1 : -1) * parseInt(mAD[2]); any = true; }
  const mUD = t.match(/\b(up|down)\s+(\d+)/);
  if (mUD) { corr.vert = (mUD[1] === 'up' ? 1 : -1) * parseInt(mUD[2]); any = true; }
  /* F5, second half — the STT_FIX table repairs the variants seen so far, but it
     cannot cover every mis-transcription, and the old failure mode was SILENT: a
     number whose proword did not match was simply dropped and the FDC read back a
     correction the observer had not sent. Count the numbers in the transmission
     against the ones actually consumed, so an unclaimed number can be reported
     instead of vanishing. Being told "I heard a number I could not attach to
     anything" is recoverable; a silently halved correction is not. */
  const numCount = (t.match(/\b\d+\b/g) || []).length;
  const stray = numCount - (mLR ? 1 : 0) - (mAD ? 1 : 0) - (mUD ? 1 : 0);
  // G13 — REPEAT riding on a correction ("right 50, repeat"): after a volley it
  // re-fires the volley at the corrected aimpoint; flagged here, decided there.
  if (any || ffe) return { type: 'adjust', corr, any, ffe, stray,
                           rep: /\brepeat\b/.test(t), raw: t, toks };
  if (/\brepeat\b/.test(t)) return { type: 'repeat' };
  /* G22 — a bare WARNING ORDER is doctrinal Transmission 1 ("HELLHOUND FIRES,
     this is MUSTANG 12, adjust fire, over"). It used to fall through to `unknown`
     and be answered with the reply reserved for gibberish, which meant an observer
     who had learned the real three-transmission format could not use it.

     Bare "fire for effect" is deliberately NOT caught here: it is already the
     mid-mission command to shift into effect, and it reaches this function as
     `adjust` with `any === false`. onPlayerMessage decides which one it is from
     mission state, because that is the only place that knows. */
  /* G7 — the observer transmitting his OT FACTOR. The factor is the OBSERVER'S
     own correction arithmetic (OT distance ÷ 1000 — it converts his mil spotting
     to meters); it is never net traffic, unlike OT DIRECTION, which is. The user
     flagged the two being conflated. Audited first: no FDC line asks for or
     reads back a factor, and the mil card, laser readout and spotting coach all
     teach it as an observer tool — so the only fix needed is here, answering the
     observer who sends it. Tested after grid/shift/polar so a real call is never
     claimed; "factor" has no other use on this net. */
  if (t.includes('factor')) return { type: 'otfactor', raw: t, toks };
  /* G24 — these two must be tested HERE, after grid/shift/polar, not up with the
     other prowords. "at my command" is a legal element of transmission 3, so a
     complete call for fire contains it; matching it early would misread the whole
     call as a bare fire-control request. */
  if (t.includes('at my command')) return { type: 'amc', on: true, raw: t, toks };
  /* The bare FIRE proword that releases a round held at my command. Deliberately
     strict: "fire" appears inside "adjust fire", "fire for effect" and "check
     firing", all of which are already handled above, so this only fires when FIRE
     is the whole transmission apart from callsigns and prowords. */
  {
    const strip = new Set(['over', 'out', 'break', 'hellhound', 'hacksaw', 'fires',
                           'this', 'is', 'mustang', '12']);
    const core = toks.filter(w => !strip.has(w) && !/^\d+$/.test(w));
    if (core.length === 1 && core[0] === 'fire') return { type: 'fire', raw: t, toks };
  }
  if (/\badjust fire\b/.test(t)) return { type: 'warno', warno: 'adjust', raw: t, toks };
  return { type: 'unknown', raw: t, toks };
}

function formatNotes(p) {
  const notes = [];
  const t = p.raw;
  if (!t.includes(fdcShort().toLowerCase()) || !t.includes('mustang'))
    notes.push(`Open with both callsigns: "${fdcCall()}, THIS IS MUSTANG 12".`);
  if (p.warno === 'none')
    notes.push('No warning order — say ADJUST FIRE or FIRE FOR EFFECT.');
  if (p.method === 'grid' && p.digits.length === 8 &&
      !(activeChapter && activeChapter.asset === 'mortar60'))
    notes.push('8-digit grid is 60mm mortar precision; artillery works in hundreds (6-digit).');
  if (!p.desc)
    notes.push('No target description ("infantry in the open", etc.).');
  if (!p.toks.includes('over'))
    notes.push('End each transmission with OVER.');
  return notes;
}

