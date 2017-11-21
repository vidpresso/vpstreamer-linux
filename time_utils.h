//
// Created by pauli on 4/18/17.
//

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
