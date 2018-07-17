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


#include "image_utils.h"
#include <jpeglib.h>
#include <png.h>


int writePNG(const char *filename, int w, int h, size_t rowBytes, const uint8_t *srcBuf)
{
    FILE *outfile = fopen(filename, "wb");
    if (!outfile) {
        printf("Error opening output png file %s\n", filename);
        return 1;
    }

    png_byte color_type = PNG_COLOR_TYPE_RGBA;
    png_byte bit_depth = 8;

    png_structp png_ptr;
    png_infop info_ptr;
    int number_of_passes;
    png_bytep * row_pointers;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info_ptr = png_create_info_struct(png_ptr);

    png_init_io(png_ptr, outfile);


    png_set_IHDR(png_ptr, info_ptr, w, h,
                 bit_depth, color_type, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);

    for (int y = 0; y < h; y++) {
        png_bytep rowData = (png_bytep) (srcBuf + y*rowBytes);
        png_write_row(png_ptr, rowData);
    }

    png_write_end(png_ptr, NULL);


    fclose(outfile);
    return 0;
}


int writeJPEG(const char *filename, int w, int h, size_t rowBytes, const uint8_t *srcBuf, int numChannels, int jpegQuality)
{
    FILE *outfile = fopen(filename, "wb");
    if ( !outfile) {
        printf("Error opening output jpeg file %s\n", filename );
        return 1;
    }

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];

    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, outfile);
    cinfo.err = jpeg_std_error(&jerr);
    cinfo.image_width = (JDIMENSION)w;
    cinfo.image_height = (JDIMENSION)h;
    cinfo.input_components = numChannels;
    cinfo.in_color_space = (numChannels == 4) ? JCS_EXT_BGRA : JCS_RGB;
    cinfo.input_gamma = 1;

    jpeg_set_defaults(&cinfo);

    jpeg_set_quality(&cinfo, jpegQuality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    while(cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = (uint8_t *)(srcBuf + cinfo.next_scanline*rowBytes);
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(outfile);
    return 0;
}

int writeJPEGToMemory(int w, int h, size_t rowBytes, const uint8_t *srcBuf, int numChannels, int jpegQuality,
                      uint8_t **pDstBuf, size_t *pDstSize)
{
    if ( !pDstBuf || !pDstSize)
        return -1;

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];

    jpeg_create_compress(&cinfo);

    unsigned long dstSize = *pDstSize;
    jpeg_mem_dest(&cinfo, pDstBuf, &dstSize);

    cinfo.err = jpeg_std_error(&jerr);
    cinfo.image_width = (JDIMENSION)w;
    cinfo.image_height = (JDIMENSION)h;
    cinfo.input_components = numChannels;
    cinfo.in_color_space = (numChannels == 4) ? JCS_EXT_BGRA : JCS_RGB;
    cinfo.input_gamma = 1;

    jpeg_set_defaults(&cinfo);

    jpeg_set_quality(&cinfo, jpegQuality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    while(cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = (uint8_t *)(srcBuf + cinfo.next_scanline*rowBytes);
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    *pDstSize = dstSize;
    return 0;
}