from pathlib import Path

Import("env")

version_file = Path("version.txt")
if version_file.exists():
    with open(version_file, "r", encoding="utf-8") as fp:
        version = fp.readline().strip()
        if version:
            env.Replace(PROGNAME=f"firmware_{version.replace('.', '_')}")
            print(f"Building firmware version: {version}")
        else:
            print("WARNING: version.txt is empty, using default firmware name")
else:
    print("WARNING: version.txt not found, using default firmware name")
