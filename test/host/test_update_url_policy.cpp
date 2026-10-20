// Host test for include/update_url_policy.h — the device-side mirror of the
// manifest allowlist in scripts/device_manifest.py. Boundary cases only; the
// rule itself is documented in docs/AUTO_UPDATE_PLAN.md rule 7.

#include <cstdio>

#include "update_url_policy.h"

static int failures = 0;

static void expect_allowed(const char *url)
{
    if (!update_url_policy_allows(url)) {
        std::printf("FAIL: expected allowed: %s\n", url);
        failures++;
    }
}

static void expect_rejected(const char *url)
{
    if (update_url_policy_allows(url)) {
        std::printf("FAIL: expected rejected: %s\n", url);
        failures++;
    }
}

int main()
{
    // Exactly the hosts the generator emits.
    expect_allowed("https://github.com/Xerolux/HB-RF-ETH-ng/releases/download/v2.3.0/firmware_2.3.0.bin");
    expect_allowed("https://objects.githubusercontent.com/releases/asset.bin");
    // github.io is deliberately NOT allowlisted for downloads: the device
    // manifest is fetched from Pages via a compile-time constant URL, never
    // from a manifest field, and binaries are served from GitHub releases.

    // Scheme and transport.
    expect_rejected(NULL);
    expect_rejected("");
    expect_rejected("https://");
    expect_rejected("http://github.com/asset.bin");
    expect_rejected("ftp://github.com/asset.bin");
    expect_rejected("github.com/asset.bin");

    // Host trust boundaries.
    expect_rejected("https://evil.example/firmware.bin");
    expect_rejected("https://github.com.evil.example/firmware.bin");
    expect_rejected("https://githubusercontent.com.evil.example/firmware.bin");
    expect_rejected("https://github.com@evil.example/firmware.bin");
    expect_allowed("https://user:pass@github.com/firmware.bin@evil.example/x"); // '@' past the first '/' is path, not authority

    // Ports never appear in generator output.
    expect_rejected("https://github.com:8443/firmware.bin");
    expect_rejected("https://github.com:443/");

    // Structural edge cases.
    expect_allowed("https://@github.com/firmware.bin");    // empty userinfo, host still github.com
    expect_rejected("https://github.com@/firmware.bin");   // empty host after '@'
    expect_allowed("https://GITHUB.COM/asset.bin");        // host comparison is case-insensitive
    expect_allowed("https://github.com");                  // no path is still a host

    if (failures != 0) {
        std::printf("%d update_url_policy check(s) failed\n", failures);
        return 1;
    }
    std::printf("update_url_policy: all checks passed\n");
    return 0;
}
