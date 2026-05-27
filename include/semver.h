#ifndef SEMVER_H
#define SEMVER_H

#include <stdio.h>

static inline int compareVersions(const char* v1, const char* v2) {
    int v1_parts[3] = {0, 0, 0};
    int v2_parts[3] = {0, 0, 0};
    const char* v1_pre = NULL;
    const char* v2_pre = NULL;

    // Parse major.minor.patch and optional pre-release tag
    (void)sscanf(v1, "%d.%d.%d", &v1_parts[0], &v1_parts[1], &v1_parts[2]);
    (void)sscanf(v2, "%d.%d.%d", &v2_parts[0], &v2_parts[1], &v2_parts[2]);

    // Find pre-release suffix (e.g. "-Beta.1")
    const char* dash = strchr(v1, '-');
    if (dash) v1_pre = dash + 1;
    dash = strchr(v2, '-');
    if (dash) v2_pre = dash + 1;

    for (int i = 0; i < 3; i++) {
        if (v1_parts[i] > v2_parts[i]) return 1;
        if (v1_parts[i] < v2_parts[i]) return -1;
    }

    // Pre-release versions are lower than release versions (semver spec)
    if (!v1_pre && !v2_pre) return 0;
    if (!v1_pre) return 1;   // v1 is release, v2 is pre-release → v1 > v2
    if (!v2_pre) return -1;  // v1 is pre-release, v2 is release → v1 < v2

    // Both have pre-release tags, compare lexicographically
    return strcmp(v1_pre, v2_pre);
}

#endif
