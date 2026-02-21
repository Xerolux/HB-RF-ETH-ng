import argparse
import json
import time
import urllib.request
import urllib.parse
import urllib.error
import sys

def get_token(ip, password):
    url = f"http://{ip}/login.json"
    headers = {"Content-Type": "application/json"}
    data = json.dumps({"password": password}).encode("utf-8")

    try:
        req = urllib.request.Request(url, data=data, headers=headers, method="POST")
        with urllib.request.urlopen(req) as response:
            res_body = response.read().decode("utf-8")
            res_json = json.loads(res_body)
            if res_json.get("isAuthenticated"):
                return res_json.get("token")
            else:
                print(f"Login failed: {res_json.get('error')}")
                return None
    except Exception as e:
        print(f"Error during login: {e}")
        return None

def trigger_ota(ip, token, ota_url):
    url = f"http://{ip}/api/ota_url"
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Token {token}"
    }
    data = json.dumps({"url": ota_url}).encode("utf-8")

    try:
        req = urllib.request.Request(url, data=data, headers=headers, method="POST")
        with urllib.request.urlopen(req) as response:
            res_body = response.read().decode("utf-8")
            res_json = json.loads(res_body)
            if res_json.get("success"):
                print(f"OTA Triggered: {res_json.get('message')}")
                return True
            else:
                print(f"OTA Trigger Failed: {res_json.get('error')}")
                return False
    except Exception as e:
        print(f"Error triggering OTA: {e}")
        return False

def check_status(ip, token):
    url = f"http://{ip}/api/ota_status"
    headers = {
        "Authorization": f"Token {token}"
    }

    try:
        req = urllib.request.Request(url, headers=headers, method="GET")
        with urllib.request.urlopen(req) as response:
            res_body = response.read().decode("utf-8")
            return json.loads(res_body)
    except Exception as e:
        # Ignore network errors during reboot phase
        pass
        return None

def main():
    parser = argparse.ArgumentParser(description="Test OTA Update Function on HB-RF-ETH-ng")
    parser.add_argument("ip", help="IP address of the device")
    parser.add_argument("password", help="Admin password of the device")
    parser.add_argument("--url", default="https://github.com/Xerolux/HB-RF-ETH-ng/releases/download/v2.1.7/firmware_2.1.7.bin", help="OTA URL to use")

    args = parser.parse_args()

    print(f"Connecting to {args.ip}...")
    token = get_token(args.ip, args.password)

    if not token:
        print("Could not authenticate. Exiting.")
        sys.exit(1)

    print(f"Authenticated. Token: {token[:10]}...")

    print(f"Triggering OTA with URL: {args.url}")
    if not trigger_ota(args.ip, token, args.url):
        print("Failed to trigger OTA. Exiting.")
        sys.exit(1)

    print("Monitoring OTA progress...")
    last_progress = -1

    # Timeout after 5 minutes
    start_time = time.time()

    while time.time() - start_time < 300:
        status = check_status(args.ip, token)

        if status:
            state = status.get("status")
            progress = status.get("progress", 0)
            error = status.get("error", "")

            if progress != last_progress or state == "failed" or state == "success":
                print(f"Status: {state}, Progress: {progress}% {f'({error})' if error else ''}")
                last_progress = progress

            if state == "success":
                print("OTA Update Successful! Device is restarting...")
                sys.exit(0)
            elif state == "failed":
                print(f"OTA Update Failed: {error}")
                sys.exit(1)

        time.sleep(1)

    print("Timeout waiting for OTA completion.")
    sys.exit(1)

if __name__ == "__main__":
    main()
