/*  Algorithms used in the internal guider.
    Copyright (C) 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq for KStars.
    SPDX-FileCopyrightText: Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "guidealgorithms.h"

#include <set>
#include <QObject>

#include "ekos_guide_debug.h"
#include "ekos/auxiliary/stellarsolverprofileeditor.h"
#include "Options.h"
#include "fitsviewer/fitsdata.h"

#define SMART_THRESHOLD    0
#define SEP_THRESHOLD      1
#define CENTROID_THRESHOLD 2
#define AUTO_THRESHOLD     3
#define NO_THRESHOLD       4
#define SEP_MULTISTAR      5

// smart threshold algorithm param
// width of outer frame for background calculation
#define SMART_FRAME_WIDTH 4
// cut-factor above average threshold
#define SMART_CUT_FACTOR 0.1

struct Peak
{
    int x;
    int y;
    float val;

    Peak() = default;
    Peak(int x_, int y_, float val_) : x(x_), y(y_), val(val_) { }
    bool operator<(const Peak &rhs) const
    {
        return val < rhs.val;
    }
};

// JM: Why not use QPoint?
typedef struct
{
    int x, y;
} point_t;

namespace
{

void psf_conv(float *dst, const float *src, int width, int height)
{
    //dst.Init(src.Size);

    //                       A      B1     B2    C1     C2    C3     D1     D2     D3
    const double PSF[] = { 0.906, 0.584, 0.365, .117, .049, -0.05, -.064, -.074, -.094 };

    //memset(dst.px, 0, src.NPixels * sizeof(float));

    /* PSF Grid is:
    D3 D3 D3 D3 D3 D3 D3 D3 D3
    D3 D3 D3 D2 D1 D2 D3 D3 D3
    D3 D3 C3 C2 C1 C2 C3 D3 D3
    D3 D2 C2 B2 B1 B2 C2 D2 D3
    D3 D1 C1 B1 A  B1 C1 D1 D3
    D3 D2 C2 B2 B1 B2 C2 D2 D3
    D3 D3 C3 C2 C1 C2 C3 D3 D3
    D3 D3 D3 D2 D1 D2 D3 D3 D3
    D3 D3 D3 D3 D3 D3 D3 D3 D3

    1@A
    4@B1, B2, C1, C3, D1
    8@C2, D2
    44 * D3
    */

    int psf_size = 4;

    for (int y = psf_size; y < height - psf_size; y++)
    {
        for (int x = psf_size; x < width - psf_size; x++)
        {
            float A, B1, B2, C1, C2, C3, D1, D2, D3;

#define PX(dx, dy) *(src + width * (y + (dy)) + x + (dx))
            A =  PX(+0, +0);
            B1 = PX(+0, -1) + PX(+0, +1) + PX(+1, +0) + PX(-1, +0);
            B2 = PX(-1, -1) + PX(+1, -1) + PX(-1, +1) + PX(+1, +1);
            C1 = PX(+0, -2) + PX(-2, +0) + PX(+2, +0) + PX(+0, +2);
            C2 = PX(-1, -2) + PX(+1, -2) + PX(-2, -1) + PX(+2, -1) + PX(-2, +1) + PX(+2, +1) + PX(-1, +2) + PX(+1, +2);
            C3 = PX(-2, -2) + PX(+2, -2) + PX(-2, +2) + PX(+2, +2);
            D1 = PX(+0, -3) + PX(-3, +0) + PX(+3, +0) + PX(+0, +3);
            D2 = PX(-1, -3) + PX(+1, -3) + PX(-3, -1) + PX(+3, -1) + PX(-3, +1) + PX(+3, +1) + PX(-1, +3) + PX(+1, +3);
            D3 = PX(-4, -2) + PX(-3, -2) + PX(+3, -2) + PX(+4, -2) + PX(-4, -1) + PX(+4, -1) + PX(-4, +0) + PX(+4, +0) + PX(-4,
                    +1) + PX(+4, +1) + PX(-4, +2) + PX(-3, +2) + PX(+3, +2) + PX(+4, +2);
#undef PX
            int i;
            const float *uptr;

            uptr = src + width * (y - 4) + (x - 4);
            for (i = 0; i < 9; i++)
                D3 += *uptr++;

            uptr = src + width * (y - 3) + (x - 4);
            for (i = 0; i < 3; i++)
                D3 += *uptr++;
            uptr += 3;
            for (i = 0; i < 3; i++)
                D3 += *uptr++;

            uptr = src + width * (y + 3) + (x - 4);
            for (i = 0; i < 3; i++)
                D3 += *uptr++;
            uptr += 3;
            for (i = 0; i < 3; i++)
                D3 += *uptr++;

            uptr = src + width * (y + 4) + (x - 4);
            for (i = 0; i < 9; i++)
                D3 += *uptr++;

            double mean = (A + B1 + B2 + C1 + C2 + C3 + D1 + D2 + D3) / 81.0;
            double PSF_fit = PSF[0] * (A - mean) + PSF[1] * (B1 - 4.0 * mean) + PSF[2] * (B2 - 4.0 * mean) +
                             PSF[3] * (C1 - 4.0 * mean) + PSF[4] * (C2 - 8.0 * mean) + PSF[5] * (C3 - 4.0 * mean) +
                             PSF[6] * (D1 - 4.0 * mean) + PSF[7] * (D2 - 8.0 * mean) + PSF[8] * (D3 - 44.0 * mean);

            dst[width * y + x] = (float) PSF_fit;
        }
    }
}

void GetStats(double *mean, double *stdev, int width, const float *img, const QRect &win)
{
    // Determine the mean and standard deviation
    double sum = 0.0;
    double a = 0.0;
    double q = 0.0;
    double k = 1.0;
    double km1 = 0.0;

    const float *p0 = img + win.top() * width + win.left();
    for (int y = 0; y < win.height(); y++)
    {
        const float *end = p0 + win.height();
        for (const float *p = p0; p < end; p++)
        {
            double const x = (double) * p;
            sum += x;
            double const a0 = a;
            a += (x - a) / k;
            q += (x - a0) * (x - a);
            km1 = k;
            k += 1.0;
        }
        p0 += width;
    }

    *mean = sum / km1;
    *stdev = sqrt(q / km1);
}

void RemoveItems(std::set<Peak> &stars, const std::set<int> &to_erase)
{
    int n = 0;
    for (std::set<Peak>::iterator it = stars.begin(); it != stars.end(); n++)
    {
        if (to_erase.find(n) != to_erase.end())
        {
            std::set<Peak>::iterator next = it;
            ++next;
            stars.erase(it);
            it = next;
        }
        else
            ++it;
    }
}

float *createFloatImage(const QSharedPointer<FITSData> &target)
{
    QSharedPointer<FITSData> imageData;

    /*************
    if (target.isNull())
        imageData = m_ImageData;
    else
    *********/ // shouldn't be null
    imageData = target;

    // #1 Convert to float array
    // We only process 1st plane if it is a color image
    uint32_t imgSize = imageData->width() * imageData->height();
    float *imgFloat  = new float[imgSize];

    if (imgFloat == nullptr)
    {
        qCritical() << "Not enough memory for float image array!";
        return nullptr;
    }

    switch (imageData->getStatistics().dataType)
    {
        case TBYTE:
        {
            uint8_t const *buffer = imageData->getImageBuffer();
            for (uint32_t i = 0; i < imgSize; i++)
                imgFloat[i] = buffer[i];
        }
        break;

        case TSHORT:
        {
            int16_t const *buffer = reinterpret_cast<int16_t const *>(imageData->getImageBuffer());
            for (uint32_t i = 0; i < imgSize; i++)
                imgFloat[i] = buffer[i];
        }
        break;

        case TUSHORT:
        {
            uint16_t const *buffer = reinterpret_cast<uint16_t const*>(imageData->getImageBuffer());
            for (uint32_t i = 0; i < imgSize; i++)
                imgFloat[i] = buffer[i];
        }
        break;

        case TLONG:
        {
            int32_t const *buffer = reinterpret_cast<int32_t const*>(imageData->getImageBuffer());
            for (uint32_t i = 0; i < imgSize; i++)
                imgFloat[i] = buffer[i];
        }
        break;

        case TULONG:
        {
            uint32_t const *buffer = reinterpret_cast<uint32_t const*>(imageData->getImageBuffer());
            for (uint32_t i = 0; i < imgSize; i++)
                imgFloat[i] = buffer[i];
        }
        break;

        case TFLOAT:
        {
            float const *buffer = reinterpret_cast<float const*>(imageData->getImageBuffer());
            for (uint32_t i = 0; i < imgSize; i++)
                imgFloat[i] = buffer[i];
        }
        break;

        case TLONGLONG:
        {
            int64_t const *buffer = reinterpret_cast<int64_t const*>(imageData->getImageBuffer());
            for (uint32_t i = 0; i < imgSize; i++)
                imgFloat[i] = buffer[i];
        }
        break;

        case TDOUBLE:
        {
            double const *buffer = reinterpret_cast<double const*>(imageData->getImageBuffer());
            for (uint32_t i = 0; i < imgSize; i++)
                imgFloat[i] = buffer[i];
        }
        break;

        default:
            delete[] imgFloat;
            return nullptr;
    }

    return imgFloat;
}

}  // namespace

// Based on PHD2 algorithm
QList<Edge*> GuideAlgorithms::detectStars(const QSharedPointer<FITSData> &imageData, const QRect &trackingBox)
{
    //Debug.Write(wxString::Format("Star::AutoFind called with edgeAllowance = %d searchRegion = %d\n", extraEdgeAllowance, searchRegion));

    // run a 3x3 median first to eliminate hot pixels
    //usImage smoothed;
    //smoothed.CopyFrom(image);
    //Median3(smoothed);
    constexpr int extraEdgeAllowance = 0;
    const QSharedPointer<FITSData> smoothed(new FITSData(imageData));
    smoothed->applyFilter(FITS_MEDIAN);

    int searchRegion = trackingBox.width();

    int subW = smoothed->width();
    int subH = smoothed->height();
    int size = subW * subH;

    // convert to floating point
    float *conv = createFloatImage(smoothed);

    // run the PSF convolution
    {
        float *tmp = new float[size] {};
        psf_conv(tmp, conv, subW, subH);
        delete [] conv;
        // Swap
        conv = tmp;
    }

    enum { CONV_RADIUS = 4 };
    int dw = subW;      // width of the downsampled image
    int dh = subH;     // height of the downsampled image
    QRect convRect(CONV_RADIUS, CONV_RADIUS, dw - 2 * CONV_RADIUS, dh - 2 * CONV_RADIUS);  // region containing valid data

    enum { TOP_N = 100 };  // keep track of the brightest stars
    std::set<Peak> stars;  // sorted by ascending intensity

    double global_mean, global_stdev;
    GetStats(&global_mean, &global_stdev, subW, conv, convRect);

    //Debug.Write(wxString::Format("AutoFind: global mean = %.1f, stdev %.1f\n", global_mean, global_stdev));

    const double threshold = 0.1;
    //Debug.Write(wxString::Format("AutoFind: using threshold = %.1f\n", threshold));

    // find each local maximum
    int srch = 4;
    for (int y = convRect.top() + srch; y <= convRect.bottom() - srch; y++)
    {
        for (int x = convRect.left() + srch; x <= convRect.right() - srch; x++)
        {
            float val = conv[dw * y + x];
            bool ismax = false;
            if (val > 0.0)
            {
                ismax = true;
                for (int j = -srch; j <= srch; j++)
                {
                    for (int i = -srch; i <= srch; i++)
                    {
                        if (i == 0 && j == 0)
                            continue;
                        if (conv[dw * (y + j) + (x + i)] > val)
                        {
                            ismax = false;
                            break;
                        }
                    }
                }
            }
            if (!ismax)
                continue;

            // compare local maximum to mean value of surrounding pixels
            const int local = 7;
            double local_mean, local_stdev;
            QRect localRect(x - local, y - local, 2 * local + 1, 2 * local + 1);
            localRect = localRect.intersected(convRect);
            GetStats(&local_mean, &local_stdev, subW, conv, localRect);

            // this is our measure of star intensity
            double h = (val - local_mean) / global_stdev;

            if (h < threshold)
            {
                //  Debug.Write(wxString::Format("AG: local max REJECT [%d, %d] PSF %.1f SNR %.1f\n", imgx, imgy, val, SNR));
                continue;
            }

            // coordinates on the original image
            int downsample = 1;
            int imgx = x * downsample + downsample / 2;
            int imgy = y * downsample + downsample / 2;

            stars.insert(Peak(imgx, imgy, h));
            if (stars.size() > TOP_N)
                stars.erase(stars.begin());
        }
    }

    //for (std::set<Peak>::const_reverse_iterator it = stars.rbegin(); it != stars.rend(); ++it)
    //qCDebug(KSTARS_EKOS_GUIDE) << "AutoFind: local max [" << it->x << "," << it->y << "]" << it->val;

    // merge stars that are very close into a single star
    {
        const int minlimitsq = 5 * 5;
repeat:
        for (std::set<Peak>::const_iterator a = stars.begin(); a != stars.end(); ++a)
        {
            std::set<Peak>::const_iterator b = a;
            ++b;
            for (; b != stars.end(); ++b)
            {
                int dx = a->x - b->x;
                int dy = a->y - b->y;
                int d2 = dx * dx + dy * dy;
                if (d2 < minlimitsq)
                {
                    // very close, treat as single star
                    //Debug.Write(wxString::Format("AutoFind: merge [%d, %d] %.1f - [%d, %d] %.1f\n", a->x, a->y, a->val, b->x, b->y, b->val));
                    // erase the dimmer one
                    stars.erase(a);
                    goto repeat;
                }
            }
        }
    }

    // exclude stars that would fit within a single searchRegion box
    {
        // build a list of stars to be excluded
        std::set<int> to_erase;
        const int extra = 5; // extra safety margin
        const int fullw = searchRegion + extra;
        for (std::set<Peak>::const_iterator a = stars.begin(); a != stars.end(); ++a)
        {
            std::set<Peak>::const_iterator b = a;
            ++b;
            for (; b != stars.end(); ++b)
            {
                int dx = abs(a->x - b->x);
                int dy = abs(a->y - b->y);
                if (dx <= fullw && dy <= fullw)
                {
                    // stars closer than search region, exclude them both
                    // but do not let a very dim star eliminate a very bright star
                    if (b->val / a->val >= 5.0)
                    {
                        //Debug.Write(wxString::Format("AutoFind: close dim-bright [%d, %d] %.1f - [%d, %d] %.1f\n", a->x, a->y, a->val, b->x, b->y, b->val));
                    }
                    else
                    {
                        //Debug.Write(wxString::Format("AutoFind: too close [%d, %d] %.1f - [%d, %d] %.1f\n", a->x, a->y, a->val, b->x, b->y, b->val));
                        to_erase.insert(std::distance(stars.begin(), a));
                        to_erase.insert(std::distance(stars.begin(), b));
                    }
                }
            }
        }
        RemoveItems(stars, to_erase);
    }

    // exclude stars too close to the edge
    {
        enum { MIN_EDGE_DIST = 40 };
        int edgeDist = MIN_EDGE_DIST;//pConfig->Profile.GetInt("/StarAutoFind/MinEdgeDist", MIN_EDGE_DIST);
        if (edgeDist < searchRegion)
            edgeDist = searchRegion;
        edgeDist += extraEdgeAllowance;

        std::set<Peak>::iterator it = stars.begin();
        while (it != stars.end())
        {
            std::set<Peak>::iterator next = it;
            ++next;
            if (it->x <= edgeDist || it->x >= subW - edgeDist ||
                    it->y <= edgeDist || it->y >= subH - edgeDist)
            {
                //Debug.Write(wxString::Format("AutoFind: too close to edge [%d, %d] %.1f\n", it->x, it->y, it->val));
                stars.erase(it);
            }
            it = next;
        }
    }

    QList<Edge*> centers;
    for (std::set<Peak>::reverse_iterator it = stars.rbegin(); it != stars.rend(); ++it)
    {
        Edge *center = new Edge;
        center->x = it->x;
        center->y = it->y;
        center->val = it->val;
        centers.append(center);
    }

    delete [] conv;

    return centers;
}


template <typename T>
GuiderUtils::Vector GuideAlgorithms::findLocalStarPosition(QSharedPointer<FITSData> &imageData,
        const int algorithmIndex,
        const int videoWidth,
        const int videoHeight,
        const QRect &trackingBox)
{
    static const double P0 = 0.906, P1 = 0.584, P2 = 0.365, P3 = 0.117, P4 = 0.049, P5 = -0.05, P6 = -0.064, P7 = -0.074,
                        P8 = -0.094;

    GuiderUtils::Vector ret(-1, -1, -1);
    int i, j;
    double resx, resy, mass, threshold, pval;
    T const *psrc    = nullptr;
    T const *porigin = nullptr;
    T const *pptr;

    if (trackingBox.isValid() == false)
        return GuiderUtils::Vector(-1, -1, -1);

    if (imageData.isNull())
    {
        qCWarning(KSTARS_EKOS_GUIDE) << "Cannot process a nullptr image.";
        return GuiderUtils::Vector(-1, -1, -1);
    }

    if (algorithmIndex == SEP_THRESHOLD)
    {
        QVariantMap settings;
        settings["optionsProfileIndex"] = Options::guideOptionsProfile();
        settings["optionsProfileGroup"] = static_cast<int>(Ekos::GuideProfiles);
        imageData->setSourceExtractorSettings(settings);
        QFuture<bool> result = imageData->findStars(ALGORITHM_SEP, trackingBox);
        result.waitForFinished();
        if (result.result())
        {
            imageData->getHFR(HFR_MEDIAN);
            const Edge star = imageData->getSelectedHFRStar();
            ret = GuiderUtils::Vector(star.x, star.y, 0);
        }
        else
            ret = GuiderUtils::Vector(-1, -1, -1);

        return ret;
    }

    T const *pdata = reinterpret_cast<T const*>(imageData->getImageBuffer());

    qCDebug(KSTARS_EKOS_GUIDE) << "Tracking Square " << trackingBox;

    double square_square = trackingBox.width() * trackingBox.width();

    psrc = porigin = pdata + trackingBox.y() * videoWidth + trackingBox.x();

    resx = resy = 0;
    threshold = mass = 0;

    // several threshold adaptive smart algorithms
    switch (algorithmIndex)
    {
        case CENTROID_THRESHOLD:
        {
            int width  = trackingBox.width();
            int height = trackingBox.width();
            float i0, i1, i2, i3, i4, i5, i6, i7, i8;
            int ix = 0, iy = 0;
            int xM4;
            T const *p;
            double average, fit, bestFit = 0;
            int minx = 0;
            int maxx = width;
            int miny = 0;
            int maxy = height;
            for (int x = minx; x < maxx; x++)
                for (int y = miny; y < maxy; y++)
                {
                    i0 = i1 = i2 = i3 = i4 = i5 = i6 = i7 = i8 = 0;
                    xM4                                        = x - 4;
                    p                                          = psrc + (y - 4) * videoWidth + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = psrc + (y - 3) * videoWidth + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i7 += *p++;
                    i6 += *p++;
                    i7 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = psrc + (y - 2) * videoWidth + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i5 += *p++;
                    i4 += *p++;
                    i3 += *p++;
                    i4 += *p++;
                    i5 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = psrc + (y - 1) * videoWidth + xM4;
                    i8 += *p++;
                    i7 += *p++;
                    i4 += *p++;
                    i2 += *p++;
                    i1 += *p++;
                    i2 += *p++;
                    i4 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = psrc + (y + 0) * videoWidth + xM4;
                    i8 += *p++;
                    i6 += *p++;
                    i3 += *p++;
                    i1 += *p++;
                    i0 += *p++;
                    i1 += *p++;
                    i3 += *p++;
                    i6 += *p++;
                    i8 += *p++;
                    p = psrc + (y + 1) * videoWidth + xM4;
                    i8 += *p++;
                    i7 += *p++;
                    i4 += *p++;
                    i2 += *p++;
                    i1 += *p++;
                    i2 += *p++;
                    i4 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = psrc + (y + 2) * videoWidth + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i5 += *p++;
                    i4 += *p++;
                    i3 += *p++;
                    i4 += *p++;
                    i5 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = psrc + (y + 3) * videoWidth + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i7 += *p++;
                    i6 += *p++;
                    i7 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = psrc + (y + 4) * videoWidth + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    average = (i0 + i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8) / 85.0;
                    fit     = P0 * (i0 - average) + P1 * (i1 - 4 * average) + P2 * (i2 - 4 * average) +
                              P3 * (i3 - 4 * average) + P4 * (i4 - 8 * average) + P5 * (i5 - 4 * average) +
                              P6 * (i6 - 4 * average) + P7 * (i7 - 8 * average) + P8 * (i8 - 48 * average);
                    if (bestFit < fit)
                    {
                        bestFit = fit;
                        ix      = x;
                        iy      = y;
                    }
                }

            if (bestFit > 50)
            {
                double sumX  = 0;
                double sumY  = 0;
                double total = 0;
                for (int y = iy - 4; y <= iy + 4; y++)
                {
                    p = psrc + y * width + ix - 4;
                    for (int x = ix - 4; x <= ix + 4; x++)
                    {
                        double w = *p++;
                        sumX += x * w;
                        sumY += y * w;
                        total += w;
                    }
                }
                if (total > 0)
                {
                    ret = (GuiderUtils::Vector(trackingBox.x(), trackingBox.y(), 0) + GuiderUtils::Vector(sumX / total, sumY / total, 0));
                    return ret;
                }
            }

            return GuiderUtils::Vector(-1, -1, -1);
        }
        break;
        // Alexander's Stepanenko smart threshold algorithm
        case SMART_THRESHOLD:
        {
            point_t bbox_lt = { trackingBox.x() - SMART_FRAME_WIDTH, trackingBox.y() - SMART_FRAME_WIDTH };
            point_t bbox_rb = { trackingBox.x() + trackingBox.width() + SMART_FRAME_WIDTH,
                                trackingBox.y() + trackingBox.width() + SMART_FRAME_WIDTH
                              };
            int offset      = 0;

            // clip frame
            if (bbox_lt.x < 0)
                bbox_lt.x = 0;
            if (bbox_lt.y < 0)
                bbox_lt.y = 0;
            if (bbox_rb.x > videoWidth)
                bbox_rb.x = videoWidth;
            if (bbox_rb.y > videoHeight)
                bbox_rb.y = videoHeight;

            // calc top bar
            int box_wd  = bbox_rb.x - bbox_lt.x;
            int box_ht  = trackingBox.y() - bbox_lt.y;
            int pix_cnt = 0;
            if (box_wd > 0 && box_ht > 0)
            {
                pix_cnt += box_wd * box_ht;
                for (j = bbox_lt.y; j < trackingBox.y(); ++j)
                {
                    offset = j * videoWidth;
                    for (i = bbox_lt.x; i < bbox_rb.x; ++i)
                    {
                        pptr = pdata + offset + i;
                        threshold += *pptr;
                    }
                }
            }
            // calc left bar
            box_wd = trackingBox.x() - bbox_lt.x;
            box_ht = trackingBox.width();
            if (box_wd > 0 && box_ht > 0)
            {
                pix_cnt += box_wd * box_ht;
                for (j = trackingBox.y(); j < trackingBox.y() + box_ht; ++j)
                {
                    offset = j * videoWidth;
                    for (i = bbox_lt.x; i < trackingBox.x(); ++i)
                    {
                        pptr = pdata + offset + i;
                        threshold += *pptr;
                    }
                }
            }
            // calc right bar
            box_wd = bbox_rb.x - trackingBox.x() - trackingBox.width();
            box_ht = trackingBox.width();
            if (box_wd > 0 && box_ht > 0)
            {
                pix_cnt += box_wd * box_ht;
                for (j = trackingBox.y(); j < trackingBox.y() + box_ht; ++j)
                {
                    offset = j * videoWidth;
                    for (i = trackingBox.x() + trackingBox.width(); i < bbox_rb.x; ++i)
                    {
                        pptr = pdata + offset + i;
                        threshold += *pptr;
                    }
                }
            }
            // calc bottom bar
            box_wd = bbox_rb.x - bbox_lt.x;
            box_ht = bbox_rb.y - trackingBox.y() - trackingBox.width();
            if (box_wd > 0 && box_ht > 0)
            {
                pix_cnt += box_wd * box_ht;
                for (j = trackingBox.y() + trackingBox.width(); j < bbox_rb.y; ++j)
                {
                    offset = j * videoWidth;
                    for (i = bbox_lt.x; i < bbox_rb.x; ++i)
                    {
                        pptr = pdata + offset + i;
                        threshold += *pptr;
                    }
                }
            }
            // find maximum
            double max_val = 0;
            for (j = 0; j < trackingBox.width(); ++j)
            {
                for (i = 0; i < trackingBox.width(); ++i)
                {
                    pptr = psrc + i;
                    if (*pptr > max_val)
                        max_val = *pptr;
                }
                psrc += videoWidth;
            }
            if (pix_cnt != 0)
                threshold /= (double)pix_cnt;

            // cut by 10% higher then average threshold
            if (max_val > threshold)
                threshold += (max_val - threshold) * SMART_CUT_FACTOR;

            //log_i("smart thr. = %f cnt = %d", threshold, pix_cnt);
            break;
        }
        // simple adaptive threshold
        case AUTO_THRESHOLD:
        {
            for (j = 0; j < trackingBox.width(); ++j)
            {
                for (i = 0; i < trackingBox.width(); ++i)
                {
                    pptr = psrc + i;
                    threshold += *pptr;
                }
                psrc += videoWidth;
            }
            threshold /= square_square;
            break;
        }
        // no threshold subtracion
        default:
        {
        }
    }

    psrc = porigin;
    for (j = 0; j < trackingBox.width(); ++j)
    {
        for (i = 0; i < trackingBox.width(); ++i)
        {
            pptr = psrc + i;
            pval = *pptr - threshold;
            pval = pval < 0 ? 0 : pval;

            resx += (double)i * pval;
            resy += (double)j * pval;

            mass += pval;
        }
        psrc += videoWidth;
    }

    if (mass == 0)
        mass = 1;

    resx /= mass;
    resy /= mass;

    ret = GuiderUtils::Vector(trackingBox.x(), trackingBox.y(), 0) + GuiderUtils::Vector(resx, resy, 0);

    return ret;
}

GuiderUtils::Vector GuideAlgorithms::findLocalStarPosition(QSharedPointer<FITSData> &imageData,
        const int algorithmIndex,
        const int videoWidth,
        const int videoHeight,
        const QRect &trackingBox)
{
    switch (imageData->dataType())
    {
        case TBYTE:
            return findLocalStarPosition<uint8_t>(imageData, algorithmIndex, videoWidth, videoHeight, trackingBox);

        case TSHORT:
            return findLocalStarPosition<int16_t>(imageData, algorithmIndex, videoWidth, videoHeight, trackingBox);

        case TUSHORT:
            return findLocalStarPosition<uint16_t>(imageData, algorithmIndex, videoWidth, videoHeight, trackingBox);

        case TLONG:
            return findLocalStarPosition<int32_t>(imageData, algorithmIndex, videoWidth, videoHeight, trackingBox);

        case TULONG:
            return findLocalStarPosition<uint32_t>(imageData, algorithmIndex, videoWidth, videoHeight, trackingBox);

        case TFLOAT:
            return findLocalStarPosition<float>(imageData, algorithmIndex, videoWidth, videoHeight, trackingBox);

        case TLONGLONG:
            return findLocalStarPosition<int64_t>(imageData, algorithmIndex, videoWidth, videoHeight, trackingBox);

        case TDOUBLE:
            return findLocalStarPosition<double>(imageData, algorithmIndex, videoWidth, videoHeight, trackingBox);

        default:
            break;
    }

    return GuiderUtils::Vector(-1, -1, -1);
}
