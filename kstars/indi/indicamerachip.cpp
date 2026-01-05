/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "indicamerachip.h"
#include "indicamera.h"

#include "Options.h"

#include "indi_debug.h"

namespace ISD
{

CameraChip::CameraChip(ISD::Camera *camera, ChipType type): m_Camera(camera), m_Type(type) {}

FITSView *CameraChip::getImageView(FITSMode imageType)
{
    switch (imageType)
    {
        case FITS_NORMAL:
            return normalImage;

        case FITS_FOCUS:
            return focusImage;

        case FITS_GUIDE:
            return guideImage;

        case FITS_CALIBRATE:
            return calibrationImage;

        case FITS_ALIGN:
            return alignImage;

        default:
            break;
    }

    return nullptr;
}

void CameraChip::setImageView(FITSView *image, FITSMode imageType)
{
    switch (imageType)
    {
        case FITS_NORMAL:
            normalImage = image;
            break;

        case FITS_FOCUS:
            focusImage = image;
            break;

        case FITS_GUIDE:
            guideImage = image;
            break;

        case FITS_CALIBRATE:
            calibrationImage = image;
            break;

        case FITS_ALIGN:
            alignImage = image;
            break;

        default:
            break;
    }

    if (image)
        imageData = image->imageData();
}

bool CameraChip::getFrameMinMax(int *minX, int *maxX, int *minY, int *maxY, int *minW, int *maxW, int *minH, int *maxH)
{
    INDI::PropertyNumber nvp = (m_Type == PRIMARY_CCD) ? m_Camera->getNumber("CCD_FRAME") : m_Camera->getNumber("GUIDER_FRAME");

    if (!nvp)
        return false;

    auto x = nvp.findWidgetByName("X");

    if (x == nullptr)
        return false;

    if (minX)
        *minX = x->getMin();
    if (maxX)
        *maxX = x->getMax();

    auto y = nvp.findWidgetByName("Y");

    if (y == nullptr)
        return false;

    if (minY)
        *minY = y->getMin();
    if (maxY)
        *maxY = y->getMax();

    auto width = nvp.findWidgetByName("WIDTH");

    if (width == nullptr)
        return false;

    if (minW)
        *minW = width->getMin();
    if (maxW)
        *maxW = width->getMax();

    auto height = nvp.findWidgetByName("HEIGHT");

    if (height == nullptr)
        return false;

    if (minH)
        *minH = height->getMin();
    if (maxH)
        *maxH = height->getMax();

    return true;
}

bool CameraChip::setImageInfo(uint16_t width, uint16_t height, double pixelX, double pixelY, uint8_t bitdepth)
{
    INDI::PropertyNumber nvp = (m_Type == PRIMARY_CCD) ? m_Camera->getNumber("CCD_INFO") : m_Camera->getNumber("GUIDER_INFO");

    if (!nvp)
        return false;

    nvp[0].setValue(width);
    nvp[1].setValue(height);
    nvp[2].setValue(std::hypotf(pixelX, pixelY));
    nvp[3].setValue(pixelX);
    nvp[4].setValue(pixelY);
    nvp[5].setValue(bitdepth);

    m_Camera->sendNewProperty(nvp);

    return true;
}

bool CameraChip::getImageInfo(uint16_t &width, uint16_t &height, double &pixelX, double &pixelY, uint8_t &bitdepth)
{
    INDI::PropertyNumber nvp = (m_Type == PRIMARY_CCD) ? m_Camera->getNumber("CCD_INFO") : m_Camera->getNumber("GUIDER_INFO");

    if (!nvp)
        return false;

    width    = nvp[0].getValue();
    height   = nvp[1].getValue();
    pixelX   = nvp[2].getValue();
    pixelY   = nvp[3].getValue();
    bitdepth = nvp[5].getValue();

    return true;
}

bool CameraChip::getBayerInfo(uint16_t &offsetX, uint16_t &offsetY, QString &pattern)
{
    INDI::PropertyText tvp = m_Camera->getText("CCD_CFA");
    if (!tvp)
        return false;

    offsetX = QString(tvp[0].getText()).toInt();
    offsetY = QString(tvp[1].getText()).toInt();
    pattern = QString(tvp[2].getText());

    return true;
}

bool CameraChip::getFrame(int *x, int *y, int *w, int *h)
{
    INDI::PropertyNumber nvp = (m_Type == PRIMARY_CCD) ? m_Camera->getNumber("CCD_FRAME") : m_Camera->getNumber("GUIDER_FRAME");

    if (!nvp)
        return false;

    auto X = nvp.findWidgetByName("X");

    if (X == nullptr)
        return false;
    *x = X->getValue();

    auto Y = nvp.findWidgetByName("Y");

    if (Y == nullptr)
        return false;
    *y = Y->getValue();

    auto width = nvp.findWidgetByName("WIDTH");

    if (width == nullptr)
        return false;
    *w = width->getValue();

    auto height = nvp.findWidgetByName("HEIGHT");

    if (height == nullptr)
        return false;

    *h = height->getValue();

    return true;
}

bool CameraChip::resetFrame()
{
    INDI::PropertyNumber nvp = (m_Type == PRIMARY_CCD) ? m_Camera->getNumber("CCD_FRAME") : m_Camera->getNumber("GUIDER_FRAME");

    if (!nvp)
        return false;

    auto xarg = nvp.findWidgetByName("X");
    auto yarg = nvp.findWidgetByName("Y");
    auto warg = nvp.findWidgetByName("WIDTH");
    auto harg = nvp.findWidgetByName("HEIGHT");

    if (xarg && yarg && warg && harg)
    {
        if (!std::fabs(xarg->getValue() - xarg->getMin()) &&
                !std::fabs(yarg->getValue() - yarg->getMin()) &&
                !std::fabs(warg->getValue() - warg->getMax()) &&
                !std::fabs(harg->getValue() - harg->getMax()))
            return false;

        xarg->value = xarg->min;
        yarg->value = yarg->min;
        warg->value = warg->max;
        harg->value = harg->max;

        m_Camera->sendNewProperty(nvp);
        return true;
    }

    return false;
}

bool CameraChip::setFrame(int x, int y, int w, int h, bool force)
{
    INDI::PropertyNumber nvp = (m_Type == PRIMARY_CCD) ? m_Camera->getNumber("CCD_FRAME") : m_Camera->getNumber("GUIDER_FRAME");

    if (!nvp)
        return false;

    auto xarg = nvp.findWidgetByName("X");
    auto yarg = nvp.findWidgetByName("Y");
    auto warg = nvp.findWidgetByName("WIDTH");
    auto harg = nvp.findWidgetByName("HEIGHT");

    if (xarg && yarg && warg && harg)
    {
        if (!force &&
                !std::fabs(xarg->getValue() - x) &&
                !std::fabs(yarg->getValue() - y) &&
                !std::fabs(warg->getValue() - w) &&
                !std::fabs(harg->getValue() - h))
            return true;

        xarg->setValue(x);
        yarg->setValue(y);
        warg->setValue(w);
        harg->setValue(h);

        m_Camera->sendNewProperty(nvp);
        return true;
    }

    return false;
}

bool CameraChip::capture(double exposure)
{
    INDI::PropertyNumber nvp = (m_Type == PRIMARY_CCD) ? m_Camera->getNumber("CCD_EXPOSURE") :
                               m_Camera->getNumber("GUIDER_EXPOSURE");

    if (!nvp)
        return false;

    // If we have exposure presets, let's limit the exposure value
    // to the preset values if it falls within their range of max/min
    if (Options::forceDSLRPresets())
    {
        QMap<QString, double> exposurePresets = m_Camera->getExposurePresets();
        if (!exposurePresets.isEmpty())
        {
            double min, max;
            QPair<double, double> minmax = m_Camera->getExposurePresetsMinMax();
            min = minmax.first;
            max = minmax.second;
            if (exposure > min && exposure < max)
            {
                double diff = 1e6;
                double closestMatch = exposure;
                for (const auto &oneValue : exposurePresets.values())
                {
                    double newDiff = std::fabs(exposure - oneValue);
                    if (newDiff < diff)
                    {
                        closestMatch = oneValue;
                        diff = newDiff;
                    }
                }

                qCDebug(KSTARS_INDI) << "Requested exposure" << exposure << "closes match is" << closestMatch;
                exposure = closestMatch;
            }
        }
    }

    // clone the INumberVectorProperty, to avoid modifications to the same
    // property from two threads
    INDI::PropertyNumber newExposure(nvp);
    nvp[0].setValue(exposure);
    m_Camera->sendNewProperty(newExposure);
    return true;
}

bool CameraChip::abortExposure()
{
    if (!m_Camera) return false;

    INDI::PropertySwitch svp = (m_Type == PRIMARY_CCD) ? m_Camera->getSwitch("CCD_ABORT_EXPOSURE") :
                               m_Camera->getSwitch("GUIDER_ABORT_EXPOSURE");

    if (!svp)
        return false;


    auto sp = svp.findWidgetByName("ABORT");

    if (sp == nullptr)
        return false;

    sp->setState(ISS_ON);

    m_Camera->sendNewProperty(svp);

    return true;
}
bool CameraChip::canBin() const
{
    return CanBin && m_Camera->getEncodingFormat() != QLatin1String("Native");
}

void CameraChip::setCanBin(bool value)
{
    CanBin = value;
}

bool CameraChip::canSubframe() const
{
    return CanSubframe && m_Camera->getEncodingFormat() != QLatin1String("Native");
}

void CameraChip::setCanSubframe(bool value)
{
    CanSubframe = value;
}
bool CameraChip::canAbort() const
{
    return CanAbort;
}

void CameraChip::setCanAbort(bool value)
{
    CanAbort = value;
}

const QSharedPointer<FITSData> &CameraChip::getImageData() const
{
    return imageData;
}

int CameraChip::getISOIndex() const
{
    auto isoProp = m_Camera->getSwitch("CCD_ISO");

    if (!isoProp)
        return -1;

    return isoProp.findOnSwitchIndex();
}

bool CameraChip::getISOValue(QString &value) const
{
    auto index = getISOIndex();
    auto list = getISOList();
    if (!list.isEmpty() && index >= 0 && index < list.count())
    {
        value = list[index];
        return true;
    }

    return false;
}

bool CameraChip::setISOIndex(int value)
{
    auto isoProp = m_Camera->getSwitch("CCD_ISO");

    if (!isoProp)
        return false;

    isoProp.reset();
    isoProp[value].setState(ISS_ON);

    m_Camera->sendNewProperty(isoProp);

    return true;
}

QStringList CameraChip::getISOList() const
{
    QStringList isoList;

    auto isoProp = m_Camera->getSwitch("CCD_ISO");

    if (!isoProp)
        return isoList;

    for (auto it : isoProp)
        isoList << it.getLabel();

    return isoList;
}

bool CameraChip::isCapturing()
{
    if (!m_Camera) return false;

    INDI::PropertyNumber nvp = (m_Type == PRIMARY_CCD) ? m_Camera->getNumber("CCD_EXPOSURE") :
                               m_Camera->getNumber("GUIDER_EXPOSURE");

    if (!nvp)
        return false;

    return (nvp.getState() == IPS_BUSY);
}

bool CameraChip::setFrameType(const QString &name)
{
    CCDFrameType fType = FRAME_LIGHT;

    if (name == "FRAME_LIGHT" || name == "Light")
        fType = FRAME_LIGHT;
    else if (name == "FRAME_DARK" || name == "Dark")
        fType = FRAME_DARK;
    else if (name == "FRAME_BIAS" || name == "Bias")
        fType = FRAME_BIAS;
    else if (name == "FRAME_FLAT" || name == "Flat")
        fType = FRAME_FLAT;
    else
    {
        qCWarning(KSTARS_INDI) << name << " frame type is unknown." ;
        return false;
    }

    return setFrameType(fType);
}

bool CameraChip::setFrameType(CCDFrameType fType)
{
    INDI::PropertySwitch svp = (m_Type == PRIMARY_CCD) ? m_Camera->getSwitch("CCD_FRAME_TYPE") :
                               m_Camera->getSwitch("GUIDER_FRAME_TYPE");
    if (!svp)
        return false;

    INDI::WidgetViewSwitch *ccdFrame = nullptr;

    if (fType == FRAME_LIGHT)
        ccdFrame = svp.findWidgetByName("FRAME_LIGHT");
    else if (fType == FRAME_DARK)
        ccdFrame = svp.findWidgetByName("FRAME_DARK");
    else if (fType == FRAME_BIAS)
        ccdFrame = svp.findWidgetByName("FRAME_BIAS");
    else if (fType == FRAME_FLAT)
        ccdFrame = svp.findWidgetByName("FRAME_FLAT");

    if (ccdFrame == nullptr)
        return false;

    if (ccdFrame->s == ISS_ON)
        return true;

    if (fType != FRAME_LIGHT)
        captureMode = FITS_CALIBRATE;

    svp.reset();
    ccdFrame->s = ISS_ON;

    m_Camera->sendNewProperty(svp);

    return true;
}

CCDFrameType CameraChip::getFrameType()
{
    CCDFrameType fType = FRAME_LIGHT;
    INDI::PropertySwitch svp = (m_Type == PRIMARY_CCD) ? m_Camera->getSwitch("CCD_FRAME_TYPE") :
                               m_Camera->getSwitch("GUIDER_FRAME_TYPE");

    if (!svp)
        return fType;

    INDI::WidgetViewSwitch *ccdFrame = nullptr;

    ccdFrame = svp.findOnSwitch();

    if (ccdFrame == nullptr)
    {
        qCWarning(KSTARS_INDI) << "ISD:CCD Cannot find active frame in CCD!";
        return fType;
    }

    if (ccdFrame->isNameMatch("FRAME_LIGHT"))
        fType = FRAME_LIGHT;
    else if (ccdFrame->isNameMatch("FRAME_DARK"))
        fType = FRAME_DARK;
    else if (ccdFrame->isNameMatch("FRAME_FLAT"))
        fType = FRAME_FLAT;
    else if (ccdFrame->isNameMatch("FRAME_BIAS"))
        fType = FRAME_BIAS;

    return fType;
}

bool CameraChip::setBinning(CCDBinType binType)
{
    switch (binType)
    {
        case SINGLE_BIN:
            return setBinning(1, 1);
        case DOUBLE_BIN:
            return setBinning(2, 2);
        case TRIPLE_BIN:
            return setBinning(3, 3);
        case QUADRAPLE_BIN:
            return setBinning(4, 4);
    }

    return false;
}

CCDBinType CameraChip::getBinning()
{
    CCDBinType binType = SINGLE_BIN;
    INDI::PropertyNumber nvp = (m_Type == PRIMARY_CCD) ? m_Camera->getNumber("CCD_BINNING") :
                               m_Camera->getNumber("GUIDER_BINNING");

    if (!nvp)
        return binType;


    auto horBin = nvp.findWidgetByName("HOR_BIN");
    auto verBin = nvp.findWidgetByName("VER_BIN");

    if (!horBin || !verBin)
        return binType;

    switch (static_cast<int>(horBin->getValue()))
    {
        case 2:
            binType = DOUBLE_BIN;
            break;

        case 3:
            binType = TRIPLE_BIN;
            break;

        case 4:
            binType = QUADRAPLE_BIN;
            break;

        default:
            break;
    }

    return binType;
}

bool CameraChip::getBinning(int *bin_x, int *bin_y)
{
    *bin_x = *bin_y = 1;
    INDI::PropertyNumber nvp = (m_Type == PRIMARY_CCD) ? m_Camera->getNumber("CCD_BINNING") :
                               m_Camera->getNumber("GUIDER_BINNING");

    if (!nvp)
        return false;

    auto horBin = nvp.findWidgetByName("HOR_BIN");
    auto verBin = nvp.findWidgetByName("VER_BIN");

    if (!horBin || !verBin)
        return false;

    *bin_x = horBin->getValue();
    *bin_y = verBin->getValue();

    return true;
}

bool CameraChip::getMaxBin(int *max_xbin, int *max_ybin)
{
    if (!max_xbin || !max_ybin)
        return false;

    *max_xbin = *max_ybin = 1;
    INDI::PropertyNumber nvp = (m_Type == PRIMARY_CCD) ? m_Camera->getNumber("CCD_BINNING") :
                               m_Camera->getNumber("GUIDER_BINNING");

    if (!nvp)
        return false;

    auto horBin = nvp.findWidgetByName("HOR_BIN");
    auto verBin = nvp.findWidgetByName("VER_BIN");

    if (!horBin || !verBin)
        return false;

    *max_xbin = horBin->getMax();
    *max_ybin = verBin->getMax();

    return true;
}

bool CameraChip::setBinning(int bin_x, int bin_y)
{
    INDI::PropertyNumber nvp = (m_Type == PRIMARY_CCD) ? m_Camera->getNumber("CCD_BINNING") :
                               m_Camera->getNumber("GUIDER_BINNING");

    if (!nvp)
        return false;

    auto horBin = nvp.findWidgetByName("HOR_BIN");
    auto verBin = nvp.findWidgetByName("VER_BIN");

    if (!horBin || !verBin)
        return false;

    if (!std::fabs(horBin->getValue() - bin_x) && !std::fabs(verBin->getValue() - bin_y))
        return true;

    if (bin_x > horBin->getMax() || bin_y > verBin->getMax())
        return false;

    horBin->setValue(bin_x);
    verBin->setValue(bin_y);

    m_Camera->sendNewProperty(nvp);

    return true;
}

}
