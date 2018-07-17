//
// Created by pauli on 4/21/17.
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

#ifndef VPCONDUIT_IMAGE_UTILS_H
#define VPCONDUIT_IMAGE_UTILS_H

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>


#ifdef __cplusplus
extern "C" {
#endif

int writePNG(const char *filename, int w, int h, size_t rowBytes, const uint8_t *srcBuf);


int writeJPEG(const char *filename, int w, int h, size_t rowBytes, const uint8_t *srcBuf, int numChannels, int jpegQuality);

int writeJPEGToMemory(int w, int h, size_t rowBytes, const uint8_t *srcBuf, int numChannels, int jpegQuality,
                      uint8_t **pDstBuf, size_t *pDstSize);


#ifdef __cplusplus
}
#endif

#endif //VPCONDUIT_IMAGE_UTILS_H
