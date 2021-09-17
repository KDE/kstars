/*  Stretch

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "stretch.h"

#include <fitsio.h>
#include <math.h>
#include <QtConcurrent>

namespace
{

// Returns the median value of the vector.
// The vector is modified in an undefined way.
template <typename T>
T median(std::vector<T> &values)
{
    const int middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + middle, values.end());
    return values[middle];
}

// Returns the rough max of the buffer.
template <typename T>
T sampledMax(T const *values, int size, int sampleBy)
{
    T maxVal = 0;
    for (int i = 0; i < size; i += sampleBy)
        if (maxVal < values[i])
            maxVal = values[i];
    return  maxVal;
}

// Returns the median of the sample values.
// The values are not modified.
template <typename T>
T median(T const *values, int size, int sampleBy)
{
    const int downsampled_size = size / sampleBy;
    std::vector<T> samples(downsampled_size);
    for (int index = 0, i = 0; i < downsampled_size; ++i, index += sampleBy)
        samples[i] = values[index];
    return median(samples);
}

// This stretches one channel given the input parameters.
// Based on the spec in section 8.5.6
// https://pixinsight.com/doc/docs/XISF-1.0-spec/XISF-1.0-spec.html
// Uses multiple threads, blocks until done.
// The extension parameters are not used.
// Sampling is applied to the output (that is, with sampling=2, we compute every other output
// sample both in width and height, so the output would have about 4X fewer pixels.
template <typename T>
void stretchOneChannel(T *input_buffer, QImage *output_image,
                       const StretchParams &stretch_params,
                       int input_range, int image_height, int image_width, int sampling)
{
    QVector<QFuture<void>> futures;

    // We're outputting uint8, so the max output is 255.
    constexpr int maxOutput = 255;

    // Maximum possible input value (e.g. 1024*64 - 1 for a 16 bit unsigned int).
    const float maxInput = input_range > 1 ? input_range - 1 : input_range;

    const float midtones   = stretch_params.grey_red.midtones;
    const float highlights = stretch_params.grey_red.highlights;
    const float shadows    = stretch_params.grey_red.shadows;

    // Precomputed expressions moved out of the loop.
    // highlights - shadows, protecting for divide-by-0, in a 0->1.0 scale.
    const float hsRangeFactor = highlights == shadows ? 1.0f : 1.0f / (highlights - shadows);
    // Shadow and highlight values translated to the ADU scale.
    const T nativeShadows = shadows * maxInput;
    const T nativeHighlights = highlights * maxInput;
    // Constants based on above needed for the stretch calculations.
    const float k1 = (midtones - 1) * hsRangeFactor * maxOutput / maxInput;
    const float k2 = ((2 * midtones) - 1) * hsRangeFactor / maxInput;

    // Increment the input index by the sampling, the output index increments by 1.
    for (int j = 0, jout = 0; j < image_height; j += sampling, jout++)
    {
        futures.append(QtConcurrent::run([ = ]()
        {
            T * inputLine  = input_buffer + j * image_width;
            auto * scanLine = output_image->scanLine(jout);

            for (int i = 0, iout = 0; i < image_width; i += sampling, iout++)
            {
                const T input = inputLine[i];
                if (input < nativeShadows) scanLine[iout] = 0;
                else if (input >= nativeHighlights) scanLine[iout] = maxOutput;
                else
                {
                    const T inputFloored = (input - nativeShadows);
                    scanLine[iout] = (inputFloored * k1) / (inputFloored * k2 - midtones);
                }
            }
        }));
    }
    for(QFuture<void> future : futures)
        future.waitForFinished();
}

// This is like the above 1-channel stretch, but extended for 3 channels.
// This could have been more modular, but the three channels are combined
// into a single qRgb value at the end, so it seems the simplest thing is to
// replicate the code. It is assume the colors are not interleaved--the red image
// is stored fully, then the green, then the blue.
// Sampling is applied to the output (that is, with sampling=2, we compute every other output
// sample both in width and height, so the output would have about 4X fewer pixels.
template <typename T>
void stretchThreeChannels(T *inputBuffer, QImage *outputImage,
                          const StretchParams &stretchParams,
                          int inputRange, int imageHeight, int imageWidth, int sampling)
{
    QVector<QFuture<void>> futures;

    // We're outputting uint8, so the max output is 255.
    constexpr int maxOutput = 255;

    // Maximum possible input value (e.g. 1024*64 - 1 for a 16 bit unsigned int).
    const float maxInput = inputRange > 1 ? inputRange - 1 : inputRange;

    const float midtonesR   = stretchParams.grey_red.midtones;
    const float highlightsR = stretchParams.grey_red.highlights;
    const float shadowsR    = stretchParams.grey_red.shadows;
    const float midtonesG   = stretchParams.green.midtones;
    const float highlightsG = stretchParams.green.highlights;
    const float shadowsG    = stretchParams.green.shadows;
    const float midtonesB   = stretchParams.blue.midtones;
    const float highlightsB = stretchParams.blue.highlights;
    const float shadowsB    = stretchParams.blue.shadows;

    // Precomputed expressions moved out of the loop.
    // highlights - shadows, protecting for divide-by-0, in a 0->1.0 scale.
    const float hsRangeFactorR = highlightsR == shadowsR ? 1.0f : 1.0f / (highlightsR - shadowsR);
    const float hsRangeFactorG = highlightsG == shadowsG ? 1.0f : 1.0f / (highlightsG - shadowsG);
    const float hsRangeFactorB = highlightsB == shadowsB ? 1.0f : 1.0f / (highlightsB - shadowsB);
    // Shadow and highlight values translated to the ADU scale.
    const T nativeShadowsR = shadowsR * maxInput;
    const T nativeShadowsG = shadowsG * maxInput;
    const T nativeShadowsB = shadowsB * maxInput;
    const T nativeHighlightsR = highlightsR * maxInput;
    const T nativeHighlightsG = highlightsG * maxInput;
    const T nativeHighlightsB = highlightsB * maxInput;
    // Constants based on above needed for the stretch calculations.
    const float k1R = (midtonesR - 1) * hsRangeFactorR * maxOutput / maxInput;
    const float k1G = (midtonesG - 1) * hsRangeFactorG * maxOutput / maxInput;
    const float k1B = (midtonesB - 1) * hsRangeFactorB * maxOutput / maxInput;
    const float k2R = ((2 * midtonesR) - 1) * hsRangeFactorR / maxInput;
    const float k2G = ((2 * midtonesG) - 1) * hsRangeFactorG / maxInput;
    const float k2B = ((2 * midtonesB) - 1) * hsRangeFactorB / maxInput;

    const int size = imageWidth * imageHeight;

    for (int j = 0, jout = 0; j < imageHeight; j += sampling, jout++)
    {
        futures.append(QtConcurrent::run([ = ]()
        {
            // R, G, B input images are stored one after another.
            T * inputLineR  = inputBuffer + j * imageWidth;
            T * inputLineG  = inputLineR + size;
            T * inputLineB  = inputLineG + size;

            auto * scanLine = reinterpret_cast<QRgb*>(outputImage->scanLine(jout));

            for (int i = 0, iout = 0; i < imageWidth; i += sampling, iout++)
            {
                const T inputR = inputLineR[i];
                const T inputG = inputLineG[i];
                const T inputB = inputLineB[i];

                uint8_t red, green, blue;

                if (inputR < nativeShadowsR) red = 0;
                else if (inputR >= nativeHighlightsR) red = maxOutput;
                else
                {
                    const T inputFloored = (inputR - nativeShadowsR);
                    red = (inputFloored * k1R) / (inputFloored * k2R - midtonesR);
                }

                if (inputG < nativeShadowsG) green = 0;
                else if (inputG >= nativeHighlightsG) green = maxOutput;
                else
                {
                    const T inputFloored = (inputG - nativeShadowsG);
                    green = (inputFloored * k1G) / (inputFloored * k2G - midtonesG);
                }

                if (inputB < nativeShadowsB) blue = 0;
                else if (inputB >= nativeHighlightsB) blue = maxOutput;
                else
                {
                    const T inputFloored = (inputB - nativeShadowsB);
                    blue = (inputFloored * k1B) / (inputFloored * k2B - midtonesB);
                }
                scanLine[iout] = qRgb(red, green, blue);
            }
        }));
    }
    for(QFuture<void> future : futures)
        future.waitForFinished();
}

template <typename T>
void stretchChannels(T *input_buffer, QImage *output_image,
                     const StretchParams &stretch_params,
                     int input_range, int image_height, int image_width, int num_channels, int sampling)
{
    if (num_channels == 1)
        stretchOneChannel(input_buffer, output_image, stretch_params, input_range,
                          image_height, image_width, sampling);
    else if (num_channels == 3)
        stretchThreeChannels(input_buffer, output_image, stretch_params, input_range,
                             image_height, image_width, sampling);
}

// See section 8.5.7 in above link  https://pixinsight.com/doc/docs/XISF-1.0-spec/XISF-1.0-spec.html
template <typename T>
void computeParamsOneChannel(T const *buffer, StretchParams1Channel *params,
                             int inputRange, int height, int width)
{
    // Find the median sample.
    constexpr int maxSamples = 500000;
    const int sampleBy = width * height < maxSamples ? 1 : width * height / maxSamples;

    T medianSample = median(buffer, width * height, sampleBy);
    // Find the Median deviation: 1.4826 * median of abs(sample[i] - median).
    const int numSamples = width * height / sampleBy;
    std::vector<T> deviations(numSamples);
    for (int index = 0, i = 0; i < numSamples; ++i, index += sampleBy)
    {
        if (medianSample > buffer[index])
            deviations[i] = medianSample - buffer[index];
        else
            deviations[i] = buffer[index] - medianSample;
    }

    // Shift everything to 0 -> 1.0.
    const float medDev = median(deviations);
    const float normalizedMedian = medianSample / static_cast<float>(inputRange);
    const float MADN = 1.4826 * medDev / static_cast<float>(inputRange);

    const bool upperHalf = normalizedMedian > 0.5;

    const float shadows = (upperHalf || MADN == 0) ? 0.0 :
                          fmin(1.0, fmax(0.0, (normalizedMedian + -2.8 * MADN)));

    const float highlights = (!upperHalf || MADN == 0) ? 1.0 :
                             fmin(1.0, fmax(0.0, (normalizedMedian - -2.8 * MADN)));

    float X, M;
    constexpr float B = 0.25;
    if (!upperHalf)
    {
        X = normalizedMedian - shadows;
        M = B;
    }
    else
    {
        X = B;
        M = highlights - normalizedMedian;
    }
    float midtones;
    if (X == 0) midtones = 0.0f;
    else if (X == M) midtones = 0.5f;
    else if (X == 1) midtones = 1.0f;
    else midtones = ((M - 1) * X) / ((2 * M - 1) * X - M);

    // Store the params.
    params->shadows = shadows;
    params->highlights = highlights;
    params->midtones = midtones;
    params->shadows_expansion = 0.0;
    params->highlights_expansion = 1.0;
}

// Need to know the possible range of input values.
// Using the type of the sample and guessing.
// Perhaps we should examine the contents for the file
// (e.g. look at maximum value and extrapolate from that).
int getRange(int data_type)
{
    switch (data_type)
    {
        case TBYTE:
            return 256;
        case TSHORT:
            return 64 * 1024;
        case TUSHORT:
            return 64 * 1024;
        case TLONG:
            return 64 * 1024;
        case TFLOAT:
            return 64 * 1024;
        case TLONGLONG:
            return 64 * 1024;
        case TDOUBLE:
            return 64 * 1024;
        default:
            return 64 * 1024;
    }
}

}  // namespace

Stretch::Stretch(int width, int height, int channels, int data_type)
{
    image_width = width;
    image_height = height;
    image_channels = channels;
    dataType = data_type;
    input_range = getRange(dataType);
}

void Stretch::run(uint8_t const *input, QImage *outputImage, int sampling)
{
    Q_ASSERT(outputImage->width() == (image_width + sampling - 1) / sampling);
    Q_ASSERT(outputImage->height() == (image_height + sampling - 1) / sampling);
    recalculateInputRange(input);

    switch (dataType)
    {
        case TBYTE:
            stretchChannels(reinterpret_cast<uint8_t const*>(input), outputImage, params,
                            input_range, image_height, image_width, image_channels, sampling);
            break;
        case TSHORT:
            stretchChannels(reinterpret_cast<short const*>(input), outputImage, params,
                            input_range, image_height, image_width, image_channels, sampling);
            break;
        case TUSHORT:
            stretchChannels(reinterpret_cast<unsigned short const*>(input), outputImage, params,
                            input_range, image_height, image_width, image_channels, sampling);
            break;
        case TLONG:
            stretchChannels(reinterpret_cast<long const*>(input), outputImage, params,
                            input_range, image_height, image_width, image_channels, sampling);
            break;
        case TFLOAT:
            stretchChannels(reinterpret_cast<float const*>(input), outputImage, params,
                            input_range, image_height, image_width, image_channels, sampling);
            break;
        case TLONGLONG:
            stretchChannels(reinterpret_cast<long long const*>(input), outputImage, params,
                            input_range, image_height, image_width, image_channels, sampling);
            break;
        case TDOUBLE:
            stretchChannels(reinterpret_cast<double const*>(input), outputImage, params,
                            input_range, image_height, image_width, image_channels, sampling);
            break;
        default:
            break;
    }
}

// The input range for float/double is ambiguous, and we can't tell without the buffer,
// so we set it to 64K and possibly reduce it when we see the data.
void Stretch::recalculateInputRange(uint8_t const *input)
{
    if (input_range <= 1) return;
    if (dataType != TFLOAT && dataType != TDOUBLE) return;

    float mx = 0;
    if (dataType == TFLOAT)
        mx = sampledMax(reinterpret_cast<float const*>(input), image_height * image_width, 1000);
    else if (dataType == TDOUBLE)
        mx = sampledMax(reinterpret_cast<double const*>(input), image_height * image_width, 1000);
    if (mx <= 1.01f) input_range = 1;
}

StretchParams Stretch::computeParams(uint8_t const *input)
{
    recalculateInputRange(input);
    StretchParams result;
    for (int channel = 0; channel < image_channels; ++channel)
    {
        int offset = channel * image_width * image_height;
        StretchParams1Channel *params = channel == 0 ? &result.grey_red :
                                        (channel == 1 ? &result.green : &result.blue);
        switch (dataType)
        {
            case TBYTE:
            {
                auto buffer = reinterpret_cast<uint8_t const*>(input);
                computeParamsOneChannel(buffer + offset, params, input_range,
                                        image_height, image_width);
                break;
            }
            case TSHORT:
            {
                auto buffer = reinterpret_cast<short const*>(input);
                computeParamsOneChannel(buffer + offset, params, input_range,
                                        image_height, image_width);
                break;
            }
            case TUSHORT:
            {
                auto buffer = reinterpret_cast<unsigned short const*>(input);
                computeParamsOneChannel(buffer + offset, params, input_range,
                                        image_height, image_width);
                break;
            }
            case TLONG:
            {
                auto buffer = reinterpret_cast<long const*>(input);
                computeParamsOneChannel(buffer + offset, params, input_range,
                                        image_height, image_width);
                break;
            }
            case TFLOAT:
            {
                auto buffer = reinterpret_cast<float const*>(input);
                computeParamsOneChannel(buffer + offset, params, input_range,
                                        image_height, image_width);
                break;
            }
            case TLONGLONG:
            {
                auto buffer = reinterpret_cast<long long const*>(input);
                computeParamsOneChannel(buffer + offset, params, input_range,
                                        image_height, image_width);
                break;
            }
            case TDOUBLE:
            {
                auto buffer = reinterpret_cast<double const*>(input);
                computeParamsOneChannel(buffer + offset, params, input_range,
                                        image_height, image_width);
                break;
            }
            default:
                break;
        }
    }
    return result;
}
