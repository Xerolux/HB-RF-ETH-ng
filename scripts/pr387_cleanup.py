from pathlib import Path
import re


def apply():
    for obsolete in [
        'archive.json',
        'main/generated/archive.json.gz',
        'scripts/update_archive.py',
        '.github/workflows/rebuild-archive.yml',
    ]:
        path = Path(obsolete)
        if path.exists():
            path.unlink()

    for workflow in Path('.github/workflows').glob('*.yml'):
        text = workflow.read_text(encoding='utf-8')
        text = re.sub(
            r"\n\s*- name: Regenerate embedded firmware archive\n(?:\s*#.*\n)*\s*run: python3 scripts/update_archive\.py\n",
            '\n',
            text,
        )
        workflow.write_text(text, encoding='utf-8')
