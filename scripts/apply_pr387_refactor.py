from pathlib import Path

from pr387_settings import apply as apply_settings
from pr387_versions import apply as apply_versions
from pr387_backend import apply as apply_backend
from pr387_mqtt import apply as apply_mqtt
from pr387_cleanup import apply as apply_cleanup

apply_settings()
apply_versions()
apply_backend()
apply_mqtt()
apply_cleanup()

for helper in [
    'scripts/pr387_settings.py',
    'scripts/pr387_versions.py',
    'scripts/pr387_backend.py',
    'scripts/pr387_mqtt.py',
    'scripts/pr387_cleanup.py',
    'scripts/sitecustomize.py',
    'scripts/pr387-refactor.trigger',
]:
    path = Path(helper)
    if path.exists():
        path.unlink()

print('Applied PR #387 manual update model and fixed defaults.')
