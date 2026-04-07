import os
path = "src/CMakeLists.txt"
with open(path, 'r') as f:
    content = f.read()

content = content.replace("REQUIRES driver", "REQUIRES driver lan87xx")

with open(path, 'w') as f:
    f.write(content)
