//
// Created by pauli on 5/25/17.
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

#ifndef VPSTREAMER_SHAREDMEM_CONSUMER_H
#define VPSTREAMER_SHAREDMEM_CONSUMER_H

#include <stdint.h>
#include <pthread.h>


#define SHAREDMEM_HEADER_SIZE   (sizeof(uint64_t) + sizeof(pthread_mutex_t) + sizeof(int64_t) + 6*sizeof(int32_t))
#define SHAREDMEM_DATA_SIZE     (32*1024*1024)
#define SHAREDMEM_FILE_SIZE     (SHAREDMEM_HEADER_SIZE + SHAREDMEM_DATA_SIZE)

#define SHAREDMEM_MAGIC_NUM     (uint64_t)0xc0c1c2c3ffccafcf


typedef struct {
    uint64_t magic;
    pthread_mutex_t mutex;
    int64_t msgId;
    int32_t msgDataSize;
    int32_t w;
    int32_t h;
    int32_t audioDataSize;
    int32_t propsDataSize;  // for extension
    int32_t dataCapacity;
    int8_t data[SHAREDMEM_DATA_SIZE];
} VPConduitSharedMemData;



#ifdef __cplusplus
extern "C" {
#endif


VPConduitSharedMemData *vpconduit_shm_open_consumer(const char *filename);

void vpconduit_shm_close_consumer();

// access data with the shared mutex lock acquired
VPConduitSharedMemData *vpconduit_shm_lock();

void vpconduit_shm_unlock();


#ifdef __cplusplus
}
#endif

#endif //VPSTREAMER_SHAREDMEM_CONSUMER_H
