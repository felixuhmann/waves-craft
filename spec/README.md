# Implementation specifications

Behavioural specifications handed to the implementation side of the clean-room process
(see [docs/process.md](../docs/process.md)). Each revision is frozen; the implementation may use only these
documents, public references and its own work.

Specs live in `spec/<slug>/r<N>/SPEC.md`, where `<slug>` is the plugin's directory under `plugins/`.

| Slug | Revision | Target | Status | SHA-256 |
|---|---|---|---|---|
| `stereo-imager` | [r1](stereo-imager/r1/SPEC.md) | Stereo imager (Full / Imager / MS matrix) | Approved 2026-10-29 | dad6a104e00c7296284e86a984cf5c10ce8e29b2b2bb06d9ee9b969ec0955dcc |
| `color-delay` | [r1](color-delay/r1/SPEC.md) | Colour delay (delay, filters, compressor, reverb/distortion/doubler/phaser colours) | Approved 2026-10-29 | a07b05796212aee5ac905bf44f50d6613add25e8f7ffbdcbb64121840902eb55 |
| `color-delay` | [r2](color-delay/r2/SPEC.md) | Colour delay (supersedes r1: sync rounding, compressor high-level gains, per-channel effect routing, reverb/distortion/doubler targets) | Approved 2026-10-29 | 080593b053714469f8cbdeda2512fdb90d63cf1d2a596b6d86ebccca97d8bc76 |
