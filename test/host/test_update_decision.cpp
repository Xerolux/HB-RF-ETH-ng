#include "semver.h"

#include <cassert>

// The rule that decides whether the WebUI may offer an update:
//   a published version counts only when it is strictly newer than what runs.
//
// Testing this on the host matters because the alternative - a plain string
// inequality - looks correct on paper and then offers a device on 2.2.7-Beta.8
// a "newer" 2.2.5 the moment someone selects the stable channel.
static bool update_available(const char *published, const char *running)
{
    if (!running || !running[0] || !published || !published[0]) return false;
    return compareVersions(published, running) > 0;
}

int main()
{
    // Plain upgrades.
    assert(update_available("2.2.7", "2.2.5"));
    assert(update_available("2.3.0", "2.2.99"));

    // Identical versions are never an update.
    assert(!update_available("2.2.5", "2.2.5"));
    assert(!update_available("2.2.7-Beta.8", "2.2.7-Beta.8"));

    // Downgrades are never offered. This is the case that matters in the
    // field: a beta tester switching to the stable channel must not be told
    // that the older stable release is an update.
    assert(!update_available("2.2.5", "2.2.7-Beta.8"));
    assert(!update_available("2.2.4", "2.2.5"));

    // A release supersedes its own pre-releases.
    assert(update_available("2.2.7", "2.2.7-Beta.8"));
    assert(!update_available("2.2.7-Beta.8", "2.2.7"));

    // Pre-release ordering is numeric, not lexicographic: Beta.10 follows
    // Beta.9. A string comparison would get this backwards.
    assert(update_available("2.2.7-Beta.10", "2.2.7-Beta.9"));
    assert(!update_available("2.2.7-Beta.9", "2.2.7-Beta.10"));

    // A pre-release of a later version still beats an earlier release.
    assert(update_available("2.2.7-Beta.1", "2.2.6"));

    // WebUI versions use the same rule and the same comparator.
    assert(update_available("1.0.0", "1.0.0-Beta.17"));
    assert(!update_available("1.0.0", "1.0.0"));

    // Missing information is never an update: before the first successful
    // search, and on a device whose WebUI version could not be determined,
    // the answer must be "no", not a crash and not a false positive.
    assert(!update_available("2.2.7", ""));
    assert(!update_available("", "2.2.5"));
    assert(!update_available(nullptr, "2.2.5"));
    assert(!update_available("2.2.7", nullptr));

    return 0;
}
