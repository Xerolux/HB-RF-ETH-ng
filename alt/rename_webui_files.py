"""
Script to rename Parcel 2 output files with content hashes to fixed names.
"""
from pathlib import Path
import shutil

try:
    Import("env")
except NameError:
    pass

def rename_webui_files():
    """Rename webui dist files to fixed names expected by platformio.ini"""
    dist_dir = Path("webui/dist")

    if not dist_dir.exists():
        print("WARNING: webui/dist directory not found")
        return

    # Mapping of patterns to target filenames
    renames = [
        ("webui.*.js.gz", "main.js.gz"),
        ("webui.*.css.gz", "main.css.gz"),
    ]

    # Track old -> new names for HTML replacement
    replacements = {}

    for pattern, target in renames:
        matches = list(dist_dir.glob(pattern))

        if not matches:
            print(f"WARNING: No files matching {pattern}")
            continue

        source = matches[0]
        dest = dist_dir / target

        # Store replacement mapping (without .gz extension for HTML)
        old_name = source.name.replace('.gz', '')
        new_name = target.replace('.gz', '')

        replacements[old_name] = new_name

        # Remove old target if exists
        if dest.exists():
            dest.unlink()

        # Rename source to target
        shutil.move(str(source), str(dest))
        print(f"Renamed {source.name} -> {target}")

    # Update index.html.gz with new filenames
    index_html_gz = dist_dir / "index.html.gz"
    if index_html_gz.exists() and replacements:
        import gzip

        # Decompress
        with gzip.open(index_html_gz, 'rt', encoding='utf-8') as f:
            html_content = f.read()

        # Replace old filenames with new ones
        for old_name, new_name in replacements.items():
            html_content = html_content.replace(old_name, new_name)
            print(f"Replaced {old_name} -> {new_name} in index.html")

        # Also update favicon references
        for f in dist_dir.glob("favicon.*.ico"):
            old_favicon = f.name
            html_content = html_content.replace(old_favicon, "favicon.ico")
            print(f"Replaced {old_favicon} -> favicon.ico in index.html")

        # Recompress
        with gzip.open(index_html_gz, 'wt', encoding='utf-8') as f:
            f.write(html_content)
        print("Updated index.html.gz with new filenames")

    # Handle favicon - copy from webui root and compress
    webui_favicon = Path("webui/favicon.ico")
    dist_favicon = dist_dir / "favicon.ico"
    favicon_gz = dist_dir / "favicon.ico.gz"

    # Delete any hashed favicon files
    for f in dist_dir.glob("favicon.*.ico"):
        f.unlink()
        print(f"Removed {f.name}")

    # Copy favicon from webui root to dist
    if webui_favicon.exists():
        if not dist_favicon.exists():
            shutil.copy2(str(webui_favicon), str(dist_favicon))
            print("Copied favicon.ico to dist")

        # Compress favicon
        if dist_favicon.exists():
            import gzip
            with open(dist_favicon, 'rb') as f_in:
                with gzip.open(favicon_gz, 'wb') as f_out:
                    shutil.copyfileobj(f_in, f_out)
            print("Created favicon.ico.gz")
    else:
        print("WARNING: webui/favicon.ico not found")

rename_webui_files()
