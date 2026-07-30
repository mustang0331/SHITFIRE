/* ============================================================ FDC VOICE (SpeechSynthesis) */
let fdcVoice = null;
function pickVoice() {
  if (!('speechSynthesis' in window)) return;
  const vs = speechSynthesis.getVoices();
  for (const p of CONFIG.VOICE.preferredVoices) {
    const v = vs.find(v => (v.name + ' ' + v.lang).toLowerCase().includes(p));
    if (v) { fdcVoice = v; return; }
  }
  if (vs.length) fdcVoice = vs[0];
}
if ('speechSynthesis' in window) {
  pickVoice();
  speechSynthesis.onvoiceschanged = pickVoice;
}
function speakFDC(msg) {
  if (!CONFIG.VOICE.ttsEnabled || !('speechSynthesis' in window)) return;
  // Read numbers digit-by-digit, radio style: "GRID 245 523" -> "2 4 5, 5 2 3"
  const spoken = msg.replace(/\d+/g, m => m.split('').join(' '));
  const u = new SpeechSynthesisUtterance(spoken);
  u.lang = CONFIG.VOICE.ttsLang;
  u.rate = CONFIG.VOICE.rate;
  u.pitch = CONFIG.VOICE.pitch;
  u.volume = CONFIG.VOICE.volume;
  if (fdcVoice) u.voice = fdcVoice;
  u.onstart = () => hiss(true);
  u.onend = u.onerror = () => hiss(false);
  speechSynthesis.speak(u);
}

const QUIPS = {
  greet: [
    'HELLHOUND FIRES on this net. The guns are hot, the coffee is cold, and I am surrounded by men who could not find their own ass with a compass and a head start. Give me a target, over.',
    'HELLHOUND FIRES. We have been sitting in this goddamn gun pit since dawn watching the flies fight. Send us some work, over.',
    'HELLHOUND FIRES up. Somewhere on this miserable rock is a grid square God has given up on. Point me at it, over.',
    'HELLHOUND FIRES on the net. I have six guns, a full ammo rack, and no patience whatsoever. Impress me, over.',
    'HELLHOUND FIRES. Speak clearly, speak correctly, and nobody has to get embarrassed today, over.',
  ],
  readbackTail: [
    'And MUSTANG — do try to hit the correct fucking island this time.',
    'Rounds are cheap. My patience is a finite and dwindling resource. Standing by.',
    'If anything friendly is standing on that grid, now is the moment to confess.',
    'Copy all. This had better be worth the powder, son.',
    'Outstanding. The tubes were getting cold and the crew had started a book club.',
  ],
  corrSnark: [
    'That is one HELL of a walk, MUSTANG. Did the first round land in a different fucking hemisphere?',
    'Copy. For the record: the gun did not move. Your math moved.',
    'A bold correction. Somewhere out there a map is being held upside down, and I believe I know by whom.',
    'Shifting fire. The fish send their regards for your earlier donation.',
  ],
  ffeAck: [
    'FIRE FOR EFFECT. Now THIS is the part of the job I love.',
    'FIRE FOR EFFECT. God help whatever is standing on that grid, because nothing else will.',
    'Copy FIRE FOR EFFECT. Six guns, one grid, zero fucking sympathy.',
    'FIRE FOR EFFECT. Drink it up, MUSTANG. Drriiink it up.',
  ],
  completeTail: [
    'That grid square has been redecorated down to the bedrock.',
    'Rounds complete. Whatever stood there is now a fine mist and somebody else\'s paperwork.',
    'Rounds complete. I have looked upon that smoke and found it good.',
    'Tubes cooling. Take a long look, son. That is what competence smells like.',
  ],
  eomGood: [
    'Good effect. I will be goddamned — the observer can actually observe.',
    'Target neutralized. Mark this day, MUSTANG. It will not come again.',
    'A clean kill. I had frankly abandoned all hope of seeing one from you.',
    'End of mission logged. There is a competence in you, boy, and today it finally showed up for work.',
  ],
  eomBad: [
    'End of mission. The target is alive and well and telling this story at dinner parties. We converted the taxpayers\' money into noise and regret.',
    'Logged. We will call that one "suppressive" and never, ever speak of it again.',
    'End of mission. The enemy remains. My disappointment, however, is total, permanent, and load-bearing.',
    'Copy end of mission. You abandoned that fire mission, MUSTANG. You ABANDONED it.',
  ],
  badGrid: [
    'MUSTANG 12, that grid is not on anybody\'s goddamn map. Say again your target location, over.',
    'Negative copy. That location does not exist on this or any other earth. Say again, over.',
    'I plotted that grid twice, and both times it insulted me. Say again, over.',
    'That is not a grid, MUSTANG, that is a cry for help. Say again, over.',
  ],
  water: [
    'Be advised, that grid is in the goddamn ocean. Engaging the fish as requested.',
    'That grid is wet, MUSTANG. We are now shelling the Pacific, which never did a thing to us. Firing.',
    'Copy. Naval gunfire it is. Bold fucking choice.',
  ],
  noMission: [
    'MUSTANG 12, there is no mission on this net. You are adjusting fire that exists only in your heart. Send a call for fire, over.',
    'MUSTANG 12, you are correcting rounds we never fired. I admire the confidence. Send a call for fire first, over.',
    'There is no mission, MUSTANG. There is only you, me, and this dead air you keep filling. Call for fire, over.',
  ],
  inFlight: [
    'Rounds are in the air, MUSTANG. Physics is doing its part. Do yours — wait for splash, over.',
    'Still flying. Unless you can steer a shell with your mind, wait for the goddamn splash, over.',
    'Patience, MUSTANG. The rounds arrive when they arrive. Much like your competence, over.',
  ],
  unknown: [
    'Say again, over. Slower, and in English this time.',
    'That transmission had a callsign and then a stroke. Say again, over.',
    'MUSTANG, I have heard clearer traffic from a dying radio at the bottom of a well. Say again, over.',
    'Say again, over. And this time pretend the radio is graded — because it is. By me.',
  ],
  complete: [
    'MUSTANG 12, mission is complete. Send end of mission with your BDA, over.',
  ],
  /* G13 — the volley ended but the target did not. Suppressed-only: heads are
     down and the clock is running; the observer owns the continue-or-end call.
     No effect: the rounds were a fireworks display. Neither line may skip the
     doctrinal fact that the net is waiting on the observer's decision. */
  suppressedOnly: [
    'MUSTANG 12, they are heads-down, not dead — that is suppression, and it expires. Continue the mission or send end of mission, your call, over.',
    'MUSTANG 12, you have bought silence, not results. The moment we stop, they get back on their guns. Correct and REPEAT, or close it out, over.',
  ],
  noEffect: [
    'MUSTANG 12, rounds complete, and the target could not care less. Where you put that volley and where they are living are two different places. Correct and REPEAT, or send end of mission and we will both pretend this was training, over.',
    'MUSTANG 12, no effect observed. We just gave the taxpayers a very expensive drum solo. Fix your data and REPEAT, or end the mission, over.',
  ],
  dangerClose: [
    'MUSTANG 12, that target is DANGER CLOSE to friendly infantry and you did not say the fucking proword. I do not drop shells next to our own people on a mumble. Say again the full call with DANGER CLOSE, over.',
    'Negative. Friendlies are close enough to that grid to read the lot numbers off the fuzes. You want this mission? Say DANGER CLOSE like you mean it, over.',
  ],
  wrongWay: [
    'MUSTANG. The rounds are getting FARTHER from the target. You are adjusting fire like a blind man swatting bees. Look at your burst. Look at your target. THINK. Then talk, over.',
    'Wrong way, son. WRONG WAY. Every correction you send wounds me personally. Left is left. Add is away. It is not fucking calculus, over.',
  ],
  rantLost: [
    'STOP. Just — stop. MUSTANG, I have drunk deep of your transmissions and found NOTHING. No grid. No direction. No sense. You are a forward observer. So OBSERVE something, and then TELL ME ABOUT IT in the format God and the field manual intended. Warning order. Location. Description. OVER.',
    'MUSTANG, listen to me very carefully, because I will say this once. I do not know what you are doing on that hill, but it is not calling for fire. Take a breath. Look at your map. Send: warning order, target location, description. In that order. Like a professional. Over.',
  ],
  // Doctrinal coaching — HELLHOUND teaches adjustment instead of only
  // mocking it. Easy/Normal only; on Hard you are expected to know.
  coachTimid: [
    'MUSTANG, you are NIBBLING. Your burst was {MISS} meters off and you sent a correction a quarter that size. That is not adjustment, that is negotiation. Bracket it — throw a correction big enough to land on the OTHER side, then cut it in half.',
    'Negative technique, MUSTANG. {MISS} meters of error, and your correction would not move a burst out of its own crater. Doctrine: eight hundred, four hundred, two hundred. Bold, then halve. Send it again with conviction.',
  ],
  coachBracket: [
    'MUSTANG, every round you have fired has landed on the SAME side of that target. You are not adjusting, you are commuting. Send a correction large enough to cross the target — then split the difference. That is a bracket. Use one.',
    'Listen to me, MUSTANG. All short. Every single one. You cannot split a bracket you have never established. Overshoot it on purpose, then halve. That is the whole trade.',
  ],
  coachBracketGood: [
    'THERE it is — one over, one short. You have a bracket, MUSTANG. Now halve it. Halve it again. Then fire for effect like a professional.',
    'Bracket established. Cut your correction in half each round from here and you will be in effect before I finish this cigarette.',
  ],
  coachStagnant: [
    'MUSTANG, that correction accomplished NOTHING. Same burst, different paperwork. Look at the burst, look at the target, MEASURE the difference, and send me THAT number.',
    'We just spent a round and a minute of the war to move that burst almost nowhere. Measure the error. Send the whole error. Not a polite fraction of it.',
  ],
  coachLoc: [
    'MUSTANG — that first round is {MISS} meters off target. That is not the gun, son, that is your grid. Standard is two hundred meters. Work your map before you key that mic next time.',
  ],
  careerFrat: [
    'And MUSTANG — before we begin: the friendlies have asked me to remind you which color smoke is theirs. All of them. They held a meeting.',
    'One administrative note, MUSTANG: your file now has a tab. Files should not have tabs. Send your traffic.',
  ],
  careerCollat: [
    'MUSTANG, the village council has formally requested you be issued a map with pictures. Motion carried unanimously. Proceed.',
    'Before you key up, MUSTANG — the huts are still no-strike. Yes, still. Yes, all of them. Send it.',
  ],
  careerVet: [
    'MUSTANG 12 on the net. The guns know your voice now, son. They perk up. It is almost touching.',
    'Ah, MUSTANG. The log says we have done this dance a few times. Lead, this time, over.',
  ],
  snide: [
    'Copy your... whatever that was. The manual describes a format, MUSTANG. It is one page. There are pictures.',
    'I processed that call out of charity. Callsigns, warning order, location, description. In that order. Like your instructor begged you.',
    'That transmission was to a call for fire what a mudslide is to architecture. We will fire it anyway.',
    'Copy. And to think they told me the radio was a professional instrument.',
  ],
  snideRound: [
    'The guns round to tens, son — your boutique little numbers are a fantasy and my time is real. Firing.',
    'Copy your artisanal correction. Deviation comes in tens, range in hundreds. We will do the arithmetic you would not. Firing.',
    'The fire direction computer just sighed out loud, MUSTANG. Round your numbers like a professional. Firing anyway.',
  ],
  nearCiv: [
    'Be advised — that grid sits close to the village. There are civilians on this rock, MUSTANG. Put one round in the market square and your war is over.',
    'Copy all. Check your map, son — the village is a short walk from your splash pattern. Adjust like you mean it.',
  ],
  unsafeCorr: [
    'NEGATIVE. CHECK YOUR CORRECTION, MUSTANG. That shift walks rounds onto {WHO}. Look at your burst, look at your map, and send me a correction that does not end careers. Say again, over.',
    'I am NOT sending that. Plot it, son. Your correction lands on {WHO}. You have one radio and you are currently using it for manslaughter. Say again, over.',
  ],
  unsafeInsist: [
    'Your funeral, MUSTANG. Shifting fire as ordered. I am writing your name in the log as I do it.',
    'Copy. On your head, son. The board of inquiry meets Tuesdays. Shifting fire.',
  ],
  // who the cue came from — never HELLHOUND himself; a gun battery does not observe
  spotSrc: [
    'A RECON PATROL THAT WENT HOME AN HOUR AGO',
    'THE OBSERVER YOU RELIEVED, WHO IS NOW ASLEEP',
    'AN ADJACENT COMPANY WITH BETTER THINGS TO DO',
    'A COASTWATCHER WHO SPEAKS THREE WORDS OF ENGLISH',
    'BATTALION S-TWO, WHO WERE VERY PLEASED WITH THEMSELVES',
    'AN AIR OBSERVER ON HIS WAY TO SOMEWHERE ELSE',
  ],
  spotTail: [
    'That is a CUE, MUSTANG, not a target location. Somebody has to actually look. That somebody is you.',
    'I am not going to pretend that is a grid. Get your glass up and turn it into one.',
    'That report has been passed through four radios and two liars. Verify it yourself.',
    'You have been given a neighborhood. I require an address. Over.',
    'Do not read that back to me as a call for fire. I will know, and I will be unkind about it.',
    'Somebody saw something once. Go and see it properly, over.',
  ],
  spotNone: [
    'MUSTANG 12, HELLHOUND — nobody has sent us a thing. No patrol report, no overlay, no cue. Start glassing and tell me what is out there, over.',
    'MUSTANG 12, HELLHOUND — the intelligence picture this morning consists of one rumour and a wet map. You are the sensor. Get to work, over.',
  ],
  rantCiv: [
    ['CHECK FIRE. CHECK FIRE.',
     'That round went into the VILLAGE, MUSTANG. The civilian village. The one on your map — plotted, labeled, and full of people who were having a perfectly fine morning until you found the radio.',
     'There is no report I can write that survives this. END OF MISSION. Get off my net and go sit somewhere quiet and think about maps.'],
    ['CHECK FIRE. GOD ALMIGHTY, CHECK FIRE.',
     'You just serviced a fishing village with one-five-five, you catastrophic instrument. Those were CIVILIANS. The huts were on the sheet. The brief said no-strike.',
     'I have to make phone calls now, MUSTANG. Phone calls with generals on the other end. END OF MISSION.'],
  ],
  rantFrat: [
    ['CHECK FIRE. CHECK FIRE. CHECK FIRE.',
     'You just dropped one-five-five on the FRIENDLY position, MUSTANG. Do you understand me? Those are OUR people in that smoke. I am looking at the plot, and the plot says YOU did this.',
     'I told them not to hand you a map. I TOLD them. You are done on this net, son. Get off my radio and go practice on a lake. END OF MISSION.'],
    ['CHECK FIRE. GOD DAMN IT, CHECK FIRE.',
     'That was the friendly position, you catastrophic son of a bitch. Men with radios are standing in that smoke right now saying your callsign with FEELING.',
     'I have seen bad observers. I have TRAINED bad observers. But you — you are a whole new church of wrong. END OF MISSION.'],
    ['CHECK FIRE. CHECK FIRE.',
     'MUSTANG, you walked artillery onto our own infantry. There are two kinds of people in this war — those who read grids, and casualties. Today you manufactured the second kind out of the first.',
     'When this is over, you and I will have a conversation about maps, and you will not enjoy it. END OF MISSION. Get off my net.'],
  ],
};
function pick(arr) {
  if (arr.length === 1) return arr[0];
  let i = Math.floor(Math.random() * arr.length);
  if (i === arr._last) i = (i + 1) % arr.length;
  arr._last = i;
  return arr[i];
}

/* ============================================================ SPOT REPORT (SALUTE) */
/* The observer's hardest un-taught problem is knowing WHERE TO LOOK. A real FO
   is cued onto an area by somebody — higher, an adjacent unit, the man he
   relieved — and then does the work of fixing it precisely. That cue was missing
   here: the mission opened with an empty horizon and no reason to point the
   glass anywhere. This sends one at mission start and again if the target
   displaces.

   It must ORIENT WITHOUT ANSWERING. Locating the target to 100 m (10 m with the
   sixty) is the graded skill, so the LOCATION line names an AREA, never a point:

     easy    the 1 km grid square, plus the nearest map landmark named
     normal  a cardinal-8 sector from a landmark, "within R metres of it"
     hard    a 90-degree quadrant from a landmark, "within R metres of it"
     (no landmark within CONFIG.SPOT.landmarkMax -> a bearing sector and a range
      band from the OP instead, same ladder)

   The critical rule, learned the hard way from a harness that measured it: NEVER
   state a distance to the target from anything. A leg like "600 metres northwest
   of the airfield" collapses the cue to 3-6 possible 100 m cells when the
   landmark is close — that is the answer, not a cue. The stated radius R is a
   FLOOR (2000 normal / 3000 hard) that is only ever widened to keep the claim
   true, so the cued area cannot shrink no matter where the landmark sits.
   Measured over 100 random layouts: easy 1.0 km2 flat, normal 1.18-3.5 km2,
   hard 3.2-9.4 km2, strictly ordered, and the true target always inside its own
   cue. A 6-digit call still requires the observer to find it to 100 m.

   Two forms, chosen off the mission seed so a chapter always cues the same way:
   a full six-line SALUTE, and a looser "activity observed vicinity <feature>".
   Both are relayed BY the FDC FROM somebody else — a gun battery does not
   observe — and both stay clear of the doctrinal readback: they fire once, at
   mission start, before any call for fire exists to read back. */
const CARD8 = ['NORTH', 'NORTHEAST', 'EAST', 'SOUTHEAST',
               'SOUTH', 'SOUTHWEST', 'WEST', 'NORTHWEST'];
// two forms of the same 90-degree quadrant: the adjective reads "NORTHERN
// QUADRANT", the bare noun reads "NORTH OF THE AIRFIELD"
const CARD4_ADJ = ['NORTHERN', 'EASTERN', 'SOUTHERN', 'WESTERN'];
const CARD4_DIR = ['NORTH', 'EAST', 'SOUTH', 'WEST'];
function cardinal8(rad) { return CARD8[Math.round(radToMils(rad) / 800) % 8]; }
function quad4i(rad) { return Math.round(radToMils(rad) / 1600) % 4; }
function quadrant4(rad) { return CARD4_ADJ[quad4i(rad)]; }
function quadDir4(rad) { return CARD4_DIR[quad4i(rad)]; }
function milsWrap(m) { return ((m % 6400) + 6400) % 6400; }
function roundTo(v, n) { return Math.round(v / n) * n; }
function grid4(x, z) {            // 1 km grid square: a 1000 m box, not an answer
  const en = worldToEN(x, z);
  return `${Math.floor(en.e / 1000)}${Math.floor(en.n / 1000)}`;
}
const LANDMARK_SPOKEN = { airfield: 'THE AIRFIELD', mast: 'THE RADIO MAST',
  gun: 'THE OLD COASTAL GUN', fuel: 'THE FUEL POINT', ammo: 'THE AMMO DEPOT' };
// Permanent map features only — these are exactly the things that ARE printed on
// the sheet, so a landmark cue is a terrain-association problem, not a giveaway.
function nearestLandmark(x, z) {
  let best = null, bd = Infinity;
  for (const f of WORLD.facilities) {
    const d = dist2(x, z, f.x, f.z);
    if (d < bd) { bd = d; best = { name: LANDMARK_SPOKEN[f.kind] || f.name, x: f.x, z: f.z, d }; }
  }
  for (const v of WORLD.villages) {
    const d = dist2(x, z, v.x, v.z);
    if (d < bd) { bd = d; best = { name: `THE VILLAGE OF ${v.name}`, x: v.x, z: v.z, d }; }
  }
  /* E7 — the named outcrops are landmarks too, and inland they are often the
     ONLY one: every man-made feature on this island is within a few hundred
     metres of the coast road, so a target on the far ridge used to get no cue
     at all. They are plotted on the sheet under the same name the net uses. */
  for (const r of WORLD.rocks) {
    if (!r.name) continue;
    const d = dist2(x, z, r.x, r.z);
    if (d < bd) { bd = d; best = { name: r.name, x: r.x, z: r.z, d }; }
  }
  return best && best.d <= CONFIG.SPOT.landmarkMax ? best : null;
}
const KM_WORDS = { 2000: 'TWO THOUSAND', 2500: 'TWO AND A HALF THOUSAND',
  3000: 'THREE THOUSAND', 3500: 'THREE AND A HALF THOUSAND',
  4000: 'FOUR THOUSAND' };
function kmWords(m) { return KM_WORDS[m] || `${m}`; }
function spotLocation(x, z, diff) {
  const lm = nearestLandmark(x, z);
  const dOT = dist2(OP.x, OP.z, x, z);
  const azOT = azTo(OP.x, OP.z, x, z);
  if (diff === 'easy')
    return `GRID SQUARE ${grid4(x, z)}` + (lm ? `, VICINITY OF ${lm.name}` : '');
  const norm = diff === 'normal';
  if (lm) {
    // radius is a floor, widened only far enough to keep the statement TRUE
    const R = Math.max(norm ? 2000 : 3000, Math.ceil(lm.d / 500) * 500);
    const dir = norm ? cardinal8(azTo(lm.x, lm.z, x, z))
                     : quadDir4(azTo(lm.x, lm.z, x, z));
    return `${dir} OF ${lm.name}, WITHIN ${kmWords(R)} METRES OF IT`;
  }
  if (norm) {
    const b = roundTo(radToMils(azOT), 800), k = Math.floor(dOT / 1000);
    // GRID, like everything else on the net (G2). The band is +/-400 mils, so a
    // 124-mil G-M angle would not put the target outside it either way — but the
    // report still says which north it is measured from, because a cue that
    // leaves that ambiguous teaches the observer to stop asking.
    return `BEARING ${fmtMils(milsWrap(b - 400))} TO ${fmtMils(milsWrap(b + 400))} GRID ` +
           `FROM YOUR TOWER, ${k} TO ${k + 1} THOUSAND OUT`;
  }
  const b0 = Math.floor(dOT / 2000) * 2;
  return `${quadrant4(azOT)} QUADRANT FROM YOUR TOWER, ` +
         (b0 === 0 ? 'INSIDE TWO THOUSAND' : `${b0} TO ${b0 + 2} THOUSAND`) + ' OUT';
}
const SPOT_SCN = {
  strongpoint: { size: 'SQUAD PLUS — EIGHT OR MORE DISMOUNTS AND TWO LIGHT VEHICLES',
                 act: 'ASSAULTING A FRIENDLY STRONGPOINT',
                 eq: 'SMALL ARMS, ONE CREW-SERVED WEAPON, TWO SOFT-SKIN TRUCKS' },
  troops:      { size: 'SQUAD PLUS — EIGHT OR MORE DISMOUNTS',
                 act: 'INFANTRY IN THE OPEN, NO OVERHEAD COVER',
                 eq: 'SMALL ARMS AND PACKS. NO VEHICLES SEEN' },
  bunker:      { size: 'ONE FORTIFIED POSITION, SQUAD SIZE',
                 act: 'DUG IN AND OBSERVING. THEY ARE NOT GOING ANYWHERE',
                 eq: 'AUTOMATIC WEAPON IN A COVERED EMPLACEMENT, OVERHEAD PROTECTION' },
  convoy:      { size: 'FOUR VEHICLES IN COLUMN',
                 act: 'MOTORISED COLUMN ON THE MOVE',
                 eq: 'CARGO TRUCKS. ESCORT UNKNOWN. NO ARMOUR REPORTED' },
  assault:     { size: 'PLATOON MINUS ON THE OBJECTIVE',
                 act: 'HOLDING GROUND AGAINST A FRIENDLY ADVANCE. OUR PEOPLE ARE CLOSING ON THEM',
                 eq: 'DUG-IN SMALL ARMS AND AT LEAST ONE MACHINE GUN' },
  wreck:       { size: 'ONE DERELICT LANDING CRAFT',
                 act: 'STATIC. ABANDONED. DEEPLY UNTHREATENING',
                 eq: 'NONE. IT IS SCRAP, AND IT IS YOURS' },
  raid:        { size: 'EIGHT OR MORE DISMOUNTS AND ONE VEHICLE',
                 act: 'RAIDING A CIVILIAN VILLAGE',
                 eq: 'SMALL ARMS AND ONE ARMED PICKUP' },
};
const SPOT_UNIT = [
  'UNKNOWN. NO MARKINGS, NO INSIGNIA',
  'ASSESSED AS THIRD BATTALION REMNANTS, UNCONFIRMED',
  'UNIDENTIFIED. MIXED KIT, MIXED DISCIPLINE',
  'IRREGULARS. THEY ARE NOT WEARING ANYBODY\'S UNIFORM',
  'UNKNOWN. HIGHER HAS A GUESS, AND HIGHER HAS BEEN WRONG BEFORE',
];
// reason: 'start' | 'displace'
function sendSpotReport(reason) {
  const S = Scenario;
  if (!CONFIG.SPOT.enabled || !S) return;
  const d = SPOT_SCN[S.type];
  if (!d) return;
  const P = CONFIG.SPOT;
  const rng = mulberry32((S.seed * 30011 + (reason === 'displace' ? 977 : 7)) >>> 0);
  const loc = spotLocation(S.enemy.x, S.enemy.z, S.difficulty);

  if (reason === 'displace') {
    FDC.say(`${CONFIG.FDC.obs}, ${fdcCall()} — UPDATED SPOT REPORT. ` +
            `SIZE UNCHANGED. ACTIVITY: DISPERSING AND GOING TO GROUND. ` +
            `LOCATION: ${loc}. RE-ACQUIRE AND SEND YOUR CORRECTION, OVER.`, { delay: P.gap });
    TLOG.add('spot', '', loc, { form: 'update', diff: S.difficulty, scn: S.type });
    return;
  }

  const src = SPOT_SRC_SEEDED(rng);
  const mins = 2 + Math.floor(rng() * 18);
  const salute = rng() < P.saluteChance;
  if (salute) {
    FDC.say(`${CONFIG.FDC.obs}, ${fdcCall()} — I HAVE A SPOT REPORT FOR YOU, ` +
            `RELAYED FROM ${src}. SALUTE FORMAT. STAND BY, OVER.`, { delay: P.lead });
    FDC.say(`SIZE: ${d.size}. ACTIVITY: ${d.act}. LOCATION: ${loc}, OVER.`, { delay: P.gap });
    FDC.say(`UNIT: ${SPOT_UNIT[Math.floor(rng() * SPOT_UNIT.length)]}. ` +
            `TIME: ${mins} MINUTES OLD. EQUIPMENT: ${d.eq}. OVER.`, { delay: P.gap });
  } else {
    FDC.say(`${CONFIG.FDC.obs}, ${fdcCall()} — ${src} REPORTS ACTIVITY ` +
            `IN THIS GENERAL AREA — ${loc}. ${d.act}. ` +
            `${mins} MINUTES OLD, OVER.`, { delay: P.lead });
    FDC.say(`THAT IS ALL THEY GAVE ME. ${d.size}, ${d.eq}. NOTHING FINER, OVER.`, { delay: P.gap });
  }
  FDC.say(pick(QUIPS.spotTail), { delay: 1.7 });
  TLOG.add('spot', '', loc, { form: salute ? 'salute' : 'loose',
                              diff: S.difficulty, scn: S.type });
}
// seeded pick that does not disturb the shared QUIPS._last rotation
function SPOT_SRC_SEEDED(rng) {
  return QUIPS.spotSrc[Math.floor(rng() * QUIPS.spotSrc.length)];
}

/* ============================================================ AUDIO (minimal boom; polish in stage 4) */
let audioCtx = null, noiseBuf = null;
function ensureAudio() {
  if (audioCtx) return;
  try {
    audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    noiseBuf = audioCtx.createBuffer(1, audioCtx.sampleRate * 1.5, audioCtx.sampleRate);
    const d = noiseBuf.getChannelData(0);
    for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1;
  } catch (e) { audioCtx = null; }
}
function boom(delaySec, dist) {
  if (!audioCtx || audioCtx.state !== 'running') return;
  const t0 = audioCtx.currentTime + Math.max(0, delaySec);
  const loud = clamp(1400 / Math.max(dist, 300), 0.12, 0.9);
  const src = audioCtx.createBufferSource(); src.buffer = noiseBuf;
  const lp = audioCtx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 240;
  const g = audioCtx.createGain();
  g.gain.setValueAtTime(0.0001, t0);
  g.gain.exponentialRampToValueAtTime(loud, t0 + 0.02);
  g.gain.exponentialRampToValueAtTime(0.0001, t0 + 1.5);
  src.connect(lp); lp.connect(g); g.connect(audioCtx.destination);
  src.start(t0); src.stop(t0 + 1.6);
  const osc = audioCtx.createOscillator(); osc.frequency.value = 42;
  const g2 = audioCtx.createGain();
  g2.gain.setValueAtTime(0.0001, t0);
  g2.gain.exponentialRampToValueAtTime(loud * 0.8, t0 + 0.03);
  g2.gain.exponentialRampToValueAtTime(0.0001, t0 + 1.1);
  osc.connect(g2); g2.connect(audioCtx.destination);
  osc.start(t0); osc.stop(t0 + 1.2);
}
// radio-net textures: key-up squelch, hiss bed under FDC speech, lase tick
function squelch() {
  if (!audioCtx || audioCtx.state !== 'running') return;
  const t0 = audioCtx.currentTime;
  const src = audioCtx.createBufferSource(); src.buffer = noiseBuf;
  const bp = audioCtx.createBiquadFilter();
  bp.type = 'bandpass'; bp.frequency.value = 1800; bp.Q.value = 0.8;
  const g = audioCtx.createGain();
  g.gain.setValueAtTime(0.12, t0);
  g.gain.exponentialRampToValueAtTime(0.001, t0 + 0.07);
  src.connect(bp); bp.connect(g); g.connect(audioCtx.destination);
  src.start(t0, Math.random() * 0.5, 0.09);
}
let hissGain = null;
function hiss(on) {
  if (!audioCtx || audioCtx.state !== 'running') return;
  if (!hissGain) {
    const src = audioCtx.createBufferSource(); src.buffer = noiseBuf; src.loop = true;
    const bp = audioCtx.createBiquadFilter();
    bp.type = 'bandpass'; bp.frequency.value = 1200; bp.Q.value = 0.5;
    hissGain = audioCtx.createGain();
    hissGain.gain.value = 0;
    src.connect(bp); bp.connect(hissGain); hissGain.connect(audioCtx.destination);
    src.start();
  }
  hissGain.gain.setTargetAtTime(on ? 0.035 : 0.0001, audioCtx.currentTime, 0.05);
}
function laseTick() {
  if (!audioCtx || audioCtx.state !== 'running') return;
  const t0 = audioCtx.currentTime;
  const osc = audioCtx.createOscillator();
  osc.type = 'square'; osc.frequency.value = 2400;
  const g = audioCtx.createGain();
  g.gain.setValueAtTime(0.05, t0);
  g.gain.exponentialRampToValueAtTime(0.001, t0 + 0.05);
  osc.connect(g); g.connect(audioCtx.destination);
  osc.start(t0); osc.stop(t0 + 0.06);
}
addEventListener('pointerdown', () => { ensureAudio(); if (audioCtx) audioCtx.resume(); }, { once: false });
addEventListener('keydown', () => { ensureAudio(); if (audioCtx) audioCtx.resume(); }, { once: false });

