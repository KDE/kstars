/*
 * 1394-Based Digital Camera Control Library
 *
 * Bayer pattern decoding functions
 *
 * Written by Damien Douxchamps and Frederic Devernay
 * The original VNG and AHD Bayer decoding are from Dave Coffin's DCRAW.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <inttypes.h>
#include "bayer.h"


#define CLIP_FLOAT(in, out)\
    in = in < FLT_MIN ? FLT_MIN : in;\
    in = in > FLT_MAX ? FLT_MAX : in;\
    out=in;

void
ClearBorders_float(float * rgb, int sx, int sy, int w)
{
    int i, j;

    /* black edges:*/
    i = 3 * sx * w - 1;
    j = 3 * sx * sy - 1;
    while (i >= 0) {
        rgb[i--] = 0;
        rgb[j--] = 0;
    }

    int low = sx * (w - 1) * 3 - 1 + w * 3;
    i = low + sx * (sy - w * 2 + 1) * 3;
    while (i > low) {
        j = 6 * w;
        while (j > 0) {
            rgb[i--] = 0;
            j--;
        }
        i -= (sx - 2 * w) * 3;
    }

}

/**************************************************************
 *     Color conversion functions for cameras that can        *
 * output raw-Bayer pattern images, such as some Basler and   *
 * Point Grey camera. Most of the algos presented here come   *
 * from http://www-ise.stanford.edu/~tingchen/ and have been  *
 * converted from Matlab to C and extended to all elementary  *
 * patterns.                                                  *
 **************************************************************/

/* insprired by OpenCV's Bayer decoding */
dc1394error_t
dc1394_bayer_NearestNeighbor_float(const float * bayer, float * rgb, int sx, int sy, int offsetX, int offsetY, dc1394color_filter_t tile)
{
    const int bayerStep = sx;
    const int rgbStep = 3 * sx;
    int width = sx;
    int height = sy;
    int blue = tile == DC1394_COLOR_FILTER_BGGR
        || tile == DC1394_COLOR_FILTER_GBRG ? -1 : 1;
    int start_with_green = tile == DC1394_COLOR_FILTER_GBRG
        || tile == DC1394_COLOR_FILTER_GRBG;


    if ((tile>DC1394_COLOR_FILTER_MAX)||(tile<DC1394_COLOR_FILTER_MIN))
      return DC1394_INVALID_COLOR_FILTER;

    int i, iinc, imax;
    imax = sx * sy * 3;
    for (i = sx * (sy - 1) * 3; i < imax; i++) {
        rgb[i] = 0;
    }
    iinc = (sx - 1) * 3;
    for (i = (sx - 1) * 3; i < imax; i += iinc) {
        rgb[i++] = 0;
        rgb[i++] = 0;
        rgb[i++] = 0;
    }

    rgb += 1;
    height -= 1;
    width -= 1;

    if (offsetY == 1)
    {
        bayer += bayerStep;
        height--;
    }

    if (offsetX == 1)
    {
         bayer++;
    }

    for (; height--; bayer += bayerStep, rgb += rgbStep) {
      /*int t0, t1;*/
        const float *bayerEnd = bayer + width;

        if (start_with_green) {
            rgb[-blue] = bayer[1];
            rgb[0] = bayer[bayerStep + 1];
            rgb[blue] = bayer[bayerStep];
            bayer++;
            rgb += 3;
        }

        if (blue > 0) {
            for (; bayer <= bayerEnd - 2; bayer += 2, rgb += 6) {
                rgb[-1] = bayer[0];
                rgb[0] = bayer[1];
                rgb[1] = bayer[bayerStep + 1];

                rgb[2] = bayer[2];
                rgb[3] = bayer[bayerStep + 2];
                rgb[4] = bayer[bayerStep + 1];
            }
        } else {
            for (; bayer <= bayerEnd - 2; bayer += 2, rgb += 6) {
                rgb[1] = bayer[0];
                rgb[0] = bayer[1];
                rgb[-1] = bayer[bayerStep + 1];

                rgb[4] = bayer[2];
                rgb[3] = bayer[bayerStep + 2];
                rgb[2] = bayer[bayerStep + 1];
            }
        }

        if (bayer < bayerEnd) {
            rgb[-blue] = bayer[0];
            rgb[0] = bayer[1];
            rgb[blue] = bayer[bayerStep + 1];
            bayer++;
            rgb += 3;
        }

        bayer -= width;
        rgb -= width * 3;

        blue = -blue;
        start_with_green = !start_with_green;
    }

    return DC1394_SUCCESS;

}
/* OpenCV's Bayer decoding */
dc1394error_t
dc1394_bayer_Bilinear_float(const float * bayer, float * rgb, int sx, int sy, int offsetX, int offsetY, dc1394color_filter_t tile)
{
    const int bayerStep = sx;
    const int rgbStep = 3 * sx;
    int width = sx;
    int height = sy;
    int blue = tile == DC1394_COLOR_FILTER_BGGR
        || tile == DC1394_COLOR_FILTER_GBRG ? -1 : 1;
    int start_with_green = tile == DC1394_COLOR_FILTER_GBRG
        || tile == DC1394_COLOR_FILTER_GRBG;

    if ((tile>DC1394_COLOR_FILTER_MAX)||(tile<DC1394_COLOR_FILTER_MIN))
      return DC1394_INVALID_COLOR_FILTER;

    rgb += rgbStep + 3 + 1;
    height -= 2;
    width -= 2;

    if (offsetY == 1)
    {
        bayer += bayerStep;
        height--;
    }

    if (offsetX == 1)
    {
         bayer++;
    }

    for (; height--; bayer += bayerStep, rgb += rgbStep) {
        int t0, t1;
        const float *bayerEnd = bayer + width;

        if (start_with_green) {
            /* OpenCV has a bug in the next line, which was
               t0 = (bayer[0] + bayer[bayerStep * 2] + 1) / 2; */
            t0 = (bayer[1] + bayer[bayerStep * 2 + 1] + 1) / 2;
            t1 = (bayer[bayerStep] + bayer[bayerStep + 2] + 1) / 2;
            rgb[-blue] = (float) t0;
            rgb[0] = bayer[bayerStep + 1];
            rgb[blue] = (float) t1;
            bayer++;
            rgb += 3;
        }

        if (blue > 0) {
            for (; bayer <= bayerEnd - 2; bayer += 2, rgb += 6) {
                t0 = (bayer[0] + bayer[2] + bayer[bayerStep * 2] +
                      bayer[bayerStep * 2 + 2] + 2) / 4;
                t1 = (bayer[1] + bayer[bayerStep] +
                      bayer[bayerStep + 2] + bayer[bayerStep * 2 + 1] +
                      2) / 4;
                rgb[-1] = (float) t0;
                rgb[0] = (float) t1;
                rgb[1] = bayer[bayerStep + 1];

                t0 = (bayer[2] + bayer[bayerStep * 2 + 2] + 1) / 2;
                t1 = (bayer[bayerStep + 1] + bayer[bayerStep + 3] +
                      1) / 2;
                rgb[2] = (float) t0;
                rgb[3] = bayer[bayerStep + 2];
                rgb[4] = (float) t1;
            }
        } else {
            for (; bayer <= bayerEnd - 2; bayer += 2, rgb += 6) {
                t0 = (bayer[0] + bayer[2] + bayer[bayerStep * 2] +
                      bayer[bayerStep * 2 + 2] + 2) / 4;
                t1 = (bayer[1] + bayer[bayerStep] +
                      bayer[bayerStep + 2] + bayer[bayerStep * 2 + 1] +
                      2) / 4;
                rgb[1] = (float) t0;
                rgb[0] = (float) t1;
                rgb[-1] = bayer[bayerStep + 1];

                t0 = (bayer[2] + bayer[bayerStep * 2 + 2] + 1) / 2;
                t1 = (bayer[bayerStep + 1] + bayer[bayerStep + 3] +
                      1) / 2;
                rgb[4] = (float) t0;
                rgb[3] = bayer[bayerStep + 2];
                rgb[2] = (float) t1;
            }
        }

        if (bayer < bayerEnd) {
            t0 = (bayer[0] + bayer[2] + bayer[bayerStep * 2] +
                  bayer[bayerStep * 2 + 2] + 2) / 4;
            t1 = (bayer[1] + bayer[bayerStep] +
                  bayer[bayerStep + 2] + bayer[bayerStep * 2 + 1] +
                  2) / 4;
            rgb[-blue] = (float) t0;
            rgb[0] = (float) t1;
            rgb[blue] = bayer[bayerStep + 1];
            bayer++;
            rgb += 3;
        }

        bayer -= width;
        rgb -= width * 3;

        blue = -blue;
        start_with_green = !start_with_green;
    }

    return DC1394_SUCCESS;

}

/* High-Quality Linear Interpolation For Demosaicing Of
   Bayer-Patterned Color Images, by Henrique S. Malvar, Li-wei He, and
   Ross Cutler, in ICASSP'04 */
dc1394error_t
dc1394_bayer_HQLinear_float(const float * bayer, float * rgb, int sx, int sy, int offsetX, int offsetY, dc1394color_filter_t tile)
{
    const int bayerStep = sx;
    const int rgbStep = 3 * sx;
    int width = sx;
    int height = sy;
    /*
       the two letters  of the OpenCV name are respectively
       the 4th and 3rd letters from the blinky name,
       and we also have to switch R and B (OpenCV is BGR)

       CV_BayerBG2BGR <-> DC1394_COLOR_FILTER_BGGR
       CV_BayerGB2BGR <-> DC1394_COLOR_FILTER_GBRG
       CV_BayerGR2BGR <-> DC1394_COLOR_FILTER_GRBG

       int blue = tile == CV_BayerBG2BGR || tile == CV_BayerGB2BGR ? -1 : 1;
       int start_with_green = tile == CV_BayerGB2BGR || tile == CV_BayerGR2BGR;
     */
    int blue = tile == DC1394_COLOR_FILTER_BGGR
        || tile == DC1394_COLOR_FILTER_GBRG ? -1 : 1;
    int start_with_green = tile == DC1394_COLOR_FILTER_GBRG
        || tile == DC1394_COLOR_FILTER_GRBG;

    if ((tile>DC1394_COLOR_FILTER_MAX)||(tile<DC1394_COLOR_FILTER_MIN))
      return DC1394_INVALID_COLOR_FILTER;

    ClearBorders_float(rgb, sx, sy, 2);
    rgb += 2 * rgbStep + 6 + 1;
    height -= 4;
    width -= 4;

    if (offsetY == 1)
    {
        bayer += bayerStep;
        height--;
    }

    if (offsetX == 1)
    {
         bayer++;
    }

    /* We begin with a (+1 line,+1 column) offset with respect to bilinear decoding, so start_with_green is the same, but blue is opposite */
    blue = -blue;

    for (; height--; bayer += bayerStep, rgb += rgbStep) {
        int t0, t1;
        const float *bayerEnd = bayer + width;
        const int bayerStep2 = bayerStep * 2;
        const int bayerStep3 = bayerStep * 3;
        const int bayerStep4 = bayerStep * 4;

        if (start_with_green) {
            /* at green pixel */
            rgb[0] = bayer[bayerStep2 + 2];
            t0 = rgb[0] * 5
                + ((bayer[bayerStep + 2] + bayer[bayerStep3 + 2]) * 4)
                - bayer[2]
                - bayer[bayerStep + 1]
                - bayer[bayerStep + 3]
                - bayer[bayerStep3 + 1]
                - bayer[bayerStep3 + 3]
                - bayer[bayerStep4 + 2]
                + ((bayer[bayerStep2] + bayer[bayerStep2 + 4] + 1) / 2);
            t1 = rgb[0] * 5 +
                ((bayer[bayerStep2 + 1] + bayer[bayerStep2 + 3]) * 4)
                - bayer[bayerStep2]
                - bayer[bayerStep + 1]
                - bayer[bayerStep + 3]
                - bayer[bayerStep3 + 1]
                - bayer[bayerStep3 + 3]
                - bayer[bayerStep2 + 4]
                + ((bayer[2] + bayer[bayerStep4 + 2] + 1) / 2);
            t0 = (t0 + 4) >> 3;
            CLIP_FLOAT(t0, rgb[-blue]);
            t1 = (t1 + 4) >> 3;
            CLIP_FLOAT(t1, rgb[blue]);
            bayer++;
            rgb += 3;
        }

        if (blue > 0) {
            for (; bayer <= bayerEnd - 2; bayer += 2, rgb += 6) {
                /* B at B */
                rgb[1] = bayer[bayerStep2 + 2];
                /* R at B */
                t0 = ((bayer[bayerStep + 1] + bayer[bayerStep + 3] +
                       bayer[bayerStep3 + 1] + bayer[bayerStep3 + 3]) * 2)
                    -
                    (((bayer[2] + bayer[bayerStep2] +
                       bayer[bayerStep2 + 4] + bayer[bayerStep4 +
                                                     2]) * 3 + 1) / 2)
                    + rgb[1] * 6;
                /* G at B */
                t1 = ((bayer[bayerStep + 2] + bayer[bayerStep2 + 1] +
                       bayer[bayerStep2 + 3] + bayer[bayerStep * 3 +
                                                     2]) * 2)
                    - (bayer[2] + bayer[bayerStep2] +
                       bayer[bayerStep2 + 4] + bayer[bayerStep4 + 2])
                    + (rgb[1] * 4);
                t0 = (t0 + 4) >> 3;
                CLIP_FLOAT(t0, rgb[-1]);
                t1 = (t1 + 4) >> 3;
                CLIP_FLOAT(t1, rgb[0]);
                /* at green pixel */
                rgb[3] = bayer[bayerStep2 + 3];
                t0 = rgb[3] * 5
                    + ((bayer[bayerStep + 3] + bayer[bayerStep3 + 3]) * 4)
                    - bayer[3]
                    - bayer[bayerStep + 2]
                    - bayer[bayerStep + 4]
                    - bayer[bayerStep3 + 2]
                    - bayer[bayerStep3 + 4]
                    - bayer[bayerStep4 + 3]
                    +
                    ((bayer[bayerStep2 + 1] + bayer[bayerStep2 + 5] +
                      1) / 2);
                t1 = rgb[3] * 5 +
                    ((bayer[bayerStep2 + 2] + bayer[bayerStep2 + 4]) * 4)
                    - bayer[bayerStep2 + 1]
                    - bayer[bayerStep + 2]
                    - bayer[bayerStep + 4]
                    - bayer[bayerStep3 + 2]
                    - bayer[bayerStep3 + 4]
                    - bayer[bayerStep2 + 5]
                    + ((bayer[3] + bayer[bayerStep4 + 3] + 1) / 2);
                t0 = (t0 + 4) >> 3;
                CLIP_FLOAT(t0, rgb[2]);
                t1 = (t1 + 4) >> 3;
                CLIP_FLOAT(t1, rgb[4]);
            }
        } else {
            for (; bayer <= bayerEnd - 2; bayer += 2, rgb += 6) {
                /* R at R */
                rgb[-1] = bayer[bayerStep2 + 2];
                /* B at R */
                t0 = ((bayer[bayerStep + 1] + bayer[bayerStep + 3] +
                       bayer[bayerStep * 3 + 1] + bayer[bayerStep3 +
                                                        3]) * 2)
                    -
                    (((bayer[2] + bayer[bayerStep2] +
                       bayer[bayerStep2 + 4] + bayer[bayerStep4 +
                                                     2]) * 3 + 1) / 2)
                    + rgb[-1] * 6;
                /* G at R */
                t1 = ((bayer[bayerStep + 2] + bayer[bayerStep2 + 1] +
                       bayer[bayerStep2 + 3] + bayer[bayerStep3 + 2]) * 2)
                    - (bayer[2] + bayer[bayerStep2] +
                       bayer[bayerStep2 + 4] + bayer[bayerStep4 + 2])
                    + (rgb[-1] * 4);
                t0 = (t0 + 4) >> 3;
                CLIP_FLOAT(t0, rgb[1]);
                t1 = (t1 + 4) >> 3;
                CLIP_FLOAT(t1, rgb[0]);

                /* at green pixel */
                rgb[3] = bayer[bayerStep2 + 3];
                t0 = rgb[3] * 5
                    + ((bayer[bayerStep + 3] + bayer[bayerStep3 + 3]) * 4)
                    - bayer[3]
                    - bayer[bayerStep + 2]
                    - bayer[bayerStep + 4]
                    - bayer[bayerStep3 + 2]
                    - bayer[bayerStep3 + 4]
                    - bayer[bayerStep4 + 3]
                    +
                    ((bayer[bayerStep2 + 1] + bayer[bayerStep2 + 5] +
                      1) / 2);
                t1 = rgb[3] * 5 +
                    ((bayer[bayerStep2 + 2] + bayer[bayerStep2 + 4]) * 4)
                    - bayer[bayerStep2 + 1]
                    - bayer[bayerStep + 2]
                    - bayer[bayerStep + 4]
                    - bayer[bayerStep3 + 2]
                    - bayer[bayerStep3 + 4]
                    - bayer[bayerStep2 + 5]
                    + ((bayer[3] + bayer[bayerStep4 + 3] + 1) / 2);
                t0 = (t0 + 4) >> 3;
                CLIP_FLOAT(t0, rgb[4]);
                t1 = (t1 + 4) >> 3;
                CLIP_FLOAT(t1, rgb[2]);
            }
        }

        if (bayer < bayerEnd) {
            /* B at B */
            rgb[blue] = bayer[bayerStep2 + 2];
            /* R at B */
            t0 = ((bayer[bayerStep + 1] + bayer[bayerStep + 3] +
                   bayer[bayerStep3 + 1] + bayer[bayerStep3 + 3]) * 2)
                -
                (((bayer[2] + bayer[bayerStep2] +
                   bayer[bayerStep2 + 4] + bayer[bayerStep4 +
                                                 2]) * 3 + 1) / 2)
                + rgb[blue] * 6;
            /* G at B */
            t1 = (((bayer[bayerStep + 2] + bayer[bayerStep2 + 1] +
                    bayer[bayerStep2 + 3] + bayer[bayerStep3 + 2])) * 2)
                - (bayer[2] + bayer[bayerStep2] +
                   bayer[bayerStep2 + 4] + bayer[bayerStep4 + 2])
                + (rgb[blue] * 4);
            t0 = (t0 + 4) >> 3;
            CLIP_FLOAT(t0, rgb[-blue]);
            t1 = (t1 + 4) >> 3;
            CLIP_FLOAT(t1, rgb[0]);
            bayer++;
            rgb += 3;
        }

        bayer -= width;
        rgb -= width * 3;

        blue = -blue;
        start_with_green = !start_with_green;
    }

    return DC1394_SUCCESS;
}

/* coriander's Bayer decoding */
dc1394error_t
dc1394_bayer_Simple_float(const float * bayer, float * rgb, int sx, int sy, int offsetX, int offsetY, dc1394color_filter_t tile)
{
    float *outR, *outG, *outB;
    register int i, j;
    int tmp, base;

    /* sx and sy should be even*/
    switch (tile) {
    case DC1394_COLOR_FILTER_GRBG:
    case DC1394_COLOR_FILTER_BGGR:
        outR = &rgb[0];
        outG = &rgb[1];
        outB = &rgb[2];
        break;
    case DC1394_COLOR_FILTER_GBRG:
    case DC1394_COLOR_FILTER_RGGB:
        outR = &rgb[2];
        outG = &rgb[1];
        outB = &rgb[0];
        break;
    default:
      return DC1394_INVALID_COLOR_FILTER;
    }

    switch (tile) {
    case DC1394_COLOR_FILTER_GRBG:
    case DC1394_COLOR_FILTER_BGGR:
        outR = &rgb[0];
        outG = &rgb[1];
        outB = &rgb[2];
        break;
    case DC1394_COLOR_FILTER_GBRG:
    case DC1394_COLOR_FILTER_RGGB:
        outR = &rgb[2];
        outG = &rgb[1];
        outB = &rgb[0];
        break;
    default:
        outR = NULL;
        outG = NULL;
        outB = NULL;
        break;
    }

    switch (tile) {
    case DC1394_COLOR_FILTER_GRBG:
    case DC1394_COLOR_FILTER_GBRG:
        for (i = 0; i < sy - 1; i += 2) {
            for (j = 0; j < sx - 1; j += 2) {
                base = i * sx + j;
                tmp = ((bayer[base] + bayer[base + sx + 1]) / 2);
                CLIP_FLOAT(tmp, outG[base * 3]);
                tmp = bayer[base + 1];
                CLIP_FLOAT(tmp, outR[base * 3]);
                tmp = bayer[base + sx];
                CLIP_FLOAT(tmp, outB[base * 3]);
            }
        }
        for (i = 0; i < sy - 1; i += 2) {
            for (j = 1; j < sx - 1; j += 2) {
                base = i * sx + j;
                tmp = ((bayer[base + 1] + bayer[base + sx]) / 2);
                CLIP_FLOAT(tmp, outG[(base) * 3]);
                tmp = bayer[base];
                CLIP_FLOAT(tmp, outR[(base) * 3]);
                tmp = bayer[base + 1 + sx];
                CLIP_FLOAT(tmp, outB[(base) * 3]);
            }
        }
        for (i = 1; i < sy - 1; i += 2) {
            for (j = 0; j < sx - 1; j += 2) {
                base = i * sx + j;
                tmp = ((bayer[base + sx] + bayer[base + 1]) / 2);
                CLIP_FLOAT(tmp, outG[base * 3]);
                tmp = bayer[base + sx + 1];
                CLIP_FLOAT(tmp, outR[base * 3]);
                tmp = bayer[base];
                CLIP_FLOAT(tmp, outB[base * 3]);
            }
        }
        for (i = 1; i < sy - 1; i += 2) {
            for (j = 1; j < sx - 1; j += 2) {
                base = i * sx + j;
                tmp = ((bayer[base] + bayer[base + 1 + sx]) / 2);
                CLIP_FLOAT(tmp, outG[(base) * 3]);
                tmp = bayer[base + sx];
                CLIP_FLOAT(tmp, outR[(base) * 3]);
                tmp = bayer[base + 1];
                CLIP_FLOAT(tmp, outB[(base) * 3]);
            }
        }
        break;
    case DC1394_COLOR_FILTER_BGGR:
    case DC1394_COLOR_FILTER_RGGB:
        for (i = 0; i < sy - 1; i += 2) {
            for (j = 0; j < sx - 1; j += 2) {
                base = i * sx + j;
                tmp = ((bayer[base + sx] + bayer[base + 1]) / 2);
                CLIP_FLOAT(tmp, outG[base * 3]);
                tmp = bayer[base + sx + 1];
                CLIP_FLOAT(tmp, outR[base * 3]);
                tmp = bayer[base];
                CLIP_FLOAT(tmp, outB[base * 3]);
            }
        }
        for (i = 1; i < sy - 1; i += 2) {
            for (j = 0; j < sx - 1; j += 2) {
                base = i * sx + j;
                tmp = ((bayer[base] + bayer[base + 1 + sx]) / 2);
                CLIP_FLOAT(tmp, outG[(base) * 3]);
                tmp = bayer[base + 1];
                CLIP_FLOAT(tmp, outR[(base) * 3]);
                tmp = bayer[base + sx];
                CLIP_FLOAT(tmp, outB[(base) * 3]);
            }
        }
        for (i = 0; i < sy - 1; i += 2) {
            for (j = 1; j < sx - 1; j += 2) {
                base = i * sx + j;
                tmp = ((bayer[base] + bayer[base + sx + 1]) / 2);
                CLIP_FLOAT(tmp, outG[base * 3]);
                tmp = bayer[base + sx];
                CLIP_FLOAT(tmp, outR[base * 3]);
                tmp = bayer[base + 1];
                CLIP_FLOAT(tmp, outB[base * 3]);
            }
        }
        for (i = 1; i < sy - 1; i += 2) {
            for (j = 1; j < sx - 1; j += 2) {
                base = i * sx + j;
                tmp = ((bayer[base + 1] + bayer[base + sx]) / 2);
                CLIP_FLOAT(tmp, outG[(base) * 3]);
                tmp = bayer[base];
                CLIP_FLOAT(tmp, outR[(base) * 3]);
                tmp = bayer[base + 1 + sx];
                CLIP_FLOAT(tmp, outB[(base) * 3]);
            }
        }
        break;
    }

    /* add black border
    for (i = sx * (sy - 1) * 3; i < sx * sy * 3; i++) {
        rgb[i] = 0;
    }
    for (i = (sx - 1) * 3; i < sx * sy * 3; i += (sx - 1) * 3) {
        rgb[i++] = 0;
        rgb[i++] = 0;
        rgb[i++] = 0;
    }
    */

    return DC1394_SUCCESS;

}

/* Variable Number of Gradients, from dcraw <http://www.cybercom.net/~dcoffin/dcraw/> */
/* Ported to libdc1394 by Frederic Devernay */

#define FORC3 for (c=0; c < 3; c++)

#define SQR(x) ((x)*(x))
#define ABS(x) (((int)(x) ^ ((int)(x) >> 31)) - ((int)(x) >> 31))
#ifndef MIN
  #define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
  #define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
#define LIM(x,min,max) MAX(min,MIN(x,max))
#define ULIM(x,y,z) ((y) < (z) ? LIM(x,y,z) : LIM(x,z,y))
/*
   In order to inline this calculation, I make the risky
   assumption that all filter patterns can be described
   by a repeating pattern of eight rows and two columns

   Return values are either 0/1/2/3 = G/M/C/Y or 0/1/2/3 = R/G1/B/G2
 */
#define FC(row,col) \
        (filters >> ((((row) * 2 & 14) + ((col) & 1)) * 2) & 3)

/*
   This algorithm is officially called:

   "Interpolation using a Threshold-based variable number of gradients"

   described in http://www-ise.stanford.edu/~tingchen/algodep/vargra.html

   I've extended the basic idea to work with non-Bayer filter arrays.
   Gradients are numbered clockwise from NW=0 to W=7.
 */
static const signed char bayervng_terms[] = {
    -2,-2,+0,-1,0,0x01, -2,-2,+0,+0,1,0x01, -2,-1,-1,+0,0,0x01,
    -2,-1,+0,-1,0,0x02, -2,-1,+0,+0,0,0x03, -2,-1,+0,+1,1,0x01,
    -2,+0,+0,-1,0,0x06, -2,+0,+0,+0,1,0x02, -2,+0,+0,+1,0,0x03,
    -2,+1,-1,+0,0,0x04, -2,+1,+0,-1,1,0x04, -2,+1,+0,+0,0,0x06,
    -2,+1,+0,+1,0,0x02, -2,+2,+0,+0,1,0x04, -2,+2,+0,+1,0,0x04,
    -1,-2,-1,+0,0,0x80, -1,-2,+0,-1,0,0x01, -1,-2,+1,-1,0,0x01,
    -1,-2,+1,+0,1,0x01, -1,-1,-1,+1,0,0x88, -1,-1,+1,-2,0,0x40,
    -1,-1,+1,-1,0,0x22, -1,-1,+1,+0,0,0x33, -1,-1,+1,+1,1,0x11,
    -1,+0,-1,+2,0,0x08, -1,+0,+0,-1,0,0x44, -1,+0,+0,+1,0,0x11,
    -1,+0,+1,-2,1,0x40, -1,+0,+1,-1,0,0x66, -1,+0,+1,+0,1,0x22,
    -1,+0,+1,+1,0,0x33, -1,+0,+1,+2,1,0x10, -1,+1,+1,-1,1,0x44,
    -1,+1,+1,+0,0,0x66, -1,+1,+1,+1,0,0x22, -1,+1,+1,+2,0,0x10,
    -1,+2,+0,+1,0,0x04, -1,+2,+1,+0,1,0x04, -1,+2,+1,+1,0,0x04,
    +0,-2,+0,+0,1,0x80, +0,-1,+0,+1,1,0x88, +0,-1,+1,-2,0,0x40,
    +0,-1,+1,+0,0,0x11, +0,-1,+2,-2,0,0x40, +0,-1,+2,-1,0,0x20,
    +0,-1,+2,+0,0,0x30, +0,-1,+2,+1,1,0x10, +0,+0,+0,+2,1,0x08,
    +0,+0,+2,-2,1,0x40, +0,+0,+2,-1,0,0x60, +0,+0,+2,+0,1,0x20,
    +0,+0,+2,+1,0,0x30, +0,+0,+2,+2,1,0x10, +0,+1,+1,+0,0,0x44,
    +0,+1,+1,+2,0,0x10, +0,+1,+2,-1,1,0x40, +0,+1,+2,+0,0,0x60,
    +0,+1,+2,+1,0,0x20, +0,+1,+2,+2,0,0x10, +1,-2,+1,+0,0,0x80,
    +1,-1,+1,+1,0,0x88, +1,+0,+1,+2,0,0x08, +1,+0,+2,-1,0,0x40,
    +1,+0,+2,+1,0,0x10
}, bayervng_chood[] = { -1,-1, -1,0, -1,+1, 0,+1, +1,+1, +1,0, +1,-1, 0,-1 };

dc1394error_t
dc1394_bayer_VNG_float(const float * bayer, float * dst, int sx, int sy, int offsetX, int offsetY, dc1394color_filter_t pattern)
{
    const int height = sy, width = sx;
    static const signed char *cp;
    /* the following has the same type as the image */
    float (*brow[5])[3], *pix;          /* [FD] */
    int code[8][2][320], *ip, gval[8], gmin, gmax, sum[4];
    int row, col, x, y, x1, x2, y1, y2, t, weight, grads, diag;
    int g, diff, thold, num;
    uint32_t c, color;
    uint32_t filters;                     /* [FD] */

    /* first, use bilinear bayer decoding */

    dc1394_bayer_Bilinear_float(bayer, dst, sx, sy, offsetX, offsetY, pattern);

    switch(pattern) {
    case DC1394_COLOR_FILTER_BGGR:
        filters = 0x16161616;
        break;
    case DC1394_COLOR_FILTER_GRBG:
        filters = 0x61616161;
        break;
    case DC1394_COLOR_FILTER_RGGB:
        filters = 0x94949494;
        break;
    case DC1394_COLOR_FILTER_GBRG:
        filters = 0x49494949;
        break;
    default:
        return DC1394_INVALID_COLOR_FILTER;
    }

    for (row=0; row < 8; row++) {                /* Precalculate for VNG */
        for (col=0; col < 2; col++) {
            ip = code[row][col];
            for (cp=bayervng_terms, t=0; t < 64; t++) {
                y1 = *cp++;  x1 = *cp++;
                y2 = *cp++;  x2 = *cp++;
                weight = *cp++;
                grads = *cp++;
                color = FC(row+y1,col+x1);
                if (FC(row+y2,col+x2) != color) continue;
                diag = (FC(row,col+1) == color && FC(row+1,col) == color) ? 2:1;
                if (abs(y1-y2) == diag && abs(x1-x2) == diag) continue;
                *ip++ = (y1*width + x1)*3 + color; /* [FD] */
                *ip++ = (y2*width + x2)*3 + color; /* [FD] */
                *ip++ = weight;
                for (g=0; g < 8; g++)
                    if (grads & 1<<g) *ip++ = g;
                *ip++ = -1;
            }
            *ip++ = INT_MAX;
            for (cp=bayervng_chood, g=0; g < 8; g++) {
                y = *cp++;  x = *cp++;
                *ip++ = (y*width + x) * 3;      /* [FD] */
                color = FC(row,col);
                if (FC(row+y,col+x) != color && FC(row+y*2,col+x*2) == color)
                    *ip++ = (y*width + x) * 6 + color; /* [FD] */
                else
                    *ip++ = 0;
            }
        }
    }
    brow[4] = calloc (width*3, sizeof **brow);
    /*merror (brow[4], "vng_interpolate()");*/
    for (row=0; row < 3; row++)
        brow[row] = brow[4] + row*width;
    for (row=2; row < height-2; row++) {                /* Do VNG interpolation */
        for (col=2; col < width-2; col++) {
            pix = dst + (row*width+col)*3;  /* [FD] */
            ip = code[row & 7][col & 1];
            memset (gval, 0, sizeof gval);
            while ((g = ip[0]) != INT_MAX) {                /* Calculate gradients */
                diff = ABS(pix[g] - pix[ip[1]]) << ip[2];
                gval[ip[3]] += diff;
                ip += 5;
                if ((g = ip[-1]) == -1) continue;
                gval[g] += diff;
                while ((g = *ip++) != -1)
                    gval[g] += diff;
            }
            ip++;
            gmin = gmax = gval[0];                        /* Choose a threshold */
            for (g=1; g < 8; g++) {
                if (gmin > gval[g]) gmin = gval[g];
                if (gmax < gval[g]) gmax = gval[g];
            }
            if (gmax == 0) {
                memcpy (brow[2][col], pix, 3 * sizeof *dst); /* [FD] */
                continue;
            }
            thold = gmin + (gmax / 2);
            memset (sum, 0, sizeof sum);
            color = FC(row,col);
            for (num=g=0; g < 8; g++,ip+=2) {                /* Average the neighbors */
                if (gval[g] <= thold) {
                    for (c=0; c < 3; c++)         /* [FD] */
                        if (c == color && ip[1])
                            sum[c] += (pix[c] + pix[ip[1]]) / 2;
                        else
                            sum[c] += pix[ip[0] + c];
                    num++;
                }
            }
            for (c=0; c < 3; c++) {           /* [FD] Save to buffer */
                t = pix[color];
                if (c != color)
                    t += (sum[c] - sum[color]) / num;
                CLIP_FLOAT(t,brow[2][col][c]);        /* [FD] */
            }
        }
        if (row > 3)                                /* Write buffer to image */
            memcpy (dst + 3*((row-2)*width+2), brow[0]+2, (width-4)*3*sizeof *dst); /* [FD] */
        for (g=0; g < 4; g++)
            brow[(g-1) & 3] = brow[g];
    }
    memcpy (dst + 3*((row-2)*width+2), brow[0]+2, (width-4)*3*sizeof *dst);
    memcpy (dst + 3*((row-1)*width+2), brow[1]+2, (width-4)*3*sizeof *dst);
    free (brow[4]);

    return DC1394_SUCCESS;
}



/* AHD interpolation ported from dcraw to libdc1394 by Samuel Audet */
static dc1394bool_t ahd_inited = DC1394_FALSE; /* WARNING: not multi-processor safe */

#define CLIPOUT(x)        LIM(x,0,255)
#define CLIPOUT_FLOAT(x) LIM(x,FLT_MIN,FLT_MAX)

static const double xyz_rgb[3][3] = {                        /* XYZ from RGB */
  { 0.412453, 0.357580, 0.180423 },
  { 0.212671, 0.715160, 0.072169 },
  { 0.019334, 0.119193, 0.950227 } };
static const float d65_white[3] = { 0.950456, 1, 1.088754 };

static void cam_to_cielab (float cam[3], float lab[3]) /* [SA] */
{
    int c, i, j;
    float r, xyz[3];
    static float cbrt[0x10000], xyz_cam[3][4];

    if (cam == NULL) {
        for (i=0; i < 0x10000; i++) {
            r = i / 65535.0;
            cbrt[i] = r > 0.008856 ? pow(r,1/3.0) : 7.787*r + 16/116.0;
        }
        for (i=0; i < 3; i++)
            for (j=0; j < 3; j++)                           /* [SA] */
                xyz_cam[i][j] = xyz_rgb[i][j] / d65_white[i]; /* [SA] */
    } else {
        xyz[0] = xyz[1] = xyz[2] = 0.5;
        FORC3 { /* [SA] */
            xyz[0] += xyz_cam[0][c] * cam[c];
            xyz[1] += xyz_cam[1][c] * cam[c];
            xyz[2] += xyz_cam[2][c] * cam[c];
        }
        xyz[0] = cbrt[(int) CLIPOUT_FLOAT(xyz[0])];        /* [SA] */
        xyz[1] = cbrt[(int) CLIPOUT_FLOAT(xyz[1])];        /* [SA] */
        xyz[2] = cbrt[(int) CLIPOUT_FLOAT(xyz[2])];        /* [SA] */
        lab[0] = 116 * xyz[1] - 16;
        lab[1] = 500 * (xyz[0] - xyz[1]);
        lab[2] = 200 * (xyz[1] - xyz[2]);
    }
}

dc1394error_t
dc1394_bayer_decoding_float(const float * bayer, float * rgb, uint32_t sx, uint32_t sy, int offsetX, int offsetY, dc1394color_filter_t tile, dc1394bayer_method_t method)
{
    switch (method) {
    case DC1394_BAYER_METHOD_NEAREST:
        return dc1394_bayer_NearestNeighbor_float(bayer, rgb, sx, sy, offsetX, offsetY, tile);
    case DC1394_BAYER_METHOD_SIMPLE:
        return dc1394_bayer_Simple_float(bayer, rgb, sx, sy,  offsetX, offsetY, tile);
    case DC1394_BAYER_METHOD_BILINEAR:
        return dc1394_bayer_Bilinear_float(bayer, rgb, sx, sy,  offsetX, offsetY, tile);
    case DC1394_BAYER_METHOD_HQLINEAR:
        return dc1394_bayer_HQLinear_float(bayer, rgb, sx, sy,  offsetX, offsetY, tile);
    case DC1394_BAYER_METHOD_VNG:
        return dc1394_bayer_VNG_float(bayer, rgb, sx, sy, offsetX, offsetY, tile);
    default:
        return DC1394_INVALID_BAYER_METHOD;
    }

}

