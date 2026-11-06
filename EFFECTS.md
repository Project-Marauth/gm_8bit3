# Effects and layers (2.1.0)

gmsv_eightbit 2.1.0 adds a set of realistic built-in voices, a layer system that stacks them, and the DSP stages behind both, which `.gvox` presets can use too.

## Effect ids

An effect id is a **base preset** in the low 16 bits plus any number of **layer flags** above them:

```lua
local effect = bit.bor(eightbit.EFF_METROCOP_MODULATOR, eightbit.EFF_FLAG_RADIO)
eightbit.EnableEffect(ply:UserID(), effect)
```

The base is the voice itself (a modulator, or `EFF_NONE` for a natural voice). Each layer is something the voice passes through on its way to the listener. Layers always run in physical order, whatever order the flags are OR'd in:

| Order | Flag | Built-in preset it runs | What it is |
|---|---|---|---|
| 1 | `EFF_FLAG_CLOTH` | `EFF_CLOTH_MASK` | Scarf, bandana or surgical mask |
| 2 | `EFF_FLAG_MUFFLE` | `EFF_GAS_MASK` | Rubber respirator |
| 3 | `EFF_FLAG_HELMET` | `EFF_SEALED_HELMET` | Hazmat hood or sealed visor |
| 4 | `EFF_FLAG_MEGAPHONE` | `EFF_LOUDHAILER` | Handheld megaphone |
| 5 | `EFF_FLAG_RADIO` | `EFF_WALKIE_TALKIE` | Handheld FM radio |
| 6 | `EFF_FLAG_TELEPHONE` | `EFF_TELEPHONE` | Telephone line |
| 7 | `EFF_FLAG_INTERCOM` | `EFF_PA_SYSTEM` | Building intercom or tannoy |
| 8 | `EFF_FLAG_WEAK_SIGNAL` | `EFF_WEAK_SIGNAL` | A transmission at the edge of range |
| 9 | `EFF_FLAG_WALL` | `EFF_THROUGH_WALL` | Heard from the next room |
| 10 | `EFF_FLAG_UNDERWATER` | `EFF_UNDERWATER` | Submerged |

So a metrocop with the modulator on, keying a radio at the edge of range, is `EFF_METROCOP_MODULATOR | EFF_FLAG_RADIO | EFF_FLAG_WEAK_SIGNAL`, and a hazmat worker using a loudhailer is `EFF_FLAG_HELMET | EFF_FLAG_MEGAPHONE`.

Every id works in both `EnableEffect` (what everyone hears from a speaker) and `SetReceiverEffect` (what one listener hears), so a per-listener route can add a wall or a phone line on top of the speaker's own voice by passing the combined id.

Custom `.gvox` ids (1000 to 65534) fit in the base bits and take layers the same way.

## New built-in presets

| Id | Constant | Sound |
|---|---|---|
| 15 | `EFF_METROCOP_MODULATOR` | Civil Protection mask vocoder: honking respirator cavity, crushed radio band, static burst on key-up |
| 16 | `EFF_OVERWATCH_MODULATOR` | Overwatch soldier: lowered, synthetic, heavily compressed, two-tone chirp on key-up |
| 17 | `EFF_COMBINE_DISPATCH` | City announcer over loudspeakers with a long slapback off the buildings |
| 18 | `EFF_GAS_MASK` | Rubber respirator |
| 19 | `EFF_CLOTH_MASK` | Cloth over the mouth |
| 20 | `EFF_SEALED_HELMET` | Hazmat hood or sealed visor |
| 21 | `EFF_WALKIE_TALKIE` | Handheld FM radio with automatic gain, hiss and squelch |
| 22 | `EFF_WEAK_SIGNAL` | Heavy static, crackle and dropouts |
| 23 | `EFF_LOUDHAILER` | Handheld megaphone |
| 24 | `EFF_PA_SYSTEM` | Ceiling speakers in a large hall |
| 25 | `EFF_THROUGH_WALL` | Only the low end gets through |
| 26 | `EFF_UNDERWATER` | Dark, wavering, swallowed by the water |
| 27 | `EFF_COMBINE_GRUNT` | Heavy field vocoder: almost no dry voice left (2.2.0) |

Ids 0 to 14 are unchanged. Their pitch shift is now continuous across packets instead of restarting every packet, so pitched presets (`EFF_COMB`, `EFF_COMBO`, `EFF_CHIPMUNK` and so on) lose their buzz at the packet rate but otherwise sound the same.

## Transmissions

A transmission starts when a speaker's stream for an effect has been silent for more than 350 ms, or is used for the first time. At that moment every stage is reset (so no delay line replays the end of the previous sentence) and any squelch burst or key-up chirp the preset declares plays over the opening milliseconds. Switching a speaker from `base` to `base | EFF_FLAG_RADIO` when they key up therefore gets the radio's squelch on every transmission.

## Lua API additions

- `eightbit.EFF_FLAG_*`, `eightbit.EFF_BASE_MASK`, `eightbit.EFF_FLAG_MASK`, `eightbit.EFF_BUILTIN_LAST`, `eightbit.CUSTOM_ID_MAX`, and the new preset constants above.
- `eightbit.ComposeEffect(...)`: ORs any number of ids and flags together and returns the result, or `nil` if it is not valid.
- `eightbit.GetEffectBase(id)`, `eightbit.GetEffectFlags(id)`: split an id.
- `eightbit.GetLayers()`: list of `{ flag, preset, key, name }` in signal order.
- `eightbit.GetPresetName(id)` and `eightbit.IsValidPreset(id)` accept combined ids; the name joins the parts, for example `Metrocop modulator + Walkie-talkie`.

## New `.gvox` fields

Every field is optional and off by default, so existing presets are unaffected.

| Field | Range | Stage |
|---|---|---|
| `band_low_hz`, `band_high_hz` | 0 to 11000 | Butterworth high/low cut (steeper than `highpass_hz` / `lowpass_hz`) |
| `band_order` | 1 or 2 | 12 or 24 dB per octave |
| `peak1_hz`, `peak1_db`, `peak1_q` (also `peak2_*`, `peak3_*`) | Hz, -24 to 24 dB, 0.1 to 24 | Resonances: mask cavities, horns, rooms |
| `tube_ms`, `tube_feedback`, `tube_mix` | 0 to 30, 0 to 0.95, 0 to 1 | Short feedback comb: the cavity in front of the mouth |
| `compress_threshold_db`, `compress_ratio`, `compress_makeup_db` | -60 to 0, 1 to 40, -24 to 36 | Compressor or radio AGC |
| `hiss` | 0 to 0.25 | Channel noise, shaped by the band filter |
| `crackle` | 0 to 1 | Static impulses |
| `dropout` | 0 to 1 | How often the voice fades under the static |
| `squelch_ms`, `squelch_level` | 0 to 1000, 0 to 1 | Noise burst at the start of a transmission |
| `chirp_hz`, `chirp_ms`, `chirp_level`, `chirp_count` | Hz, ms, 0 to 1, 1 to 4 | Key-up beeps |
| `reverb_mix`, `reverb_size`, `reverb_damp` | 0 to 1 each | Small room reverb |
| `wobble_hz`, `wobble_ms` | 0 to 40, 0 to 20 | Vibrato |
| `vocoder_mix` | 0 to 1 | Channel vocoder, blended over the dry voice (see below) |
| `vocoder_bands`, `vocoder_low_hz`, `vocoder_high_hz`, `vocoder_width` | 4 to 32, Hz, Hz, 0.25 to 4 | Band count, range and width |
| `vocoder_attack_ms`, `vocoder_release_ms` | 0.1 to 50, 1 to 200 | How fast each band follows the voice |
| `vocoder_noise`, `vocoder_buzz`, `vocoder_self`, `vocoder_buzz_hz` | 0 to 1 each, 20 to 1000 Hz | Carrier blend |
| `vocoder_gain` | 0 to 8 | Vocoder output level |
| `vocoder_formant` | 0.5 to 2 | Re-synthesise every band at a shifted frequency: moves formants independently of pitch |
| `vocoder_flatten` | 0 to 1 | Level each band against its own slow average, removing the speaker's long-term timbre |
| `vocoder_smooth` | 0 to 1 | Blur neighbouring bands, discarding fine formant detail |
| `limit` | boolean | Soft output limiter |

Signal order inside one preset: pitch, vocoder, wobble, one-pole filters, ring modulation, tube, downsample, drive, quantise, compressor, dropout, hiss/crackle/squelch/chirp, band and peaks, echo, reverb, white noise and gain, limiter.

Example, a sealed diving helmet:

```ini
version=1
name=Diving helmet
band_low_hz=110
band_high_hz=2200
peak1_hz=420
peak1_db=6
peak1_q=1.3
tube_ms=1.1
tube_feedback=0.55
tube_mix=0.45
reverb_mix=0.15
reverb_size=0.2
reverb_damp=0.8
limit=true
```

## The Combine vocoder (2.2.0)

There is no primary source for how Valve processed the HL2 Combine voices, but every community recreation of the soldier voice is a channel vocoder (typically FL Studio's Vocodex, "Unpitched" preset, around 23 bands, upper mids dipped), and Half-Life: Alyx's grunts lean on a vocoder much harder. The 2.1 modulators had no vocoder at all, only filters, a comb, clipping and compression, which is why they read as a compressed radio voice.

The vocoder stage splits the voice into `vocoder_bands` log-spaced bands between `vocoder_low_hz` and `vocoder_high_hz`, follows each band's envelope, and imposes those envelopes on a synthetic carrier split through matching bands. The carrier is a blend of:

- `vocoder_noise`: white noise. Unpitched and whispered; the classic Combine soldier texture.
- `vocoder_buzz`: a sawtooth at `vocoder_buzz_hz`. A monotone robot.
- `vocoder_self`: the voice hard-clipped into a square wave. Follows the speaker's own pitch, so it stays expressive but gritty.

Each carrier band is normalised by its own level, so switching carriers changes texture, not loudness. More bands and a short release keep words intelligible; fewer bands and a long release sound more machine. Bands are sixth-order Butterworth bandpasses with contiguous edges between `vocoder_low_hz` and `vocoder_high_hz`. Simpler bands were tried first and smeared words into noise.

## Hiding who is talking (2.3.0)

A mask should hide the person behind it. Identity survives a vocoder through four things, and the Combine presets remove each:

| What gives a speaker away | What removes it |
|---|---|
| Any dry voice in the mix | `vocoder_mix=1` |
| Pitch and intonation | A noise or fixed buzz carrier, never `vocoder_self` |
| Formant positions (vocal tract size) | `vocoder_formant` |
| Long-term timbre and fine formant detail | `vocoder_flatten`, `vocoder_smooth`, fewer bands |

Measured on 58 LibriSpeech test-clean recordings from 12 speakers with an ECAPA-TDNN speaker-recognition model (SpeechBrain), which is far harder to fool than a human ear. "Link" is the equal error rate of matching a processed clip to the speaker's real voice, "same mask" of telling two masked speakers apart; 50% is a coin toss. "Words" is the word error rate of Whisper base.en, a pessimistic stand-in for a human listener on vocoded speech.

| Preset | Link | Same mask | Words |
|---|---|---|---|
| Dry voice | 0% | 0% | 6% |
| Metrocop modulator, 2.2.0 | 2% | 1% | 10% |
| Metrocop modulator, 2.3.0 | 33% | 20% | 31% |
| Overwatch modulator, 2.2.0 | 11% | 4% | 19% |
| Overwatch modulator, 2.3.0 | 38% | 36% | 43% |
| Combine grunt, 2.3.0 | 39% | 26% | 54% |

For reference, Valve's two beta metrocop actors, through Valve's own processing, score 33.5% "same mask" against each other. Anonymity and intelligibility trade against each other: the metrocop keeps 12 bands so officers stay understandable in roleplay, Overwatch uses 10 and the grunt 8.

## Voice lab

`tools/voicelab` auditions presets without building or loading the DLL. It compiles `tools/voicelab/voicelab.cpp` against this repository's own `source/` with a local C++17 compiler, so what it plays is the module's engine, packet by packet, not an imitation.

```sh
python tools/voicelab/server.py
```

Open http://127.0.0.1:8765, record a line or load an audio file, and build a chain from the built-in presets and layers. Every field of every stage has a slider; each change re-renders in under 100 ms and plays back, with a dry A/B, a spectrogram, and WAV download. "Copy .gvox" exports the stage as a preset file the server loads unchanged, and the renderer rebuilds itself when `source/audio_effects.h` changes, so engine edits can be heard the same way. Needs Python 3 and `g++` or `clang++` on the PATH (or `CXX`). The Opus round trip the server adds is not simulated.

The renderer also works on its own: `voicelab list` prints every built-in as `.gvox`, and `voicelab render in.wav out.wav builtin:16 my_radio.gvox` renders a chain over a 24 kHz mono 16-bit WAV.

## Performance

The hot path no longer allocates per voice packet: decode, effect and encode buffers are reused, a stream's resolved chain is cached until presets or tuning factors change, and the "does this speaker have routes" check is a single lookup instead of a scan over every route. In the DSP, the compressor computes its gain every 16 samples, and the pitch window, ring modulator and vibrato no longer call `sin` per sample. On a desktop CPU a 20 ms packet costs about 5 to 16 µs for one preset and under 30 µs for a three-layer chain.
