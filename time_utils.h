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

#ifndef VPCONDUIT_TIME_UTILS_H
#define VPCONDUIT_TIME_UTILS_H

#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif


double VPMonotonicTime();

bool VPMonotonicTimeSleepTo(double targetTime);


#ifdef __cplusplus
}
#endif

#endif //VPCONDUIT_TIME_UTILS_H
