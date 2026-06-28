#!/usr/bin/env python3
"""
Automatically generates changelog entries from git commits since the last tag.
Usage: python update_changelog.py <new_version>
"""

import sys
import subprocess
import re
from datetime import datetime

def get_last_tag():
    try:
        tag = subprocess.check_output(['git', 'describe', '--tags', '--abbrev=0']).decode('utf-8').strip()
        return tag
    except subprocess.CalledProcessError:
        return None

def get_commits_since_tag(tag):
    try:
        if tag:
            commits = subprocess.check_output(['git', 'log', f'{tag}..HEAD', '--pretty=format:- %s']).decode('utf-8').strip()
        else:
            commits = subprocess.check_output(['git', 'log', '--pretty=format:- %s']).decode('utf-8').strip()
        return commits
    except subprocess.CalledProcessError:
        return ""

def update_changelog(version):
    with open('CHANGELOG.md', 'r') as f:
        content = f.read()

    # Idempotent: never duplicate an existing version section. This lets a
    # release workflow re-run safely when the changelog was curated by hand.
    if re.search(rf"## \[{re.escape(version)}\]", content):
        print(f"Section ## [{version}] already present in CHANGELOG.md — leaving it unchanged.")
        return

    last_tag = get_last_tag()
    commits = get_commits_since_tag(last_tag)

    if not commits:
        commits = "- No new commits since last release"

    date_str = datetime.now().strftime('%Y-%m-%d')
    new_entry = f"## [{version}] - {date_str}\n\n### Changes\n{commits}\n\n"

    unreleased_pattern = r"## \[Unreleased\]"
    if re.search(unreleased_pattern, content):
        content = re.sub(unreleased_pattern, f"## [Unreleased]\n\n{new_entry.strip()}", content, count=1)
    else:
        # If no Unreleased section, prepend it after the header
        header_end = content.find('\n\n')
        if header_end != -1:
            content = content[:header_end+2] + new_entry + content[header_end+2:]
        else:
            content = new_entry + content

    with open('CHANGELOG.md', 'w') as f:
        f.write(content)

def main():
    if len(sys.argv) != 2:
        print("Usage: python update_changelog.py <new_version>")
        sys.exit(1)

    version = sys.argv[1].lstrip('v')
    update_changelog(version)
    print(f"CHANGELOG.md updated for version {version}")

if __name__ == '__main__':
    main()
