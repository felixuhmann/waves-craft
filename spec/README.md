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
