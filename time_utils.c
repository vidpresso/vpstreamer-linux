//
// Created by pauli on 4/18/17.
//
#include "time_utils.h"
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>


double VPMonotonicTime()
{
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);

    return (double)spec.tv_sec + (double)spec.tv_nsec/1.0e9;
}

bool VPMonotonicTimeSleepTo(double targetTime)
{
    double t = VPMonotonicTime();
    if (targetTime < t)
        return false;

    uint64_t tDiff_ns = (uint64_t) ((targetTime - t) * 1.0e9);

    struct timespec req, remain;
    memset(&req, 0, sizeof(req));
    memset(&remain, 0, sizeof(remain));
    req.tv_sec =  tDiff_ns/1000000000;
    req.tv_nsec = tDiff_ns%1000000000;

    while (nanosleep(&req, &remain)) {
        req = remain;
        memset(&remain, 0, sizeof(remain));
    }

    return true;
}
