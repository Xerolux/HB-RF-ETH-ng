from playwright.sync_api import sync_playwright
import time

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        context = browser.new_context()
        page = context.new_page()

        # Inject auth token to bypass login
        # The 404 implies URL mapping issue or single page app router issue
        # With http.server, we access files directly
        page.goto("http://localhost:8080/index.html")

        # Verify if page loaded or 404
        if "Error response" in page.title():
            print("Failed to load page: 404")
            browser.close()
            return

        page.evaluate("sessionStorage.setItem('hb-rf-eth-ng-pw', 'mock-token')")

        # Reload to apply auth
        page.reload()

        # Wait for app to mount
        time.sleep(2)

        # Navigate to Settings - Use exact text match or more specific selector
        try:
            page.click("a.nav-link:has-text('Settings')")
            time.sleep(1) # wait for transition
        except:
            print("Could not find Settings link. Screenshotting home.")
            page.screenshot(path="verification_debug.png", full_page=True)
            browser.close()
            return

        # Try to find the checkbox by label text which is more robust
        try:
            # Enable experimental features
            page.locator("label:has-text('Enable Experimental Features')").click()
            time.sleep(0.5)

            # Change mode to Slave to see IP input
            page.select_option("select.form-select", value="2")
            time.sleep(0.5)

        except Exception as e:
            print(f"Interaction failed: {e}")

        # Take screenshot of the proxy settings area
        page.screenshot(path="verification_proxy_settings.png", full_page=True)

        browser.close()

if __name__ == "__main__":
    run()
