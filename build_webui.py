from subprocess import run, CalledProcessError, PIPE
import sys
import os
import platform
from pathlib import Path

Import("env")

def is_tool(name):
    """Check if a tool is available in PATH."""
    cmd = "where" if platform.system() == "Windows" else "which"
    try:
        result = run([cmd, name], capture_output=True, check=False)
        return result.returncode == 0
    except Exception:
        return False

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

    original_dir = os.getcwd()
    try:
        os.chdir(webui_dir)
        print("Building web UI...")

        npm_cmd = "npm.cmd" if platform.system() == "Windows" else "npm"

        # Always install/update dependencies to ensure we have the latest packages
        # Use 'npm ci' if package-lock.json exists for faster, reproducible builds
        print("Installing npm dependencies...")
        if Path("package-lock.json").exists():
            result = run([npm_cmd, "ci"], capture_output=True, text=True, encoding="utf-8")
        else:
            result = run([npm_cmd, "install"], capture_output=True, text=True, encoding="utf-8")

        if result.returncode != 0:
            print(f"ERROR: npm install failed:\n{result.stderr}")
            return

        # Build the web UI
        result = run([npm_cmd, "run", "build"], capture_output=True, text=True, encoding="utf-8")
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
