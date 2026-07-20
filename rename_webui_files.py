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
        with gzip.open(target, "wb", compresslevel=9) as f_out:
            shutil.copyfileobj(f_in, f_out)


def build_spiffs_source(dist_dir: Path) -> None:
    """Create the minimal source tree for spiffs_create_partition_image()."""
    image_dir = Path("webui/spiffs_image")
    if image_dir.exists():
        shutil.rmtree(image_dir)
    image_dir.mkdir(parents=True)

    # The full single-file Vue application is too large for the existing
    # 0x50000-byte partition when stored as gzip. Vite already creates Brotli
    # variants through vite-plugin-compression2; modern browsers decode them
    # directly via Content-Encoding: br. HTML stays gzip because it is tiny and
    # must be regenerated after the cache-busting plugin rewrites its URLs.
    mandatory_assets = [
        "index.html.gz",
        "main.js.br",
        "main.css.br",
    ]
    optional_assets = [
        "favicon.ico.gz",
        "manifest.webmanifest.gz",
    ]

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
                "encodings": {
                    "index.html": "gzip",
                    "main.js": "br",
                    "main.css": "br",
                },
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
    # Reserve room for SPIFFS metadata/pages rather than merely comparing raw
    # bytes with the partition boundary.
    safety_margin = 12 * 1024
    if total_size > partition_size - safety_margin:
        raise RuntimeError(
            f"New Design assets use {total_size} bytes; maximum with SPIFFS "
            f"headroom is {partition_size - safety_margin} bytes"
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

    # Compatibility with historical Vite/Parcel hashed output. Current Vite
    # emits stable main.js/main.css names directly.
    renames = [
        ("index-*.js.gz", "main.js.gz"),
        ("index-*.css.gz", "main.css.gz"),
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

    # Update index.html and regenerate its compressed copy after all Vite
    # plugins have finished rewriting it.
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

        with gzip.open(index_html_gz, "wt", encoding="utf-8", compresslevel=9) as f_out:
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

    # Ensure gzip variants remain available to the embedded transition UI.
    for filename in ["main.js", "main.css"]:
        source = dist_dir / filename
        target = dist_dir / f"{filename}.gz"
        if source.exists():
            gzip_file(source, target)
            print(f"Created {target.name}")
        elif not target.exists():
            print(f"WARNING: {filename} not found in dist")

    # PWA assets remain embedded. The compact manifest also enters SPIFFS; the
    # larger PNG icon is served by the embedded fallback route.
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
