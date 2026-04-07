import os
path = "/home/jules/.pyenv/versions/3.12.13/lib/python3.12/site-packages/platformio/package/manager/_install.py"
with open(path, 'r') as f:
    content = f.read()

import re
content = content.replace(
"""            if isinstance(dependency, str): dependency = {"name": dependency, "version": "*"}""",
"""            if isinstance(dependency, str): dependency = {"name": dependency, "version": "*"}
            if isinstance(dependency, dict) and dependency.get("name") == "framework-espidf": continue"""
)

with open(path, 'w') as f:
    f.write(content)

path = "/home/jules/.pyenv/versions/3.12.13/lib/python3.12/site-packages/platformio/package/meta.py"
with open(path, 'r') as f:
    content = f.read()

content = content.replace("assert isinstance(dependency, dict)", "if not isinstance(dependency, dict):\n            return PackageCompatibility()")

with open(path, 'w') as f:
    f.write(content)
