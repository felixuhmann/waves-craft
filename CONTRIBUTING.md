# Contributing a plugin

Each Craft Audio plugin is self-contained under `plugins/<slug>/`, so several people can add plugins in
parallel without touching shared files. The root `CMakeLists.txt` adds every `plugins/*/CMakeLists.txt`
automatically (sorted), CI builds and tests all of them on Windows (MSVC) and Ubuntu 22.04, and
`ci/package.py` packages every built VST3 / CLAP / Standalone it finds.

Follow the clean-room process in [docs/process.md](docs/process.md): build only from an approved spec.

## Layout

```
plugins/<slug>/
  CMakeLists.txt           targets of this plugin (no FetchContent, no project-wide settings)
  source/                  DSP core (ideally host-independent) + JUCE processor/editor
  tests/                   unit tests (Catch2), registered with CTest
  IMPLEMENTATION_NOTES.md  architecture, spec mapping, tests, assumptions, status
spec/<slug>/r<N>/SPEC.md   approved spec (add a row to spec/README.md)
docs/verification/<slug>-r<N>.md   independent verification report
```

`<slug>` is lower-case kebab-case and names the product family, e.g. `stereo-imager`. Use
[`plugins/stereo-imager`](plugins/stereo-imager) as the reference.

## CMakeLists.txt rules

- JUCE 8, clap-juce-extensions and Catch2 are already available (fetched once in the root); do not fetch them
  again. A new third-party dependency is the one case that needs a root change; discuss it first.
- Wrap plugin targets in `if (CRAFT_BUILD_PLUGINS)`, tests in `if (CRAFT_BUILD_TESTS)`, optional dev tools in
  their own option (e.g. `CRAFT_BUILD_SNAPSHOT`).
- CMake target, function and cache-variable names are global: prefix them with the slug
  (`stereo_imager_dsp`, `stereo_imager_add_variant`, …). Paths must be relative to `CMAKE_CURRENT_SOURCE_DIR`.
- Tests: `include (Catch)` and `catch_discover_tests (<slug>_tests TEST_PREFIX "<slug>/")`. The root calls
  `enable_testing()`, so `ctest --test-dir build` at the top level runs every plugin's tests.

## Plugin identity

| Field | Value |
|---|---|
| `COMPANY_NAME` | `"Craft Audio"` |
| `PLUGIN_MANUFACTURER_CODE` | `Crft` |
| `PLUGIN_CODE` | 4 characters, at least one upper-case, **unique across the repository** — `grep -r PLUGIN_CODE plugins/` and also check the codes passed to helper functions (stereo-imager uses `CShf`, `CImg`, `CMsm`) |
| `PRODUCT_NAME` | `"Craft <Name>"` — unique; it also becomes the zip name (`craft-<name>-<version>-<platform>.zip`) |
| `BUNDLE_ID` | `audio.craft.<PLUGIN_CODE>` |
| CLAP id | `com.craftaudio.<slug>` (one product), or `com.craftaudio.<slug>.<variant>` for several products in one slug. (The stereo-imager products predate this rule and keep their released ids `com.craftaudio.shuffler` / `.imager` / `.msmatrix`, which must not be reused.) |
| `FORMATS` | `VST3 Standalone` plus `clap_juce_extensions_plugin()` for CLAP |

Never use vendor or third-party trademarks in product names, parameter names, GUI text, file names or
identifiers (no Waves, SSL, CLA, Abbey Road, …). Ship under our own `Craft …` names only.

## Checklist for a pull request

- [ ] `plugins/<slug>/` with `CMakeLists.txt`, `source/`, `tests/`, `IMPLEMENTATION_NOTES.md`
- [ ] Unique plugin codes, product names and CLAP ids; no vendor trademarks
- [ ] `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build && ctest --test-dir build`
      passes from the repository root (CI checks Windows and Linux)
- [ ] Spec at `spec/<slug>/r<N>/SPEC.md` with its row in `spec/README.md`
- [ ] README implementation-queue row ticked once released
