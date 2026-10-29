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
| `de-esser` | [r1](de-esser/r1/SPEC.md) | De-esser (HP/BP sidechain, split/wideband, monitor) | Approved 2026-10-29 | db412694de49002fa324bbfe9253bd1951c4ae211289d937e4f2bf2cdea4c651 |
| `de-esser` | [r2](de-esser/r2/SPEC.md) | De-esser — r2: two-component release law, exact envelope vectors (supersedes r1) | Approved 2026-10-29 | 44818b32f75d1ca64fe55276e09c1610162557986ff9dcdfd4c31950b0c7e539 |
| `de-esser` | [r3](de-esser/r3/SPEC.md) | De-esser — r3: normative §5 pseudocode (slow-part charge rule, detector constants), table-point tolerances (supersedes r1, r2) | Approved 2026-10-29 | a6bdc14ef26db113f85c2e9ae4545458fc82d353e5a2e6ea8dfcdf1e0dba7ad8 |
| `doubler` | [r1](doubler/r1/SPEC.md) | Voice doubler (2/4 voices, pitch shift, delay, pan, EQ, feedback, LFO) | Approved 2026-10-29 | 8ed06647b921399dd5507b4bbf0e08683b7cafd34899e0761d34ec9b7fd32caa |
| `peak-limiter` | [r1](peak-limiter/r1/SPEC.md) | Look-ahead brickwall peak limiter (fixed/auto release) | Approved 2026-10-29 | b25832e96a22a42a1b3040c01bc6a4a69f981f6e6d3425414143d5cf8f96778c |
| `flanger` | [r1](flanger/r1/SPEC.md) | Flanger (sine/triangle LFO, tape mode, feedback, FIR filter) | Approved 2026-10-29 | 44cd458b9febc279b66539c1cf390e2c48bc5ebfe1846dcc3e6612808aeab2b9 |
| `level-rider` | [r1](level-rider/r1/SPEC.md) | Automatic vocal level rider (static law, ballistics, activity gate, idle, music sidechain) | Approved 2026-10-29 | 7479a9b2d903ca102e03d82c13787132f230e28c062ea855cfb32a967dc04212 |
