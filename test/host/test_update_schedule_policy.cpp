#include "update_schedule_policy.h"

#include <cassert>
#include <cstdio>
#include <cstring>

// Boundary tests for the automatic-update scheduling policy (stages B2/C/D).
// Every assertion pins a rule from docs/AUTO_UPDATE_PLAN.md: first-check
// jitter, the retry ladder, window wraparound, the §4.3 clock gate, the
// install decision order, two-stage marker expiry and pending-entry expiry.

static void test_mode_bounds()
{
    assert(update_mode_valid(UPDATE_MODE_OFF));
    assert(update_mode_valid(UPDATE_MODE_NOTIFY));
    assert(update_mode_valid(UPDATE_MODE_AUTO_WEBUI));
    assert(update_mode_valid(UPDATE_MODE_AUTO_ALL));
    assert(!update_mode_valid(4));
    assert(!update_mode_valid(-1));

    // Wire format round trip; unknown input degrades to "off", never to an
    // installing mode.
    assert(strcmp(update_mode_name(UPDATE_MODE_AUTO_ALL), "auto_all") == 0);
    assert(strcmp(update_mode_name(UPDATE_MODE_OFF), "off") == 0);
    assert(update_mode_from_name("notify") == UPDATE_MODE_NOTIFY);
    assert(update_mode_from_name("auto_webui") == UPDATE_MODE_AUTO_WEBUI);
    assert(update_mode_from_name("auto_all") == UPDATE_MODE_AUTO_ALL);
    assert(update_mode_from_name("nonsense") == UPDATE_MODE_OFF);
    assert(update_mode_from_name(nullptr) == UPDATE_MODE_OFF);
    for (int mode = 0; mode <= 3; ++mode) {
        assert(update_mode_from_name(update_mode_name(mode)) == mode);
    }

    // Channels must match the manifest file names exactly.
    assert(update_channel_name_valid("stable"));
    assert(update_channel_name_valid("beta"));
    assert(!update_channel_name_valid("Beta"));
    assert(!update_channel_name_valid(""));
    assert(!update_channel_name_valid("../v1/stable"));
    assert(!update_channel_name_valid(nullptr));

    // Interval bounds: 6..168 hours inclusive.
    assert(!update_interval_valid(5));
    assert(update_interval_valid(6));
    assert(update_interval_valid(24));
    assert(update_interval_valid(168));
    assert(!update_interval_valid(169));

    assert(update_window_start_valid(0));
    assert(update_window_start_valid(23));
    assert(!update_window_start_valid(24));
    assert(!update_window_start_valid(-1));
    assert(update_window_len_valid(1));
    assert(update_window_len_valid(24));
    assert(!update_window_len_valid(0));
    assert(!update_window_len_valid(25));
}

static void test_first_check_jitter()
{
    // Deterministic 10..69 minutes; a fleet of devices with distinct last MAC
    // octets must spread across the whole hour, never below ten minutes.
    uint32_t seen_min = 0xFFFFFFFFu, seen_max = 0;
    for (unsigned mac = 0; mac <= 255; ++mac) {
        const uint32_t delay = update_schedule_first_delay_minutes((uint8_t)mac);
        assert(delay >= 10);
        assert(delay <= 69);
        if (delay < seen_min) seen_min = delay;
        if (delay > seen_max) seen_max = delay;
    }
    assert(seen_min == 10);  // mac 0, 60, 120, 180, 240
    assert(seen_max == 69);  // mac 59, 119, ...
    assert(update_schedule_first_delay_minutes(0) == 10);
    assert(update_schedule_first_delay_minutes(59) == 69);
    assert(update_schedule_first_delay_minutes(120) == 10); // wraps via % 60
}

static void test_backoff_ladder()
{
    // Three accelerated retries, then the full configured interval. The
    // ladder can never exceed the interval.
    assert(update_schedule_delay_after_failures(0, 24) == 24); // success path
    assert(update_schedule_delay_after_failures(1, 24) == 1);
    assert(update_schedule_delay_after_failures(2, 24) == 2);
    assert(update_schedule_delay_after_failures(3, 24) == 4);
    assert(update_schedule_delay_after_failures(4, 24) == 24); // give up early retries
    assert(update_schedule_delay_after_failures(37, 24) == 24);

    // A short interval caps the ladder: with 6 h the second retry already
    // waits the whole interval.
    assert(update_schedule_delay_after_failures(1, 6) == 1);
    assert(update_schedule_delay_after_failures(2, 6) == 2);
    assert(update_schedule_delay_after_failures(3, 6) == 4);
    assert(update_schedule_delay_after_failures(4, 6) == 6);

    // Defensive clamp for an unvalidated interval argument.
    assert(update_schedule_delay_after_failures(1, 0) == 1);
    assert(update_schedule_delay_after_failures(3, 2) == 2);
}

static void test_window_wraparound()
{
    // Ordinary daytime window.
    assert(update_window_contains(3, 3, 2));   // 03:00 start, 2 h
    assert(update_window_contains(4, 3, 2));
    assert(!update_window_contains(2, 3, 2));
    assert(!update_window_contains(5, 3, 2));

    // Midnight wrap: 23:00 + 4 h covers 23,0,1,2.
    assert(update_window_contains(23, 23, 4));
    assert(update_window_contains(0, 23, 4));
    assert(update_window_contains(1, 23, 4));
    assert(update_window_contains(2, 23, 4));
    assert(!update_window_contains(3, 23, 4));
    assert(!update_window_contains(22, 23, 4));

    // Full-day window always contains.
    for (int hour = 0; hour < 24; ++hour) {
        assert(update_window_contains(hour, 0, 24));
        assert(update_window_contains(hour, 17, 24));
    }

    // Malformed arguments are never "inside".
    assert(!update_window_contains(-1, 3, 2));
    assert(!update_window_contains(24, 3, 2));
    assert(!update_window_contains(3, -1, 2));
    assert(!update_window_contains(3, 24, 2));
    assert(!update_window_contains(3, 3, 0));
    assert(!update_window_contains(3, 3, 25));
}

static void test_time_and_idle_gates()
{
    // Same sanity floor as update_check: pre-2020 clocks are "unsynced".
    assert(!update_schedule_time_valid(0));
    assert(!update_schedule_time_valid(1600000000LL));
    assert(update_schedule_time_valid(1600000001LL));
    assert(update_schedule_time_valid(1760000000LL));

    // An active CCU session always blocks an unattended install; a session
    // that just dropped must not count as idle yet (5-minute quiet period).
    assert(!update_schedule_idle_ok(true, 3600));
    assert(!update_schedule_idle_ok(false, 0));
    assert(!update_schedule_idle_ok(false, 299));
    assert(update_schedule_idle_ok(false, 300));
    assert(update_schedule_idle_ok(false, 3600));
}

static void test_auto_action()
{
    const bool in = true, tv = true, idle = true;

    // Off and notify never install anything, regardless of offers.
    assert(update_schedule_auto_action(UPDATE_MODE_OFF, true, true, in, tv, idle) ==
           UPDATE_AUTO_ACTION_NONE);
    assert(update_schedule_auto_action(UPDATE_MODE_NOTIFY, true, true, in, tv, idle) ==
           UPDATE_AUTO_ACTION_NONE);

    // AUTO_ALL prefers the firmware (§4.5 order: firmware -> reboot -> WebUI).
    assert(update_schedule_auto_action(UPDATE_MODE_AUTO_ALL, true, true, in, tv, idle) ==
           UPDATE_AUTO_ACTION_INSTALL_FIRMWARE);
    // Only a WebUI offer: AUTO_ALL installs just the WebUI.
    assert(update_schedule_auto_action(UPDATE_MODE_AUTO_ALL, false, true, in, tv, idle) ==
           UPDATE_AUTO_ACTION_INSTALL_WEBUI);
    // AUTO_WEBUI never touches the firmware even when one is offered.
    assert(update_schedule_auto_action(UPDATE_MODE_AUTO_WEBUI, true, true, in, tv, idle) ==
           UPDATE_AUTO_ACTION_INSTALL_WEBUI);
    assert(update_schedule_auto_action(UPDATE_MODE_AUTO_WEBUI, true, false, in, tv, idle) ==
           UPDATE_AUTO_ACTION_NONE);
    assert(update_schedule_auto_action(UPDATE_MODE_AUTO_ALL, true, false, in, tv, idle) ==
           UPDATE_AUTO_ACTION_INSTALL_FIRMWARE);

    // Any missing gate stops the install (§4.3: no window, no valid clock or a
    // busy relay each postpone it; the search itself is unaffected).
    assert(update_schedule_auto_action(UPDATE_MODE_AUTO_ALL, true, true, false, tv, idle) ==
           UPDATE_AUTO_ACTION_NONE);
    assert(update_schedule_auto_action(UPDATE_MODE_AUTO_WEBUI, false, true, in, false, idle) ==
           UPDATE_AUTO_ACTION_NONE);
    assert(update_schedule_auto_action(UPDATE_MODE_AUTO_ALL, true, true, in, tv, false) ==
           UPDATE_AUTO_ACTION_NONE);

    // Nothing offered -> nothing to do, even fully inside the window.
    assert(update_schedule_auto_action(UPDATE_MODE_AUTO_ALL, false, false, in, tv, idle) ==
           UPDATE_AUTO_ACTION_NONE);
}

static void test_install_blocked_reason()
{
    const bool tv = true, in = true, idle = true;

    // Not wanted -> no reason to report.
    assert(update_schedule_install_blocked(false, false, false, false, false) ==
           UPDATE_INSTALL_BLOCKED_NONE);
    assert(update_install_blocked_name(UPDATE_INSTALL_BLOCKED_NONE) == nullptr);

    // §4.3 order: the clock outranks the window because without a trustworthy
    // clock the window is meaningless.
    assert(update_schedule_install_blocked(true, false, false, false, false) ==
           UPDATE_INSTALL_BLOCKED_NO_VALID_TIME);
    assert(update_schedule_install_blocked(true, false, true, true, false) ==
           UPDATE_INSTALL_BLOCKED_NO_VALID_TIME);
    assert(strcmp(update_install_blocked_name(UPDATE_INSTALL_BLOCKED_NO_VALID_TIME),
                  "no_valid_time") == 0);
    assert(update_schedule_install_blocked(true, tv, false, true, false) ==
           UPDATE_INSTALL_BLOCKED_OUTSIDE_WINDOW);
    assert(update_schedule_install_blocked(true, tv, true, false, false) ==
           UPDATE_INSTALL_BLOCKED_SESSION_ACTIVE);
    assert(update_schedule_install_blocked(true, tv, true, true, false) ==
           UPDATE_INSTALL_BLOCKED_NONE);

    // An active exclusive operation (manual install/upload running) wins over
    // everything else: nothing else may start alongside it.
    assert(update_schedule_install_blocked(true, false, false, false, true) ==
           UPDATE_INSTALL_BLOCKED_BUSY);
    assert(update_schedule_install_blocked(true, tv, in, idle, true) ==
           UPDATE_INSTALL_BLOCKED_BUSY);
}

static void test_stage_marker()
{
    char buffer[UPDATE_STAGE_MAX_BYTES];
    char fw[32], ui[32];

    // Round trip.
    assert(update_stage_format(buffer, sizeof(buffer), "2.3.0", "1.1.0"));
    assert(strcmp(buffer, "waf|2.3.0|1.1.0") == 0);
    assert(update_stage_parse(buffer, fw, sizeof(fw), ui, sizeof(ui)));
    assert(strcmp(fw, "2.3.0") == 0);
    assert(strcmp(ui, "1.1.0") == 0);

    // Long pre-release versions survive the round trip.
    assert(update_stage_format(buffer, sizeof(buffer), "2.2.9-Beta.2", "1.0.7"));
    assert(update_stage_parse(buffer, fw, sizeof(fw), ui, sizeof(ui)));
    assert(strcmp(fw, "2.2.9-Beta.2") == 0);
    assert(strcmp(ui, "1.0.7") == 0);

    // Malformed stored values are rejected, not half-parsed.
    assert(!update_stage_parse("", fw, sizeof(fw), ui, sizeof(ui)));
    assert(!update_stage_parse("waf|", fw, sizeof(fw), ui, sizeof(ui)));
    assert(!update_stage_parse("waf|2.3.0", fw, sizeof(fw), ui, sizeof(ui)));
    assert(!update_stage_parse("waf|2.3.0|", fw, sizeof(fw), ui, sizeof(ui)));
    assert(!update_stage_parse("fw|2.3.0|1.1.0", fw, sizeof(fw), ui, sizeof(ui)));
    assert(!update_stage_parse("waf|2.3.0|1.1.0|extra", fw, sizeof(fw), ui, sizeof(ui)));
    assert(!update_stage_parse(nullptr, fw, sizeof(fw), ui, sizeof(ui)));

    // Truncating output buffers must fail loudly instead of silently
    // shortening a version string.
    char tiny_fw[3], tiny_ui[32];
    assert(update_stage_parse("waf|2.3.0|1.1.0", tiny_fw, sizeof(tiny_fw), tiny_ui, sizeof(tiny_ui)) == false);

    // Oversized input cannot be formatted into a small buffer.
    char small[8];
    assert(!update_stage_format(small, sizeof(small), "2.3.0", "1.1.0"));

    // Applicability: boots_since_write 0 means "written during the current
    // boot, install reboot still ahead" -> not yet applicable. 1 and 2 are
    // the plan's two-boot window; 3+ expired; a rollback (running firmware no
    // longer matches) invalidates immediately.
    assert(!update_stage_applicable(0, "2.3.0", "2.3.0"));
    assert(update_stage_applicable(1, "2.3.0", "2.3.0"));
    assert(update_stage_applicable(2, "2.3.0", "2.3.0"));
    assert(!update_stage_applicable(3, "2.3.0", "2.3.0"));
    assert(!update_stage_applicable(1, "2.3.0", "2.2.8")); // rolled back
    assert(!update_stage_applicable(1, nullptr, "2.3.0"));
    assert(!update_stage_applicable(1, "2.3.0", nullptr));
}

static void test_pending_entry()
{
    char buffer[UPDATE_PENDING_MAX_BYTES];
    char version[32];
    bool firmware = false;
    int64_t created = 0;

    // Round trip firmware and WebUI targets.
    assert(update_pending_format(buffer, sizeof(buffer), true, "2.3.0", 1760000000LL));
    assert(strcmp(buffer, "fw|2.3.0|1760000000") == 0);
    assert(update_pending_parse(buffer, &firmware, version, sizeof(version), &created));
    assert(firmware);
    assert(strcmp(version, "2.3.0") == 0);
    assert(created == 1760000000LL);

    assert(update_pending_format(buffer, sizeof(buffer), false, "1.1.0", 1760000001LL));
    assert(strcmp(buffer, "ui|1.1.0|1760000001") == 0);
    assert(update_pending_parse(buffer, &firmware, version, sizeof(version), &created));
    assert(!firmware);
    assert(strcmp(version, "1.1.0") == 0);
    assert(created == 1760000001LL);

    // Malformed entries are rejected.
    assert(!update_pending_parse("fw|2.3.0", &firmware, version, sizeof(version), &created));
    assert(!update_pending_parse("fw||1760000000", &firmware, version, sizeof(version), &created));
    assert(!update_pending_parse("fw|2.3.0|notanumber", &firmware, version, sizeof(version), &created));
    assert(!update_pending_parse("fw|2.3.0|0", &firmware, version, sizeof(version), &created));
    assert(!update_pending_parse("os|2.3.0|1760000000", &firmware, version, sizeof(version), &created));
    assert(!update_pending_parse(" bootloader|2.3.0|1760000000", &firmware, version, sizeof(version), &created));
    assert(!update_pending_parse(nullptr, &firmware, version, sizeof(version), &created));

    // Truncated version buffer fails instead of shortening.
    char tiny[4];
    assert(!update_pending_parse("fw|2.3.0|1760000000", &firmware, tiny, sizeof(tiny), &created));

    // Expiry: fresh entries are valid, entries older than seven days are not,
    // and entries without a sane clock context never count.
    const int64_t now = 1760000000LL;
    assert(update_pending_valid(now - 1, now));
    assert(update_pending_valid(now - 7LL * 24LL * 3600LL, now));
    assert(!update_pending_valid(now - 7LL * 24LL * 3600LL - 1, now));
    assert(!update_pending_valid(0, now));
    assert(!update_pending_valid(now, 0));
}

int main()
{
    test_mode_bounds();
    test_first_check_jitter();
    test_backoff_ladder();
    test_window_wraparound();
    test_time_and_idle_gates();
    test_auto_action();
    test_install_blocked_reason();
    test_stage_marker();
    test_pending_entry();
    std::puts("update_schedule_policy: all assertions passed");
    return 0;
}
