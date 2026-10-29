# Implementation specifications

Behavioural specifications handed to the implementation side of the clean-room process
(see [docs/process.md](../docs/process.md)). Each revision is frozen; the implementation may use only these
documents, public references and its own work.

Specs live in `spec/<slug>/r<N>/SPEC.md`, where `<slug>` is the plugin's directory under `plugins/`.

| Slug | Revision | Target | Status | SHA-256 |
|---|---|---|---|---|
| `stereo-imager` | [r1](stereo-imager/r1/SPEC.md) | Stereo imager (Full / Imager / MS matrix) | Approved 2026-10-29 | dad6a104e00c7296284e86a984cf5c10ce8e29b2b2bb06d9ee9b969ec0955dcc |
| `de-esser` | [r1](de-esser/r1/SPEC.md) | De-esser (HP/BP sidechain, split/wideband, monitor) | Approved 2026-10-29 | db412694de49002fa324bbfe9253bd1951c4ae211289d937e4f2bf2cdea4c651 |
| `de-esser` | [r2](de-esser/r2/SPEC.md) | De-esser — r2: two-component release law, exact envelope vectors (supersedes r1) | Approved 2026-10-29 | 44818b32f75d1ca64fe55276e09c1610162557986ff9dcdfd4c31950b0c7e539 |
| `de-esser` | [r3](de-esser/r3/SPEC.md) | De-esser — r3: normative §5 pseudocode (slow-part charge rule, detector constants), table-point tolerances (supersedes r1, r2) | Approved 2026-10-29 | a6bdc14ef26db113f85c2e9ae4545458fc82d353e5a2e6ea8dfcdf1e0dba7ad8 |
