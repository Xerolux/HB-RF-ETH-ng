from playwright.sync_api import sync_playwright
import time
import os

def run(playwright):
    browser = playwright.chromium.launch(headless=True)
    page = browser.new_page()

    page.on("console", lambda msg: print(f"Console: {msg.text}"))
    page.on("pageerror", lambda err: print(f"Page Error: {err}"))

    # Mock API responses
    page.route("**/sysinfo.json", lambda route: route.fulfill(
        status=200,
        content_type="application/json",
        body='{"sysInfo": {"ethernetConnected": true, "ethernetSpeed": 1000, "ethernetDuplex": "Full", "cpuUsage": 15, "memoryUsage": 30, "serial": "ABC1234567", "currentVersion": "2.1.2", "latestVersion": "2.1.2", "uptimeSeconds": 3600, "boardRevision": "1.0", "radioModuleType": "RPI-RF-MOD"}}'
    ))

    page.route("**/login.json", lambda route: route.fulfill(
        status=200,
        content_type="application/json",
        body='{"isAuthenticated": true, "token": "dummy_token", "passwordChanged": true}'
    ))

    page.route("**/settings.json", lambda route: route.fulfill(
        status=200,
        content_type="application/json",
        body='{"settings": {"enableIPv6": false, "useDHCP": true, "hostname": "hb-rf-eth"}}'
    ))

    page.route("**/api/monitoring", lambda route: route.fulfill(
        status=200,
        content_type="application/json",
        body='{"snmp": {"enabled": false}, "checkmk": {"enabled": false}, "mqtt": {"enabled": false}}'
    ))

    # 1. Login Page Verification
    print("Navigating to Login page...")
    try:
        page.goto("http://localhost:1234/")
        page.wait_for_timeout(2000)

        # Check if footer exists
        if page.locator(".login-footer").count() > 0:
            footer_text = page.locator(".login-footer").text_content()
            print(f"Login Footer Text: {footer_text}")

        page.screenshot(path="login_page.png")

        # 2. Login
        print("Logging in...")
        page.fill("input[type='password']", "password")
        page.click("button.login-btn")

        # Wait for dashboard
        page.wait_for_selector(".dashboard-container", timeout=5000)

        # 3. Dashboard Verification
        print("Verifying Dashboard...")

        # Check Dashboard Title (Changed from Greeting)
        header_text = page.locator(".header-text h1").text_content()
        print(f"Dashboard Header Text: {header_text}")

        # Check Localized Strings
        online_text = page.locator(".indicator-text").text_content()
        print(f"Online Status Text: {online_text}")

        connected_text = page.locator(".widget-status-text").text_content()
        print(f"Connected Status Text: {connected_text}")

        page.screenshot(path="dashboard.png")

        # 4. Header Menu Verification
        print("Verifying Header Menu...")
        page.click(".settings-dropdown .dropdown-toggle")
        page.wait_for_timeout(1000)

        firmware_item = page.locator(".dropdown-menu a[href='/firmware']")
        monitoring_item = page.locator(".dropdown-menu a[href='/monitoring']")

        print(f"Firmware Item in Dropdown: {firmware_item.is_visible()}")
        print(f"Monitoring Item in Dropdown: {monitoring_item.is_visible()}")

        page.screenshot(path="menu_dropdown.png")

        # 5. Check Sponsor Modal Text
        print("Verifying Sponsor Modal...")
        page.click(".sponsor-btn")
        page.wait_for_selector(".sponsor-modal-content", timeout=2000)

        sponsor_desc = page.locator(".sponsor-modal-body .description").text_content()
        print(f"Sponsor Description: {sponsor_desc}")

        page.screenshot(path="sponsor_modal.png")

    except Exception as e:
        print(f"Error: {e}")
        page.screenshot(path="error.png")

    browser.close()

with sync_playwright() as playwright:
    run(playwright)
