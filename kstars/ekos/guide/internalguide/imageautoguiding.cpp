/*
    SPDX-FileCopyrightText: 2017 Bob Majewski <cygnus257@yahoo.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "imageautoguiding.h"

#include <qglobal.h>

#include <cmath>

#define SWAP(a, b) \
    tempr = (a);   \
    (a)   = (b);   \
    (b)   = tempr
#define NR_END   1
#define FREE_ARG char *

#define TWOPI   6.28318530717959
#define FFITMAX 0.05

//void ImageAutoGuiding1(float *ref,float *im,int n,float *xshift,float *yshift);

float ***f3tensorSP(long nrl, long nrh, long ncl, long nch, long ndl, long ndh);
void free_f3tensorSP(float ***t, long nrl, long nrh, long ncl, long nch, long ndl, long ndh);

float **matrixSP(long nrl, long nrh, long ncl, long nch);

void free_matrixSP(float **m, long nrl, long nrh, long ncl, long nch);
void nrerrorNR(void);

void fournNR(float data[], unsigned long nn[], long ndim, long isign);

void rlft3NR(float ***data, float **speq, unsigned long nn1, unsigned long nn2, unsigned long nn3, long isign);

void ShiftEST(float ***testimage, float ***refimage, int n, float *xshift, float *yshift, int k);

namespace ImageAutoGuiding
{
void ImageAutoGuiding1(float *ref, float *im, int n, float *xshift, float *yshift)
{
    float ***RefImage, ***TestImage;
    int i, j, k;
    float x, y;

    /* Allocate memory */

    RefImage  = f3tensorSP(1, 1, 1, n, 1, n);
    TestImage = f3tensorSP(1, 1, 1, n, 1, n);

    /* Load Data */

    k = 0;

    for (j = 1; j <= n; ++j)
    {
        for (i = 1; i <= n; ++i)
        {
            RefImage[1][j][i]  = ref[k];
            TestImage[1][j][i] = im[k];

            k += 1;
        }
    }

    /* Calculate Image Shifts  */

    ShiftEST(TestImage, RefImage, n, &x, &y, 1);

    *xshift = x;
    *yshift = y;

    /* free memory */

    free_f3tensorSP(RefImage, 1, 1, 1, n, 1, n);
    free_f3tensorSP(TestImage, 1, 1, 1, n, 1, n);
}
}

// Calculates Image Shifts

void ShiftEST(float ***testimage, float ***refimage, int n, float *xshift, float *yshift, int k)
{
    int ix, iy, nh, nhplusone;
    double deltax, deltay, fx2sum, fy2sum, phifxsum, phifysum, fxfysum;
    double fx, fy, ff, fn, re, im, testre, testim, rev, imv, phi;
    double power, dem, f2, f2limit;
    float **speq;

    f2limit = FFITMAX * FFITMAX;

    speq = matrixSP(1, 1, 1, 2 * n);

    nh        = n / 2;
    nhplusone = nh + 1;

    fn = ((float)n);
    ff = 1.0 / fn;

    /* FFT of Reference */

    for (ix = 1; ix <= 2 * n; ++ix)
    {
        speq[1][ix] = 0.0;
    }

    rlft3NR(refimage, speq, 1, n, n, 1);

    /* FFT of Test Image */

    for (ix = 1; ix <= 2 * n; ++ix)
    {
        speq[1][ix] = 0.0;
    }

    rlft3NR(testimage, speq, 1, n, n, 1);

    /* Solving for slopes  */

    fx2sum = 0.0;
    fy2sum = 0.0;

    phifxsum = 0.0;
    phifysum = 0.0;

    fxfysum = 0.0;

    for (ix = 1; ix <= n; ++ix)
    {
        if (ix <= nhplusone)
        {
            fx = ff * ((float)(ix - 1));
        }
        else
        {
            fx = -ff * ((float)(n + 1 - ix));
        }

        for (iy = 1; iy <= nh; ++iy)
        {
            fy = ff * ((float)(iy - 1));

            f2 = fx * fx + fy * fy;

            /* Limit to Low Spatial Frequencies */

            if (f2 < f2limit)
            {
                /* Real part reference image */

                re = refimage[k][ix][2 * iy - 1];

                /* Imaginary part reference image */

                im = refimage[k][ix][2 * iy];

                power = re * re + im * im;

                /* Real part test image */

                testre = testimage[k][ix][2 * iy - 1];

                /* Imaginary part image */

                testim = testimage[k][ix][2 * iy];

                rev = re * testre + im * testim;
                imv = re * testim - im * testre;

                /* Find Phase */

                phi = atan2(imv, rev);

                fx2sum += power * fx * fx;
                fy2sum += power * fy * fy;

                phifxsum += power * fx * phi;
                phifysum += power * fy * phi;

                fxfysum += power * fx * fy;
            }
        }
    }

    /* calculate subpixel shift */

    dem = fx2sum * fy2sum - fxfysum * fxfysum;

    deltax = (phifxsum * fy2sum - fxfysum * phifysum) / (dem * TWOPI);
    deltay = (phifysum * fx2sum - fxfysum * phifxsum) / (dem * TWOPI);

    free_matrixSP(speq, 1, 1, 1, 2 * n);

    /* You can change the shift mapping here */

    *xshift = deltax;
    *yshift = deltay;
}

// 2 and 3 Dimensional FFT Routine for Real Data
// Numerical Recipes in C Second Edition
// The Art of Scientific Computing
// 1999

void rlft3NR(float ***data, float **speq, unsigned long nn1, unsigned long nn2, unsigned long nn3, long isign)
{
    void fournNR(float data[], unsigned long nn[], long ndim, long isign);
    void nrerror();
    unsigned long i1, i2, i3, j1, j2, j3, nn[4], ii3;
    double theta, wpi, wpr, wtemp;
    float c1, c2, h1r, h1i, h2r, h2i;
    float wi, wr;

    if ((unsigned long)(1 + &data[nn1][nn2][nn3] - &data[1][1][1]) != nn1 * nn2 * nn3)
        nrerrorNR();
    c1    = 0.5;
    c2    = -0.5 * isign;
    theta = isign * (6.28318530717959 / nn3);
    wtemp = sin(0.5 * theta);
    wpr   = -2.0 * wtemp * wtemp;
    wpi   = sin(theta);
    nn[1] = nn1;
    nn[2] = nn2;
    nn[3] = nn3 >> 1;
    if (isign == 1)
    {
        fournNR(&data[1][1][1] - 1, nn, 3, isign);
        for (i1 = 1; i1 <= nn1; i1++)
            for (i2 = 1, j2 = 0; i2 <= nn2; i2++)
            {
                speq[i1][++j2] = data[i1][i2][1];
                speq[i1][++j2] = data[i1][i2][2];
            }
    }
    for (i1 = 1; i1 <= nn1; i1++)
    {
        j1 = (i1 != 1 ? nn1 - i1 + 2 : 1);
        wr = 1.0;
        wi = 0.0;
        for (ii3 = 1, i3 = 1; i3 <= (nn3 >> 2) + 1; i3++, ii3 += 2)
        {
            for (i2 = 1; i2 <= nn2; i2++)
            {
                if (i3 == 1)
                {
                    j2               = (i2 != 1 ? ((nn2 - i2) << 1) + 3 : 1);
                    h1r              = c1 * (data[i1][i2][1] + speq[j1][j2]);
                    h1i              = c1 * (data[i1][i2][2] - speq[j1][j2 + 1]);
                    h2i              = c2 * (data[i1][i2][1] - speq[j1][j2]);
                    h2r              = -c2 * (data[i1][i2][2] + speq[j1][j2 + 1]);
                    data[i1][i2][1]  = h1r + h2r;
                    data[i1][i2][2]  = h1i + h2i;
                    speq[j1][j2]     = h1r - h2r;
                    speq[j1][j2 + 1] = h2i - h1i;
                }
                else
                {
                    j2                    = (i2 != 1 ? nn2 - i2 + 2 : 1);
                    j3                    = nn3 + 3 - (i3 << 1);
                    h1r                   = c1 * (data[i1][i2][ii3] + data[j1][j2][j3]);
                    h1i                   = c1 * (data[i1][i2][ii3 + 1] - data[j1][j2][j3 + 1]);
                    h2i                   = c2 * (data[i1][i2][ii3] - data[j1][j2][j3]);
                    h2r                   = -c2 * (data[i1][i2][ii3 + 1] + data[j1][j2][j3 + 1]);
                    data[i1][i2][ii3]     = h1r + wr * h2r - wi * h2i;
                    data[i1][i2][ii3 + 1] = h1i + wr * h2i + wi * h2r;
                    data[j1][j2][j3]      = h1r - wr * h2r + wi * h2i;
                    data[j1][j2][j3 + 1]  = -h1i + wr * h2i + wi * h2r;
                }
            }
            wr = (wtemp = wr) * wpr - wi * wpi + wr;
            wi = wi * wpr + wtemp * wpi + wi;
        }
    }
    if (isign == -1)
        fournNR(&data[1][1][1] - 1, nn, 3, isign);
}

// Multi-Dimensional FFT Routine for Complex Data
// Numerical Recipes in C Second Edition
// The Art of Scientific Computing
// 1999
// Used in rlft3

void fournNR(float data[], unsigned long nn[], long ndim, long isign)
{
    long idim;
    unsigned long i1, i2rev, i3rev, ip1, ip2, ip3, ifp1, ifp2;
    unsigned long i2, i3;
    unsigned long ibit, k1 = 0, k2, n, nprev, nrem, ntot;
    float wi, wr, tempi, tempr;
    double theta, wpi, wpr, wtemp;

    for (ntot = 1, idim = 1; idim <= ndim; idim++)
        ntot *= nn[idim];
    nprev = 1;
    for (idim = ndim; idim >= 1; idim--)
    {
        n     = nn[idim];
        nrem  = ntot / (n * nprev);
        ip1   = nprev << 1;
        ip2   = ip1 * n;
        ip3   = ip2 * nrem;
        i2rev = 1;
        for (i2 = 1; i2 <= ip2; i2 += ip1)
        {
            if (i2 < i2rev)
            {
                for (i1 = i2; i1 <= i2 + ip1 - 2; i1 += 2)
                {
                    for (i3 = i1; i3 <= ip3; i3 += ip2)
                    {
                        i3rev = i2rev + i3 - i2;
                        SWAP(data[i3], data[i3rev]);
                        SWAP(data[i3 + 1], data[i3rev + 1]);
                    }
                }
            }
            ibit = ip2 >> 1;
            while (ibit >= ip1 && i2rev > ibit)
            {
                i2rev -= ibit;
                ibit >>= 1;
            }
            i2rev += ibit;
        }
        ifp1 = ip1;
        while (ifp1 < ip2)
        {
            ifp2  = ifp1 << 1;
            theta = isign * 6.28318530717959 / (ifp2 / ip1);
            wtemp = sin(0.5 * theta);
            wpr   = -2.0 * wtemp * wtemp;
            wpi   = sin(theta);
            wr    = 1.0;
            wi    = 0.0;
            for (i3 = 1; i3 <= ifp1; i3 += ip1)
            {
                for (i1 = i3; i1 <= i3 + ip1 - 2; i1 += 2)
                {
                    for (i2 = i1; i2 <= ip3; i2 += ifp2)
                    {
                        k1           = i2;
                        k2           = k1 + ifp1;
                        tempr        = (double)wr * data[k2] - (double)wi * data[k2 + 1];
                        tempi        = (double)wr * data[k2 + 1] + (double)wi * data[k2];
                        data[k2]     = data[k1] - tempr;
                        data[k2 + 1] = data[k1 + 1] - tempi;
                        data[k1] += tempr;
                        data[k1 + 1] += tempi;
                    }
                }
                wr = (wtemp = wr) * wpr - wi * wpi + wr;
                wi = wi * wpr + wtemp * wpi + wi;
            }
            ifp1 = ifp2;
        }
        nprev *= n;
    }
}

void nrerrorNR(void)
{
}

// Numerical Recipes memory allocation routines
// One based arrays

float **matrixSP(long nrl, long nrh, long ncl, long nch)
/* allocate a float matrix with subscript range m[nrl..nrh][ncl..nch] */
{
    long i, nrow = nrh - nrl + 1, ncol = nch - ncl + 1;
    float **m;

    /* allocate pointers to rows */
    m = (float **)malloc((size_t)((nrow + NR_END) * sizeof(float *)));
    if (!m)
        nrerrorNR();
    m += NR_END;
    m -= nrl;

    /* allocate rows and set pointers to them */
    m[nrl] = (float *)malloc((size_t)((nrow * ncol + NR_END) * sizeof(float)));
    if (!m[nrl])
        nrerrorNR();
    m[nrl] += NR_END;
    m[nrl] -= ncl;

    for (i = nrl + 1; i <= nrh; i++)
        m[i] = m[i - 1] + ncol;

    /* return pointer to array of pointers to rows */
    return m;
}

float ***f3tensorSP(long nrl, long nrh, long ncl, long nch, long ndl, long ndh)
/* allocate a float 3tensor with range t[nrl..nrh][ncl..nch][ndl..ndh] */
{
    long i, j, nrow = nrh - nrl + 1, ncol = nch - ncl + 1, ndep = ndh - ndl + 1;
    float ***t;

    /* allocate pointers to pointers to rows */
    t = (float ***)malloc((size_t)((nrow + NR_END) * sizeof(float **)));
    if (!t)
        nrerrorNR();
    t += NR_END;
    t -= nrl;

    /* allocate pointers to rows and set pointers to them */
    t[nrl] = (float **)malloc((size_t)((nrow * ncol + NR_END) * sizeof(float *)));
    if (!t[nrl])
        nrerrorNR();
    t[nrl] += NR_END;
    t[nrl] -= ncl;

    /* allocate rows and set pointers to them */
    t[nrl][ncl] = (float *)malloc((size_t)((nrow * ncol * ndep + NR_END) * sizeof(float)));
    if (!t[nrl][ncl])
        nrerrorNR();
    t[nrl][ncl] += NR_END;
    t[nrl][ncl] -= ndl;

    for (j = ncl + 1; j <= nch; j++)
        t[nrl][j] = t[nrl][j - 1] + ndep;
    for (i = nrl + 1; i <= nrh; i++)
    {
        t[i]      = t[i - 1] + ncol;
        t[i][ncl] = t[i - 1][ncl] + ncol * ndep;
        for (j = ncl + 1; j <= nch; j++)
            t[i][j] = t[i][j - 1] + ndep;
    }

    /* return pointer to array of pointers to rows */
    return t;
}

void free_matrixSP(float **m, long nrl, long nrh, long ncl, long nch)
/* free a float matrix allocated by matrix() */
{
    Q_UNUSED(nrh);
    Q_UNUSED(nch);
    free((FREE_ARG)(m[nrl] + ncl - NR_END));
    free((FREE_ARG)(m + nrl - NR_END));
}

void free_f3tensorSP(float ***t, long nrl, long nrh, long ncl, long nch, long ndl, long ndh)
/* free a float f3tensor allocated by f3tensor() */
{
    Q_UNUSED(nrh);
    Q_UNUSED(nch);
    Q_UNUSED(ndh);
    free((FREE_ARG)(t[nrl][ncl] + ndl - NR_END));
    free((FREE_ARG)(t[nrl] + ncl - NR_END));
    free((FREE_ARG)(t + nrl - NR_END));
}
