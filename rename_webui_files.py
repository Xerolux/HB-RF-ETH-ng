"""
Prepare the Vite output for firmware embedding and the standalone New Design
WWW image.

Only the current New Design is packaged. The retired UI and `alt/webui` are
never included.
"""

from pathlib import Path
import gzip
import json
import re
import shutil

try:
    Import("env")
except NameError:
    pass


PARTITION_SIZE = 0x50000
SPIFFS_HEADROOM = 24 * 1024


def gzip_file(source: Path, target: Path) -> None:
    with source.open("rb") as source_file:
        with gzip.open(target, "wb", compresslevel=9) as target_file:
            shutil.copyfileobj(source_file, target_file)


def load_release_metadata() -> tuple[str, int, str]:
    package = json.loads(Path("webui/package.json").read_text(encoding="utf-8"))
    compatibility = json.loads(
        Path("webui/compatibility.json").read_text(encoding="utf-8")
    )

    version = str(package.get("version", "")).strip()
    api_version = compatibility.get("apiVersion")
    minimum_firmware = str(compatibility.get("minFirmwareVersion", "")).strip()

    if not re.fullmatch(r"\d+\.\d+\.\d+(?:-[A-Za-z]+\.\d+)?", version):
        raise RuntimeError(f"Invalid WebUI version: {version}")
    if not isinstance(api_version, int) or api_version < 1:
        raise RuntimeError(f"Invalid WebUI API version: {api_version}")
    if not re.fullmatch(
        r"\d+\.\d+\.\d+(?:-[A-Za-z]+\.\d+)?", minimum_firmware
    ):
        raise RuntimeError(
            f"Invalid minimum firmware version: {minimum_firmware}"
        )

    return version, api_version, minimum_firmware


def build_spiffs_source(dist_dir: Path) -> None:
    """Create the smallest complete standalone New Design image source."""
    image_dir = Path("webui/spiffs_image")
    if image_dir.exists():
        shutil.rmtree(image_dir)
    image_dir.mkdir(parents=True)

    assets = [
        "index.html.gz",
        "main.js.br",
        "main.css.br",
    ]

    missing = [name for name in assets if not (dist_dir / name).is_file()]
    if missing:
        raise RuntimeError(
            "Cannot build standalone New Design image; missing: "
            + ", ".join(missing)
        )

    for name in assets:
        shutil.copy2(dist_dir / name, image_dir / name)

    version, api_version, minimum_firmware = load_release_metadata()

    manifest = {
        "format": 1,
        "product": "HB-RF-ETH-ng",
        "design": "newdesign",
        "version": version,
        "apiVersion": api_version,
        "minFirmwareVersion": minimum_firmware,
        "assets": assets,
        "encodings": {
            "index.html": "gzip",
            "main.js": "br",
            "main.css": "br",
        },
        "embeddedFallbackAssets": [
            "favicon.ico.gz",
            "manifest.webmanifest.gz",
            "icon-256.png.gz",
        ],
    }
    (image_dir / "webui-manifest.json").write_text(
        json.dumps(manifest, separators=(",", ":"), sort_keys=True) + "\n",
        encoding="utf-8",
    )

    files = sorted(path for path in image_dir.iterdir() if path.is_file())
    total_size = sum(path.stat().st_size for path in files)
    maximum_payload = PARTITION_SIZE - SPIFFS_HEADROOM
    if total_size > maximum_payload:
        details = ", ".join(f"{path.name}={path.stat().st_size}" for path in files)
        raise RuntimeError(
            f"New Design payload uses {total_size} bytes; maximum with SPIFFS "
            f"headroom is {maximum_payload} bytes ({details})"
        )

    details = ", ".join(f"{path.name}={path.stat().st_size}" for path in files)
    print(
        f"Prepared standalone New Design image: version {version}, API "
        f"{api_version}, firmware >= {minimum_firmware}, "
        f"{total_size}/{PARTITION_SIZE} raw bytes ({details})"
    )


def rename_legacy_outputs(dist_dir: Path) -> dict[str, str]:
    """Normalize historical Vite/Parcel hashed gzip names when encountered."""
    renames = [
        ("index-*.js.gz", "main.js.gz"),
        ("index-*.css.gz", "main.css.gz"),
        ("webui.*.js.gz", "main.js.gz"),
        ("webui.*.css.gz", "main.css.gz"),
    ]
    replacements: dict[str, str] = {}

    for pattern, target_name in renames:
        matches = list(dist_dir.glob(pattern))
        if not matches:
            continue
        source = matches[0]
        target = dist_dir / target_name
        replacements[source.name.removesuffix(".gz")] = target_name.removesuffix(".gz")
        target.unlink(missing_ok=True)
        shutil.move(str(source), str(target))
        print(f"Renamed {source.name} -> {target_name}")

    return replacements


def prepare_index(dist_dir: Path, replacements: dict[str, str]) -> None:
    index_html = dist_dir / "index.html"
    if not index_html.exists():
        raise RuntimeError("webui/dist/index.html is missing")

    html = index_html.read_text(encoding="utf-8")
    for old_name, new_name in replacements.items():
        html = html.replace(old_name, new_name)

    favicon_pattern = re.compile(r"(?:index|favicon)-[A-Za-z0-9_-]+\.ico")
    html = favicon_pattern.sub("favicon.ico", html)
    index_html.write_text(html, encoding="utf-8")

    with gzip.open(
        dist_dir / "index.html.gz", "wt", encoding="utf-8", compresslevel=9
    ) as target:
        target.write(html)
    print("Created index.html.gz")


def prepare_embedded_assets(dist_dir: Path) -> None:
    for filename in ("main.js", "main.css"):
        source = dist_dir / filename
        if not source.is_file():
            raise RuntimeError(f"webui/dist/{filename} is missing")
        gzip_file(source, dist_dir / f"{filename}.gz")
        print(f"Created {filename}.gz")

    favicon_source = Path("webui/favicon.ico")
    favicon_target = dist_dir / "favicon.ico"
    if favicon_source.is_file():
        shutil.copy2(favicon_source, favicon_target)
        gzip_file(favicon_target, dist_dir / "favicon.ico.gz")
        print("Created favicon.ico.gz")

    for filename in ("manifest.webmanifest", "icon-256.png"):
        source = dist_dir / filename
        if source.is_file():
            gzip_file(source, dist_dir / f"{filename}.gz")
            print(f"Created {filename}.gz")


def rename_webui_files() -> None:
    dist_dir = Path("webui/dist")
    if not dist_dir.is_dir():
        raise RuntimeError("webui/dist directory not found; run the WebUI build first")

    replacements = rename_legacy_outputs(dist_dir)
    prepare_index(dist_dir, replacements)
    prepare_embedded_assets(dist_dir)
    build_spiffs_source(dist_dir)


rename_webui_files()
