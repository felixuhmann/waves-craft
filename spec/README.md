# Implementation specifications

Behavioural specifications handed to the implementation side of the clean-room process
(see [docs/process.md](../docs/process.md)). Each revision is frozen; the implementation may use only these
documents, public references and its own work.

Specs live in `spec/<slug>/r<N>/SPEC.md`, where `<slug>` is the plugin's directory under `plugins/`.

| Slug | Revision | Target | Status | SHA-256 |
|---|---|---|---|---|
| `stereo-imager` | [r1](stereo-imager/r1/SPEC.md) | Stereo imager (Full / Imager / MS matrix) | Approved 2026-10-29 | dad6a104e00c7296284e86a984cf5c10ce8e29b2b2bb06d9ee9b969ec0955dcc |
| `motion-filter` | [r1](motion-filter/r1/SPEC.md) | Modulated multimode filter (LFO/sequencer/follower, comb, delay, drive, crush) | Approved 2026-10-29 | 4fd105a9f069561bb815467763eb5ffbc1f9feeb4a589d55ee386fd3aeee92cc |
| `motion-filter` | [r2](motion-filter/r2/SPEC.md) | Modulated multimode filter — r2: BP gain, Q above Res 90, follower law, crush law | Approved 2026-10-29 | 764676474a100daff4f779bccc2d69a3aa091b457b7224ea1db520462c5edaf8 |
| `h-delay` | [r1](h-delay/r1/SPEC.md) | Hybrid delay (stereo, ping-pong, filters, modulation, analog modes) | Approved 2026-10-29 | 2ceea26722543f5a180d1de71ff1e563f960d174db7bc779fdc6ba4fd76922ba |
| `h-delay` | [r2](h-delay/r2/SPEC.md) | Hybrid delay — supersedes r1 (envelope saturation, short-delay loss, analog noise in loop, filter end points, modulation test vectors) | Approved 2026-10-29 | 80f750585e68503ab9ccbcece8f81ace88ee50b310d331cee29cc311efc9e67a |
| `h-delay` | [r3](h-delay/r3/SPEC.md) | Hybrid delay — supersedes r2 (analog colouring once on the wet output, click test vectors) | Approved 2026-10-29 | 4c366d873f74fcb3199ea7f8eb9db3e4d4a9cff398988a38ffa540cf3b61f2ba |
| `de-esser` | [r1](de-esser/r1/SPEC.md) | De-esser (HP/BP sidechain, split/wideband, monitor) | Approved 2026-10-29 | db412694de49002fa324bbfe9253bd1951c4ae211289d937e4f2bf2cdea4c651 |
| `de-esser` | [r2](de-esser/r2/SPEC.md) | De-esser — r2: two-component release law, exact envelope vectors (supersedes r1) | Approved 2026-10-29 | 44818b32f75d1ca64fe55276e09c1610162557986ff9dcdfd4c31950b0c7e539 |
| `de-esser` | [r3](de-esser/r3/SPEC.md) | De-esser — r3: normative §5 pseudocode (slow-part charge rule, detector constants), table-point tolerances (supersedes r1, r2) | Approved 2026-10-29 | a6bdc14ef26db113f85c2e9ae4545458fc82d353e5a2e6ea8dfcdf1e0dba7ad8 |
| `doubler` | [r1](doubler/r1/SPEC.md) | Voice doubler (2/4 voices, pitch shift, delay, pan, EQ, feedback, LFO) | Approved 2026-10-29 | 8ed06647b921399dd5507b4bbf0e08683b7cafd34899e0761d34ec9b7fd32caa |
| `peak-limiter` | [r1](peak-limiter/r1/SPEC.md) | Look-ahead brickwall peak limiter (fixed/auto release) | Approved 2026-10-29 | b25832e96a22a42a1b3040c01bc6a4a69f981f6e6d3425414143d5cf8f96778c |
| `flanger` | [r1](flanger/r1/SPEC.md) | Flanger (sine/triangle LFO, tape mode, feedback, FIR filter) | Approved 2026-10-29 | 44cd458b9febc279b66539c1cf390e2c48bc5ebfe1846dcc3e6612808aeab2b9 |
| `vocal-compressor` | [r1](vocal-compressor/r1/SPEC.md) | One-knob vocal compressor with gate (look-ahead) | Approved 2026-10-29 | 1cb058153a6908b1ce800de8a9ee4463f5bee39a2a7fc8fdd9ba097ca21a4a16 |
| `vocal-compressor` | [r2](vocal-compressor/r2/SPEC.md) | One-knob vocal compressor with gate (supersedes r1: gate release law, release patterns, ripple) | Approved 2026-10-29 | c7e1616760938f7ccd120295390fcc9b31d3426aa7f0a33eb4115fbc5b0938ed |
| `vocal-compressor` | [r3](vocal-compressor/r3/SPEC.md) | One-knob vocal compressor with gate (supersedes r2: ripple measurement conditions) | Approved 2026-10-29 | 3462e6788439792abd73da35171be65f24403a707a94a9fc8d898ba5bc72da34 |
| `dynamics` | [r1](dynamics/r1/SPEC.md) | Compressor / gate / expander with sidechain filter module (4 variants) | Approved 2026-10-29 | b2f51f841c1dd2a14d8217eb00beb6a43ab8c46b832c4892db03a45a41c7ac6a |
| `dynamics` | [r2](dynamics/r2/SPEC.md) | Compressor / gate / expander — r2: rectified detector, envelope law + trajectory tables, expander dynamics, allpass sign, monitor, PeakRef output limit | Approved 2026-10-29 (supersedes r1) | cdcc7df3d211b79fb051e97773081fbe599eb8ced69e2bb080c5e206fd36a47e |
| `dynamics` | [r3](dynamics/r3/SPEC.md) | Compressor / gate / expander — r3: hard output clip at 0 dBFS (unclipped expansion curves), expander calibration constant, reset state | Approved 2026-10-29 (supersedes r2) | a5b254fe01ad341ccb67f06ed3203a46c4607e4726a6bc304b96ff322af56ffb |
| `mastering-limiter` | [r1](mastering-limiter/r1/SPEC.md) | Mastering limiter (look-ahead, adaptive release, requantise, dither, noise shaping) | Approved 2026-10-29 | eef416035a3a776990da029f7884516b67f8ac367fbea1a83a7fe8641503b630 |
| `mastering-limiter` | [r2](mastering-limiter/r2/SPEC.md) | Mastering limiter — r2: full-scale rule, 88.2/96 kHz shaping tolerances, Type 2 constant-input rule | Approved 2026-10-29 | 56622bd27fd58279e1d866ad6925bccf218c4d29ff3b96dd2825859da3ae183b |
