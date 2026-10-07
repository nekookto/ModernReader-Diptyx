#pragma once
// Progressive JPEG support for the e-reader.
//
// The normal JPEG decoder (TJpgDec) only understands baseline JPEG files. Most commercial EPUBs (manga, light novels,
// anything exported from InDesign or Photoshop with "progressive" turned on) contain progressive JPEGs, which used to be
// skipped. This small decoder reads a progressive JPEG and produces only the brightness (luma) channel, which is all a
// black and white e-ink screen needs. To keep memory use low it only keeps the lowest N x N frequencies of every 8 x 8
// block and reconstructs the picture at N/8 of its size (N = 1..8), so a 1400 x 2000 picture needs about 1 MB of RAM.
#include <stddef.h>
#include <stdint.h>

struct ProgressiveJpegInfo {
    int width = 0;
    int height = 0;
    int components = 0;
    bool progressive = false;   // true when the file is a progressive (SOF2) JPEG
};

// Reads the frame header only. Returns false when the data is not a usable JPEG.
bool pjpeg_get_info(const uint8_t *data, size_t size, ProgressiveJpegInfo *info);

// Decodes the luma channel at scaleN/8 of the original size (scaleN: 1..8). The output is width*height bytes
// (0 = black, 255 = white), allocated with malloc(); the caller frees it. maxBytes limits the working memory:
// when the picture would need more, the scale is lowered automatically (the result is then smaller than requested).
// Returns false on error. outW/outH receive the size of the decoded picture.
bool pjpeg_decode_gray(const uint8_t *data, size_t size, int scaleN, size_t maxBytes,
                       uint8_t **out, int *outW, int *outH);

// Smallest scale (1..8) at which a picture of srcW x srcH is at least targetW x targetH.
int pjpeg_pick_scale(int srcW, int srcH, int targetW, int targetH);
