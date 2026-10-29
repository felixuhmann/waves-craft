# Implementation specifications

Behavioural specifications handed to the implementation side of the clean-room process
(see [docs/process.md](../docs/process.md)). Each revision is frozen; the implementation may use only these
documents, public references and its own work.

Specs live in `spec/<slug>/r<N>/SPEC.md`, where `<slug>` is the plugin's directory under `plugins/`.

| Slug | Revision | Target | Status | SHA-256 |
|---|---|---|---|---|
| `stereo-imager` | [r1](stereo-imager/r1/SPEC.md) | Stereo imager (Full / Imager / MS matrix) | Approved 2026-10-29 | dad6a104e00c7296284e86a984cf5c10ce8e29b2b2bb06d9ee9b969ec0955dcc |
| `flanger` | [r1](flanger/r1/SPEC.md) | Flanger (sine/triangle LFO, tape mode, feedback, FIR filter) | Approved 2026-10-29 | 44cd458b9febc279b66539c1cf390e2c48bc5ebfe1846dcc3e6612808aeab2b9 |
| `mastering-limiter` | [r1](mastering-limiter/r1/SPEC.md) | Mastering limiter (look-ahead, adaptive release, requantise, dither, noise shaping) | Approved 2026-10-29 | eef416035a3a776990da029f7884516b67f8ac367fbea1a83a7fe8641503b630 |
| `mastering-limiter` | [r2](mastering-limiter/r2/SPEC.md) | Mastering limiter — r2: full-scale rule, 88.2/96 kHz shaping tolerances, Type 2 constant-input rule | Approved 2026-10-29 | 56622bd27fd58279e1d866ad6925bccf218c4d29ff3b96dd2825859da3ae183b |
