/*
    SPDX-FileCopyrightText: 2014 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "eyepiecefield.h"

#include "fov.h"
#include "ksdssdownloader.h"
#include "kstars.h"
#include "Options.h"
#include "skymap.h"
#include "skyqpainter.h"

#include <QBitmap>
#include <QTemporaryFile>
#include <QSvgGenerator>
#include <QSvgRenderer>

#include <kstars_debug.h>

void EyepieceField::generateEyepieceView(SkyPoint *sp, QImage *skyChart, QImage *skyImage, const FOV *fov,
        const QString &imagePath)
{
    if (fov)
    {
        generateEyepieceView(sp, skyChart, skyImage, fov->sizeX(), fov->sizeY(), imagePath);
    }
    else
    {
        generateEyepieceView(sp, skyChart, skyImage, -1.0, -1.0, imagePath);
    }
}

void EyepieceField::generateEyepieceView(SkyPoint *sp, QImage *skyChart, QImage *skyImage, double fovWidth,
        double fovHeight, const QString &imagePath)
{
    SkyMap *map = SkyMap::Instance();
    KStars *ks  = KStars::Instance();

    Q_ASSERT(sp);
    Q_ASSERT(map);
    Q_ASSERT(ks);
    Q_ASSERT(skyChart);

    if (!skyChart)
        return;

    if (!map) // Requires initialization of Sky map.
        return;

    if (fovWidth <= 0)
    {
        if (!QFile::exists(imagePath))
            return;
        // Otherwise, we will assume that the user wants the FOV of the image and we'll try to guess it from there
    }
    if (fovHeight <= 0)
        fovHeight = fovWidth;

    // Get DSS image width / height
    double dssWidth = 0, dssHeight = 0;

    if (QFile::exists(imagePath))
    {
        KSDssImage dssImage(imagePath);
        dssWidth  = dssImage.getMetadata().width;
        dssHeight = dssImage.getMetadata().height;
        if (!dssImage.getMetadata().isValid() || dssWidth == 0 || dssHeight == 0)
        {
            // Metadata unavailable, guess based on most common DSS arcsec/pixel
            //const double dssArcSecPerPixel = 1.01;
            dssWidth  = dssImage.getImage().width() * 1.01 / 60.0;
            dssHeight = dssImage.getImage().height() * 1.01 / 60.0;
        }
        qCDebug(KSTARS) << "DSS width: " << dssWidth << " height: " << dssHeight;
    }

    // Set FOV width/height from DSS if necessary
    if (fovWidth <= 0)
    {
        fovWidth  = dssWidth;
        fovHeight = dssHeight;
    }

    // Grab the sky chart
    // Save the current state of the sky map
    SkyPoint *oldFocus   = map->focus();
    double oldZoomFactor = Options::zoomFactor();

    // Set the right zoom
    ks->setApproxFOV(((fovWidth > fovHeight) ? fovWidth : fovHeight) / 15.0);

    //    map->setFocus( sp ); // FIXME: Why does setFocus() need a non-const SkyPoint pointer?
    KStarsData *const data = KStarsData::Instance();
    sp->updateCoords(data->updateNum(), true, data->geo()->lat(), data->lst(), false);
    map->setClickedPoint(sp);
    map->slotCenter();
    qApp->processEvents();

    // Repeat -- dirty workaround for some problem in KStars
    map->setClickedPoint(sp);
    map->slotCenter();
    qApp->processEvents();

    // determine screen arcminutes per pixel value
    const double arcMinToScreen = dms::PI * Options::zoomFactor() / 10800.0;

    // Vector export
    QTemporaryFile myTempSvgFile;
    myTempSvgFile.open();

    // export as SVG
    QSvgGenerator svgGenerator;
    svgGenerator.setFileName(myTempSvgFile.fileName());
    // svgGenerator.setTitle(i18n(""));
    // svgGenerator.setDescription(i18n(""));
    svgGenerator.setSize(QSize(map->width(), map->height()));
    svgGenerator.setResolution(qMax(map->logicalDpiX(), map->logicalDpiY()));
    svgGenerator.setViewBox(QRect(map->width() / 2.0 - arcMinToScreen * fovWidth / 2.0,
                                  map->height() / 2.0 - arcMinToScreen * fovHeight / 2.0, arcMinToScreen * fovWidth,
                                  arcMinToScreen * fovHeight));

    SkyQPainter painter(KStars::Instance(), &svgGenerator);
    painter.begin();

    map->exportSkyImage(&painter);

    painter.end();

    // Render SVG file on raster QImage canvas
    QSvgRenderer svgRenderer(myTempSvgFile.fileName());
    QImage *mySkyChart = new QImage(arcMinToScreen * fovWidth * 2.0, arcMinToScreen * fovHeight * 2.0,
                                    QImage::Format_ARGB32); // 2 times bigger in both dimensions.
    QPainter p2(mySkyChart);
    svgRenderer.render(&p2);
    p2.end();
    *skyChart = *mySkyChart;
    delete mySkyChart;

    myTempSvgFile.close();

    // Reset the sky-map
    map->setZoomFactor(oldZoomFactor);
    map->setClickedPoint(oldFocus);
    map->slotCenter();
    qApp->processEvents();

    // Repeat -- dirty workaround for some problem in KStars
    map->setZoomFactor(oldZoomFactor);
    map->setClickedPoint(oldFocus);
    map->slotCenter();
    qApp->processEvents();
    map->forceUpdate();

    // Prepare the sky image
    if (QFile::exists(imagePath) && skyImage)
    {
        QImage *mySkyImage = new QImage(int(arcMinToScreen * fovWidth * 2.0), int(arcMinToScreen * fovHeight * 2.0),
                                        QImage::Format_ARGB32);

        mySkyImage->fill(Qt::transparent);

        QPainter p(mySkyImage);
        QImage rawImg(imagePath);

        if (rawImg.isNull())
        {
            qWarning() << "Image constructed from " << imagePath
                       << "is a null image! Are you sure you supplied an image file? Continuing nevertheless...";
        }

        QImage img     = rawImg.scaled(arcMinToScreen * dssWidth * 2.0, arcMinToScreen * dssHeight * 2.0,
                                       Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        const auto ksd = KStarsData::Instance();
        sp->updateCoordsNow(ksd->updateNum());

        if (Options::useAltAz())
        {
            // Need to rotate the image so that up is towards zenith rather than north.
            sp->EquatorialToHorizontal(ksd->lst(), ksd->geo()->lat());
            dms northBearing = findNorthAngle(sp, ksd->geo()->lat());
            qCDebug(KSTARS) << "North angle = " << northBearing.toDMSString();

            QTransform transform;

            transform.rotate(northBearing.Degrees());
            img = img.transformed(transform, Qt::SmoothTransformation);
        }
        p.drawImage(
            QPointF(mySkyImage->width() / 2.0 - img.width() / 2.0, mySkyImage->height() / 2.0 - img.height() / 2.0),
            img);
        p.end();

        *skyImage = *mySkyImage;
        delete mySkyImage;
    }
}

void EyepieceField::renderEyepieceView(const QImage *skyChart, QPixmap *renderChart, const double rotation,
                                       const double scale, const bool flip, const bool invert, const QImage *skyImage,
                                       QPixmap *renderImage, const bool overlay, const bool invertColors)
{
    QTransform transform;
    bool deleteRenderImage = false;
    transform.rotate(rotation);
    if (flip)
        transform.scale(-1, 1);
    if (invert)
        transform.scale(-1, -1);
    transform.scale(scale, scale);

    Q_ASSERT(skyChart && renderChart);
    if (!skyChart || !renderChart)
        return;

    *renderChart = QPixmap::fromImage(skyChart->transformed(transform, Qt::SmoothTransformation));

    if (skyImage)
    {
        Q_ASSERT(overlay || renderImage); // in debug mode, check for calls that supply skyImage but not renderImage
    }
    if (overlay && !renderImage)
    {
        renderImage       = new QPixmap(); // temporary, used for rendering skymap before overlay is done.
        deleteRenderImage = true;          // we created it, so we must delete it.
    }

    if (skyImage && renderImage)
    {
        if (skyImage->isNull())
            qWarning() << "Sky image supplied to renderEyepieceView() for rendering is a Null image!";
        QImage i;
        i = skyImage->transformed(transform, Qt::SmoothTransformation);
        if (invertColors)
            i.invertPixels();
        *renderImage = QPixmap::fromImage(i);
    }
    if (overlay && skyImage)
    {
        QColor skyColor = KStarsData::Instance()->colorScheme()->colorNamed("SkyColor");
        QBitmap mask    = QBitmap::fromImage(
                              skyChart->createMaskFromColor(skyColor.rgb()).transformed(transform, Qt::SmoothTransformation));
        renderChart->setMask(mask);
        QPainter p(renderImage);
        p.drawImage(QPointF(renderImage->width() / 2.0 - renderChart->width() / 2.0,
                            renderImage->height() / 2.0 - renderChart->height() / 2.0),
                    renderChart->toImage());
        QPixmap temp(renderImage->width(), renderImage->height());
        temp.fill(skyColor);
        QPainter p2(&temp);
        p2.drawImage(QPointF(0, 0), renderImage->toImage());
        p2.end();
        p.end();
        *renderChart = *renderImage = temp;
    }
    if (deleteRenderImage)
        delete renderImage;
}

void EyepieceField::renderEyepieceView(SkyPoint *sp, QPixmap *renderChart, double fovWidth, double fovHeight,
                                       const double rotation, const double scale, const bool flip, const bool invert,
                                       const QString &imagePath, QPixmap *renderImage, const bool overlay,
                                       const bool invertColors)
{
    QImage *skyChart, *skyImage = nullptr;
    skyChart = new QImage();
    if (QFile::exists(imagePath) && (renderImage || overlay))
        skyImage = new QImage();
    generateEyepieceView(sp, skyChart, skyImage, fovWidth, fovHeight, imagePath);
    renderEyepieceView(skyChart, renderChart, rotation, scale, flip, invert, skyImage, renderImage, overlay,
                       invertColors);
    delete skyChart;
    delete skyImage;
}

dms EyepieceField::findNorthAngle(const SkyPoint *sp, const dms *lat)
{
    Q_ASSERT(sp && lat);

    // NOTE: northAngle1 is the correction due to lunisolar precession
    // (needs testing and checking). northAngle2 is the correction due
    // to going from equatorial to horizontal coordinates.

    // FIXME: The following code is a guess at how to handle
    // precession. While it might work in many cases, it might fail in
    // some. Careful testing will be needed to ensure that all
    // conditions are met, esp. with getting the signs right when
    // using arccosine! Nutation and planetary precession corrections
    // have not been included. -- asimha
    // TODO: Look at the Meeus book and see if it has some formulas -- asimha
    const double equinoxPrecessionPerYear =
        (50.35 /
         3600.0); // Equinox precession in ecliptic longitude per year in degrees (ref: http://star-www.st-and.ac.uk/~fv/webnotes/chapt16.htm)
    dms netEquinoxPrecession(double((sp->getLastPrecessJD() - J2000) / 365.25) * equinoxPrecessionPerYear);
    double cosNorthAngle1 =
        (netEquinoxPrecession.cos() - sp->dec0().sin() * sp->dec().sin()) / (sp->dec0().cos() * sp->dec().cos());
    double northAngle1 = acos(cosNorthAngle1);
    if (sp->getLastPrecessJD() < J2000)
        northAngle1 = -northAngle1;
    if (sp->dec0().Degrees() < 0)
        northAngle1 = -northAngle1;
    // We trust that EquatorialToHorizontal has been called on sp, after all, how else can it have an alt/az representation.
    // Use spherical cosine rule (the triangle with vertices at sp, zenith and NCP) to compute the angle between direction of increasing altitude and north
    double cosNorthAngle2 = (lat->sin() - sp->alt().sin() * sp->dec().sin()) / (sp->alt().cos() * sp->dec().cos());
    double northAngle2    = acos(cosNorthAngle2); // arccosine is blind to sign of the angle
    if (sp->az().reduce().Degrees() < 180.0)      // if on the eastern hemisphere, flip sign
        northAngle2 = -northAngle2;
    double northAngle = northAngle1 + northAngle2;
    return dms(northAngle * 180 / M_PI);
}
