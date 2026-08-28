# Research 06 — Speech I/O in UE5 (STT, TTS, recorded lines, radio DSP)

Agent report, checks performed 2026-08-27. Synthesis: [../UNREAL_RESEARCH.md](../UNREAL_RESEARCH.md) §6.
Hard requirements: fully offline, no keys, Windows primary. Vocabulary: digits, directions, ~40 prowords.

## 1. whisper.cpp in UE

- whisper.cpp: MIT, actively maintained (5,000+ commits, CI through 2026) [verified]. Models: tiny 75 MiB / ~273 MB RAM, base 142 MiB / ~388 MB, small 466 MiB / ~852 MB [verified].
- **"Whisper has no grammar constraint" is wrong for whisper.cpp**: `whisper_full_params` contains `grammar_rules`, `n_grammar_rules`, `i_start_rule`, `grammar_penalty`, plus `initial_prompt` (~224 tokens) [verified: whisper.h header, exact fields]. A `grammars/` folder and an `examples/command` guided mode exist ("specify a list of commands and the transcription will be guided to classify your command into one from the list") — demonstrated with tiny.en on a Raspberry Pi 4 [verified: examples/command]. No UE plugin exposes the grammar machinery (none found).
- UE plugins: **RuntimeSpeechRecognizer** (gtreshchev) — MIT, whisper.cpp-based, **archived Feb 23, 2025**; maintained paid Fab version supports UE 4.27–5.8, five model sizes, streaming + buffered, on-device [verified: repo + solutions.georgy.dev; Fab price 403'd]. Archived MIT source still forkable; which UE 5.x it compiles against out of the box: not checked. **WhisperRealtime** (Akiya Research Institute) — real-time, in-process, one Blueprint component; Fab-distributed; UE 5.1 demo [verified: manual + demo repo]; license/price not checked.
- Accuracy on radio speech: stock Whisper degrades on radio phraseology — Whisper-ATC fine-tunes exist (~3.88% WER on ATCOSIM, 13.5% ATCO2, ~30% out-of-domain American radio) [verified: repo + ICRAT2024 paper]. [inferred] For a ~60-word closed vocabulary, utterance-final transcript + edit-distance fuzzy match against the legal phrase set absorbs most errors; `initial_prompt` seeded with the proword list helps.
- Latency: no published short-utterance numbers [not checked — none found]. [inferred] from the Pi-4 guided demo: modern laptop CPU transcribes a 2–5 s PTT utterance with tiny/base well under real-time — sub-second to ~2 s after release. Whisper is not streaming; for PTT, utterance-final is the natural fit.

## 2. Vosk in UE

- **No maintained open-source Vosk UE plugin exists** [verified by search]. IlgarLunin VoskPlugin: closed-source Fab item, docs repo public; in-process language server or WebSocket; Windows-tested; no grammar constraint documented [verified: docs]; Fab 403'd.
- **vosk-api directly** — Apache-2.0 [verified]; last commit Aug 9, 2026 (doc fix) — alive, slow-drip; prebuilt `vosk-win64-0.3.42.zip` (~13 MB, libvosk.dll) on the v0.3.42 release [verified: releases API].
- **Grammar constraint verified at the C API level** [verified: vosk_api.h]:
  ```c
  VoskRecognizer *vosk_recognizer_new_grm(VoskModel *model, float sample_rate, const char *grammar);
  // grammar example: ["one two three four five", "[unk]"]
  void vosk_recognizer_set_grm(VoskRecognizer *r, const char *grammar);   // swap at runtime
  void vosk_recognizer_set_partial_words(VoskRecognizer *r, int partial); // partials + confidences
  ```
  Header doc: "when you don't need to recognize large vocabulary you can specify a list of phrases." `[unk]` catches out-of-grammar speech.
- Model: vosk-model-small-en-us-0.15, **40 MB, Apache-2.0** [verified: alphacephei.com/vosk/models].
- Wrap effort [inferred]: one C header + one DLL + a UE ThirdParty module; 16 kHz mono PCM from UE Audio Capture during PTT; FinalResult on release. Days, not weeks — ~15 functions, no model-format tooling. Streaming-native: partials during the hold are free.

## 3. Other offline STT

- **sherpa-onnx** (k2-fsa) — Apache-2.0, very active; streaming + offline ASR, keyword spotting, VAD, TTS, speaker ID; C API; Windows x64 [verified]. Biasing = **hotwords** (boost scores, transducer + modified_beam_search) — softer than Vosk's hard grammar [verified: docs]. No UE integration found.
- April-ASR — **GPL-3.0**, "facing major rewrites over 2025" [verified]. GPL makes the linked binary GPL [inferred, standard interpretation]. Skip.
- Moonshine (Useful Sensors) — MIT code **and** models (except legacy non-English non-streaming under a non-commercial community license); streaming-optimized [verified]. No turnkey C API, no UE integration; model-size claims looked off, unverified.
- Windows built-in: **Windows.Media.SpeechRecognition (WinRT)** has exactly the right feature (SpeechRecognitionListConstraint, offline) but **requires package identity — unpackaged Win32 exes cannot call it** [verified: MS docs]; MSIX-packaging a UE build adds a distribution constraint. **SAPI 5 (COM)** works from any Win32 process with offline SRGS grammars — precedent unktomi/SAPIPlugin (UE4, 2016, 9 stars, **no license file**) [verified]; SAPI quality is legacy-grade [inferred].
- **UE built-in STT: none.** Only first-party speech feature is the accessibility TextToSpeech plugin (§4) [verified by absence across docs searches].

## 4. Offline TTS in UE

- **Piper status**: original rhasspy/piper is MIT but **archived Oct 6, 2025**; successor **OHF-Voice/piper1-gpl is GPL-3.0** (embeds espeak-ng); has CLI/server/Python/C++ API (libpiper); OHF "looking for maintainers" [verified: both repos]. Linking GPL libpiper into a UE game makes the game GPL [inferred, standard]; a sidecar process avoids that but adds ops. **Voice files carry per-voice dataset licenses** (e.g. en_US-lessac MODEL_CARD → Blizzard 2013 terms) [verified: MODEL_CARD].
- **sherpa-onnx TTS** — the clean in-engine route: Apache-2.0, C API, Windows; runs Piper-format (VITS, documented conversion), Kokoro, Matcha, KittenTTS voices [verified: docs]. No UE plugin; same ThirdParty wrap as Vosk [inferred].
- Windows SAPI TTS from UE: precedent BraceYourselfGames/UE-BYGTextToSpeech (UE4) [existence verified; UE5 compat not checked]. Stock robotic voice [inferred].
- UE built-in TextToSpeech plugin: ships since 5.0, Experimental, uses CMU **Flite**, built for accessibility narration [verified: quickstart doc]. Emergency fallback only [inferred].
- Commercial: ReadSpeaker on-device Fab plugin exists [listing verified; terms not checked].

## 5. Recorded-voice-lines route (the Godot-era favorite — still the recommendation)

- **Kokoro-82M — Apache-2.0 for code AND weights, commercial deployment allowed, no restriction on generated audio** [verified: github.com/hexgrad/kokoro]. The license-clean generator for the ~120 quips + digit/proword bank.
- Piper as generator: GPL covers the tool, not the WAVs [inferred, standard position]; per-voice licenses still apply (§4).
- **XTTS-v2: avoid.** Coqui Public Model License — non-commercial only, **covering the model and its audio output**; Coqui shut down Jan 2024, no commercial license can ever be bought [verified: HF card + shutdown discussions]. Same caveat class for most voice-cloning models: check the weights license, not the repo license.
- Digit-splicing precedent: FlightGear ATIS = prerecorded Festival samples concatenated at runtime [verified: wiki]; Falcon BMS "voice frag" workflows [verified: forum]. Proven for exactly this radio-readback use; cadence seams read as authentic radio.
- UE runtime stitching, both mechanisms confirmed: **SoundCue Concatenator** node (static graphs, small inter-asset delay caveat) [verified: Sound Cue Reference]; **MetaSounds** Wave Player has an On Finished trigger output wireable to the next player's Play, and the Wave Asset pin is settable by name from Blueprint — a dynamic clip queue for arbitrary grids [verified pins: uhiyama-lab guide; pattern inferred, not built].

## 6. Radio-effect DSP

- Native yes: **Synthesis and DSP Effects plugin** ships source/submix effects (bit crusher, filters, ring mod) + a Signal Processing C++ module [verified: plugin index + Blueprint API]. Standard chain: bandpass ~300–3000 Hz (submix filter) → light distortion/bitcrush → noise bed → squelch clips at PTT edges (§5 stitching provides). Submix filter params drivable from Blueprint [verified: forum]. Free worked example video with shared graph exists [verified existence; quality not checked]. No official Epic radio preset found.

---

## Recommendation

**STT: wrap vosk-api directly** — the only option with a verified hard grammar constraint (`vosk_recognizer_new_grm` + `[unk]`), Apache-2.0, prebuilt win64 DLL, 40 MB model, streaming partials during PTT. Grammar = full legal phrases + digits; keep a fuzzy post-parse anyway. Fallback: fork archived RuntimeSpeechRecognizer / integrate whisper.cpp with tiny.en/base.en + initial_prompt + edit-distance matching.
**TTS/voice: recorded lines + splicing, generated with Kokoro-82M.** One voice, batch-render once, ship WAVs; MetaSound Wave-Player queue behind bandpass/bitcrush/noise submix. Zero runtime TTS deps, zero inference latency, DSP hides seams. Fallback for truly dynamic speech: sherpa-onnx C API running a converted Piper/Kokoro voice — Apache-2.0 end to end. Never link GPL piper1-gpl; never touch XTTS.

## Top risks (ranked by expected cost)

1. Vosk module integration friction (moderate, bounded): DLL packaging, 48 kHz→16 kHz resample, threading. Prototype outside UE first. [inferred effort]
2. Proword accuracy under real conditions: no benchmark of vosk-small on radio-procedure speech exists to check. **Silent-failure guard: grammar-constrained recognizers force-match garbage to legal phrases — always include `[unk]` + a confidence gate so a bad read becomes "SAY AGAIN," never a wrong fire mission.**
3. License trap on TTS-generated assets (low if Kokoro): decide the voice **before** recording 120+ lines — regenerating the bank is the expensive part.
4. Whisper-path latency/drift (only if fallback taken): utterance-final, ~1–2 s post-release [inferred, unmeasured], documented radio-phraseology drift.
5. Splice cadence quality **[YOUR CALL]** — an hour of listening; the radio DSP covers most seams.

Sources: RuntimeSpeechRecognizer + docs · WhisperRealtime manual + demo · whisper.cpp + whisper.h + examples/command · WhisperATC + ICRAT2024 · vosk-api + vosk_api.h + models page · VoskPlugin-docs · sherpa-onnx + hotwords + TTS docs · april-asr · moonshine · MS speech-recognition docs · SAPIPlugin · rhasspy/piper · piper1-gpl · lessac MODEL_CARD · kokoro · XTTS-v2 + Coqui shutdown · Sound Cue Reference · MetaSounds basics (uhiyama-lab) · UE TextToSpeech quickstart · Synthesis plugin · FlightGear Communications wiki · Falcon BMS voice-frags thread · MetaSound radio-effect video · UE-BYGTextToSpeech
