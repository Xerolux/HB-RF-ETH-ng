#!/usr/bin/env python3
"""
Automatically updates CHANGELOG.md with a new version entry and commit logs.
"""

import sys
import subprocess
from datetime import datetime
from pathlib import Path

def get_git_log(from_ref, to_ref):
    """Get git log between two refs."""
    cmd = ["git", "log", f"{from_ref}..{to_ref}", "--pretty=format:- %s (%h)"]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        print(f"Error running git log: {e}")
        return ""

def get_tags():
    """Get all tags sorted by creation date (newest first)."""
    cmd = ["git", "tag", "--sort=-creatordate"]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        return [t for t in result.stdout.splitlines() if t.strip()]
    except subprocess.CalledProcessError:
        return []

def main():
    if len(sys.argv) < 2:
        print("Usage: python update_changelog.py <version>")
        sys.exit(1)

    version = sys.argv[1]
    changelog_path = Path("CHANGELOG.md")

    if not changelog_path.exists():
        print("CHANGELOG.md not found!")
        sys.exit(1)

    content = changelog_path.read_text()

    # Check if version already exists to avoid duplicates
    # We check for generic patterns
    if (f"## Version {version}" in content or
        f"## {version}" in content or
        f"## [{version}]" in content):
        print(f"Changelog entry for {version} already exists. Skipping.")
        return

    tags = get_tags()
    prev_tag = None

    # Determine the previous tag
    # If the current version corresponds to the latest tag (e.g. we are running on the tag),
    # we want the one before it.
    # If the current version is not in tags yet, we want the latest tag.

    latest_tag = tags[0] if tags else None

    # Check if latest tag matches current version (approx)
    if latest_tag and version in latest_tag:
        if len(tags) > 1:
            prev_tag = tags[1]
    else:
        prev_tag = latest_tag

    commits = ""
    if prev_tag:
        print(f"Generating changelog from {prev_tag} to HEAD")
        commits = get_git_log(prev_tag, "HEAD")
    else:
        print("No previous tag found. Using basic placeholder.")

    if not commits:
        commits = "- Automated release update"

    date_str = datetime.now().strftime("%Y-%m-%d")
    year_str = datetime.now().strftime("%Y")

    # Format matches existing: ## Version X.X.X (YYYY)
    new_header = f"## Version {version} ({year_str})"
    new_entry = f"{new_header}\n\n### Änderungen\n\n{commits}\n"

    # Insert at the top (after main header)
    lines = content.splitlines()
    insert_idx = -1

    # Find where to insert (before the first version header)
    for i, line in enumerate(lines):
        if line.startswith("## Version") or line.startswith("## [") or (line.startswith("## ") and any(c.isdigit() for c in line)):
            insert_idx = i
            break

    if insert_idx != -1:
        lines.insert(insert_idx, new_entry)
        lines.insert(insert_idx + 1, "") # Add spacing
    else:
        # Append if no existing version headers found
        lines.append("")
        lines.append(new_entry)

    # Write back
    changelog_path.write_text("\n".join(lines) + "\n")
    print(f"✅ Successfully added changelog entry for {version}")

if __name__ == "__main__":
    main()
