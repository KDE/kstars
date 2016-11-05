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

#ifndef BAYER_H
#define BAYER_H

#include <stdint.h>

#define restrict __restrict

/**
 * Error codes returned by most libdc1394 functions.
 *
 * General rule: 0 is success, negative denotes a problem.
 */
typedef enum {
    DC1394_SUCCESS                     =  0,
    DC1394_FAILURE                     = -1,
    DC1394_NOT_A_CAMERA                = -2,
    DC1394_FUNCTION_NOT_SUPPORTED      = -3,
    DC1394_CAMERA_NOT_INITIALIZED      = -4,
    DC1394_MEMORY_ALLOCATION_FAILURE   = -5,
    DC1394_TAGGED_REGISTER_NOT_FOUND   = -6,
    DC1394_NO_ISO_CHANNEL              = -7,
    DC1394_NO_BANDWIDTH                = -8,
    DC1394_IOCTL_FAILURE               = -9,
    DC1394_CAPTURE_IS_NOT_SET          = -10,
    DC1394_CAPTURE_IS_RUNNING          = -11,
    DC1394_RAW1394_FAILURE             = -12,
    DC1394_FORMAT7_ERROR_FLAG_1        = -13,
    DC1394_FORMAT7_ERROR_FLAG_2        = -14,
    DC1394_INVALID_ARGUMENT_VALUE      = -15,
    DC1394_REQ_VALUE_OUTSIDE_RANGE     = -16,
    DC1394_INVALID_FEATURE             = -17,
    DC1394_INVALID_VIDEO_FORMAT        = -18,
    DC1394_INVALID_VIDEO_MODE          = -19,
    DC1394_INVALID_FRAMERATE           = -20,
    DC1394_INVALID_TRIGGER_MODE        = -21,
    DC1394_INVALID_TRIGGER_SOURCE      = -22,
    DC1394_INVALID_ISO_SPEED           = -23,
    DC1394_INVALID_IIDC_VERSION        = -24,
    DC1394_INVALID_COLOR_CODING        = -25,
    DC1394_INVALID_COLOR_FILTER        = -26,
    DC1394_INVALID_CAPTURE_POLICY      = -27,
    DC1394_INVALID_ERROR_CODE          = -28,
    DC1394_INVALID_BAYER_METHOD        = -29,
    DC1394_INVALID_VIDEO1394_DEVICE    = -30,
    DC1394_INVALID_OPERATION_MODE      = -31,
    DC1394_INVALID_TRIGGER_POLARITY    = -32,
    DC1394_INVALID_FEATURE_MODE        = -33,
    DC1394_INVALID_LOG_TYPE            = -34,
    DC1394_INVALID_BYTE_ORDER          = -35,
    DC1394_INVALID_STEREO_METHOD       = -36,
    DC1394_BASLER_NO_MORE_SFF_CHUNKS   = -37,
    DC1394_BASLER_CORRUPTED_SFF_CHUNK  = -38,
    DC1394_BASLER_UNKNOWN_SFF_CHUNK    = -39
} dc1394error_t;
#define DC1394_ERROR_MIN  DC1394_BASLER_UNKNOWN_SFF_CHUNK
#define DC1394_ERROR_MAX  DC1394_SUCCESS
#define DC1394_ERROR_NUM (DC1394_ERROR_MAX-DC1394_ERROR_MIN+1)

/**
 * Enumeration of video modes. Note that the notion of IIDC "format" is not present here, except in the format_7 name.
 */
typedef enum {
    DC1394_VIDEO_MODE_160x120_YUV444= 64,
    DC1394_VIDEO_MODE_320x240_YUV422,
    DC1394_VIDEO_MODE_640x480_YUV411,
    DC1394_VIDEO_MODE_640x480_YUV422,
    DC1394_VIDEO_MODE_640x480_RGB8,
    DC1394_VIDEO_MODE_640x480_MONO8,
    DC1394_VIDEO_MODE_640x480_MONO16,
    DC1394_VIDEO_MODE_800x600_YUV422,
    DC1394_VIDEO_MODE_800x600_RGB8,
    DC1394_VIDEO_MODE_800x600_MONO8,
    DC1394_VIDEO_MODE_1024x768_YUV422,
    DC1394_VIDEO_MODE_1024x768_RGB8,
    DC1394_VIDEO_MODE_1024x768_MONO8,
    DC1394_VIDEO_MODE_800x600_MONO16,
    DC1394_VIDEO_MODE_1024x768_MONO16,
    DC1394_VIDEO_MODE_1280x960_YUV422,
    DC1394_VIDEO_MODE_1280x960_RGB8,
    DC1394_VIDEO_MODE_1280x960_MONO8,
    DC1394_VIDEO_MODE_1600x1200_YUV422,
    DC1394_VIDEO_MODE_1600x1200_RGB8,
    DC1394_VIDEO_MODE_1600x1200_MONO8,
    DC1394_VIDEO_MODE_1280x960_MONO16,
    DC1394_VIDEO_MODE_1600x1200_MONO16,
    DC1394_VIDEO_MODE_EXIF,
    DC1394_VIDEO_MODE_FORMAT7_0,
    DC1394_VIDEO_MODE_FORMAT7_1,
    DC1394_VIDEO_MODE_FORMAT7_2,
    DC1394_VIDEO_MODE_FORMAT7_3,
    DC1394_VIDEO_MODE_FORMAT7_4,
    DC1394_VIDEO_MODE_FORMAT7_5,
    DC1394_VIDEO_MODE_FORMAT7_6,
    DC1394_VIDEO_MODE_FORMAT7_7
} dc1394video_mode_t;
#define DC1394_VIDEO_MODE_MIN            DC1394_VIDEO_MODE_160x120_YUV444
#define DC1394_VIDEO_MODE_MAX       DC1394_VIDEO_MODE_FORMAT7_7
#define DC1394_VIDEO_MODE_NUM      (DC1394_VIDEO_MODE_MAX - DC1394_VIDEO_MODE_MIN + 1)

/* Special min/max are defined for Format_7 */
#define DC1394_VIDEO_MODE_FORMAT7_MIN       DC1394_VIDEO_MODE_FORMAT7_0
#define DC1394_VIDEO_MODE_FORMAT7_MAX       DC1394_VIDEO_MODE_FORMAT7_7
#define DC1394_VIDEO_MODE_FORMAT7_NUM      (DC1394_VIDEO_MODE_FORMAT7_MAX - DC1394_VIDEO_MODE_FORMAT7_MIN + 1)

/**
 * Enumeration of colour codings. For details on the data format please read the IIDC specifications.
 */
typedef enum {
    DC1394_COLOR_CODING_MONO8= 352,
    DC1394_COLOR_CODING_YUV411,
    DC1394_COLOR_CODING_YUV422,
    DC1394_COLOR_CODING_YUV444,
    DC1394_COLOR_CODING_RGB8,
    DC1394_COLOR_CODING_MONO16,
    DC1394_COLOR_CODING_RGB16,
    DC1394_COLOR_CODING_MONO16S,
    DC1394_COLOR_CODING_RGB16S,
    DC1394_COLOR_CODING_RAW8,
    DC1394_COLOR_CODING_RAW16
} dc1394color_coding_t;
#define DC1394_COLOR_CODING_MIN     DC1394_COLOR_CODING_MONO8
#define DC1394_COLOR_CODING_MAX     DC1394_COLOR_CODING_RAW16
#define DC1394_COLOR_CODING_NUM    (DC1394_COLOR_CODING_MAX - DC1394_COLOR_CODING_MIN + 1)

/**
 * RAW sensor filters. These elementary tiles tesselate the image plane in RAW modes. RGGB should be interpreted in 2D as
 *
 *    RG
 *    GB
 *
 * and similarly for other filters.
 */
typedef enum {
    DC1394_COLOR_FILTER_RGGB = 512,
    DC1394_COLOR_FILTER_GBRG,
    DC1394_COLOR_FILTER_GRBG,
    DC1394_COLOR_FILTER_BGGR
} dc1394color_filter_t;
#define DC1394_COLOR_FILTER_MIN        DC1394_COLOR_FILTER_RGGB
#define DC1394_COLOR_FILTER_MAX        DC1394_COLOR_FILTER_BGGR
#define DC1394_COLOR_FILTER_NUM       (DC1394_COLOR_FILTER_MAX - DC1394_COLOR_FILTER_MIN + 1)

/**
 * Byte order for YUV formats (may be expanded to RGB in the future)
 *
 * IIDC cameras always return data in UYVY order, but conversion functions can change this if requested.
 */
typedef enum {
    DC1394_BYTE_ORDER_UYVY=800,
    DC1394_BYTE_ORDER_YUYV
} dc1394byte_order_t;
#define DC1394_BYTE_ORDER_MIN        DC1394_BYTE_ORDER_UYVY
#define DC1394_BYTE_ORDER_MAX        DC1394_BYTE_ORDER_YUYV
#define DC1394_BYTE_ORDER_NUM       (DC1394_BYTE_ORDER_MAX - DC1394_BYTE_ORDER_MIN + 1)

/**
 * A struct containing a list of color codings
 */
typedef struct
{
    uint32_t                num;
    dc1394color_coding_t    codings[DC1394_COLOR_CODING_NUM];
} dc1394color_codings_t;

/**
 * A struct containing a list of video modes
 */
typedef struct
{
    uint32_t                num;
    dc1394video_mode_t      modes[DC1394_VIDEO_MODE_NUM];
} dc1394video_modes_t;

/**
 * Yet another boolean data type
 */
typedef enum {
    DC1394_FALSE= 0,
    DC1394_TRUE
} dc1394bool_t;

/**
 * Yet another boolean data type, a bit more oriented towards electrical-engineers
 */
typedef enum {
    DC1394_OFF= 0,
    DC1394_ON
} dc1394switch_t;

/**
 * A list of de-mosaicing techniques for Bayer-patterns.
 *
 * The speed of the techniques can vary greatly, as well as their quality.
 */
typedef enum {
    DC1394_BAYER_METHOD_NEAREST=0,
    DC1394_BAYER_METHOD_SIMPLE,
    DC1394_BAYER_METHOD_BILINEAR,
    DC1394_BAYER_METHOD_HQLINEAR,
    DC1394_BAYER_METHOD_DOWNSAMPLE,
    DC1394_BAYER_METHOD_EDGESENSE,
    DC1394_BAYER_METHOD_VNG,
    DC1394_BAYER_METHOD_AHD
} dc1394bayer_method_t;
#define DC1394_BAYER_METHOD_MIN      DC1394_BAYER_METHOD_NEAREST
#define DC1394_BAYER_METHOD_MAX      DC1394_BAYER_METHOD_AHD
#define DC1394_BAYER_METHOD_NUM     (DC1394_BAYER_METHOD_MAX-DC1394_BAYER_METHOD_MIN+1)

typedef struct
{
    dc1394bayer_method_t method;    /* Debayer method */
    dc1394color_filter_t filter;    /* Debayer pattern */
    int offsetX, offsetY;           /* Debayer offset */
} BayerParams;

/************************************************************************************************
 *                                                                                              *
 *      Color conversion functions for cameras that can output raw Bayer pattern images (color  *
 *  codings DC1394_COLOR_CODING_RAW8 and DC1394_COLOR_CODING_RAW16).                            *
 *                                                                                              *
 *  Credits and sources:                                                                        *
 *  - Nearest Neighbor : OpenCV library                                                         *
 *  - Bilinear         : OpenCV library                                                         *
 *  - HQLinear         : High-Quality Linear Interpolation For Demosaicing Of Bayer-Patterned   *
 *                       Color Images, by Henrique S. Malvar, Li-wei He, and Ross Cutler,       *
 *                       in Proceedings of the ICASSP'04 Conference.                            *
 *  - Edge Sense II    : Laroche, Claude A. "Apparatus and method for adaptively interpolating  *
 *                       a full color image utilizing chrominance gradients"                    *
 *                       U.S. Patent 5,373,322. Based on the code found on the website          *
 *                       http://www-ise.stanford.edu/~tingchen/ Converted to C and adapted to   *
 *                       all four elementary patterns.                                          *
 *  - Downsample       : "Known to the Ancients"                                                *
 *  - Simple           : Implemented from the information found in the manual of Allied Vision  *
 *                       Technologies (AVT) cameras.                                            *
 *  - VNG              : Variable Number of Gradients, a method described in                    *
 *                       http://www-ise.stanford.edu/~tingchen/algodep/vargra.html              *
 *                       Sources import from DCRAW by Frederic Devernay. DCRAW is a RAW         *
 *                       converter program by Dave Coffin. URL:                                 *
 *                       http://www.cybercom.net/~dcoffin/dcraw/                                *
 *  - AHD              : Adaptive Homogeneity-Directed Demosaicing Algorithm, by K. Hirakawa    *
 *                       and T.W. Parks, IEEE Transactions on Image Processing, Vol. 14, Nr. 3, *
 *                       March 2005, pp. 360 - 369.                                             *
 *                                                                                              *
 ************************************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Perform de-mosaicing on an 8-bit image buffer
 */
dc1394error_t
dc1394_bayer_decoding_8bit(const uint8_t *bayer, uint8_t *rgb,
                           uint32_t width, uint32_t height, dc1394color_filter_t tile,
                           dc1394bayer_method_t method);

/**
 * Perform de-mosaicing on an 16-bit image buffer
 */
dc1394error_t
dc1394_bayer_decoding_16bit(const uint16_t *bayer, uint16_t *rgb,
                            uint32_t width, uint32_t height, dc1394color_filter_t tile,
                            dc1394bayer_method_t method, uint32_t bits);

#ifdef __cplusplus
}
#endif

#endif      /* BAYER_H */
