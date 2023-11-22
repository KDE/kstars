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
    INumberVectorProperty *frameProp = nullptr;

    switch (m_Type)
    {
        case PRIMARY_CCD:
            frameProp = m_Camera->getNumber("CCD_FRAME");
            break;

        case GUIDE_CCD:
            frameProp = m_Camera->getNumber("GUIDER_FRAME");
            break;
    }

    if (frameProp == nullptr)
        return false;

    INumber *arg = IUFindNumber(frameProp, "X");

    if (arg == nullptr)
        return false;

    if (minX)
        *minX = arg->min;
    if (maxX)
        *maxX = arg->max;

    arg = IUFindNumber(frameProp, "Y");

    if (arg == nullptr)
        return false;

    if (minY)
        *minY = arg->min;
    if (maxY)
        *maxY = arg->max;

    arg = IUFindNumber(frameProp, "WIDTH");

    if (arg == nullptr)
        return false;

    if (minW)
        *minW = arg->min;
    if (maxW)
        *maxW = arg->max;

    arg = IUFindNumber(frameProp, "HEIGHT");

    if (arg == nullptr)
        return false;

    if (minH)
        *minH = arg->min;
    if (maxH)
        *maxH = arg->max;

    return true;
}

bool CameraChip::setImageInfo(uint16_t width, uint16_t height, double pixelX, double pixelY, uint8_t bitdepth)
{
    INumberVectorProperty *ccdInfoProp = nullptr;

    switch (m_Type)
    {
        case PRIMARY_CCD:
            ccdInfoProp = m_Camera->getNumber("CCD_INFO");
            break;

        case GUIDE_CCD:
            ccdInfoProp = m_Camera->getNumber("GUIDER_INFO");
            break;
    }

    if (ccdInfoProp == nullptr)
        return false;

    ccdInfoProp->np[0].value = width;
    ccdInfoProp->np[1].value = height;
    ccdInfoProp->np[2].value = std::hypotf(pixelX, pixelY);
    ccdInfoProp->np[3].value = pixelX;
    ccdInfoProp->np[4].value = pixelY;
    ccdInfoProp->np[5].value = bitdepth;

    m_Camera->sendNewProperty(ccdInfoProp);

    return true;
}

bool CameraChip::getImageInfo(uint16_t &width, uint16_t &height, double &pixelX, double &pixelY, uint8_t &bitdepth)
{
    INumberVectorProperty *ccdInfoProp = nullptr;

    switch (m_Type)
    {
        case PRIMARY_CCD:
            ccdInfoProp = m_Camera->getNumber("CCD_INFO");
            break;

        case GUIDE_CCD:
            ccdInfoProp = m_Camera->getNumber("GUIDER_INFO");
            break;
    }

    if (ccdInfoProp == nullptr)
        return false;

    width    = ccdInfoProp->np[0].value;
    height   = ccdInfoProp->np[1].value;
    pixelX   = ccdInfoProp->np[2].value;
    pixelY   = ccdInfoProp->np[3].value;
    bitdepth = ccdInfoProp->np[5].value;

    return true;
}

bool CameraChip::getBayerInfo(uint16_t &offsetX, uint16_t &offsetY, QString &pattern)
{
    ITextVectorProperty * bayerTP = m_Camera->getText("CCD_CFA");
    if (!bayerTP)
        return false;

    offsetX = QString(bayerTP->tp[0].text).toInt();
    offsetY = QString(bayerTP->tp[1].text).toInt();
    pattern = QString(bayerTP->tp[2].text);

    return true;
}

bool CameraChip::getFrame(int *x, int *y, int *w, int *h)
{
    INumberVectorProperty *frameProp = nullptr;

    switch (m_Type)
    {
        case PRIMARY_CCD:
            frameProp = m_Camera->getNumber("CCD_FRAME");
            break;

        case GUIDE_CCD:
            frameProp = m_Camera->getNumber("GUIDER_FRAME");
            break;
    }

    if (frameProp == nullptr)
        return false;

    INumber *arg = IUFindNumber(frameProp, "X");

    if (arg == nullptr)
        return false;

    *x = arg->value;

    arg = IUFindNumber(frameProp, "Y");
    if (arg == nullptr)
        return false;

    *y = arg->value;

    arg = IUFindNumber(frameProp, "WIDTH");
    if (arg == nullptr)
        return false;

    *w = arg->value;

    arg = IUFindNumber(frameProp, "HEIGHT");
    if (arg == nullptr)
        return false;

    *h = arg->value;

    return true;
}

bool CameraChip::resetFrame()
{
    INumberVectorProperty *frameProp = nullptr;

    switch (m_Type)
    {
        case PRIMARY_CCD:
            frameProp = m_Camera->getNumber("CCD_FRAME");
            break;

        case GUIDE_CCD:
            frameProp = m_Camera->getNumber("GUIDER_FRAME");
            break;
    }

    if (frameProp == nullptr)
        return false;

    INumber *xarg = IUFindNumber(frameProp, "X");
    INumber *yarg = IUFindNumber(frameProp, "Y");
    INumber *warg = IUFindNumber(frameProp, "WIDTH");
    INumber *harg = IUFindNumber(frameProp, "HEIGHT");

    if (xarg && yarg && warg && harg)
    {
        if (!std::fabs(xarg->value - xarg->min) &&
                !std::fabs(yarg->value - yarg->min) &&
                !std::fabs(warg->value - warg->max) &&
                !std::fabs(harg->value - harg->max))
            return false;

        xarg->value = xarg->min;
        yarg->value = yarg->min;
        warg->value = warg->max;
        harg->value = harg->max;

        m_Camera->sendNewProperty(frameProp);
        return true;
    }

    return false;
}

bool CameraChip::setFrame(int x, int y, int w, int h, bool force)
{
    INumberVectorProperty *frameProp = nullptr;

    switch (m_Type)
    {
        case PRIMARY_CCD:
            frameProp = m_Camera->getNumber("CCD_FRAME");
            break;

        case GUIDE_CCD:
            frameProp = m_Camera->getNumber("GUIDER_FRAME");
            break;
    }

    if (frameProp == nullptr)
        return false;

    INumber *xarg = IUFindNumber(frameProp, "X");
    INumber *yarg = IUFindNumber(frameProp, "Y");
    INumber *warg = IUFindNumber(frameProp, "WIDTH");
    INumber *harg = IUFindNumber(frameProp, "HEIGHT");

    if (xarg && yarg && warg && harg)
    {
        if (!force &&
                !std::fabs(xarg->value - x) &&
                !std::fabs(yarg->value - y) &&
                !std::fabs(warg->value - w) &&
                !std::fabs(harg->value - h))
            return true;

        xarg->value = x;
        yarg->value = y;
        warg->value = w;
        harg->value = h;

        m_Camera->sendNewProperty(frameProp);
        return true;
    }

    return false;
}

bool CameraChip::capture(double exposure)
{
    //qCDebug(KSTARS_INDI) << "IndiCCD: capture()" << (type==PRIMARY_CCD?"CCD":"Guide");
    INumberVectorProperty *expProp = nullptr;

    switch (m_Type)
    {
        case PRIMARY_CCD:
            expProp = m_Camera->getNumber("CCD_EXPOSURE");
            break;

        case GUIDE_CCD:
            expProp = m_Camera->getNumber("GUIDER_EXPOSURE");
            break;
    }

    if (expProp == nullptr)
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
    INumber n;
    strcpy(n.name, expProp->np[0].name);
    n.value = exposure;

    std::unique_ptr<INumberVectorProperty> newExpProp(new INumberVectorProperty());
    strncpy(newExpProp->device, expProp->device, MAXINDIDEVICE);
    strncpy(newExpProp->name, expProp->name, MAXINDINAME);
    strncpy(newExpProp->label, expProp->label, MAXINDILABEL);
    newExpProp->np = &n;
    newExpProp->nnp = 1;

    m_Camera->sendNewProperty(newExpProp.get());

    return true;
}

bool CameraChip::abortExposure()
{
    if (!m_Camera) return false;
    ISwitchVectorProperty *abortProp = nullptr;

    switch (m_Type)
    {
        case PRIMARY_CCD:
            abortProp = m_Camera->getSwitch("CCD_ABORT_EXPOSURE");
            break;

        case GUIDE_CCD:
            abortProp = m_Camera->getSwitch("GUIDER_ABORT_EXPOSURE");
            break;
    }

    if (abortProp == nullptr)
        return false;

    ISwitch *abort = IUFindSwitch(abortProp, "ABORT");

    if (abort == nullptr)
        return false;

    abort->s = ISS_ON;

    m_Camera->sendNewProperty(abortProp);

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

    return isoProp->findOnSwitchIndex();
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

    isoProp->reset();
    isoProp->at(value)->setState(ISS_ON);

    m_Camera->sendNewProperty(isoProp);

    return true;
}

QStringList CameraChip::getISOList() const
{
    QStringList isoList;

    auto isoProp = m_Camera->getSwitch("CCD_ISO");

    if (!isoProp)
        return isoList;

    for (const auto &it : *isoProp)
        isoList << it.getLabel();

    return isoList;
}

bool CameraChip::isCapturing()
{
    if (!m_Camera) return false;
    INumberVectorProperty *expProp = nullptr;

    switch (m_Type)
    {
        case PRIMARY_CCD:
            expProp = m_Camera->getNumber("CCD_EXPOSURE");
            break;

        case GUIDE_CCD:
            expProp = m_Camera->getNumber("GUIDER_EXPOSURE");
            break;
    }

    if (expProp == nullptr)
        return false;

    return (expProp->s == IPS_BUSY);
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
    ISwitchVectorProperty *frameProp = nullptr;

    if (m_Type == PRIMARY_CCD)
        frameProp = m_Camera->getSwitch("CCD_FRAME_TYPE");
    else
        frameProp = m_Camera->getSwitch("GUIDER_FRAME_TYPE");
    if (frameProp == nullptr)
        return false;

    ISwitch *ccdFrame = nullptr;

    if (fType == FRAME_LIGHT)
        ccdFrame = IUFindSwitch(frameProp, "FRAME_LIGHT");
    else if (fType == FRAME_DARK)
        ccdFrame = IUFindSwitch(frameProp, "FRAME_DARK");
    else if (fType == FRAME_BIAS)
        ccdFrame = IUFindSwitch(frameProp, "FRAME_BIAS");
    else if (fType == FRAME_FLAT)
        ccdFrame = IUFindSwitch(frameProp, "FRAME_FLAT");

    if (ccdFrame == nullptr)
        return false;

    if (ccdFrame->s == ISS_ON)
        return true;

    if (fType != FRAME_LIGHT)
        captureMode = FITS_CALIBRATE;

    IUResetSwitch(frameProp);
    ccdFrame->s = ISS_ON;

    m_Camera->sendNewProperty(frameProp);

    return true;
}

CCDFrameType CameraChip::getFrameType()
{
    CCDFrameType fType               = FRAME_LIGHT;
    ISwitchVectorProperty *frameProp = nullptr;

    if (m_Type == PRIMARY_CCD)
        frameProp = m_Camera->getSwitch("CCD_FRAME_TYPE");
    else
        frameProp = m_Camera->getSwitch("GUIDER_FRAME_TYPE");

    if (frameProp == nullptr)
        return fType;

    ISwitch *ccdFrame = nullptr;

    ccdFrame = IUFindOnSwitch(frameProp);

    if (ccdFrame == nullptr)
    {
        qCWarning(KSTARS_INDI) << "ISD:CCD Cannot find active frame in CCD!";
        return fType;
    }

    if (!strcmp(ccdFrame->name, "FRAME_LIGHT"))
        fType = FRAME_LIGHT;
    else if (!strcmp(ccdFrame->name, "FRAME_DARK"))
        fType = FRAME_DARK;
    else if (!strcmp(ccdFrame->name, "FRAME_FLAT"))
        fType = FRAME_FLAT;
    else if (!strcmp(ccdFrame->name, "FRAME_BIAS"))
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
    CCDBinType binType             = SINGLE_BIN;
    INumberVectorProperty *binProp = nullptr;

    switch (m_Type)
    {
        case PRIMARY_CCD:
            binProp = m_Camera->getNumber("CCD_BINNING");
            break;

        case GUIDE_CCD:
            binProp = m_Camera->getNumber("GUIDER_BINNING");
            break;
    }

    if (binProp == nullptr)
        return binType;

    INumber *horBin = nullptr, *verBin = nullptr;

    horBin = IUFindNumber(binProp, "HOR_BIN");
    verBin = IUFindNumber(binProp, "VER_BIN");

    if (!horBin || !verBin)
        return binType;

    switch (static_cast<int>(horBin->value))
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
    INumberVectorProperty *binProp = nullptr;
    *bin_x = *bin_y = 1;

    switch (m_Type)
    {
        case PRIMARY_CCD:
            binProp = m_Camera->getNumber("CCD_BINNING");
            break;

        case GUIDE_CCD:
            binProp = m_Camera->getNumber("GUIDER_BINNING");
            break;
    }

    if (binProp == nullptr)
        return false;

    INumber *horBin = nullptr, *verBin = nullptr;

    horBin = IUFindNumber(binProp, "HOR_BIN");
    verBin = IUFindNumber(binProp, "VER_BIN");

    if (!horBin || !verBin)
        return false;

    *bin_x = horBin->value;
    *bin_y = verBin->value;

    return true;
}

bool CameraChip::getMaxBin(int *max_xbin, int *max_ybin)
{
    if (!max_xbin || !max_ybin)
        return false;

    INumberVectorProperty *binProp = nullptr;

    *max_xbin = *max_ybin = 1;

    switch (m_Type)
    {
        case PRIMARY_CCD:
            binProp = m_Camera->getNumber("CCD_BINNING");
            break;

        case GUIDE_CCD:
            binProp = m_Camera->getNumber("GUIDER_BINNING");
            break;
    }

    if (binProp == nullptr)
        return false;

    INumber *horBin = nullptr, *verBin = nullptr;

    horBin = IUFindNumber(binProp, "HOR_BIN");
    verBin = IUFindNumber(binProp, "VER_BIN");

    if (!horBin || !verBin)
        return false;

    *max_xbin = horBin->max;
    *max_ybin = verBin->max;

    return true;
}

bool CameraChip::setBinning(int bin_x, int bin_y)
{
    INumberVectorProperty *binProp = nullptr;

    switch (m_Type)
    {
        case PRIMARY_CCD:
            binProp = m_Camera->getNumber("CCD_BINNING");
            break;

        case GUIDE_CCD:
            binProp = m_Camera->getNumber("GUIDER_BINNING");
            break;
    }

    if (binProp == nullptr)
        return false;

    INumber *horBin = IUFindNumber(binProp, "HOR_BIN");
    INumber *verBin = IUFindNumber(binProp, "VER_BIN");

    if (!horBin || !verBin)
        return false;

    if (!std::fabs(horBin->value - bin_x) && !std::fabs(verBin->value - bin_y))
        return true;

    if (bin_x > horBin->max || bin_y > verBin->max)
        return false;

    horBin->value = bin_x;
    verBin->value = bin_y;

    m_Camera->sendNewProperty(binProp);

    return true;
}

}
