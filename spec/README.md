# Implementation specifications

Behavioural specifications handed to the implementation side of the clean-room process
(see [docs/process.md](../docs/process.md)). Each revision is frozen; the implementation may use only these
documents, public references and its own work.

Specs live in `spec/<slug>/r<N>/SPEC.md`, where `<slug>` is the plugin's directory under `plugins/`.

| Slug | Revision | Target | Status | SHA-256 |
|---|---|---|---|---|
| `stereo-imager` | [r1](stereo-imager/r1/SPEC.md) | Stereo imager (Full / Imager / MS matrix) | Approved 2026-10-29 | dad6a104e00c7296284e86a984cf5c10ce8e29b2b2bb06d9ee9b969ec0955dcc |
| `peak-limiter` | [r1](peak-limiter/r1/SPEC.md) | Look-ahead brickwall peak limiter (fixed/auto release) | Approved 2026-10-29 | b25832e96a22a42a1b3040c01bc6a4a69f981f6e6d3425414143d5cf8f96778c |
| `flanger` | [r1](flanger/r1/SPEC.md) | Flanger (sine/triangle LFO, tape mode, feedback, FIR filter) | Approved 2026-10-29 | 44cd458b9febc279b66539c1cf390e2c48bc5ebfe1846dcc3e6612808aeab2b9 |
