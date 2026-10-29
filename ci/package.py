#!/usr/bin/env python3
"""Zip the built plug-ins: one zip per plug-in and one combined zip per platform.

Usage: package.py <build dir> <version> <platform tag> <output dir>
"""
import pathlib
import sys
import zipfile

PLUGINS = [
    ("CraftShuffler", "Craft Shuffler", "craft-shuffler"),
    ("CraftImager", "Craft Imager", "craft-imager"),
    ("CraftMsMatrix", "Craft MS Matrix", "craft-ms-matrix"),
]


def add_path(zf: zipfile.ZipFile, src: pathlib.Path, arc_prefix: str) -> None:
    if src.is_dir():
        for f in sorted(src.rglob("*")):
            if f.is_file():
                zf.write(f, f"{arc_prefix}/{src.name}/{f.relative_to(src).as_posix()}")
    else:
        zf.write(src, f"{arc_prefix}/{src.name}")


def main() -> int:
    build, version, platform, out = sys.argv[1:5]
    build_dir, out_dir = pathlib.Path(build), pathlib.Path(out)
    out_dir.mkdir(parents=True, exist_ok=True)
    combined = out_dir / f"craft-imager-bundle-{version}-{platform}.zip"
    with zipfile.ZipFile(combined, "w", zipfile.ZIP_DEFLATED) as all_zip:
        for target, name, slug in PLUGINS:
            art = build_dir / f"{target}_artefacts" / "Release"
            items = [("VST3", art / "VST3" / f"{name}.vst3"), ("CLAP", art / "CLAP" / f"{name}.clap")]
            for _, path in items:
                if not path.exists():
                    print(f"missing artefact: {path}", file=sys.stderr)
                    return 1
            single = out_dir / f"{slug}-{version}-{platform}.zip"
            with zipfile.ZipFile(single, "w", zipfile.ZIP_DEFLATED) as zf:
                for fmt, path in items:
                    add_path(zf, path, fmt)
                    add_path(all_zip, path, fmt)
            print(f"wrote {single}")
    for extra in ("LICENSE", "THIRD_PARTY.md"):
        p = pathlib.Path(extra)
        if p.exists():
            with zipfile.ZipFile(combined, "a", zipfile.ZIP_DEFLATED) as zf:
                zf.write(p, p.name)
    print(f"wrote {combined}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
