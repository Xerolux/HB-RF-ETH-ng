from subprocess import run, CalledProcessError, PIPE
import sys
import os
import platform
from pathlib import Path

# Add current directory to path to ensure we can import local modules
sys.path.append(os.getcwd())

try:
    import update_version
except ImportError:
    print("WARNING: Could not import update_version.py. Version sync might fail.")
    update_version = None

try:
    Import("env")
except NameError:
    pass

def is_tool(name):
    """Check if a tool is available in PATH."""
    cmd = "where" if platform.system() == "Windows" else "which"
    try:
        result = run([cmd, name], capture_output=True, check=False)
        return result.returncode == 0
    except Exception:
        return False

def update_version_in_files():
    """
    Read version from version.txt and update WebUI files using update_version.py logic.
    This ensures local builds match the version in version.txt.
    """
    if not update_version:
        return

    try:
        # Read version from version.txt in the project root
        version_file = Path("version.txt")
        if not version_file.exists():
            print("WARNING: version.txt not found! Skipping version update.")
            return

        version = version_file.read_text().strip()
        print(f"Syncing WebUI version to: {version}")

        # Update WebUI specific files
        update_version.update_locales(version)
        update_version.update_about_vue(version)
        update_version.update_package_json(version)

    except Exception as e:
        print(f"WARNING: Failed to update WebUI version: {e}")

def build_web():
    """Build the web UI using npm."""
    if not is_tool("npm"):
        print("WARNING: npm not found in PATH. Skipping web UI build.")
        print("Please install Node.js and npm to build the web UI.")
        return

    webui_dir = Path("webui")
    if not webui_dir.exists():
        print("ERROR: webui directory not found!")
        return

    # Sync version before building
    update_version_in_files()

    original_dir = os.getcwd()
    try:
        os.chdir(webui_dir)
        print("Building web UI...")

        npm_cmd = "npm.cmd" if platform.system() == "Windows" else "npm"

        # Run npm install first if node_modules doesn't exist
        if not Path("node_modules").exists():
            print("Installing npm dependencies...")
            result = run([npm_cmd, "install"], capture_output=True, text=True)
            if result.returncode != 0:
                print(f"ERROR: npm install failed:\n{result.stderr}")
                return

        # Build the web UI
        result = run([npm_cmd, "run", "build"], capture_output=True, text=True)
        if result.returncode == 0:
            print("Web UI built successfully!")
            if result.stdout:
                print(result.stdout)
        else:
            print(f"ERROR: Web UI build failed:\n{result.stderr}")
            sys.exit(1)

    except Exception as e:
        print(f"ERROR: Exception during web build: {e}")
        sys.exit(1)
    finally:
        os.chdir(original_dir)

build_web()
