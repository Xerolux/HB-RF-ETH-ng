#!/usr/bin/env python3
"""
OTA Upload Script for HB-RF-ETH-ng
Performs authenticated OTA firmware upload via Web API
"""

Import("env")  # PlatformIO SCons import
import requests
import sys
import os

def get_upload_params():
    """Extract upload parameters from environment"""
    # Get IP address from upload_port or UPLOAD_PORT env variable
    upload_port = env.get("UPLOAD_PORT", "")
    if not upload_port:
        upload_port = env.get("upload_port", "")

    if not upload_port:
        print("Error: No upload_port specified!")
        print("Set upload_port in platformio.ini or use UPLOAD_PORT environment variable")
        sys.exit(1)

    # Get password from environment variable or prompt
    password = os.environ.get("HB_RF_ETH_PASSWORD", "")
    if not password:
        try:
            import getpass
            password = getpass.getpass("Enter device password: ")
        except:
            print("Error: No password provided!")
            print("Set HB_RF_ETH_PASSWORD environment variable or run interactively")
            sys.exit(1)

    return upload_port, password

def login_and_get_token(ip_address, password):
    """Login to device and retrieve authentication token"""
    login_url = f"http://{ip_address}/login.json"

    print(f"Logging in to {ip_address}...")

    try:
        response = requests.post(
            login_url,
            json={"password": password},
            timeout=10
        )

        if response.status_code != 200:
            print(f"Login failed with status {response.status_code}")
            print(f"Response: {response.text}")
            sys.exit(1)

        data = response.json()

        if not data.get("isAuthenticated", False):
            print("Authentication failed! Check password.")
            sys.exit(1)

        token = data.get("token")
        if not token:
            print("No token received from device!")
            sys.exit(1)

        print("✓ Login successful, token received")
        return token

    except requests.exceptions.RequestException as e:
        print(f"Network error during login: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"Error during login: {e}")
        sys.exit(1)

def upload_firmware(ip_address, token, firmware_path):
    """Upload firmware using authentication token"""
    ota_url = f"http://{ip_address}/ota_update"

    if not os.path.exists(firmware_path):
        print(f"Firmware file not found: {firmware_path}")
        sys.exit(1)

    file_size = os.path.getsize(firmware_path)
    print(f"Uploading firmware: {os.path.basename(firmware_path)} ({file_size} bytes)")

    try:
        with open(firmware_path, 'rb') as f:
            files = {'file': (os.path.basename(firmware_path), f, 'application/octet-stream')}
            headers = {'Authorization': f'Token {token}'}

            print("Uploading... (this may take 30-60 seconds)")

            response = requests.post(
                ota_url,
                files=files,
                headers=headers,
                timeout=120  # 2 minutes timeout for upload
            )

            if response.status_code == 200:
                print("✓ Firmware uploaded successfully!")
                print("Device is restarting with new firmware...")
                print("\nOTA Update completed!")
                return True
            else:
                print(f"Upload failed with status {response.status_code}")
                print(f"Response: {response.text}")
                sys.exit(1)

    except requests.exceptions.Timeout:
        print("Upload timeout! Check network connection and device status.")
        sys.exit(1)
    except requests.exceptions.RequestException as e:
        print(f"Network error during upload: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"Error during upload: {e}")
        sys.exit(1)

def ota_upload(source, target, env):
    """Main OTA upload function called by PlatformIO"""
    print("\n" + "="*60)
    print("HB-RF-ETH-ng OTA Upload")
    print("="*60)

    # Get parameters
    ip_address, password = get_upload_params()
    firmware_path = str(target[0])

    print(f"Target device: {ip_address}")
    print(f"Firmware: {firmware_path}")
    print("-"*60)

    # Login and get token
    token = login_and_get_token(ip_address, password)

    # Upload firmware
    upload_firmware(ip_address, token, firmware_path)

# Register upload action
env.Replace(UPLOADCMD=ota_upload)
