/*
    SPDX-FileCopyrightText: 2023

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QList>
#include "../fitsviewer/fitsstardetector.h"
#include "fitsviewer/fitsview.h"
#include "auxiliary/imagemask.h"
#include "curvefit.h"
#include "../ekos.h"
#include <ekos_focus_debug.h>
#include <gsl/gsl_fft_complex.h>

namespace Ekos
{

// This class performs Fourier Transform processing on the passed in FITS image.
// The approach is based on this paper: https://doi.org/10.1093/mnras/stac189
// (preprint, see https://arxiv.org/abs/2201.12466 )
// The idea is as follows: a focus frame consists of stars and noise. As long as the
// exposure is long enough (a few seconds) then the star can be treated as gaussians.
// A fourier transform (FFT) of a gaussian is another gaussian. A narrow gaussian in the space
// domain transforms to a wider gaussian in frequency domain. So the nearer to focus we
// get, the sharper the stars (smaller HFR) become, and thereform the wider their FFTs.
// So the nearer to focus we get the bigger the frequency domain's power spectrum becomes
// which means the power reaches a maximum in the frequency domain.
//
// Random noise transforms to random noise in the frequency domain. The power spectrum of
// the FFT of noise typically swamps the power spectrum of the stars' signal so it needs to
// be dealt with. Tan and Schulz suggest treating noise as "white noise" which means it adds
// the same power component to all frequencies. This means that be averaging the power
// high frequency where the star contribution is zero, gives the noise component. This can
// then to subtracted from each frequency to leave the power contribution of just the stars.
//
// I have found better results by background subtracting the frame before fourier transforming
// which is the same thing as all thats left to FFT is an image of the stars. Currently
// fitsviewer will have procvessed the image prior to this routine being called so background
// information is available here.
//
// So we need to perform a 2D FFT on the image. GSL only performs basic 1D FFTs so this
// routine performs a FFT on each row and then uses the results to perform a FFT on each
// column. An optimisation would be to use a more sophisticated FFT routine. FFTW3 could,
// for example, be used.
//
// Currently just the first channel (if there is more than 1) is used by this routine. It would
// be possible to use all channels or offer the user a choice of which channel(s) to use. If
// this were to be done it would make sense to synchonise this functionality with fitsviwer
// functionality on HFR, FWHM, etc.

class FocusFourierPower
{
    public:

        FocusFourierPower(Mathematics::RobustStatistics::ScaleCalculation scaleCalc);
        ~FocusFourierPower();

        template <typename T>
        void processFourierPower(const T imageBuffer, const QSharedPointer<FITSData> &imageData,
                                 QSharedPointer<ImageMask> mask, double *fourierPower, double *weight)
        {
            // Initialise outputs
            *fourierPower = INVALID_STAR_MEASURE;
            *weight = 1.0;

            auto stats = imageData->getStatistics();
            const auto width = stats.width;
            const auto height = stats.height;
            const auto N = width * height;
            double *image = new double[2 * N];

            // Set the gsl error handler off as it aborts the program on error.
            auto const oldErrorHandler = gsl_set_error_handler_off();

            /* alloc memory for complex wavetables, and workspace */
            gsl_fft_complex_wavetable *rowWT = gsl_fft_complex_wavetable_alloc(width);
            gsl_fft_complex_workspace *rowWS = gsl_fft_complex_workspace_alloc(width);
            gsl_fft_complex_wavetable *colWT = gsl_fft_complex_wavetable_alloc(height);
            gsl_fft_complex_workspace *colWS = gsl_fft_complex_workspace_alloc(height);

            // Convert the image to complex double datatype as required by GSL FFT
            // The real part is just the background subtracted pixel value clipped to zero
            // The imaginary part is zero.

            auto skyBackground = imageData->getSkyBackground();
            auto bg = skyBackground.mean + 3.0 * skyBackground.sigma;

            uint16_t posX = 0, posY = 0;
            for (long i = 0; i < N; i++)
            {
                if (mask.isNull() || mask->active() == false || mask->isVisible(posX, posY))
                    image[i * 2] = std::max(0.0, (double) imageBuffer[i] - bg);
                else
                    image[i * 2] = 0.0;

                // Imaginary value is always zero
                image[i * 2 + 1] = 0.0;

                if (++posX == width)
                {
                    posX = 0;
                    posY++;
                }
            }

            // Perform FFT on all the rows
            int status = 0;
            for (int j = 0; j < height; j++)
            {
                status = gsl_fft_complex_forward(&image[j * 2 * width], 1, width, rowWT, rowWS);
                if (status != 0)
                {
                    qCDebug(KSTARS_EKOS_FOCUS) << QString("Error %1 [%2] calculating FFT on row %3").arg(status).arg(gsl_strerror(status)).
                                               arg(j);
                    break;
                }
            }

            if (status == 0)
            {
                // Perform FFT on all the cols
                for (int i = 0; i < width; i++)
                {
                    status = gsl_fft_complex_forward(&image[2 * i], width, height, colWT, colWS);
                    if (status != 0)
                    {
                        qCDebug(KSTARS_EKOS_FOCUS) << QString("Error %1 [%2] calculating FFT on col %3").arg(status).arg(gsl_strerror(status)).arg(
                                                       i);
                        break;
                    }
                }
            }

            if (status == 0)
            {
                double power = 0.0;
                for (long i = 0; i < N; i++)
                    power += pow(image[i * 2], 2.0) + pow(image[i * 2 + 1], 2.0);

                power /= pow(N, 2.0);

                qCDebug(KSTARS_EKOS_FOCUS) << "FFT power=" << power;

                *fourierPower = power;
            }

            /* free memory */
            gsl_fft_complex_wavetable_free(rowWT);
            gsl_fft_complex_workspace_free(rowWS);
            gsl_fft_complex_wavetable_free(colWT);
            gsl_fft_complex_workspace_free(colWS);

            // Restore old GSL error handler
            gsl_set_error_handler(oldErrorHandler);
        }

        static double constexpr INVALID_STAR_MEASURE = -1.0;

    private:

        Mathematics::RobustStatistics::ScaleCalculation m_ScaleCalc;
};
}
