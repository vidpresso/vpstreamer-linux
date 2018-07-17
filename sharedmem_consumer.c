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

#include "sharedmem_consumer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <errno.h>



static int g_sharedMemFd = 0;
static VPConduitSharedMemData *g_sharedMemPtr = NULL;


VPConduitSharedMemData *vpconduit_shm_open_consumer(const char *filename) {
    if (!g_sharedMemFd) {
        int shm_fd = shm_open(filename, O_RDWR, 0666);

        if (shm_fd == -1) {
            printf("** couldn't create shmem, errno %d\n", errno);
        } else {
            void *shm_ptr = mmap(0, SHAREDMEM_FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
            if (shm_ptr == MAP_FAILED) {
                printf("** %s: mmap failed, errno %d\n", __func__, errno);
                close(shm_fd);
            } else {
                g_sharedMemFd = shm_fd;
                g_sharedMemPtr = shm_ptr;
            }
        }
    }
    return g_sharedMemPtr;
}

void vpconduit_shm_close_consumer()
{
    if (g_sharedMemPtr) {
        munmap(g_sharedMemPtr, SHAREDMEM_FILE_SIZE);
        g_sharedMemPtr = NULL;
    }
    if (g_sharedMemFd) {
        close(g_sharedMemFd);
        g_sharedMemFd = 0;
    }
}

VPConduitSharedMemData *vpconduit_shm_lock()
{
    if ( !g_sharedMemPtr)
        return NULL;

    pthread_mutex_lock(&(g_sharedMemPtr->mutex));

    return g_sharedMemPtr;
}

void vpconduit_shm_unlock()
{
    if ( !g_sharedMemPtr)
        return;

    pthread_mutex_unlock(&(g_sharedMemPtr->mutex));
}
