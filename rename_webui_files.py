"""
Prepare the Vite output for both firmware embedding and the standalone WWW image.

Only the current New Design is packaged. The retired UI and the historical
`alt/webui` build are deliberately excluded.
"""
from pathlib import Path
import gzip
import json
import shutil

try:
    Import("env")
except NameError:
    pass


def gzip_file(source: Path, target: Path) -> None:
    with source.open("rb") as f_in:
        with gzip.open(target, "wb") as f_out:
            shutil.copyfileobj(f_in, f_out)


def build_spiffs_source(dist_dir: Path) -> None:
    """Create a minimal source tree for spiffs_create_partition_image()."""
    image_dir = Path("webui/spiffs_image")
    if image_dir.exists():
        shutil.rmtree(image_dir)
    image_dir.mkdir(parents=True)

    mandatory_assets = [
        "index.html.gz",
        "main.js.gz",
        "main.css.gz",
    ]
    optional_assets = [
        "favicon.ico.gz",
        "manifest.webmanifest.gz",
    ]

    # icon-256.png.gz is intentionally NOT copied to the 320 KiB WWW image. It
    # is comparatively large and remains available through the embedded New
    # Design fallback handler. This leaves enough SPIFFS headroom for future JS
    # and CSS growth without changing the existing 4 MB partition table.

    missing = [name for name in mandatory_assets if not (dist_dir / name).is_file()]
    if missing:
        raise RuntimeError(
            "Cannot build standalone New Design image; missing: "
            + ", ".join(missing)
        )

    copied = []
    for name in mandatory_assets + optional_assets:
        source = dist_dir / name
        if source.is_file():
            shutil.copy2(source, image_dir / name)
            copied.append(name)

    package_file = Path("webui/package.json")
    package = json.loads(package_file.read_text(encoding="utf-8"))
    version = str(package.get("version", "unknown")).strip() or "unknown"

    (image_dir / "webui-version.txt").write_text(
        version + "\n", encoding="utf-8"
    )
    (image_dir / "webui-manifest.json").write_text(
        json.dumps(
            {
                "format": 1,
                "product": "HB-RF-ETH-ng",
                "design": "newdesign",
                "version": version,
                "assets": copied,
                "embeddedFallbackAssets": ["icon-256.png.gz"],
            },
            separators=(",", ":"),
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )

    total_size = sum(path.stat().st_size for path in image_dir.iterdir())
    partition_size = 0x50000
    if total_size >= partition_size:
        raise RuntimeError(
            f"New Design assets use {total_size} bytes and exceed the "
            f"{partition_size}-byte SPIFFS partition before filesystem overhead"
        )

    print(
        f"Prepared standalone New Design image source: {len(copied)} assets, "
        f"version {version}, {total_size}/{partition_size} bytes before SPIFFS overhead"
    )


def rename_webui_files():
    """Rename Vite output files to the stable names expected by the firmware."""
    dist_dir = Path("webui/dist")

    if not dist_dir.exists():
        print("WARNING: webui/dist directory not found")
        return

    # Mapping of patterns to target filenames (Vite uses index-[hash].js/css).
    renames = [
        ("index-*.js.gz", "main.js.gz"),
        ("index-*.css.gz", "main.css.gz"),
        # Fallback for historical Parcel naming.
        ("webui.*.js.gz", "main.js.gz"),
        ("webui.*.css.gz", "main.css.gz"),
    ]

    replacements = {}

    for pattern, target in renames:
        matches = list(dist_dir.glob(pattern))
        if not matches:
            continue

        source = matches[0]
        dest = dist_dir / target
        old_name = source.name.replace(".gz", "")
        new_name = target.replace(".gz", "")
        replacements[old_name] = new_name

        if dest.exists():
            dest.unlink()

        shutil.move(str(source), str(dest))
        print(f"Renamed {source.name} -> {target}")

    # Update index.html and regenerate its compressed copy.
    index_html = dist_dir / "index.html"
    index_html_gz = dist_dir / "index.html.gz"

    if index_html.exists():
        html_content = index_html.read_text(encoding="utf-8")

        for old_name, new_name in replacements.items():
            html_content = html_content.replace(old_name, new_name)
            print(f"Replaced {old_name} -> {new_name} in index.html")

        import re

        favicon_pattern = re.compile(r"(?:index|favicon)-[A-Za-z0-9_-]+\.ico")
        for old_favicon in favicon_pattern.findall(html_content):
            html_content = html_content.replace(old_favicon, "favicon.ico")
            print(f"Replaced {old_favicon} -> favicon.ico in index.html")

        index_html.write_text(html_content, encoding="utf-8")
        print("Updated index.html with new filenames")

        with gzip.open(index_html_gz, "wt", encoding="utf-8") as f_out:
            f_out.write(html_content)
        print("Created index.html.gz")

    # Favicon: copy the canonical file and compress it.
    webui_favicon = Path("webui/favicon.ico")
    dist_favicon = dist_dir / "favicon.ico"
    favicon_gz = dist_dir / "favicon.ico.gz"

    for pattern in ["favicon.*.ico", "index-*.ico"]:
        for file in dist_dir.glob(pattern):
            file.unlink()
            print(f"Removed {file.name}")

    if webui_favicon.exists():
        if not dist_favicon.exists():
            shutil.copy2(webui_favicon, dist_favicon)
            print("Copied favicon.ico to dist")
        if dist_favicon.exists():
            gzip_file(dist_favicon, favicon_gz)
            print("Created favicon.ico.gz")
    else:
        print("WARNING: webui/favicon.ico not found")

    # Some Vite configurations emit uncompressed JS/CSS. Ensure compressed
    # stable-name variants exist for both embedding and SPIFFS packaging.
    for filename in ["main.js", "main.css"]:
        source = dist_dir / filename
        target = dist_dir / f"{filename}.gz"
        if source.exists():
            gzip_file(source, target)
            print(f"Created {target.name}")
        elif not target.exists():
            print(f"WARNING: {filename} not found in dist")

    # PWA assets remain embedded in the transition firmware. Only the compact
    # manifest is also copied into the standalone SPIFFS source above.
    for filename in ["manifest.webmanifest", "icon-256.png"]:
        source = dist_dir / filename
        target = dist_dir / f"{filename}.gz"
        if source.exists():
            gzip_file(source, target)
            print(f"Created {target.name}")
        else:
            print(f"WARNING: PWA asset {filename} not found in dist")

    build_spiffs_source(dist_dir)


rename_webui_files()
