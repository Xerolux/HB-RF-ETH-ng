#ifndef SEMVER_H
#define SEMVER_H

#include <stdio.h>

static inline int compareVersions(const char* v1, const char* v2) {
    int v1_parts[3] = {0, 0, 0};
    int v2_parts[3] = {0, 0, 0};

    sscanf(v1, "%d.%d.%d", &v1_parts[0], &v1_parts[1], &v1_parts[2]);
    sscanf(v2, "%d.%d.%d", &v2_parts[0], &v2_parts[1], &v2_parts[2]);

    for (int i = 0; i < 3; i++) {
        if (v1_parts[i] > v2_parts[i]) return 1;
        if (v1_parts[i] < v2_parts[i]) return -1;
    }

    return 0;
}

#endif
