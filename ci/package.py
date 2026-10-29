#!/usr/bin/env python3
"""Zip every built plug-in: one zip per plug-in and one combined Craft Audio bundle per platform.

Usage: package.py <build dir> <version> <platform tag> <output dir> [config]

The build tree is searched for JUCE `<Target>_artefacts/<config>/` directories (config defaults to Release),
so plug-ins added under plugins/<slug>/ are packaged without touching this script. A target is packaged when
it produced at least one of VST3 (`*.vst3` bundle), CLAP (`*.clap`) or Standalone; console tools such as
JUCE's juceaide or snapshot helpers have none of these and are skipped.

Output: <product-slug>-<version>-<platform>.zip for each product (e.g. craft-shuffler-0.1.7-linux-x64.zip)
and craft-audio-bundle-<version>-<platform>.zip with everything. Archive layout: VST3/, CLAP/, Standalone/,
plus LICENSE and THIRD_PARTY.md from the repository root.
"""
import pathlib
import re
import sys
import zipfile

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
EXTRAS = ("LICENSE", "THIRD_PARTY.md")
# Build by-products that are not part of a distributable standalone app.
SKIP_SUFFIXES = {".pdb", ".lib", ".exp", ".ilk", ".a"}


def slugify(name: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")


def add_path(zf: zipfile.ZipFile, src: pathlib.Path, arc_prefix: str) -> None:
    if src.is_dir():
        for f in sorted(src.rglob("*")):
            if f.is_file():
                zf.write(f, f"{arc_prefix}/{src.name}/{f.relative_to(src).as_posix()}")
    else:
        zf.write(src, f"{arc_prefix}/{src.name}")


def find_products(build_dir: pathlib.Path, config: str):
    """Return {product name: [(format, path), ...]} for every plug-in artefact directory in the build tree."""
    products = {}
    for art_root in sorted(build_dir.rglob("*_artefacts")):
        if not art_root.is_dir():
            continue
        art = art_root / config
        if not art.is_dir():
            continue
        items = []
        vst3 = art / "VST3"
        if vst3.is_dir():
            items += [("VST3", p) for p in sorted(vst3.glob("*.vst3"))]
        clap = art / "CLAP"
        if clap.is_dir():
            items += [("CLAP", p) for p in sorted(clap.glob("*.clap"))]
        standalone = art / "Standalone"
        if standalone.is_dir():
            items += [("Standalone", p) for p in sorted(standalone.iterdir())
                      if p.suffix.lower() not in SKIP_SUFFIXES]
        if not items:
            continue
        names = {p.stem if p.suffix else p.name for _, p in items}
        if len(names) != 1:
            raise SystemExit(f"{art}: artefacts disagree on the product name: {sorted(names)}")
        name = names.pop()
        if name in products:
            raise SystemExit(f"duplicate product name '{name}' (second copy in {art})")
        products[name] = items
        print(f"found {name}: " + ", ".join(f"{fmt} {p.name}" for fmt, p in items))
    return products


def main() -> int:
    if len(sys.argv) not in (5, 6):
        print(__doc__, file=sys.stderr)
        return 2
    build, version, platform, out = sys.argv[1:5]
    config = sys.argv[5] if len(sys.argv) == 6 else "Release"
    build_dir, out_dir = pathlib.Path(build), pathlib.Path(out)

    products = find_products(build_dir, config)
    if not products:
        print(f"no plug-in artefacts found under {build_dir} (config {config})", file=sys.stderr)
        return 1

    extras = [REPO_ROOT / e for e in EXTRAS if (REPO_ROOT / e).exists()]
    out_dir.mkdir(parents=True, exist_ok=True)
    combined = out_dir / f"craft-audio-bundle-{version}-{platform}.zip"
    with zipfile.ZipFile(combined, "w", zipfile.ZIP_DEFLATED) as all_zip:
        for name in sorted(products):
            single = out_dir / f"{slugify(name)}-{version}-{platform}.zip"
            with zipfile.ZipFile(single, "w", zipfile.ZIP_DEFLATED) as zf:
                for fmt, path in products[name]:
                    add_path(zf, path, fmt)
                    add_path(all_zip, path, fmt)
                for e in extras:
                    zf.write(e, e.name)
            print(f"wrote {single}")
        for e in extras:
            all_zip.write(e, e.name)
    print(f"wrote {combined} ({len(products)} plug-ins)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
