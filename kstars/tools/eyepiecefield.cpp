/*
    SPDX-FileCopyrightText: 2014 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "eyepiecefield.h"

#include "exporteyepieceview.h"
#include "fov.h"
#include "ksdssdownloader.h"
#include "kstars.h"
#include "ksnotification.h"
#include "Options.h"
#include "skymap.h"
#include "skyqpainter.h"

#include <QBitmap>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSvgGenerator>
#include <QSvgRenderer>
#include <QVBoxLayout>

#include <kstars_debug.h>

EyepieceField::EyepieceField(QWidget *parent) : QDialog(parent)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    setWindowTitle(i18nc("@title:window", "Eyepiece Field View"));

    m_sp         = nullptr;
    m_dt         = nullptr;
    m_currentFOV = nullptr;
    m_fovWidth = m_fovHeight = 0;
    m_dler                   = nullptr;

    QWidget *mainWidget     = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(mainWidget);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    buttonBox->addButton(i18nc("Export image", "Export"), QDialogButtonBox::AcceptRole);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(slotExport()));

    QVBoxLayout *rows = new QVBoxLayout;
    mainWidget->setLayout(rows);

    m_skyChartDisplay = new QLabel;
    m_skyChartDisplay->setBackgroundRole(QPalette::Base);
    m_skyChartDisplay->setScaledContents(false);
    m_skyChartDisplay->setMinimumWidth(400);

    m_skyImageDisplay = new QLabel;
    m_skyImageDisplay->setBackgroundRole(QPalette::Base);
    m_skyImageDisplay->setScaledContents(false);
    m_skyImageDisplay->setMinimumWidth(400);
    m_skyImageDisplay->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    QHBoxLayout *imageLayout = new QHBoxLayout;
    rows->addLayout(imageLayout);
    imageLayout->addWidget(m_skyChartDisplay);
    imageLayout->addWidget(m_skyImageDisplay);

    m_invertView   = new QCheckBox(i18n("Invert view"), this);
    m_flipView     = new QCheckBox(i18n("Flip view"), this);
    m_overlay      = new QCheckBox(i18n("Overlay"), this);
    m_invertColors = new QCheckBox(i18n("Invert colors"), this);
    m_getDSS       = new QPushButton(i18n("Fetch DSS image"), this);

    m_getDSS->setVisible(false);

    QHBoxLayout *optionsLayout = new QHBoxLayout;
    optionsLayout->addWidget(m_invertView);
    optionsLayout->addWidget(m_flipView);
    optionsLayout->addStretch();
    optionsLayout->addWidget(m_overlay);
    optionsLayout->addWidget(m_invertColors);
    optionsLayout->addWidget(m_getDSS);

    rows->addLayout(optionsLayout);

    m_rotationSlider = new QSlider(Qt::Horizontal, this);
    m_rotationSlider->setMaximum(180);
    m_rotationSlider->setMinimum(-180);
    m_rotationSlider->setTickInterval(30);
    m_rotationSlider->setPageStep(30);

    QLabel *sliderLabel = new QLabel(i18n("Rotation:"), this);

    m_presetCombo = new QComboBox(this);
    m_presetCombo->addItem(i18n("None"));
    m_presetCombo->addItem(i18n("Vanilla"));
    m_presetCombo->addItem(i18n("Flipped"));
    m_presetCombo->addItem(i18n("Refractor"));
    m_presetCombo->addItem(i18n("Dobsonian"));

    QLabel *presetLabel = new QLabel(i18n("Preset:"), this);

    QHBoxLayout *rotationLayout = new QHBoxLayout;
    rotationLayout->addWidget(sliderLabel);
    rotationLayout->addWidget(m_rotationSlider);
    rotationLayout->addWidget(presetLabel);
    rotationLayout->addWidget(m_presetCombo);

    rows->addLayout(rotationLayout);

    connect(m_invertView, SIGNAL(stateChanged(int)), this, SLOT(render()));
    connect(m_flipView, SIGNAL(stateChanged(int)), this, SLOT(render()));
    connect(m_invertColors, SIGNAL(stateChanged(int)), this, SLOT(render()));
    connect(m_overlay, SIGNAL(stateChanged(int)), this, SLOT(render()));
    connect(m_rotationSlider, SIGNAL(valueChanged(int)), this, SLOT(render()));
    connect(m_presetCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(slotEnforcePreset(int)));
    connect(m_presetCombo, SIGNAL(activated(int)), this, SLOT(slotEnforcePreset(int)));
    connect(m_getDSS, SIGNAL(clicked()), this, SLOT(slotDownloadDss()));
}

void EyepieceField::slotEnforcePreset(int index)
{
    if (index == -1)
        index = m_presetCombo->currentIndex();
    if (index == -1)
        index = 0;

    if (index == 0)
        return; // Preset "None" makes no changes

    double altAzRot = (m_usedAltAz ? 0.0 : findNorthAngle(m_sp, KStarsData::Instance()->geo()->lat()).Degrees());
    if (altAzRot > 180.0)
        altAzRot -= 360.0;
    double dobRot = altAzRot - m_sp->alt().Degrees(); // set rotation to altitude CW
    if (dobRot > 180.0)
        dobRot -= 360.0;
    if (dobRot < -180.0)
        dobRot += 360.0;
    switch (index)
    {
        case 1:
            // Preset vanilla
            m_rotationSlider->setValue(0.0); // reset rotation
            m_invertView->setChecked(false); // reset inversion
            m_flipView->setChecked(false);   // reset flip
            break;
        case 2:
            // Preset flipped
            m_rotationSlider->setValue(0.0); // reset rotation
            m_invertView->setChecked(false); // reset inversion
            m_flipView->setChecked(true);    // set flip
            break;
        case 3:
            // Preset refractor
            m_rotationSlider->setValue(altAzRot);
            m_invertView->setChecked(true);
            m_flipView->setChecked(false);
            break;
        case 4:
            // Preset Dobsonian
            m_rotationSlider->setValue(dobRot); // set rotation for dob
            m_invertView->setChecked(true);     // set inversion
            m_flipView->setChecked(false);
            break;
        default:
            break;
    }
}

void EyepieceField::showEyepieceField(SkyPoint *sp, FOV const *const fov, const QString &imagePath)
{
    double fovWidth, fovHeight;

    Q_ASSERT(sp);

    // See if we were supplied a sky image; if so, load its metadata
    // Set up the new sky map FOV and pointing. full map FOV = 4 times the given FOV.
    if (fov)
    {
        fovWidth  = fov->sizeX();
        fovHeight = fov->sizeY();
    }
    else if (QFile::exists(imagePath))
    {
        fovWidth = fovHeight = -1.0; // figure out from the image.
    }
    else
    {
        //Q_ASSERT( false );
        // Don't crash the program
        KSNotification::error(i18n(("No image found. Please specify the exact FOV.")));
        return;
    }

    showEyepieceField(sp, fovWidth, fovHeight, imagePath);
    m_currentFOV = fov;
}

void EyepieceField::showEyepieceField(SkyPoint *sp, const double fovWidth, double fovHeight, const QString &imagePath)
{
    if (m_skyChart.get() == nullptr)
        m_skyChart.reset(new QImage());

    if (QFile::exists(imagePath))
    {
        qCDebug(KSTARS) << "Image path " << imagePath << " exists";
        if (m_skyImage.get() == nullptr)
        {
            qCDebug(KSTARS) << "Sky image did not exist, creating.";
            m_skyImage.reset(new QImage());
        }
    }
    else
    {
        m_skyImage.reset();
    }

    m_usedAltAz = Options::useAltAz();
    generateEyepieceView(sp, m_skyChart.get(), m_skyImage.get(), fovWidth, fovHeight, imagePath);

    // Keep a copy for local purposes (computation of field rotation etc.)
    if (m_sp != sp)
    {
        if (m_sp)
            delete m_sp;
        m_sp = new SkyPoint(*sp);
    }

    // Update our date/time
    delete m_dt;
    m_dt = new KStarsDateTime(KStarsData::Instance()->ut());

    // Enforce preset as per selection, since we have loaded a new eyepiece view
    slotEnforcePreset(-1);
    // Render the display
    render();
    m_fovWidth   = fovWidth;
    m_fovHeight  = fovHeight;
    m_currentFOV = nullptr;
}

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

void EyepieceField::render()
{
    double rotation   = m_rotationSlider->value();
    bool flip         = m_flipView->isChecked();
    bool invert       = m_invertView->isChecked();
    bool invertColors = m_invertColors->isChecked();
    bool overlay      = m_overlay->isChecked() && m_skyImage.get();

    Q_ASSERT(m_skyChart.get());

    renderEyepieceView(m_skyChart.get(), &m_renderChart, rotation, 1.0, flip, invert, m_skyImage.get(), &m_renderImage,
                       overlay, invertColors);

    m_skyChartDisplay->setVisible(!overlay);
    if (m_skyImage.get() != nullptr)
    {
        m_skyImageDisplay->setVisible(true);
        m_overlay->setVisible(true);
        m_invertColors->setVisible(true);
        m_getDSS->setVisible(false);
    }
    else
    {
        m_skyImageDisplay->setVisible(false);
        m_overlay->setVisible(false);
        m_invertColors->setVisible(false);
        m_getDSS->setVisible(true);
    }

    if (!overlay)
        m_skyChartDisplay->setPixmap(m_renderChart.scaled(m_skyChartDisplay->width(), m_skyChartDisplay->height(),
                                     Qt::KeepAspectRatio, Qt::SmoothTransformation));
    if (m_skyImage.get() != nullptr)
        m_skyImageDisplay->setPixmap(m_renderImage.scaled(m_skyImageDisplay->width(), m_skyImageDisplay->height(),
                                     Qt::KeepAspectRatio, Qt::SmoothTransformation));

    update();
    show();
}

void EyepieceField::slotDownloadDss()
{
    double fovWidth = 0, fovHeight = 0;
    if (m_fovWidth == 0 && m_currentFOV == nullptr)
    {
        fovWidth = fovHeight = 15.0;
    }
    else if (m_currentFOV)
    {
        fovWidth  = m_currentFOV->sizeX();
        fovHeight = m_currentFOV->sizeY();
    }
    if (!m_dler)
    {
        m_dler = new KSDssDownloader(this);
        connect(m_dler, SIGNAL(downloadComplete(bool)), SLOT(slotDssDownloaded(bool)));
    }
    KSDssImage::Metadata md;
    m_tempFile.open();
    QUrl srcUrl = QUrl(KSDssDownloader::getDSSURL(m_sp, fovWidth, fovHeight, "all", &md));
    m_dler->startSingleDownload(srcUrl, m_tempFile.fileName(), md);
    m_tempFile.close();
}

void EyepieceField::slotDssDownloaded(bool success)
{
    if (!success)
    {
        KSNotification::sorry(i18n("Failed to download DSS/SDSS image."));
        return;
    }
    else
        showEyepieceField(m_sp, m_fovWidth, m_fovHeight, m_tempFile.fileName());
}

void EyepieceField::slotExport()
{
    bool overlay = m_overlay->isChecked() && m_skyImage.get();
    new ExportEyepieceView(m_sp, *m_dt, ((m_skyImage.get() && !overlay) ? &m_renderImage : nullptr),
                           &m_renderChart, this);
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
    dms netEquinoxPrecession(((sp->getLastPrecessJD() - J2000) / 365.25) * equinoxPrecessionPerYear);
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
    qCDebug(KSTARS) << "Data: alt = " << sp->alt().toDMSString() << "; az = " << sp->az().toDMSString() << "; ra, dec ("
                    << sp->getLastPrecessJD() / 365.25 << ") = " << sp->ra().toHMSString() << "," << sp->dec().toDMSString()
                    << "; ra0,dec0 (J2000.0) = " << sp->ra0().toHMSString() << "," << sp->dec0().toDMSString();
    qCDebug(KSTARS) << "PA corrections: precession cosine = " << cosNorthAngle1 << "; angle = " << northAngle1
                    << "; horizontal = " << cosNorthAngle2 << "; angle = " << northAngle2;
    return dms(northAngle * 180 / M_PI);
}
