# Third-party components

All dependencies are fetched at configure time by CMake `FetchContent`; none are vendored in this repository.

| Component | Version / pin | Source | Licence | Used for |
|---|---|---|---|---|
| JUCE | 8.0.16-security (zip, SHA-256 `7f75cee2…10f166`) | https://github.com/juce-framework/JUCE | AGPL-3.0 **or** commercial JUCE 8 licence (used here under AGPL-3.0) | plug-in framework, GUI, VST3 and Standalone wrappers |
| VST3 SDK (bundled inside JUCE) | as shipped with JUCE 8.0.16 | Steinberg, via JUCE | MIT | VST3 interfaces |
| clap-juce-extensions | commit `7adee3a1bd4684d4caa5601100e364abccff4b4d` (git) | https://github.com/free-audio/clap-juce-extensions | MIT | CLAP wrapper |
| CLAP SDK (submodule of clap-juce-extensions) | 1.2.7, commit `29ffcc27` | https://github.com/free-audio/clap | MIT | CLAP interfaces |
| clap-helpers (submodule of clap-juce-extensions) | commit `a61bcdf0` | https://github.com/free-audio/clap-helpers | MIT | CLAP wrapper helpers |
| Catch2 | v3.7.1 (zip, SHA-256 `7d771897…970d7`) | https://github.com/catchorg/Catch2 | BSL-1.0 | unit tests only (not shipped) |

Licence consequence: because JUCE is used under the AGPL-3.0, this project and the binaries it publishes are
distributed under the **GNU Affero General Public License v3.0** (see [LICENSE](LICENSE)). The MIT and BSL
components are compatible with that. Anyone redistributing the binaries must provide the corresponding source
(this repository at the tagged commit, plus the pinned dependencies above).

No third-party images, fonts, presets or other assets are included: all graphics are drawn in code and text
uses the platform's default sans-serif typeface.
