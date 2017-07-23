//
// Created by pauli on 4/21/17.
//

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
