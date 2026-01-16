import time
from playwright.sync_api import sync_playwright

def test_redirect():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        context = browser.new_context()
        page = context.new_page()

        # Mock API requests to avoid errors since we don't have a backend
        # Mock /login.json to simulate logged in state if needed, but since we are just checking redirect logic
        # and settings page is protected, we need to mock login check or login flow.

        # Mock login check (login store usually checks /login.json or similar, but here it's likely just token presence)
        # Actually, let's look at stores.js. It likely uses localStorage/sessionStorage.
        # But we can just navigate to /config. If it redirects to /settings, and then redirects to /login (because not auth),
        # the first redirect worked.
        # But better: Inject auth token so we see the settings page.

        # Route mocks
        page.route("**/api/monitoring", lambda route: route.fulfill(status=200, body='{}'))
        page.route("**/api/check_update", lambda route: route.fulfill(status=200, body='{}'))
        page.route("**/sysinfo.json", lambda route: route.fulfill(status=200, body='{"version": "2.1.3"}'))
        # Mock settings.json for the settings page load
        page.route("**/settings.json", lambda route: route.fulfill(
            status=200,
            content_type="application/json",
            body='{"hostname": "test-host", "useDHCP": true, "checkUpdates": false}'
        ))

        # Inject session storage for authentication
        # Key: hb-rf-eth-ng-pw
        # Value: some-token

        # Navigate to root first to set storage
        page.goto("http://localhost:1234/")

        page.evaluate("""() => {
            const loginStore = JSON.stringify({
                token: "mock-token",
                isAuthenticated: true,
                passwordChanged: true
            });
            // Pinia persistence usually uses localStorage or sessionStorage.
            // Looking at header.vue/stores.js, it seems to use a store.
            // Let's try to assume it's persisted or we can just mock the store state if possible.
            // Actually, based on memory: "Authentication persistence... relies on sessionStorage key hb-rf-eth-ng-pw"
            sessionStorage.setItem('hb-rf-eth-ng-pw', 'mock-token');
        }""")

        # Now navigate to /#/config
        print("Navigating to /#/config")
        page.goto("http://localhost:1234/#/config")

        # Wait a bit for router to process
        page.wait_for_timeout(2000)

        current_url = page.url
        print(f"Current URL: {current_url}")

        if "/#/settings" in current_url:
            print("Redirect successful!")
        else:
            print(f"Redirect failed! URL is {current_url}")

        # Take screenshot
        page.screenshot(path="verification_redirect.png")

        browser.close()

if __name__ == "__main__":
    test_redirect()
