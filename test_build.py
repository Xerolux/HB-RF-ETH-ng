import subprocess
import sys

def run_command(cmd):
    try:
        process = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        for line in process.stdout:
            print(line.decode('utf-8'), end='')
        process.wait()
        return process.returncode
    except Exception as e:
        print(f"Error: {e}")
        return 1

print("Running pio build...")
sys.exit(run_command("pio run -e hb-rf-eth-ng"))
