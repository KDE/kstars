/*  Image Guide Algorithm
    Copyright (C) 2017 Bob Majewski <cygnus257@yahoo.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

// Robert Majewski

// ImageAutoGuiding1 is self contained
// ref is the reference image
// im is the test image
// Image Size n x n


// The Input Image and the Reference images are zero based one dimensional vectors
// They MUST be Square Images
// n MUST be a Power of 2  use 128,256,512
// 256 X 256 is A good Choice
// These should be portions of the camera imagery


namespace ImageAutoGuiding
{

void ImageAutoGuiding1(float *ref,float *im,int n,float *xshift,float *yshift);

}


