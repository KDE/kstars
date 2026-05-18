/*
    SPDX-FileCopyrightText: 2013-2021 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2018-2020 Robert Lancaster <rlancaste@gmail.com>
    SPDX-FileCopyrightText: 2019-2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "align.h"
#include "align_p.h"
#include "fov.h"
#include "opsastrometry.h"
#include "polaralignmentassistant.h"
#include "Options.h"

#include "fitsviewer/fitsdata.h"
#include "kstarsdata.h"
#include "ksuserdb.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/profilesettings.h"
#include "ksnotification.h"

#include <ekos_align_debug.h>

namespace Ekos
{

void Align::getFOVScale(double &fov_w, double &fov_h, double &fov_scale)
{
    fov_w     = m_FOVWidth;
    fov_h     = m_FOVHeight;
    fov_scale = m_FOVPixelScale;
}

QList<double> Align::fov()
{
    QList<double> result;

    result << m_FOVWidth << m_FOVHeight << m_FOVPixelScale;

    return result;
}

QList<double> Align::cameraInfo()
{
    QList<double> result;

    result << m_CameraWidth << m_CameraHeight << m_CameraPixelWidth << m_CameraPixelHeight;

    return result;
}

QList<double> Align::telescopeInfo()
{
    QList<double> result;

    result << m_FocalLength << m_Aperture << m_Reducer;

    return result;
}

void Align::getCalculatedFOVScale(double &fov_w, double &fov_h, double &fov_scale)
{
    // FOV in arcsecs
    // DSLR
    auto reducedFocalLength = m_Reducer * m_FocalLength;
    if (m_FocalRatio > 0)
    {
        // The formula is in radians, must convert to degrees.
        // Then to arcsecs
        fov_w = 3600 * 2 * atan(m_CameraWidth * (m_CameraPixelWidth / 1000.0) / (2 * reducedFocalLength)) / dms::DegToRad;
        fov_h = 3600 * 2 * atan(m_CameraHeight * (m_CameraPixelHeight / 1000.0) / (2 * reducedFocalLength)) / dms::DegToRad;
    }
    // Telescope
    else
    {
        fov_w = 206264.8062470963552 * m_CameraWidth * m_CameraPixelWidth / 1000.0 / reducedFocalLength;
        fov_h = 206264.8062470963552 * m_CameraHeight * m_CameraPixelHeight / 1000.0 / reducedFocalLength;
    }

    // Pix Scale
    fov_scale = (fov_w * (alignBinning->currentIndex() + 1)) / m_CameraWidth;

    // FOV in arcmins
    fov_w /= 60.0;
    fov_h /= 60.0;
}

void Align::calculateEffectiveFocalLength(double newFOVW)
{
    if (newFOVW < 0 || isEqual(newFOVW, m_FOVWidth))
        return;

    auto reducedFocalLength = m_Reducer * m_FocalLength;
    double new_focal_length = 0;

    if (m_FocalRatio > 0)
        new_focal_length = ((m_CameraWidth * m_CameraPixelWidth / 1000.0) / tan(newFOVW * dms::DegToRad / 120.0)) / 2;
    else
        new_focal_length = ((m_CameraWidth * m_CameraPixelWidth / 1000.0) * 206264.8062470963552) / (newFOVW * 60.0);
    double focal_diff = std::abs(new_focal_length - reducedFocalLength);

    if (focal_diff > 1)
    {
        m_EffectiveFocalLength = new_focal_length / m_Reducer;
        appendLogText(i18n("Effective telescope focal length is updated to %1 mm (FL: %2 Reducer: %3 W: %4 Pitch: %5 FOVW: %6).",
                           m_EffectiveFocalLength,
                           m_FocalLength,
                           m_Reducer,
                           m_CameraWidth,
                           m_CameraPixelWidth,
                           newFOVW));
    }
}

void Align::calculateFOV()
{
    auto reducedFocalLength = m_Reducer * m_FocalLength;
    auto reducecdEffectiveFocalLength = m_Reducer * m_EffectiveFocalLength;
    auto reducedFocalRatio = m_Reducer * m_FocalRatio;

    if (m_FocalRatio > 0)
    {
        // The formula is in radians, must convert to degrees.
        // Then to arcsecs
        m_FOVWidth = 3600 * 2 * atan(m_CameraWidth * (m_CameraPixelWidth / 1000.0) / (2 * reducedFocalLength)) / dms::DegToRad;
        m_FOVHeight = 3600 * 2 * atan(m_CameraHeight * (m_CameraPixelHeight / 1000.0) / (2 * reducedFocalLength)) / dms::DegToRad;
    }
    // Telescope
    else
    {
        m_FOVWidth = 206264.8062470963552 * m_CameraWidth * m_CameraPixelWidth / 1000.0 / reducedFocalLength;
        m_FOVHeight = 206264.8062470963552 * m_CameraHeight * m_CameraPixelHeight / 1000.0 / reducedFocalLength;
    }

    // Calculate FOV

    // Pix Scale
    m_FOVPixelScale = (m_FOVWidth * (alignBinning->currentIndex() + 1)) / m_CameraWidth;

    // FOV in arcmins
    m_FOVWidth /= 60.0;
    m_FOVHeight /= 60.0;

    double calculated_fov_x = m_FOVWidth;
    double calculated_fov_y = m_FOVHeight;

    QString calculatedFOV = (QString("%1' x %2'").arg(m_FOVWidth, 0, 'f', 1).arg(m_FOVHeight, 0, 'f', 1));

    // Put FOV upper limit as 180 degrees
    if (m_FOVWidth < 1 || m_FOVWidth > 60 * 180 || m_FOVHeight < 1 || m_FOVHeight > 60 * 180)
    {
        appendLogText(
            i18n("Warning! The calculated field of view (%1) is out of bounds. Ensure the telescope focal length and camera pixel size are correct.",
                 calculatedFOV));
        return;
    }

    FocalLengthOut->setText(QString("%1 (%2)").arg(reducedFocalLength, 0, 'f', 1).
                            arg(m_EffectiveFocalLength > 0 ? reducecdEffectiveFocalLength : reducedFocalLength, 0, 'f', 1));
    // DSLR
    if (m_FocalRatio > 0)
        FocalRatioOut->setText(QString("%1 (%2)").arg(reducedFocalRatio, 0, 'f', 1).
                               arg(m_EffectiveFocalLength > 0 ? reducecdEffectiveFocalLength / m_Aperture : reducedFocalRatio, 0,
                                   'f', 1));
    // Telescope
    else if (m_Aperture > 0)
        FocalRatioOut->setText(QString("%1 (%2)").arg(reducedFocalLength / m_Aperture, 0, 'f', 1).
                               arg(m_EffectiveFocalLength > 0 ? reducecdEffectiveFocalLength / m_Aperture : reducedFocalLength / m_Aperture, 0,
                                   'f', 1));
    ReducerOut->setText(QString("%1x").arg(m_Reducer, 0, 'f', 2));

    if (m_EffectiveFocalLength > 0)
    {
        double focal_diff = std::abs(m_EffectiveFocalLength  - m_FocalLength);
        if (focal_diff < 5)
            FocalLengthOut->setStyleSheet("color:green");
        else if (focal_diff < 15)
            FocalLengthOut->setStyleSheet("color:yellow");
        else
            FocalLengthOut->setStyleSheet("color:red");
    }

    // JM 2018-04-20 Above calculations are for RAW FOV. Starting from 2.9.5, we are using EFFECTIVE FOV
    // Which is the real FOV as measured from the plate solution. The effective FOVs are stored in the database and are unique
    // per profile/pixel_size/focal_length combinations. It defaults to 0' x 0' and gets updated after the first successful solver is complete.
    getEffectiveFOV();

    if (m_FOVWidth == 0)
    {
        //FOVOut->setReadOnly(false);
        FOVOut->setToolTip(
            i18n("<p>Effective field of view size in arcminutes.</p><p>Please capture and solve once to measure the effective FOV or enter the values manually.</p><p>Calculated FOV: %1</p>",
                 calculatedFOV));
        m_FOVWidth = calculated_fov_x;
        m_FOVHeight = calculated_fov_y;
        m_EffectiveFOVPending = true;
    }
    else
    {
        m_EffectiveFOVPending = false;
        FOVOut->setToolTip(i18n("<p>Effective field of view size in arcminutes.</p>"));
    }

    solverFOV->setSize(m_FOVWidth, m_FOVHeight);
    sensorFOV->setSize(m_FOVWidth, m_FOVHeight);
    if (m_Camera)
        sensorFOV->setName(m_Camera->getDeviceName());

    FOVOut->setText(QString("%1' x %2'").arg(m_FOVWidth, 0, 'f', 1).arg(m_FOVHeight, 0, 'f', 1));

    // Enable or Disable PAA depending on current FOV
    const bool fovOK = ((m_FOVWidth + m_FOVHeight) / 2.0) > PAH_CUTOFF_FOV;
    if (m_PolarAlignmentAssistant != nullptr)
        m_PolarAlignmentAssistant->setEnabled(fovOK);

    if (Options::astrometryUseImageScale())
    {
        auto unitType = Options::astrometryImageScaleUnits();

        // Degrees
        if (unitType == 0)
        {
            double fov_low  = qMin(m_FOVWidth / 60, m_FOVHeight / 60);
            double fov_high = qMax(m_FOVWidth / 60, m_FOVHeight / 60);
            opsAstrometry->kcfg_AstrometryImageScaleLow->setValue(fov_low);
            opsAstrometry->kcfg_AstrometryImageScaleHigh->setValue(fov_high);

            Options::setAstrometryImageScaleLow(fov_low);
            Options::setAstrometryImageScaleHigh(fov_high);
        }
        // Arcmins
        else if (unitType == 1)
        {
            double fov_low  = qMin(m_FOVWidth, m_FOVHeight);
            double fov_high = qMax(m_FOVWidth, m_FOVHeight);
            opsAstrometry->kcfg_AstrometryImageScaleLow->setValue(fov_low);
            opsAstrometry->kcfg_AstrometryImageScaleHigh->setValue(fov_high);

            Options::setAstrometryImageScaleLow(fov_low);
            Options::setAstrometryImageScaleHigh(fov_high);
        }
        // Arcsec per pixel
        else
        {
            opsAstrometry->kcfg_AstrometryImageScaleLow->setValue(m_FOVPixelScale * 0.9);
            opsAstrometry->kcfg_AstrometryImageScaleHigh->setValue(m_FOVPixelScale * 1.1);

            // 10% boundary
            Options::setAstrometryImageScaleLow(m_FOVPixelScale * 0.9);
            Options::setAstrometryImageScaleHigh(m_FOVPixelScale * 1.1);
        }
    }
}

QStringList Align::generateRemoteOptions(const QVariantMap &optionsMap)
{
    QStringList solver_args;

    // -O overwrite
    // -3 Expected RA
    // -4 Expected DEC
    // -5 Radius (deg)
    // -L lower scale of image in arcminutes
    // -H upper scale of image in arcminutes
    // -u aw set scale to be in arcminutes
    // -W solution.wcs name of solution file
    // apog1.jpg name of target file to analyze
    //solve-field -O -3 06:40:51 -4 +09:49:53 -5 1 -L 40 -H 100 -u aw -W solution.wcs apod1.jpg

    // Start with always-used arguments
    solver_args << "-O"
                << "--no-plots";

    // Now go over boolean options

    // noverify
    if (optionsMap.contains("noverify"))
        solver_args << "--no-verify";

    // noresort
    if (optionsMap.contains("resort"))
        solver_args << "--resort";

    // fits2fits
    if (optionsMap.contains("nofits2fits"))
        solver_args << "--no-fits2fits";

    // downsample
    if (optionsMap.contains("downsample"))
        solver_args << "--downsample" << QString::number(optionsMap.value("downsample", 2).toInt());

    // image scale low
    if (optionsMap.contains("scaleL"))
        solver_args << "-L" << QString::number(optionsMap.value("scaleL").toDouble());

    // image scale high
    if (optionsMap.contains("scaleH"))
        solver_args << "-H" << QString::number(optionsMap.value("scaleH").toDouble());

    // image scale units
    if (optionsMap.contains("scaleUnits"))
        solver_args << "-u" << optionsMap.value("scaleUnits").toString();

    // RA
    if (optionsMap.contains("ra"))
        solver_args << "-3" << QString::number(optionsMap.value("ra").toDouble());

    // DE
    if (optionsMap.contains("de"))
        solver_args << "-4" << QString::number(optionsMap.value("de").toDouble());

    // Radius
    if (optionsMap.contains("radius"))
        solver_args << "-5" << QString::number(optionsMap.value("radius").toDouble());

    // Custom
    if (optionsMap.contains("custom"))
        solver_args << optionsMap.value("custom").toString();

    return solver_args;
}

//This will generate the high and low scale of the imager field size based on the stated units.
void Align::generateFOVBounds(double fov_w, QString &fov_low, QString &fov_high, double tolerance)
{
    // This sets the percentage we search outside the lower and upper boundary limits
    // by default, we stretch the limits by 5% (tolerance = 0.05)
    double lower_boundary = 1.0 - tolerance;
    double upper_boundary = 1.0 + tolerance;

    // JM 2019-10-20: The bounds consider image width only, not height.
    double fov_lower = fov_w * lower_boundary;
    double fov_upper = fov_w * upper_boundary;

    //No need to do anything if they are aw, since that is the default
    fov_low  = QString::number(fov_lower);
    fov_high = QString::number(fov_upper);
}

QStringList Align::generateRemoteArgs(const QSharedPointer<FITSData> &data)
{
    QVariantMap optionsMap;

    if (Options::astrometryUseNoVerify())
        optionsMap["noverify"] = true;

    if (Options::astrometryUseResort())
        optionsMap["resort"] = true;

    if (Options::astrometryUseNoFITS2FITS())
        optionsMap["nofits2fits"] = true;

    if (data == nullptr)
    {
        if (Options::astrometryUseDownsample())
        {
            if (Options::astrometryAutoDownsample() && m_CameraWidth && m_CameraHeight)
            {
                uint16_t w = m_CameraWidth / (alignBinning->currentIndex() + 1);
                optionsMap["downsample"] = getSolverDownsample(w);
            }
            else
                optionsMap["downsample"] = Options::astrometryDownsample();
        }

        //Options needed for Sextractor
        optionsMap["image_width"] = m_CameraWidth / (alignBinning->currentIndex() + 1);
        optionsMap["image_height"] = m_CameraHeight / (alignBinning->currentIndex() + 1);

        if (Options::astrometryUseImageScale() && m_FOVWidth > 0 && m_FOVHeight > 0)
        {
            QString units = "dw";
            if (Options::astrometryImageScaleUnits() == 1)
                units = "aw";
            else if (Options::astrometryImageScaleUnits() == 2)
                units = "app";
            if (Options::astrometryAutoUpdateImageScale())
            {
                QString fov_low, fov_high;
                double fov_w = m_FOVWidth;

                if (Options::astrometryImageScaleUnits() == SSolver::DEG_WIDTH)
                {
                    fov_w /= 60;
                }
                else if (Options::astrometryImageScaleUnits() == SSolver::ARCSEC_PER_PIX)
                {
                    fov_w = m_FOVPixelScale;
                }

                // If effective FOV is pending, let's set a wider tolerance range
                generateFOVBounds(fov_w, fov_low, fov_high, m_EffectiveFOVPending ? 0.3 : 0.05);

                optionsMap["scaleL"]     = fov_low;
                optionsMap["scaleH"]     = fov_high;
                optionsMap["scaleUnits"] = units;
            }
            else
            {
                optionsMap["scaleL"]     = Options::astrometryImageScaleLow();
                optionsMap["scaleH"]     = Options::astrometryImageScaleHigh();
                optionsMap["scaleUnits"] = units;
            }
        }

        if (Options::astrometryUsePosition() && m_Mount != nullptr)
        {
            double ra = 0, dec = 0;
            m_Mount->getEqCoords(&ra, &dec);

            optionsMap["ra"]     = ra * 15.0;
            optionsMap["de"]     = dec;
            optionsMap["radius"] = Options::astrometryRadius();
        }
    }
    else
    {
        // Downsample
        QVariant width;
        data->getRecordValue("NAXIS1", width);
        if (width.isValid())
        {
            optionsMap["downsample"] = getSolverDownsample(width.toInt());
        }
        else
            optionsMap["downsample"] = Options::astrometryDownsample();

        // Pixel Scale
        QVariant pixscale;
        data->getRecordValue("SCALE", pixscale);
        if (pixscale.isValid())
        {
            optionsMap["scaleL"]     = 0.8 * pixscale.toDouble();
            optionsMap["scaleH"]     = 1.2 * pixscale.toDouble();
            optionsMap["scaleUnits"] = "app";
        }

        // Position
        QVariant ra, de;
        data->getRecordValue("RA", ra);
        data->getRecordValue("DEC", de);
        if (ra.isValid() && de.isValid())
        {
            optionsMap["ra"]     = ra.toDouble();
            optionsMap["de"]     = de.toDouble();
            optionsMap["radius"] = Options::astrometryRadius();
        }
    }

    if (Options::astrometryCustomOptions().isEmpty() == false)
        optionsMap["custom"] = Options::astrometryCustomOptions();

    return generateRemoteOptions(optionsMap);
}

QVariantMap Align::getEffectiveFOV()
{
    KStarsData::Instance()->userdb()->GetAllEffectiveFOVs(effectiveFOVs);

    m_FOVWidth = m_FOVHeight = 0;

    for (auto &map : effectiveFOVs)
    {
        if (map["Profile"].toString() == m_ActiveProfile->name && map["Train"].toString() == opticalTrain())
        {
            if (isEqual(map["Width"].toInt(), m_CameraWidth) &&
                    isEqual(map["Height"].toInt(), m_CameraHeight) &&
                    isEqual(map["PixelW"].toDouble(), m_CameraPixelWidth) &&
                    isEqual(map["PixelH"].toDouble(), m_CameraPixelHeight) &&
                    isEqual(map["FocalLength"].toDouble(), m_FocalLength) &&
                    isEqual(map["FocalRedcuer"].toDouble(), m_Reducer) &&
                    isEqual(map["FocalRatio"].toDouble(), m_FocalRatio))
            {
                m_FOVWidth = map["FovW"].toDouble();
                m_FOVHeight = map["FovH"].toDouble();
                return map;
            }
        }
    }

    return QVariantMap();
}

void Align::saveNewEffectiveFOV(double newFOVW, double newFOVH)
{
    if (newFOVW < 0 || newFOVH < 0 || (isEqual(newFOVW, m_FOVWidth) && isEqual(newFOVH, m_FOVHeight)))
        return;

    QVariantMap effectiveMap = getEffectiveFOV();

    // If ID exists, delete it first.
    if (effectiveMap.isEmpty() == false)
        KStarsData::Instance()->userdb()->DeleteEffectiveFOV(effectiveMap["id"].toString());

    // If FOV is 0x0, then we just remove existing effective FOV
    if (newFOVW == 0.0 && newFOVH == 0.0)
    {
        calculateFOV();
        return;
    }

    effectiveMap["Profile"] = m_ActiveProfile->name;
    effectiveMap["Train"] = opticalTrainCombo->currentText();
    effectiveMap["Width"] = m_CameraWidth;
    effectiveMap["Height"] = m_CameraHeight;
    effectiveMap["PixelW"] = m_CameraPixelWidth;
    effectiveMap["PixelH"] = m_CameraPixelHeight;
    effectiveMap["FocalLength"] = m_FocalLength;
    effectiveMap["FocalReducer"] = m_Reducer;
    effectiveMap["FocalRatio"] = m_FocalRatio;
    effectiveMap["FovW"] = newFOVW;
    effectiveMap["FovH"] = newFOVH;

    KStarsData::Instance()->userdb()->AddEffectiveFOV(effectiveMap);

    calculateFOV();
}

void Align::syncFOV()
{
    QString newFOV = FOVOut->text();
    QRegularExpression re("(\\d+\\.*\\d*)\\D*x\\D*(\\d+\\.*\\d*)");
    QRegularExpressionMatch match = re.match(newFOV);
    if (match.hasMatch())
    {
        double newFOVW = match.captured(1).toDouble();
        double newFOVH = match.captured(2).toDouble();

        //if (newFOVW > 0 && newFOVH > 0)
        saveNewEffectiveFOV(newFOVW, newFOVH);

        FOVOut->setStyleSheet(QString());
    }
    else
    {
        KSNotification::error(i18n("Invalid FOV."));
        FOVOut->setStyleSheet("background-color:red");
    }
}

}
