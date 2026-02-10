from playwright.sync_api import sync_playwright
import time

def run(playwright):
    browser = playwright.chromium.launch(headless=True)
    context = browser.new_context(viewport={"width": 1280, "height": 720})
    page = context.new_page()

    # Mock sysinfo.json
    page.route("**/sysinfo.json", lambda route: route.fulfill(
        status=200,
        content_type="application/json",
        body='{"sysInfo": {"serial": "TestSerial", "currentVersion": "2.1.5", "ethernetConnected": true, "cpuUsage": 12, "memoryUsage": 45, "ethernetSpeed": 100, "ethernetDuplex": "Full", "uptimeSeconds": 3600, "radioModuleType": "RPI-RF-MOD"}}'
    ))

    # Mock settings.json
    page.route("**/settings.json", lambda route: route.fulfill(
        status=200,
        content_type="application/json",
        body='{"settings": {}}'
    ))

    # Mock monitoring
    page.route("**/api/monitoring", lambda route: route.fulfill(
        status=200,
        content_type="application/json",
        body='{}'
    ))

    # Mock update check
    page.route("**/version.txt", lambda route: route.fulfill(
        status=200,
        body='2.1.5'
    ))

    # Mock Changelog
    page.route("**/CHANGELOG.md", lambda route: route.fulfill(
        status=200,
        body="# Changelog\n\n## v2.1.5\n- Feature A\n- Fix B"
    ))

    # Bypass Login
    page.goto("http://localhost:1234/login")
    page.evaluate("localStorage.setItem('hb-rf-eth-ng-pw', 'dummy-token')")

    # 2. Verify Mobile Widgets Layout
    print("Navigating to dashboard...")
    page.goto("http://localhost:1234/")

    # Set mobile viewport
    print("Setting mobile viewport...")
    page.set_viewport_size({"width": 375, "height": 800})

    # Wait for widgets
    page.wait_for_selector(".widgets-row")

    # Wait for animation
    print("Waiting for animation...")
    time.sleep(2)

    # Take screenshot of dashboard
    print("Taking screenshot of dashboard...")
    page.screenshot(path="verification/dashboard_mobile.png")

    browser.close()

with sync_playwright() as playwright:
    run(playwright)
