# Implementation specifications

Behavioural specifications handed to the implementation side of the clean-room process
(see [docs/process.md](../docs/process.md)). Each revision is frozen; the implementation may use only these
documents, public references and its own work.

Specs live in `spec/<slug>/r<N>/SPEC.md`, where `<slug>` is the plugin's directory under `plugins/`.

| Slug | Revision | Target | Status | SHA-256 |
|---|---|---|---|---|
| `stereo-imager` | [r1](stereo-imager/r1/SPEC.md) | Stereo imager (Full / Imager / MS matrix) | Approved 2026-10-29 | dad6a104e00c7296284e86a984cf5c10ce8e29b2b2bb06d9ee9b969ec0955dcc |
| `h-delay` | [r1](h-delay/r1/SPEC.md) | Hybrid delay (stereo, ping-pong, filters, modulation, analog modes) | Approved 2026-10-29 | 2ceea26722543f5a180d1de71ff1e563f960d174db7bc779fdc6ba4fd76922ba |
| `h-delay` | [r2](h-delay/r2/SPEC.md) | Hybrid delay — supersedes r1 (envelope saturation, short-delay loss, analog noise in loop, filter end points, modulation test vectors) | Approved 2026-10-29 | 80f750585e68503ab9ccbcece8f81ace88ee50b310d331cee29cc311efc9e67a |
| `h-delay` | [r3](h-delay/r3/SPEC.md) | Hybrid delay — supersedes r2 (analog colouring once on the wet output, click test vectors) | Approved 2026-10-29 | 4c366d873f74fcb3199ea7f8eb9db3e4d4a9cff398988a38ffa540cf3b61f2ba |
