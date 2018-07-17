//
// Created by pauli on 4/18/17.
//
/******************************************************************************
    Copyright (C) 2017 by Vidpresso Inc.
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

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
