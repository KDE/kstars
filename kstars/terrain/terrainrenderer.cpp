/*
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "terrainrenderer.h"

#include "kstars_debug.h"
#include "Options.h"
#include "skymap.h"
#include "skyqpainter.h"
#include "projections/projector.h"
#include "skypoint.h"
#include "kstars.h"

#include <QStatusBar>

// This is the factory that builds the one-and-only TerrainRenderer.
TerrainRenderer * TerrainRenderer::_terrainRenderer = nullptr;
TerrainRenderer *TerrainRenderer::Instance()
{
    if (_terrainRenderer == nullptr)
        _terrainRenderer = new TerrainRenderer();

    return _terrainRenderer;
}

// This class implements a quick 2D float array.
class TerrainLookup
{
    public:
        TerrainLookup(int width, int height) :
            valPtr(new float[width * height]), valWidth(width)
        {
            memset(valPtr, 0, width * height * sizeof(float));
        }
        ~TerrainLookup()
        {
            delete[] valPtr;
        }
        inline float get(int w, int h)
        {
            return valPtr[h * valWidth + w];
        }
        inline void set(int w, int h, float val)
        {
            valPtr[h * valWidth + w] = val;
        }
    private:
        float *valPtr;
        int valWidth = 0;
};

// Samples 2-D array and returns interpolated values for the unsampled elements.
// Used to speed up the calculation of azimuth and altitude values for all pixels
// of the array to be rendered. Instead we compute every nth pixel's coordinates (e.g. n=4)
// and interpolate the azimuth and alitutde values (so 1/16 the number of calculations).
// This should be more than accurate enough for this rendering, and this part
// of the code definitely needs speed.
class InterpArray
{
    public:
        // Constructor calculates the downsampled size and allocates the 2D arrays
        // for azimuth and altitude.
        InterpArray(int width, int height, int samplingFactor) : sampling(samplingFactor)
        {
            int downsampledWidth = width / sampling;
            if (width % sampling != 0)
                downsampledWidth += 1;
            lastDownsampledCol = downsampledWidth - 1;

            int downsampledHeight = height / sampling;
            if (height % sampling != 0)
                downsampledHeight += 1;
            lastDownsampledRow = downsampledHeight - 1;

            azLookup = new TerrainLookup(downsampledWidth, downsampledHeight);
            altLookup = new TerrainLookup(downsampledWidth, downsampledHeight);
        }

        ~InterpArray()
        {
            delete azLookup;
            delete altLookup;
        }

        // Get the azimuth and altitude values from the 2D arrays.
        // Inputs are a full-image position
        inline void get(int x, int y, float *az, float *alt)
        {
            const bool rowSampled = y % sampling == 0;
            const bool colSampled = x % sampling == 0;
            const int xSampled = x / sampling;
            const int ySampled = y / sampling;

            // If the requested position has been calculated, the use the stored value.
            if (rowSampled && colSampled)
            {
                *az = azLookup->get(xSampled, ySampled);
                *alt = altLookup->get(xSampled, ySampled);
                return;
            }
            // The desired position has not been calculated, but values on its row have been calculated.
            // Interpolate between the nearest values on the row.
            else if (rowSampled)
            {
                if (xSampled >= lastDownsampledCol)
                {
                    // If the requested value is on or past the last calculated value,
                    // just use that last value.
                    *az = azLookup->get(xSampled, ySampled);
                    *alt = altLookup->get(xSampled, ySampled);
                    return;
                }
                // Weight the contributions of the 2 calculated values on the row by their distance to the desired position.
                const float weight2 = static_cast<float>(x) / sampling - xSampled;
                const float weight1 = 1 - weight2;
                *az  = weight1 *  azLookup->get(xSampled, ySampled) + weight2 *  azLookup->get(xSampled + 1, ySampled);
                *alt = weight1 * altLookup->get(xSampled, ySampled) + weight2 * altLookup->get(xSampled + 1, ySampled);
                return;
            }
            // The desired position has not been calculated, but values on its column have been calculated.
            // Interpolate between the nearest values on the column.
            else if (colSampled)
            {
                if (ySampled >= lastDownsampledRow)
                {
                    // If the requested value is on or past the last calculated value,
                    // just use that last value.
                    *az = azLookup->get(xSampled, ySampled);
                    *alt = altLookup->get(xSampled, ySampled);
                    return;
                }
                // Weight the contributions of the 2 calculated values on the column by their distance to the desired position.
                const float weight2 = static_cast<float>(y) / sampling - ySampled;
                const float weight1 = 1 - weight2;
                *az  = weight1 *  azLookup->get(xSampled, ySampled) + weight2 *  azLookup->get(xSampled, ySampled + 1);
                *alt = weight1 * altLookup->get(xSampled, ySampled) + weight2 * altLookup->get(xSampled, ySampled + 1);
                return;
            }
            else
            {
                // The desired position has not been calculated, and no values on its column nor row have been calculated.
                // Interpolate between the nearest 4 values.
                if (xSampled >= lastDownsampledCol || ySampled >= lastDownsampledRow)
                {
                    // If we're near the edge, just use a close value. Harder to interpolate and not too important.
                    *az = azLookup->get(xSampled, ySampled);
                    *alt = altLookup->get(xSampled, ySampled);
                    return;
                }
                // The section uses distance along the two nearest calculated rows to come up with an estimate.
                const float weight2 = static_cast<float>(x) / sampling - xSampled;
                const float weight1 = 1 - weight2;
                const float azWval  = (weight1  * ( azLookup->get(xSampled,      ySampled) +  azLookup->get(xSampled,     ySampled + 1)) +
                                       weight2  * ( azLookup->get(xSampled + 1,  ySampled) +  azLookup->get(xSampled + 1, ySampled + 1)));
                const float altWval = (weight1  * (altLookup->get(xSampled,      ySampled) + altLookup->get(xSampled,     ySampled + 1)) +
                                       weight2  * (altLookup->get(xSampled + 1,  ySampled) + altLookup->get(xSampled + 1, ySampled + 1)));

                // The section uses distance along the two nearest calculated columns to come up with an estimate.
                const float hweight2 = static_cast<float>(y) / sampling - ySampled;
                const float hweight1 = 1 - hweight2;
                const float azHval =  (hweight2 * ( azLookup->get(xSampled,  ySampled)     +  azLookup->get(xSampled + 1, ySampled)) +
                                       hweight1 * ( azLookup->get(xSampled,  ySampled + 1) +  azLookup->get(xSampled + 1, ySampled + 1)));
                const float altHval = (hweight2 * (altLookup->get(xSampled,  ySampled)     + altLookup->get(xSampled + 1, ySampled)) +
                                       hweight1 * (altLookup->get(xSampled,  ySampled + 1) + altLookup->get(xSampled + 1, ySampled + 1)));
                // We average the 2 estimates (note above actually estimated twice the value).
                *az  = (azWval + azHval) / 4.0;
                *alt = (altWval + altHval) / 4.0;
                return;
            }
        }
        TerrainLookup *azimuthLookup()
        {
            return azLookup;
        }
        TerrainLookup *altitudeLookup()
        {
            return altLookup;
        }

    private:
        // These are the indeces of the last rows and columns (in the downsampled sized space) that were filled.
        // This is needed because the downsample factor might not be an even multiple of the image size.
        int lastDownsampledCol = 0;
        int lastDownsampledRow = 0;
        // The downsample factor.
        int sampling = 0;
        // The azimuth and altitude values are stored in these 2D arrays.
        TerrainLookup *azLookup = nullptr;
        TerrainLookup *altLookup = nullptr;
};

TerrainRenderer::TerrainRenderer()
{
}

// Put degrees in the range of 0 -> 359.99999999
double rationalizeAz(double degrees)
{
    if (degrees < -1000 || degrees > 1000)
        return 0;

    while (degrees < 0)
        degrees += 360.0;
    while (degrees >= 360.0)
        degrees -= 360.0;
    return degrees;
}

// Checks that degrees in the range of -90 -> 90.
double rationalizeAlt(double degrees)
{
    if (degrees < -90.0 || degrees > 90.0)
        return 0.0;

    return degrees;
}

// Assumes the source photosphere has rows which, left-to-right go from AZ=0 to AZ=360
// and columns go from -90 altitude on the bottom to +90 on top.
// Returns the pixel for the desired azimuth and altitude.
QRgb TerrainRenderer::getPixel(double az, double alt) const
{
    az = rationalizeAz(az + Options::terrainSourceCorrectAz());
    // This may make alt > 90 (due to a negative sourceCorrectAlt).
    // If so, it returns 0, which is a transparent pixel.
    alt = alt - Options::terrainSourceCorrectAlt();
    if (az < 0 || az >= 360 || alt < -90 || alt > 90)
        return(0);

    // shift az to be -180 to 180
    if (az > 180)
        az = az - 360.0;
    const int width = sourceImage.width();
    const int height = sourceImage.height();

    if (!Options::terrainSmoothPixels())
    {
        // az=0 should be the middle of the image.
        int pixX = width / 2 + (az / 360.0) * width;
        if (pixX > width - 1)
            pixX = width - 1;
        else if (pixX < 0)
            pixX = 0;
        int pixY = ((alt + 90.0) / 180.0) * height;
        if (pixY > height - 1)
            pixY = height - 1;
        pixY = (height - 1) - pixY;
        return sourceImage.pixel(pixX, pixY);
    }

    // Get floating point pixel postions so we can interpolate.
    float pixX = width / 2 + (az / 360.0) * width;
    if (pixX > width - 1)
        pixX = width - 1;
    else if (pixX < 0)
        pixX = 0;
    float pixY = ((alt + 90.0) / 180.0) * height;
    if (pixY > height - 1)
        pixY = height - 1;
    pixY = (height - 1) - pixY;

    // Don't bother interpolating for transparent pixels.
    constexpr int lowAlpha = 0.1 * 255;
    if (qAlpha(sourceImage.pixel(pixX, pixY)) < lowAlpha)
        return sourceImage.pixel(pixX, pixY);

    // Instead of just returning the pixel at the truncated position as above,
    // below we interpolate the pixel RGBA values based on the floating-point pixel position.
    int x1 = static_cast<int>(pixX);
    int y1 = static_cast<int>(pixY);

    if ((x1 >= width - 1) || (y1 >= height - 1))
        return sourceImage.pixel(x1, y1);

    // weights for the x & x+1, and y & y+1 positions.
    float wx2 = pixX - x1;
    float wx1 = 1.0 - wx2;
    float wy2 = pixY - y1;
    float wy1 = 1.0 - wy2;

    // The pixels we'll interpolate.
    QRgb c11(qUnpremultiply(sourceImage.pixel(pixX, pixY)));
    QRgb c12(qUnpremultiply(sourceImage.pixel(pixX, pixY + 1)));
    QRgb c21(qUnpremultiply(sourceImage.pixel(pixX + 1, pixY)));
    QRgb c22(qUnpremultiply(sourceImage.pixel(pixX + 1, pixY + 1)));

    // Weights for the above pixels.
    float w11 = wx1 * wy1;
    float w12 = wx1 * wy2;
    float w21 = wx2 * wy1;
    float w22 = wx2 * wy2;

    // Finally, interpolate each component.
    int red =   w11 * qRed(c11)   + w12 * qRed(c12)   + w21 * qRed(c21)   + w22 * qRed(c22);
    int green = w11 * qGreen(c11) + w12 * qGreen(c12) + w21 * qGreen(c21) + w22 * qGreen(c22);
    int blue =  w11 * qBlue(c11)  + w12 * qBlue(c12)  + w21 * qBlue(c21)  + w22 * qBlue(c22);
    int alpha = w11 * qAlpha(c11)  + w12 * qAlpha(c12)  + w21 * qAlpha(c21)  + w22 * qAlpha(c22);
    return qPremultiply(qRgba(red, green, blue, alpha));
}

// Checks to see if the view is the same as the last call to render.
// If true, render (below) will skip its computations and return the same image
// as was previously calculated.
// If the view is not the same, this method stores away the details of the current view
// so that it may make this comparison again in the future.
bool TerrainRenderer::sameView(const Projector *proj, bool forceRefresh)
{
    ViewParams view = proj->viewParams();
    SkyPoint point = *(view.focus);
    const auto &lst = KStarsData::Instance()->lst();
    const auto &lat = KStarsData::Instance()->geo()->lat();

    // We compare az and alt, not RA and DEC, as the RA changes,
    // but the rendering is tied to azimuth and altitude which may not change.
    // Thus we convert point to horizontal coordinates.
    point.EquatorialToHorizontal(lst, lat);
    const double az = rationalizeAz(point.az().Degrees());
    const double alt = rationalizeAlt(point.alt().Degrees());

    bool ok = view.width == savedViewParams.width &&
              view.zoomFactor == savedViewParams.zoomFactor &&
              view.useRefraction == savedViewParams.useRefraction &&
              view.useAltAz == savedViewParams.useAltAz &&
              view.fillGround == savedViewParams.fillGround;
    const double azDiff = fabs(savedAz - az);
    const double altDiff = fabs(savedAlt - alt);
    if (!forceRefresh && ok && azDiff < .0001 && altDiff < .0001)
        return true;

    // Store the view
    savedViewParams = view;
    savedViewParams.focus = nullptr;
    savedAz = az;
    savedAlt = alt;
    return false;
}

bool TerrainRenderer::render(uint16_t w, uint16_t h, QImage *terrainImage, const Projector *proj)
{
    // This is used to force a re-render, e.g. when the image is changed.
    bool dirty = false;

    // Load the image if necessary.
    QString filename = Options::terrainSource();
    if (!initialized || filename != sourceFilename)
    {
        dirty = true;
        QImage image;
        if (image.load(filename))
        {
            sourceImage = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            qCDebug(KSTARS) << QString("Read terrain file %1 x %2").arg(sourceImage.width()).arg(sourceImage.height());
            sourceFilename = filename;
            initialized = true;
        }
        else
        {
            if (filename.isEmpty())
                KStars::Instance()->statusBar()->showMessage(i18n("Failed to load terrain. Set terrain file in Settings."));
            else
                KStars::Instance()->statusBar()->showMessage(i18n("Failed to load terrain image (%1). Set terrain file in Settings.",
                        filename));
            initialized = false;
            Options::setShowTerrain(false);
            KStars::Instance()->syncOps();
        }
    }

    if (!initialized)
        return false;

    // Check if parameters have changed.
    if ((terrainDownsampling != Options::terrainDownsampling()) ||
            (terrainSmoothPixels != Options::terrainSmoothPixels()) ||
            (terrainSkipSpeedup != Options::terrainSkipSpeedup()) ||
            (terrainTransparencySpeedup != Options::terrainTransparencySpeedup()) ||
            (terrainSourceCorrectAz != Options::terrainSourceCorrectAz()) ||
            (terrainSourceCorrectAlt != Options::terrainSourceCorrectAlt()))
        dirty = true;

    terrainDownsampling = Options::terrainDownsampling();
    terrainSmoothPixels = Options::terrainSmoothPixels();
    terrainSkipSpeedup = Options::terrainSkipSpeedup();
    terrainTransparencySpeedup = Options::terrainTransparencySpeedup();
    terrainSourceCorrectAz = Options::terrainSourceCorrectAz();
    terrainSourceCorrectAlt = Options::terrainSourceCorrectAlt();

    if (sameView(proj, dirty))
    {
        // Just return the previous image if the input view hasn't changed.
        *terrainImage = savedImage.copy();
        return true;
    }

    QTime timer;
    timer.start();

    // Only compute the pixel's az and alt values for every Nth pixel.
    // Get the other pixel az and alt values by interpolation.
    // This saves a lot of time.
    const int sampling = Options::terrainDownsampling();
    InterpArray interp(w, h, sampling);
    QTime setupTimer;
    setupTimer.start();
    setupLookup(w, h, sampling, proj, interp.azimuthLookup(), interp.altitudeLookup());

    const double setupTime = setupTimer.elapsed() / 1000.0; ///////////////////

    // Another speedup. If true, out calculations are downsampled by 2 in each dimension.
    const bool skip = Options::terrainSkipSpeedup() || SkyMap::IsSlewing();
    int increment = skip ? 2 : 1;

    // Assign transparent pixels everywhere by default.
    terrainImage->fill(0);

    // Go through the image, and for each pixel, using the previously computed az and alt values
    // get the corresponding pixel from the terrain image.
    bool lastTransparent = false;
    for (int j = 0; j < h; j += increment)
    {
        bool notLastRow = j != h - 1;
        for (int i = 0; i < w; i += increment)
        {
            if (lastTransparent && Options::terrainTransparencySpeedup())
            {
                // Speedup--if the last pixel was transparent, then this
                // one is assumed transparent too (but next is calculated).
                lastTransparent = false;
                continue;
            }

            const QPointF imgPoint(i, j);
            if (!proj->unusablePoint(imgPoint))
            {
                float az, alt;
                interp.get(i, j, &az, &alt);
                const QRgb pixel = getPixel(az, alt);
                terrainImage->setPixel(i, j, pixel);
                lastTransparent = (pixel == 0);

                if (skip)
                {
                    // If we've skipped, fill in the missing pixels.
                    bool notLastCol = i != w - 1;
                    if (notLastCol)
                        terrainImage->setPixel(i + 1, j, pixel);
                    if (notLastRow)
                        terrainImage->setPixel(i, j + 1, pixel);
                    if (notLastRow && notLastCol)
                        terrainImage->setPixel(i + 1, j + 1, pixel);
                }
            }
            // Otherwise terrainImage was already filled with transparent pixels
            // so i,j will be transparent.
        }
    }
    QTime copyTimer;
    copyTimer.start();
    savedImage = terrainImage->copy();

    QFile f(sourceFilename);
    QFileInfo fileInfo(f.fileName());
    QString fName(fileInfo.fileName());
    QString dbgMsg(QString("Terrain rendering: %1px, %2s (%3s) %4 ds %5 skip %6 trnsp %7 pan %8 smooth %9")
                   .arg(w * h)
                   .arg(timer.elapsed() / 1000.0, 5, 'f', 3)
                   .arg(setupTime, 5, 'f', 3)
                   .arg(fName)
                   .arg(Options::terrainDownsampling())
                   .arg(Options::terrainSkipSpeedup() ? "T" : "F")
                   .arg(Options::terrainTransparencySpeedup() ? "T" : "F")
                   .arg(Options::terrainPanning() ? "T" : "F")
                   .arg(Options::terrainSmoothPixels() ? "T" : "F"));
    //qCDebug(KSTARS) << dbgMsg;
    //fprintf(stderr, "%s\n", dbgMsg.toLatin1().data());

    dirty = false;
    return true;
}

// Goes through every Nth input pixel position, finding their azimuth and altitude
// and storing that for future use in the interpolations above.
// This is the most time-costly part of the computation.
void TerrainRenderer::setupLookup(uint16_t w, uint16_t h, int sampling, const Projector *proj, TerrainLookup *azLookup,
                                  TerrainLookup *altLookup)
{
    const auto &lst = KStarsData::Instance()->lst();
    const auto &lat = KStarsData::Instance()->geo()->lat();
    for (int j = 0, js = 0; j < h; j += sampling, js++)
    {
        for (int i = 0, is = 0; i < w; i += sampling, is++)
        {
            const QPointF imgPoint(i, j);
            if (!proj->unusablePoint(imgPoint))
            {
                SkyPoint point = proj->fromScreen(imgPoint, lst, lat, true);
                const double az = rationalizeAz(point.az().Degrees());
                const double alt = rationalizeAlt(point.alt().Degrees());
                azLookup->set(is, js, az);
                altLookup->set(is, js, alt);
            }
        }
    }
}

