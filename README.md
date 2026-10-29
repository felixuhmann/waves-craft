# Waves Craft

A checklist of every plugin in the Waves catalog (the full "Waves Complete" / Mercury set, 250 plugins).
Tick a plugin off by changing `[ ]` to `[x]` in the **Done** column.

**Progress:** 0 / 250

## Implementation queue

The plugins from the original offline install, ordered by estimated ease of a clean-room reimplementation
(see [docs/process.md](docs/process.md)). We work top to bottom, one plugin at a time. Ease is judged by how
much of the behaviour can be pinned down by black-box measurement: static/linear processing is easiest,
then deterministic time-varying effects, then dynamics, then analog-modelled nonlinearities, and last
adaptive or analysis/resynthesis processors. Reimplementations ship under our own names, never the vendor's
or third-party trademarks (SSL, CLA, Abbey Road, …).

| # | Done | Plugin | Kind | Why this rank |
|---:|:---:|---|---|---|
| 1 | [x] | S1 Stereo Imager | Stereo matrix + shuffler | Linear, time-invariant; fully measured, spec r1 approved — released as Craft Shuffler / Imager / MS Matrix |
| 2 | n/a | ModFX Autopan | Modulation | Internal module, not loadable as a host plugin — cannot be measured |
| 3 | n/a | ModFX Delay | Delay | Internal module, not loadable as a host plugin — cannot be measured |
| 4 | n/a | ModFX Chorus | Modulation | Internal module, not loadable as a host plugin — cannot be measured |
| 5 | [x] | MetaFlanger | Modulation | Flanger with LFO/feedback; deterministic, few controls  — released as Craft Flanger |
| 6 | [ ] | H-Delay | Delay | Delay + filters + modulation; deterministic, more controls |
| 7 | [ ] | MannyM-Delay | Delay | Delay with filtering/ducking; builds on #3/#6 |
| 8 | [ ] | Doubler | Pitch/delay | Detuned delayed voices; small pitch-shift component |
| 9 | [ ] | MetaFilter | Filter | Resonant filter with LFO/envelope modulation |
| 10 | n/a | ModFX Limiter | Dynamics | Internal module, not loadable as a host plugin — cannot be measured |
| 11 | [x] | L1 | Dynamics | Look-ahead brickwall limiter; measurable with steps/bursts  — released as Craft Peak Limiter |
| 12 | n/a | ModFX Compressor | Dynamics | Internal module, not loadable as a host plugin — cannot be measured |
| 13 | [ ] | DeEsser | Dynamics | Band-limited sidechain compression |
| 14 | [ ] | RVox | Dynamics | One-knob compressor/gate/limiter chain |
| 15 | [ ] | C1 | Dynamics | Compressor/gate/expander with sidechain EQ; many modes |
| 16 | [ ] | L2 | Dynamics | Limiter with adaptive release and dither |
| 17 | [ ] | SSL E-Channel | Channel strip | EQ curves measurable; dynamics section adds work |
| 18 | [ ] | SSL G-Channel | Channel strip | Same family as #17 |
| 19 | [ ] | C4 | Multiband dynamics | Crossover + 4 compressors; many interacting parameters |
| 20 | [ ] | Vocal Rider | Gain riding | Level-detection/ride behaviour needs long-signal measurement |
| 21 | n/a | ModFX Distortion | Saturation | Internal module, not loadable as a host plugin — cannot be measured |
| 22 | [ ] | MannyM-Distortion | Saturation | Multi-stage distortion; nonlinear |
| 23 | [ ] | H-Comp | Dynamics | Analog-style compressor with punch/hold; nonlinear |
| 24 | [ ] | CLA-76 | Analog model | FET compressor model; level- and frequency-dependent nonlinearity |
| 25 | [ ] | CLA-2A | Analog model | Opto compressor model; program-dependent release |
| 26 | [ ] | CLA-3A | Analog model | Opto compressor/limiter model |
| 27 | [ ] | Vitamin | Multiband enhancer | Multiband harmonic processing |
| 28 | [ ] | L3 Multi | Multiband limiter | Multiband peak limiting with adaptive behaviour |
| 29 | [ ] | L3 Ultra | Multiband limiter | Same family as #28 |
| 30 | [ ] | L3-LL Multi | Multiband limiter | Low-latency variant of #28 |
| 31 | [ ] | L3-LL Ultra | Multiband limiter | Low-latency variant of #29 |
| 32 | n/a | ModFX Reverb | Reverb | Internal module, not loadable as a host plugin — cannot be measured |
| 33 | [ ] | RVerb | Reverb | Algorithmic reverb, many parameters |
| 34 | [ ] | H-Reverb | Reverb | Long FIR-based reverb with dynamics/modulation |
| 35 | [ ] | MannyM-Reverb | Reverb | Preset-style reverb with extra processing |
| 36 | [ ] | J37 | Analog model | Tape machine model (saturation, wow/flutter, noise) |
| 37 | [ ] | KingsMic | Analog model | Microphone/preamp modelling |
| 38 | [ ] | CLA Vocals | Composite | Multi-effect chain (EQ, comp, reverb, delay, pitch) |
| 39 | [ ] | NS1 | Adaptive | Adaptive noise suppression; behaviour depends on learned noise profile |
| 40 | [ ] | Vocal Bender | Pitch/formant | Pitch and formant shifting |
| 41 | [ ] | Waves Tune Real-Time | Pitch correction | Real-time pitch detection and correction |
| 42 | [ ] | OVox | Resynthesis | Vocal resynthesis/vocoder engine; hardest to specify |

The ModFX folder contains seven small modules (Autopan, Chorus, Compressor, Delay, Distortion, Limiter, Reverb),
so the original install is 42 entries in total, counting S1 as one bundle. The ModFX modules are internal building
blocks that cannot be loaded as a plugin in a host, so they cannot be measured and are skipped (`n/a`); that
leaves **35 host-loadable plugins** to implement.

## Craft Audio plugins (monorepo)

Every reimplementation lives in its own directory under [`plugins/`](plugins) and is picked up by the root
`CMakeLists.txt` automatically; JUCE, the CLAP wrapper and Catch2 are fetched once for all of them. How to add one:
[CONTRIBUTING.md](CONTRIBUTING.md). Specs: [spec/](spec/README.md); verification reports: `docs/verification/`.

| Slug | Products | Spec | Notes |
|---|---|---|---|
| [`h-delay`](plugins/h-delay) | Craft Hybrid Delay | [r3](spec/h-delay/r3/SPEC.md) | [notes](plugins/h-delay/IMPLEMENTATION_NOTES.md), verification [r1](docs/verification/h-delay-r1.md), [r2](docs/verification/h-delay-r2.md) |
| [`stereo-imager`](plugins/stereo-imager) | Craft Shuffler, Craft Imager, Craft MS Matrix | [r1](spec/stereo-imager/r1/SPEC.md) | [notes](plugins/stereo-imager/IMPLEMENTATION_NOTES.md), [verification](docs/verification/stereo-imager-r1.md) |
| [`motion-filter`](plugins/motion-filter) | Craft Motion Filter | [r1](spec/motion-filter/r1/SPEC.md) | [notes](plugins/motion-filter/IMPLEMENTATION_NOTES.md) |
| [`de-esser`](plugins/de-esser) | Craft De-Esser, Craft De-Esser Mono | [r1](spec/de-esser/r1/SPEC.md) | [notes](plugins/de-esser/IMPLEMENTATION_NOTES.md) |
| [`doubler`](plugins/doubler) | Craft Doubler 4, Craft Doubler 2 | [r1](spec/doubler/r1/SPEC.md) | [notes](plugins/doubler/IMPLEMENTATION_NOTES.md) |
| [`peak-limiter`](plugins/peak-limiter) | Craft Peak Limiter (stereo / mono, look-ahead 64/128/256 samples) | [r1](spec/peak-limiter/r1/SPEC.md) | [notes](plugins/peak-limiter/IMPLEMENTATION_NOTES.md) |
| [`flanger`](plugins/flanger) | Craft Flanger, Craft Flanger Mono, Craft Flanger Mono-Stereo | [r1](spec/flanger/r1/SPEC.md) | [notes](plugins/flanger/IMPLEMENTATION_NOTES.md) |
| [`mastering-limiter`](plugins/mastering-limiter) | Craft Mastering Limiter | [r1](spec/mastering-limiter/r1/SPEC.md) | [notes](plugins/mastering-limiter/IMPLEMENTATION_NOTES.md) |

The first clean-room implementation, built only from [spec r1](spec/stereo-imager/r1/SPEC.md): **Craft Shuffler**,
**Craft Imager** and **Craft MS Matrix** by *Craft Audio* — a stereo width / rotation / asymmetry matrix with a
phase-compensated low-frequency shuffler, an original vector-drawn GUI, zero latency. Formats: VST3, CLAP and
Standalone for Windows x64 and Linux x64. Licensed AGPL-3.0 (JUCE); see [THIRD_PARTY.md](THIRD_PARTY.md).

**Craft Doubler 4** and **Craft Doubler 2** (built from [spec r1](spec/doubler/r1/SPEC.md)): up to four delayed,
detuned and modulated copies of the input with a correlation-spliced pitch shifter, per-voice pan, gain,
feedback and octave-down, a voices-only shelving EQ and an aligned or free direct path; zero reported latency.

Build all plugins and run all tests:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Plug-ins land in `build/plugins/<slug>/<Target>_artefacts/Release/` (e.g. `build/plugins/h-delay/CraftHybridDelay_artefacts/Release/`
for **Craft Hybrid Delay**, the stereo / mono-to-stereo tape-style delay of [`plugins/h-delay`](plugins/h-delay)). `-DCRAFT_BUILD_PLUGINS=OFF` builds only the
Plug-ins land in `build/plugins/<slug>/<Target>_artefacts/Release/` (e.g. `build/plugins/doubler/CraftDoubler4_artefacts/Release/`
for Craft Doubler 4; `ctest -R doubler` runs only the doubler tests). `-DCRAFT_BUILD_PLUGINS=OFF` builds only the
Plug-ins land in `build/plugins/<slug>/<Target>_artefacts/Release/` (e.g. `build/plugins/flanger/CraftFlanger_artefacts/Release/`
for Craft Flanger; `ctest -R flanger/` runs only the flanger tests). `-DCRAFT_BUILD_PLUGINS=OFF` builds only the
DSP unit tests (no JUCE download). Every push to `main` publishes a GitHub release with one zip per plugin and a
combined `craft-audio-bundle-<version>-<platform>.zip` per platform (packaged by `ci/package.py`).

## Full catalogue

| Done | Plugin | Category |
|:---:|---|---|
| [ ] | [Abbey Road Chambers](https://www.waves.com/plugins/abbey-road-chambers) | Abbey Road |
| [ ] | [Abbey Road EMI TG12345 Channel](https://www.waves.com/plugins/emi-tg12345) | Abbey Road |
| [ ] | [Abbey Road J37 Tape](https://www.waves.com/plugins/j37-tape) | Abbey Road |
| [ ] | [Abbey Road King’s Microphones](https://www.waves.com/plugins/the-king-s-microphones) | Abbey Road |
| [ ] | [Abbey Road REDD Consoles](https://www.waves.com/plugins/redd) | Abbey Road |
| [ ] | [Abbey Road Reel ADT](https://www.waves.com/plugins/reel-adt) | Abbey Road |
| [ ] | [Abbey Road Reverb Plates](https://www.waves.com/plugins/abbey-road-reverb-plates) | Abbey Road |
| [ ] | [Abbey Road RS124 Compressor](https://www.waves.com/plugins/abbey-road-rs124-compressor) | Abbey Road |
| [ ] | [Abbey Road RS56 Passive EQ](https://www.waves.com/plugins/rs56) | Abbey Road |
| [ ] | [Abbey Road Saturator](https://www.waves.com/plugins/abbey-road-saturator) | Abbey Road |
| [ ] | [Abbey Road Studio 3](https://www.waves.com/plugins/abbey-road-studio-3) | Abbey Road |
| [ ] | [Abbey Road TG Mastering Chain](https://www.waves.com/plugins/abbey-road-tg-mastering-chain) | Abbey Road |
| [ ] | [Abbey Road Vinyl](https://www.waves.com/plugins/abbey-road-vinyl) | Abbey Road |
| [ ] | [Aphex Vintage Aural Exciter](https://www.waves.com/plugins/aphex-vintage-aural-exciter) | Harmonics |
| [ ] | [API 2500](https://www.waves.com/plugins/api-2500) | Compressors |
| [ ] | [API 550](https://www.waves.com/plugins/api-550) | Equalizers |
| [ ] | [API 560](https://www.waves.com/plugins/api-560) | Equalizers |
| [ ] | [Atlas Reverb](https://www.waves.com/plugins/atlas-reverb) | Reverb |
| [ ] | [AudioTrack](https://www.waves.com/plugins/audiotrack) | Compressors |
| [ ] | [B360 Ambisonics Encoder](https://www.waves.com/plugins/b360-ambisonics-encoder) | Surround |
| [ ] | [Bass Fingers](https://www.waves.com/plugins/bass-fingers) | Virtual Instruments |
| [ ] | [Bass Rider](https://www.waves.com/plugins/bass-rider) | Bass |
| [ ] | [Bass Slapper](https://www.waves.com/plugins/bass-slapper) | Virtual Instruments |
| [ ] | [Berzerk Distortion](https://www.waves.com/plugins/berzerk-distortion) | Saturation & Distortion |
| [ ] | [Brauer Motion](https://www.waves.com/plugins/brauer-motion) | Effects |
| [ ] | [BSS DPR-402](https://www.waves.com/plugins/bss-dpr-402) | Compressors |
| [ ] | [Butch Vig Vocals](https://www.waves.com/plugins/butch-vig-vocals) | Signature Series |
| [ ] | [C1 Compressor](https://www.waves.com/plugins/c1-compressor) | Compressors |
| [ ] | [C360 Surround Compressor](https://www.waves.com/plugins/c360-surround-compressor) | Surround |
| [ ] | [C4 Multiband Compressor](https://www.waves.com/plugins/c4-multiband-compressor) | Compressors |
| [ ] | [C6 Multiband Compressor](https://www.waves.com/plugins/c6-multiband-compressor) | Compressors |
| [ ] | [Center](https://www.waves.com/plugins/center) | Stereo Imaging |
| [ ] | [CLA Bass](https://www.waves.com/plugins/cla-bass) | Signature Series |
| [ ] | [CLA Drums](https://www.waves.com/plugins/cla-drums) | Signature Series |
| [ ] | [CLA EchoSphere](https://www.waves.com/plugins/cla-echosphere) | Effects |
| [ ] | [CLA Effects](https://www.waves.com/plugins/cla-effects) | Signature Series |
| [ ] | [CLA Epic](https://www.waves.com/plugins/cla-epic) | Effects |
| [ ] | [CLA Guitars](https://www.waves.com/plugins/cla-guitars) | Signature Series |
| [ ] | [CLA MixDown](https://www.waves.com/plugins/cla-mixdown) | Signature Series |
| [ ] | [CLA MixHub](https://www.waves.com/plugins/cla-mixhub) | Channel Strips |
| [ ] | [CLA Nx](https://www.waves.com/plugins/cla-nx) | Headphone Mixing |
| [ ] | [CLA Unplugged](https://www.waves.com/plugins/cla-unplugged) | Signature Series |
| [ ] | [CLA Vocals](https://www.waves.com/plugins/cla-vocals) | Signature Series |
| [ ] | [CLA-2A Compressor / Limiter](https://www.waves.com/plugins/cla-2a-compressor-limiter) | Compressors |
| [ ] | [CLA-3A Compressor / Limiter](https://www.waves.com/plugins/cla-3a-compressor-limiter) | Compressors |
| [ ] | [CLA-76 Compressor / Limiter](https://www.waves.com/plugins/cla-76-compressor-limiter) | Compressors |
| [ ] | [Clarity Vx®](https://www.waves.com/plugins/clarity-vx) | Noise Reduction |
| [ ] | [Clarity Vx® DeReverb](https://www.waves.com/plugins/clarity-vx-dereverb) | Reverb Removal |
| [ ] | [Clarity Vx® DeReverb Pro](https://www.waves.com/plugins/clarity-vx-dereverb-pro) | Reverb Removal |
| [ ] | [Clarity Vx® Pro](https://www.waves.com/plugins/clarity-vx-pro) | Noise Reduction |
| [ ] | [Clarix LB](https://www.waves.com/plugins/clarix-lb) | Live |
| [ ] | [Clavinet](https://www.waves.com/plugins/clavinet) | Virtual Instruments |
| [ ] | [Cobalt Saphira](https://www.waves.com/plugins/cobalt-saphira) | Harmonics |
| [ ] | [Codex Wavetable Synth](https://www.waves.com/plugins/codex) | Synths |
| [ ] | [COSMOS Sample Finder](https://www.waves.com/plugins/cosmos-sample-finder) | Sampling |
| [ ] | [CR8 Creative Sampler](https://www.waves.com/plugins/cr8-creative-sampler) | Sampling |
| [ ] | [Curves AQ](https://www.waves.com/plugins/curves-aq) | Equalizers |
| [ ] | [Curves Equator](https://www.waves.com/plugins/curves-equator) | Equalizers |
| [ ] | [Curves Resolve](https://www.waves.com/plugins/curves-resolve) | Equalizers |
| [ ] | [dbx® 160 Compressor / Limiter](https://www.waves.com/plugins/dbx-160-compressor-limiter) | Modeling |
| [ ] | [DeBreath](https://www.waves.com/plugins/debreath) | Vocals |
| [ ] | [DeEsser](https://www.waves.com/plugins/deesser) | Vocals |
| [ ] | [Doppler](https://www.waves.com/plugins/doppler) | Effects |
| [ ] | [Dorrough Stereo](https://www.waves.com/plugins/dorrough-stereo) | Meters |
| [ ] | [Dorrough Surround](https://www.waves.com/plugins/dorrough-surround) | Meters |
| [ ] | [Doubler](https://www.waves.com/plugins/doubler) | Delay |
| [ ] | [Eddie Kramer Bass Channel](https://www.waves.com/plugins/eddie-kramer-bass-channel) | Signature Series |
| [ ] | [Eddie Kramer Drum Channel](https://www.waves.com/plugins/eddie-kramer-drum-channel) | Signature Series |
| [ ] | [Eddie Kramer Effects Channel](https://www.waves.com/plugins/eddie-kramer-effects-channel) | Signature Series |
| [ ] | [Eddie Kramer Guitar Channel](https://www.waves.com/plugins/eddie-kramer-guitar-channel) | Signature Series |
| [ ] | [Eddie Kramer Vocal Channel](https://www.waves.com/plugins/eddie-kramer-vocal-channel) | Signature Series |
| [ ] | [Electric 200 Piano](https://www.waves.com/plugins/electric-200-piano) | Virtual Instruments |
| [ ] | [Electric 88 Piano](https://www.waves.com/plugins/electric-88-piano) | Virtual Instruments |
| [ ] | [Electric Grand 80 Piano](https://www.waves.com/plugins/electric-grand-80-piano) | Virtual Instruments |
| [ ] | [Element 2.0 Virtual Analog Synth](https://www.waves.com/plugins/element) | Synths |
| [ ] | [eMo D5 Dynamics](https://www.waves.com/plugins/emo-d5-dynamics) | Compressors |
| [ ] | [eMo F2 Filter](https://www.waves.com/plugins/emo-f2-filter) | Live |
| [ ] | [eMo Generator](https://www.waves.com/plugins/emo-generator) | Live |
| [ ] | [eMo Q4 Equalizer](https://www.waves.com/plugins/emo-q4-equalizer) | Live |
| [ ] | [Enigma](https://www.waves.com/plugins/enigma) | Effects |
| [ ] | [F6 Floating-Band Dynamic EQ](https://www.waves.com/plugins/f6-floating-band-dynamic-eq) | Equalizers |
| [ ] | [Feedback Hunter](https://www.waves.com/plugins/feedback-hunter) | Live |
| [ ] | [Flow Motion FM Synth](https://www.waves.com/plugins/flow-motion-fm-synth) | Virtual Instruments |
| [ ] | [Gemstones](https://www.waves.com/plugins/gemstones) | Effects |
| [ ] | [GEQ Graphic Equalizer](https://www.waves.com/plugins/geq-graphic-equalizer) | Equalizers |
| [ ] | [Grand Rhapsody Piano](https://www.waves.com/plugins/grand-rhapsody-piano) | Virtual Instruments |
| [ ] | [Greg Wells MixCentric](https://www.waves.com/plugins/greg-wells-mixcentric) | Signature Series |
| [ ] | [Greg Wells PianoCentric](https://www.waves.com/plugins/greg-wells-pianocentric) | Signature Series |
| [ ] | [Greg Wells ToneCentric](https://www.waves.com/plugins/greg-wells-tonecentric) | Harmonics |
| [ ] | [Greg Wells VoiceCentric](https://www.waves.com/plugins/greg-wells-voicecentric) | Signature Series |
| [ ] | [GTR Solo](https://www.waves.com/plugins/gtr-solo) | Guitar |
| [ ] | [GTR3](https://www.waves.com/plugins/gtr3) | Guitar |
| [ ] | [GTR3 Amps](https://www.waves.com/plugins/gtr3-amps) | Saturation & Distortion |
| [ ] | [GTR3 Stomps](https://www.waves.com/plugins/gtr3-stomps) | Effects |
| [ ] | [GTR3 ToolRack](https://www.waves.com/plugins/gtr3-toolrack) | Effects |
| [ ] | [GTR3 Tuner](https://www.waves.com/plugins/gtr3-tuner) | Effects |
| [ ] | [H-Comp Hybrid Compressor](https://www.waves.com/plugins/h-comp-hybrid-compressor) | Compressors |
| [ ] | [H-Delay Hybrid Delay](https://www.waves.com/plugins/h-delay-hybrid-delay) | Delay |
| [ ] | [H-EQ Hybrid Equalizer](https://www.waves.com/plugins/h-eq-hybrid-equalizer) | Equalizers |
| [ ] | [H-Reverb Hybrid Reverb](https://www.waves.com/plugins/h-reverb-hybrid-reverb) | Reverb |
| [ ] | [IDR360 Bit Re-Quantizer](https://www.waves.com/plugins/idr360-bit-re-quantizer) | Surround |
| [ ] | [IDX Intelligent Dynamics](https://www.waves.com/plugins/idx-intelligent-dynamics) | Compressors |
| [ ] | [Immersive Wrapper](https://www.waves.com/plugins/immersive-wrapper) | Headphone Mixing |
| [ ] | [Infected Mushroom Pusher](https://www.waves.com/plugins/infected-mushroom-pusher) | Synths |
| [ ] | [InPhase](https://www.waves.com/plugins/inphase) | Effects |
| [ ] | [InPhase LT](https://www.waves.com/plugins/inphase-lt) | Effects |
| [ ] | [InTrigger Drum Replacer](https://www.waves.com/plugins/intrigger-drum-replacer) | Effects |
| [ ] | [IR-L Convolution Reverb](https://www.waves.com/plugins/ir-l-convolution-reverb) | Reverb |
| [ ] | [IR-Live Convolution Reverb](https://www.waves.com/plugins/ir-live-convolution-reverb) | Reverb |
| [ ] | [IR1 Convolution Reverb](https://www.waves.com/plugins/ir1-convolution-reverb) | Reverb |
| [ ] | [IR360 Convolution Reverb](https://www.waves.com/plugins/ir360-convolution-reverb) | Reverb |
| [ ] | [JJP Bass](https://www.waves.com/plugins/jjp-bass) | Signature Series |
| [ ] | [JJP Cymbals & Percussion](https://www.waves.com/plugins/jjp-cymbals-percussion) | Signature Series |
| [ ] | [JJP Drums](https://www.waves.com/plugins/jjp-drums) | Signature Series |
| [ ] | [JJP Guitars](https://www.waves.com/plugins/jjp-guitars) | Signature Series |
| [ ] | [JJP Strings & Keys](https://www.waves.com/plugins/jjp-strings-keys) | Signature Series |
| [ ] | [JJP Vocals](https://www.waves.com/plugins/jjp-vocals) | Signature Series |
| [ ] | [Kaleidoscopes](https://www.waves.com/plugins/kaleidoscopes) | Effects |
| [ ] | [Key Detector](https://www.waves.com/plugins/key-detector) | Vocals |
| [ ] | [Kramer HLS Channel](https://www.waves.com/plugins/kramer-hls-channel) | Modeling |
| [ ] | [Kramer Master Tape](https://www.waves.com/plugins/kramer-master-tape) | Modeling |
| [ ] | [Kramer PIE Compressor](https://www.waves.com/plugins/kramer-pie-compressor) | Compressors |
| [ ] | [L1 Ultramaximizer](https://www.waves.com/plugins/l1-ultramaximizer) | Mastering & Limiters |
| [ ] | [L2 Ultramaximizer](https://www.waves.com/plugins/l2-ultramaximizer) | Mastering & Limiters |
| [ ] | [L3 Multimaximizer](https://www.waves.com/plugins/l3-multimaximizer) | Mastering & Limiters |
| [ ] | [L3 Ultramaximizer](https://www.waves.com/plugins/l3-ultramaximizer) | Mastering & Limiters |
| [ ] | [L3-16 Multimaximizer](https://www.waves.com/plugins/l3-16-multimaximizer) | Mastering & Limiters |
| [ ] | [L3-LL Multimaximizer](https://www.waves.com/plugins/l3-ll-multimaximizer) | Mastering & Limiters |
| [ ] | [L3-LL Ultramaximizer](https://www.waves.com/plugins/l3-ll-ultramaximizer) | Mastering & Limiters |
| [ ] | [L360 Surround Limiter](https://www.waves.com/plugins/l360-surround-limiter) | Surround |
| [ ] | [L4 Ultramaximizer](https://www.waves.com/plugins/l4-ultramaximizer) | Mastering & Limiters |
| [ ] | [LFE360 Low-Pass Filter](https://www.waves.com/plugins/lfe360-low-pass-filter) | Surround |
| [ ] | [Linear Phase EQ](https://www.waves.com/plugins/linear-phase-eq) | Equalizers |
| [ ] | [Linear Phase Multiband Compressor](https://www.waves.com/plugins/linear-phase-multiband-compressor) | Compressors |
| [ ] | [LoAir](https://www.waves.com/plugins/loair) | Surround |
| [ ] | [Lofi Space](https://www.waves.com/plugins/lofi-space) | Effects |
| [ ] | [M360 Surround Manager & Mixdown](https://www.waves.com/plugins/m360-surround-manager-mixdown) | Surround |
| [ ] | [Magma BB Tubes](https://www.waves.com/plugins/bb-tubes) | Saturation & Distortion |
| [ ] | [Magma Lil Tube](https://www.waves.com/plugins/lil-tube) | Saturation & Distortion |
| [ ] | [Magma Springs](https://www.waves.com/plugins/magma-springs) | Reverb |
| [ ] | [Magma StressBox](https://www.waves.com/plugins/magma-stressbox) | Compressors |
| [ ] | [Magma Tube Channel Strip](https://www.waves.com/plugins/magma-tube-channel-strip) | Channel Strips |
| [ ] | [Manny Marroquin Delay](https://www.waves.com/plugins/manny-marroquin-delay) | Signature Series |
| [ ] | [Manny Marroquin Distortion](https://www.waves.com/plugins/manny-marroquin-distortion) | Saturation & Distortion |
| [ ] | [Manny Marroquin EQ](https://www.waves.com/plugins/manny-marroquin-eq) | Signature Series |
| [ ] | [Manny Marroquin Reverb](https://www.waves.com/plugins/manny-marroquin-reverb) | Signature Series |
| [ ] | [Manny Marroquin Tone Shaper](https://www.waves.com/plugins/manny-marroquin-tone-shaper) | Harmonics |
| [ ] | [Manny Marroquin Triple D](https://www.waves.com/plugins/manny-marroquin-triple-d) | Signature Series |
| [ ] | [Maserati ACG](https://www.waves.com/plugins/maserati-acg) | Signature Series |
| [ ] | [Maserati B72](https://www.waves.com/plugins/maserati-b72) | Signature Series |
| [ ] | [Maserati DRM](https://www.waves.com/plugins/maserati-drm) | Signature Series |
| [ ] | [Maserati GRP](https://www.waves.com/plugins/maserati-grp) | Signature Series |
| [ ] | [Maserati GTi](https://www.waves.com/plugins/maserati-gti) | Signature Series |
| [ ] | [Maserati HMX](https://www.waves.com/plugins/maserati-hmx) | Saturation & Distortion |
| [ ] | [Maserati VX1](https://www.waves.com/plugins/maserati-vx1) | Signature Series |
| [ ] | [MaxxBass](https://www.waves.com/plugins/maxxbass) | Bass |
| [ ] | [MaxxVolume](https://www.waves.com/plugins/maxxvolume) | Mastering & Limiters |
| [ ] | [MDMX Distortion Modules](https://www.waves.com/plugins/mdmx-distortion-modules) | Saturation & Distortion |
| [ ] | [MetaFilter](https://www.waves.com/plugins/metafilter) | Effects |
| [ ] | [MetaFlanger](https://www.waves.com/plugins/metaflanger) | Effects |
| [ ] | [MondoMod](https://www.waves.com/plugins/mondomod) | Effects |
| [ ] | [Morphoder](https://www.waves.com/plugins/morphoder) | Effects |
| [ ] | [MultiMod Rack](https://www.waves.com/plugins/multimod-rack) | Saturation & Distortion |
| [ ] | [MV2](https://www.waves.com/plugins/mv2) | Mastering & Limiters |
| [ ] | [MV360](https://www.waves.com/plugins/mv360) | Surround |
| [ ] | [NLS Non-Linear Summer](https://www.waves.com/plugins/nls-non-linear-summer) | Modeling |
| [ ] | [NS1 Noise Suppressor](https://www.waves.com/plugins/ns1-noise-suppressor) | Post Production |
| [ ] | [Nx Germano Studios New York](https://www.waves.com/plugins/nx-germano-studios-new-york) | Headphone Mixing |
| [ ] | [Nx Ocean Way Nashville](https://www.waves.com/plugins/nx-ocean-way-nashville) | Headphone Mixing |
| [ ] | [Nx – Virtual Mix Room over Headphones](https://www.waves.com/plugins/nx) | Stereo Imaging |
| [ ] | [OneKnob Brighter](https://www.waves.com/plugins/oneknob-brighter) | OneKnob |
| [ ] | [OneKnob Driver](https://www.waves.com/plugins/oneknob-driver) | Saturation & Distortion |
| [ ] | [OneKnob Filter](https://www.waves.com/plugins/oneknob-filter) | OneKnob |
| [ ] | [OneKnob Louder](https://www.waves.com/plugins/oneknob-louder) | OneKnob |
| [ ] | [OneKnob Phatter](https://www.waves.com/plugins/oneknob-phatter) | OneKnob |
| [ ] | [OneKnob Pressure](https://www.waves.com/plugins/oneknob-pressure) | OneKnob |
| [ ] | [OneKnob Pumper](https://www.waves.com/plugins/oneknob-pumper) | OneKnob |
| [ ] | [OneKnob Wetter](https://www.waves.com/plugins/oneknob-wetter) | OneKnob |
| [ ] | [OVox Vocal ReSynthesis](https://www.waves.com/plugins/ovox-vocal-resynthesis) | Vocals |
| [ ] | [PAZ Analyzer](https://www.waves.com/plugins/paz-analyzer) | Meters |
| [ ] | [Playlist Rider](https://www.waves.com/plugins/playlist-rider) | Live |
| [ ] | [Primary Source Expander](https://www.waves.com/plugins/pse-primary-source-expander) | Live |
| [ ] | [PRS SuperModels](https://www.waves.com/plugins/prs-supermodels) | Saturation & Distortion |
| [ ] | [PS22 Stereo Maker](https://www.waves.com/plugins/ps22-stereo-maker) | Stereo Imaging |
| [ ] | [PuigChild Compressor](https://www.waves.com/plugins/puigchild-compressor) | Modeling |
| [ ] | [PuigTec EQs](https://www.waves.com/plugins/puigtec-eqs) | Modeling |
| [ ] | [Q-Clone](https://www.waves.com/plugins/q-clone) | Modeling |
| [ ] | [Q10 Equalizer](https://www.waves.com/plugins/q10-equalizer) | Equalizers |
| [ ] | [R360 Surround Reverb](https://www.waves.com/plugins/r360-surround-reverb) | Surround |
| [ ] | [Renaissance Axx](https://www.waves.com/plugins/renaissance-axx) | Compressors |
| [ ] | [Renaissance Bass](https://www.waves.com/plugins/renaissance-bass) | Bass |
| [ ] | [Renaissance Channel](https://www.waves.com/plugins/renaissance-channel) | Compressors |
| [ ] | [Renaissance Compressor](https://www.waves.com/plugins/renaissance-compressor) | Compressors |
| [ ] | [Renaissance DeEsser](https://www.waves.com/plugins/renaissance-deesser) | Vocals |
| [ ] | [Renaissance Equalizer](https://www.waves.com/plugins/renaissance-equalizer) | Equalizers |
| [ ] | [Renaissance Reverb](https://www.waves.com/plugins/renaissance-reverb) | Reverb |
| [ ] | [Renaissance Vox](https://www.waves.com/plugins/renaissance-vox) | Vocals |
| [ ] | [Retro Fi](https://www.waves.com/plugins/retro-fi) | Effects |
| [ ] | [S1 Stereo Imager](https://www.waves.com/plugins/s1-stereo-imager) | Stereo Imaging |
| [ ] | [S360 Surround Imager & Panner](https://www.waves.com/plugins/s360-surround-imager-panner) | Surround |
| [ ] | [Scheps 73](https://www.waves.com/plugins/scheps-73) | Equalizers |
| [ ] | [Scheps Omni Channel 2](https://www.waves.com/plugins/scheps-omni-channel) | Channel Strips |
| [ ] | [Scheps Parallel Particles](https://www.waves.com/plugins/scheps-parallel-particles) | Signature Series |
| [ ] | [Sibilance](https://www.waves.com/plugins/sibilance) | Vocals |
| [ ] | [Silk Vocal](https://www.waves.com/plugins/silk-vocal) | Vocals |
| [ ] | [Smack Attack](https://www.waves.com/plugins/smack-attack-transient-shaper) | Compressors |
| [ ] | [SoundShifter](https://www.waves.com/plugins/soundshifter) | Pitch & Time |
| [ ] | [Space Rider](https://www.waves.com/plugins/space-rider) | Reverb |
| [ ] | [Spherix Immersive Compressor & Limiter](https://www.waves.com/plugins/spherix-immersive-compressor-limiter) | Surround |
| [ ] | [SSL E-Channel](https://www.waves.com/plugins/ssl-e-channel) | SSL |
| [ ] | [SSL EV2 Channel](https://www.waves.com/plugins/ssl-ev2-channel) | SSL |
| [ ] | [SSL G-Channel](https://www.waves.com/plugins/ssl-g-channel) | SSL |
| [ ] | [SSL G-Equalizer](https://www.waves.com/plugins/ssl-g-equalizer) | SSL |
| [ ] | [SSL G-Master Buss Compressor](https://www.waves.com/plugins/ssl-g-master-buss-compressor) | SSL |
| [ ] | [StudioVerse Audio Effects](https://www.waves.com/plugins/studioverse-audio-effects) | StudioVerse |
| [ ] | [StudioVerse Instruments](https://www.waves.com/plugins/studioverse-instruments) | StudioVerse |
| [ ] | [StudioVerse: Mix Unlock](https://www.waves.com/plugins/studioverse-mix-unlock) | StudioVerse |
| [ ] | [Sub Align](https://www.waves.com/plugins/sub-align) | Live |
| [ ] | [Submarine](https://www.waves.com/plugins/submarine) | Post Production |
| [ ] | [SuperTap](https://www.waves.com/plugins/supertap) | Delay |
| [ ] | [Sync Vx](https://www.waves.com/plugins/sync-vx) | Vocals |
| [ ] | [Torque](https://www.waves.com/plugins/torque) | Pitch & Time |
| [ ] | [TRACT SPL Meter](https://www.waves.com/plugins/tract-spl-meter) | Meters |
| [ ] | [TRACT System Calibration](https://www.waves.com/plugins/tract-system-calibration) | Live |
| [ ] | [Trans-X](https://www.waves.com/plugins/transx) | Compressors |
| [ ] | [TrueVerb](https://www.waves.com/plugins/trueverb) | Reverb |
| [ ] | [UltraPitch](https://www.waves.com/plugins/ultrapitch) | Pitch & Time |
| [ ] | [UM225 / UM226](https://www.waves.com/plugins/um225-um226) | Surround |
| [ ] | [V-Comp](https://www.waves.com/plugins/v-comp) | Modeling |
| [ ] | [V-EQ3](https://www.waves.com/plugins/v-eq3) | Modeling |
| [ ] | [V-EQ4](https://www.waves.com/plugins/v-eq4) | Modeling |
| [ ] | [Vitamin Sonic Enhancer](https://www.waves.com/plugins/vitamin) | Harmonics |
| [ ] | [Vocal Bender](https://www.waves.com/plugins/vocal-bender) | Vocals |
| [ ] | [Vocal Rider®](https://www.waves.com/plugins/vocal-rider) | Vocals |
| [ ] | [Voltage Amps](https://www.waves.com/plugins/voltage-amps) | Modeling |
| [ ] | [VU Meter](https://www.waves.com/plugins/vu-meter) | Meters |
| [ ] | [W43 Noise Reduction Plugin](https://www.waves.com/plugins/w43-noise-reduction-plugin) | Post Production |
| [ ] | [Waves Harmony](https://www.waves.com/plugins/waves-harmony) | Vocals |
| [ ] | [Waves Stream](https://www.waves.com/plugins/waves-stream) | Streaming |
| [ ] | [Waves Tune](https://www.waves.com/plugins/waves-tune) | Pitch & Time |
| [ ] | [Waves Tune LT](https://www.waves.com/plugins/waves-tune-lt) | Pitch & Time |
| [ ] | [Waves Tune Real-Time](https://www.waves.com/plugins/waves-tune-real-time) | Vocals |
| [ ] | [WLM Plus Loudness Meter](https://www.waves.com/plugins/wlm-loudness-meter) | Meters |
| [ ] | [WNS Noise Suppressor](https://www.waves.com/plugins/wns-noise-suppressor) | Post Production |
| [ ] | [X-Click](https://www.waves.com/plugins/x-click) | Post Production |
| [ ] | [X-Crackle](https://www.waves.com/plugins/x-crackle) | Post Production |
| [ ] | [X-FDBK](https://www.waves.com/plugins/x-fdbk) | Live |
| [ ] | [X-Hum](https://www.waves.com/plugins/x-hum) | Post Production |
| [ ] | [X-Noise](https://www.waves.com/plugins/x-noise) | Post Production |
| [ ] | [Z-Noise](https://www.waves.com/plugins/z-noise) | Post Production |

## Source

Plugin list scraped from the [Waves Mercury bundle page](https://www.waves.com/bundles/mercury) on 2026-10-28. Categories follow Waves' own grouping where available.
