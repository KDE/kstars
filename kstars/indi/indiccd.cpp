/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "indiccd.h"

#include "config-kstars.h"

#include "indi_debug.h"

#include "clientmanager.h"
#include "driverinfo.h"
#include "guimanager.h"
#include "kspaths.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "streamwg.h"
//#include "ekos/manager.h"
#ifdef HAVE_CFITSIO
#include "fitsviewer/fitsdata.h"
#endif

#include <KNotifications/KNotification>
#include "ksnotification.h"
#include <QImageReader>
#include <QStatusBar>
#include <QtConcurrent>

#include <basedevice.h>

const QStringList RAWFormats = { "cr2", "cr3", "crw", "nef", "raf", "dng", "arw", "orf" };

const QString &getFITSModeStringString(FITSMode mode)
{
    return FITSModes[mode];
}

namespace
{
// Internal function to write an image blob to disk.
bool WriteImageFileInternal(const QString &filename, char *buffer, const size_t size)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly))
    {
        qCCritical(KSTARS_INDI) << "ISD:CCD Error: Unable to open write file: " <<
                                filename;
        return false;
    }
    size_t n = 0;
    QDataStream out(&file);
    for (size_t nr = 0; nr < size; nr += n)
        n = out.writeRawData(buffer + nr, size - nr);
    file.flush();
    file.close();
    file.setPermissions(QFileDevice::ReadUser |
                        QFileDevice::WriteUser |
                        QFileDevice::ReadGroup |
                        QFileDevice::ReadOther);
    return true;
}
}

namespace ISD
{
CCDChip::CCDChip(ISD::CCD *ccd, ChipType cType)
{
    baseDevice    = ccd->getBaseDevice();
    clientManager = ccd->getDriverInfo()->getClientManager();
    parentCCD     = ccd;
    type          = cType;
}

FITSView *CCDChip::getImageView(FITSMode imageType)
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
    }

    return nullptr;
}

void CCDChip::setImageView(FITSView *image, FITSMode imageType)
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
    }

    if (image)
        imageData = image->imageData();
}

bool CCDChip::getFrameMinMax(int *minX, int *maxX, int *minY, int *maxY, int *minW, int *maxW, int *minH, int *maxH)
{
    INumberVectorProperty *frameProp = nullptr;

    switch (type)
    {
        case PRIMARY_CCD:
            frameProp = baseDevice->getNumber("CCD_FRAME");
            break;

        case GUIDE_CCD:
            frameProp = baseDevice->getNumber("GUIDER_FRAME");
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

bool CCDChip::setImageInfo(uint16_t width, uint16_t height, double pixelX, double pixelY, uint8_t bitdepth)
{
    INumberVectorProperty *ccdInfoProp = nullptr;

    switch (type)
    {
        case PRIMARY_CCD:
            ccdInfoProp = baseDevice->getNumber("CCD_INFO");
            break;

        case GUIDE_CCD:
            ccdInfoProp = baseDevice->getNumber("GUIDER_INFO");
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

    clientManager->sendNewNumber(ccdInfoProp);

    return true;
}

bool CCDChip::getImageInfo(uint16_t &width, uint16_t &height, double &pixelX, double &pixelY, uint8_t &bitdepth)
{
    INumberVectorProperty *ccdInfoProp = nullptr;

    switch (type)
    {
        case PRIMARY_CCD:
            ccdInfoProp = baseDevice->getNumber("CCD_INFO");
            break;

        case GUIDE_CCD:
            ccdInfoProp = baseDevice->getNumber("GUIDER_INFO");
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

bool CCDChip::getBayerInfo(uint16_t &offsetX, uint16_t &offsetY, QString &pattern)
{
    ITextVectorProperty * bayerTP = baseDevice->getText("CCD_CFA");
    if (!bayerTP)
        return false;

    offsetX = QString(bayerTP->tp[0].text).toInt();
    offsetY = QString(bayerTP->tp[1].text).toInt();
    pattern = QString(bayerTP->tp[2].text);

    return true;
}

bool CCDChip::getFrame(int *x, int *y, int *w, int *h)
{
    INumberVectorProperty *frameProp = nullptr;

    switch (type)
    {
        case PRIMARY_CCD:
            frameProp = baseDevice->getNumber("CCD_FRAME");
            break;

        case GUIDE_CCD:
            frameProp = baseDevice->getNumber("GUIDER_FRAME");
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

bool CCDChip::resetFrame()
{
    INumberVectorProperty *frameProp = nullptr;

    switch (type)
    {
        case PRIMARY_CCD:
            frameProp = baseDevice->getNumber("CCD_FRAME");
            break;

        case GUIDE_CCD:
            frameProp = baseDevice->getNumber("GUIDER_FRAME");
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

        clientManager->sendNewNumber(frameProp);
        return true;
    }

    return false;
}

bool CCDChip::setFrame(int x, int y, int w, int h, bool force)
{
    INumberVectorProperty *frameProp = nullptr;

    switch (type)
    {
        case PRIMARY_CCD:
            frameProp = baseDevice->getNumber("CCD_FRAME");
            break;

        case GUIDE_CCD:
            frameProp = baseDevice->getNumber("GUIDER_FRAME");
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

        clientManager->sendNewNumber(frameProp);
        return true;
    }

    return false;
}

bool CCDChip::capture(double exposure)
{
    //qCDebug(KSTARS_INDI) << "IndiCCD: capture()" << (type==PRIMARY_CCD?"CCD":"Guide");
    INumberVectorProperty *expProp = nullptr;

    switch (type)
    {
        case PRIMARY_CCD:
            expProp = baseDevice->getNumber("CCD_EXPOSURE");
            break;

        case GUIDE_CCD:
            expProp = baseDevice->getNumber("GUIDER_EXPOSURE");
            break;
    }

    if (expProp == nullptr)
        return false;

    // If we have exposure presets, let's limit the exposure value
    // to the preset values if it falls within their range of max/min
    if (Options::forceDSLRPresets())
    {
        QMap<QString, double> exposurePresets = parentCCD->getExposurePresets();
        if (!exposurePresets.isEmpty())
        {
            double min, max;
            QPair<double, double> minmax = parentCCD->getExposurePresetsMinMax();
            min = minmax.first;
            max = minmax.second;
            if (exposure > min && exposure < max)
            {
                double diff = 1e6;
                double closestMatch = exposure;
                for (auto oneValue : exposurePresets.values())
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

    clientManager->sendNewNumber(newExpProp.get());

    return true;
}

bool CCDChip::abortExposure()
{
    ISwitchVectorProperty *abortProp = nullptr;

    switch (type)
    {
        case PRIMARY_CCD:
            abortProp = baseDevice->getSwitch("CCD_ABORT_EXPOSURE");
            break;

        case GUIDE_CCD:
            abortProp = baseDevice->getSwitch("GUIDER_ABORT_EXPOSURE");
            break;
    }

    if (abortProp == nullptr)
        return false;

    ISwitch *abort = IUFindSwitch(abortProp, "ABORT");

    if (abort == nullptr)
        return false;

    abort->s = ISS_ON;

    clientManager->sendNewSwitch(abortProp);

    return true;
}
bool CCDChip::canBin() const
{
    return CanBin;
}

void CCDChip::setCanBin(bool value)
{
    CanBin = value;
}
bool CCDChip::canSubframe() const
{
    return CanSubframe;
}

void CCDChip::setCanSubframe(bool value)
{
    CanSubframe = value;
}
bool CCDChip::canAbort() const
{
    return CanAbort;
}

void CCDChip::setCanAbort(bool value)
{
    CanAbort = value;
}

const QSharedPointer<FITSData> &CCDChip::getImageData() const
{
    return imageData;
}

int CCDChip::getISOIndex() const
{
    auto isoProp = baseDevice->getSwitch("CCD_ISO");

    if (!isoProp)
        return -1;

    return isoProp->findOnSwitchIndex();
}

bool CCDChip::setISOIndex(int value)
{
    auto isoProp = baseDevice->getSwitch("CCD_ISO");

    if (!isoProp)
        return false;

    isoProp->reset();
    isoProp->at(value)->setState(ISS_ON);

    clientManager->sendNewSwitch(isoProp);

    return true;
}

QStringList CCDChip::getISOList() const
{
    QStringList isoList;

    auto isoProp = baseDevice->getSwitch("CCD_ISO");

    if (!isoProp)
        return isoList;

    for (const auto &it : *isoProp)
        isoList << it.getLabel();

    return isoList;
}

bool CCDChip::isCapturing()
{
    INumberVectorProperty *expProp = nullptr;

    switch (type)
    {
        case PRIMARY_CCD:
            expProp = baseDevice->getNumber("CCD_EXPOSURE");
            break;

        case GUIDE_CCD:
            expProp = baseDevice->getNumber("GUIDER_EXPOSURE");
            break;
    }

    if (expProp == nullptr)
        return false;

    return (expProp->s == IPS_BUSY);
}

bool CCDChip::setFrameType(const QString &name)
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

bool CCDChip::setFrameType(CCDFrameType fType)
{
    ISwitchVectorProperty *frameProp = nullptr;

    if (type == PRIMARY_CCD)
        frameProp = baseDevice->getSwitch("CCD_FRAME_TYPE");
    else
        frameProp = baseDevice->getSwitch("GUIDER_FRAME_TYPE");
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

    clientManager->sendNewSwitch(frameProp);

    return true;
}

CCDFrameType CCDChip::getFrameType()
{
    CCDFrameType fType               = FRAME_LIGHT;
    ISwitchVectorProperty *frameProp = nullptr;

    if (type == PRIMARY_CCD)
        frameProp = baseDevice->getSwitch("CCD_FRAME_TYPE");
    else
        frameProp = baseDevice->getSwitch("GUIDER_FRAME_TYPE");

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

bool CCDChip::setBinning(CCDBinType binType)
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

CCDBinType CCDChip::getBinning()
{
    CCDBinType binType             = SINGLE_BIN;
    INumberVectorProperty *binProp = nullptr;

    switch (type)
    {
        case PRIMARY_CCD:
            binProp = baseDevice->getNumber("CCD_BINNING");
            break;

        case GUIDE_CCD:
            binProp = baseDevice->getNumber("GUIDER_BINNING");
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

bool CCDChip::getBinning(int *bin_x, int *bin_y)
{
    INumberVectorProperty *binProp = nullptr;
    *bin_x = *bin_y = 1;

    switch (type)
    {
        case PRIMARY_CCD:
            binProp = baseDevice->getNumber("CCD_BINNING");
            break;

        case GUIDE_CCD:
            binProp = baseDevice->getNumber("GUIDER_BINNING");
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

bool CCDChip::getMaxBin(int *max_xbin, int *max_ybin)
{
    if (!max_xbin || !max_ybin)
        return false;

    INumberVectorProperty *binProp = nullptr;

    *max_xbin = *max_ybin = 1;

    switch (type)
    {
        case PRIMARY_CCD:
            binProp = baseDevice->getNumber("CCD_BINNING");
            break;

        case GUIDE_CCD:
            binProp = baseDevice->getNumber("GUIDER_BINNING");
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

bool CCDChip::setBinning(int bin_x, int bin_y)
{
    INumberVectorProperty *binProp = nullptr;

    switch (type)
    {
        case PRIMARY_CCD:
            binProp = baseDevice->getNumber("CCD_BINNING");
            break;

        case GUIDE_CCD:
            binProp = baseDevice->getNumber("GUIDER_BINNING");
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

    clientManager->sendNewNumber(binProp);

    return true;
}

CCD::CCD(GDInterface *iPtr) : DeviceDecorator(iPtr)
{
    dType = KSTARS_CCD;
    primaryChip.reset(new CCDChip(this, CCDChip::PRIMARY_CCD));

    readyTimer.reset(new QTimer());
    readyTimer.get()->setInterval(250);
    readyTimer.get()->setSingleShot(true);
    connect(readyTimer.get(), &QTimer::timeout, this, &CCD::ready);

    m_Media.reset(new WSMedia(this));
    connect(m_Media.get(), &WSMedia::newFile, this, &CCD::setWSBLOB);

    connect(clientManager, &ClientManager::newBLOBManager, this, &CCD::setBLOBManager, Qt::UniqueConnection);
    m_LastNotificationTS = QDateTime::currentDateTime();
}

CCD::~CCD()
{
    if (m_ImageViewerWindow)
        m_ImageViewerWindow->close();
    if (fileWriteThread.isRunning())
        fileWriteThread.waitForFinished();
    if (fileWriteBuffer != nullptr)
        delete [] fileWriteBuffer;
}

void CCD::setBLOBManager(const char *device, INDI::Property prop)
{
    if (!prop->getRegistered())
        return;

    if (device == getDeviceName())
        emit newBLOBManager(prop);
}

void CCD::registerProperty(INDI::Property prop)
{
    if (isConnected())
        readyTimer.get()->start();

    if (prop->isNameMatch("GUIDER_EXPOSURE"))
    {
        HasGuideHead = true;
        guideChip.reset(new CCDChip(this, CCDChip::GUIDE_CCD));
    }
    else if (prop->isNameMatch("CCD_FRAME_TYPE"))
    {
        auto ccdFrame = prop->getSwitch();

        primaryChip->clearFrameTypes();

        for (const auto &it : *ccdFrame)
            primaryChip->addFrameLabel(it.getLabel());
    }
    else if (prop->isNameMatch("CCD_FRAME"))
    {
        auto np = prop->getNumber();
        if (np && np->getPermission() != IP_RO)
            primaryChip->setCanSubframe(true);
    }
    else if (prop->isNameMatch("GUIDER_FRAME"))
    {
        auto np = prop->getNumber();
        if (np && np->getPermission() != IP_RO)
            guideChip->setCanSubframe(true);
    }
    else if (prop->isNameMatch("CCD_BINNING"))
    {
        auto np = prop->getNumber();
        if (np && np->getPermission() != IP_RO)
            primaryChip->setCanBin(true);
    }
    else if (prop->isNameMatch("GUIDER_BINNING"))
    {
        auto np = prop->getNumber();
        if (np && np->getPermission() != IP_RO)
            guideChip->setCanBin(true);
    }
    else if (prop->isNameMatch("CCD_ABORT_EXPOSURE"))
    {
        auto sp = prop->getSwitch();
        if (sp && sp->getPermission() != IP_RO)
            primaryChip->setCanAbort(true);
    }
    else if (prop->isNameMatch("GUIDER_ABORT_EXPOSURE"))
    {
        auto sp = prop->getSwitch();
        if (sp && sp->getPermission() != IP_RO)
            guideChip->setCanAbort(true);
    }
    else if (prop->isNameMatch("CCD_TEMPERATURE"))
    {
        auto np = prop->getNumber();
        HasCooler = true;
        CanCool   = (np->getPermission() != IP_RO);
        if (np)
            emit newTemperatureValue(np->at(0)->getValue());
    }
    else if (prop->isNameMatch("CCD_COOLER"))
    {
        // Can turn cooling on/off
        HasCoolerControl = true;
    }
    else if (prop->isNameMatch("CCD_VIDEO_STREAM"))
    {
        // Has Video Stream
        HasVideoStream = true;
    }
    else if (prop->isNameMatch("CCD_TRANSFER_FORMAT"))
    {
        auto sp = prop->getSwitch();
        if (sp)
        {
            auto format = sp->findWidgetByName("FORMAT_NATIVE");
            if (format && format->getState() == ISS_ON)
                transferFormat = FORMAT_NATIVE;
            else
                transferFormat = FORMAT_FITS;
        }
    }
    else if (prop->isNameMatch("CCD_EXPOSURE_PRESETS"))
    {
        auto svp = prop->getSwitch();
        if (svp)
        {
            bool ok = false;
            for (const auto &it : *svp)
            {
                QString key = QString(it.getLabel());
                double value = key.toDouble(&ok);
                if (!ok)
                {
                    QStringList parts = key.split("/");
                    if (parts.count() == 2)
                    {
                        bool numOk = false, denOk = false;
                        double numerator = parts[0].toDouble(&numOk);
                        double denominator = parts[1].toDouble(&denOk);
                        if (numOk && denOk && denominator > 0)
                        {
                            ok = true;
                            value = numerator / denominator;
                        }
                    }
                }
                if (ok)
                    m_ExposurePresets.insert(key, value);

                double min = 1e6, max = 1e-6;
                for (auto oneValue : m_ExposurePresets.values())
                {
                    if (oneValue < min)
                        min = oneValue;
                    if (oneValue > max)
                        max = oneValue;
                }
                m_ExposurePresetsMinMax = qMakePair<double, double>(min, max);
            }
        }
    }
    else if (prop->isNameMatch("CCD_EXPOSURE_LOOP"))
    {
        auto sp = prop->getSwitch();
        if (sp)
        {
            auto looping = sp->findWidgetByName("LOOP_ON");
            if (looping && looping->getState() == ISS_ON)
                IsLooping = true;
            else
                IsLooping = false;
        }
    }
    else if (prop->isNameMatch("TELESCOPE_TYPE"))
    {
        auto sp = prop->getSwitch();
        if (sp)
        {
            auto format = sp->findWidgetByName("TELESCOPE_PRIMARY");
            if (format && format->getState() == ISS_ON)
                telescopeType = TELESCOPE_PRIMARY;
            else
                telescopeType = TELESCOPE_GUIDE;
        }
    }
    else if (prop->isNameMatch("CCD_WEBSOCKET_SETTINGS"))
    {
        auto np = prop->getNumber();
        m_Media->setURL(QUrl(QString("ws://%1:%2").arg(clientManager->getHost()).arg(np->at(0)->getValue())));
        m_Media->connectServer();
    }
    else if (prop->isNameMatch("CCD1"))
    {
        IBLOBVectorProperty *bp = prop->getBLOB();
        primaryCCDBLOB = bp->bp;
        primaryCCDBLOB->bvp = bp;
    }
    // try to find gain and/or offset property, if any
    else if ( (gainN == nullptr || offsetN == nullptr) && prop->getType() == INDI_NUMBER)
    {
        // Since gain is spread among multiple property depending on the camera providing it
        // we need to search in all possible number properties
        auto controlNP = prop->getNumber();
        if (controlNP)
        {
            for (auto &it : *controlNP)
            {
                QString name  = QString(it.getName()).toLower();
                QString label = QString(it.getLabel()).toLower();

                if (name == "gain" || label == "gain")
                {
                    gainN = &it;
                    gainPerm = controlNP->getPermission();
                }
                else if (name == "offset" || label == "offset")
                {
                    offsetN = &it;
                    offsetPerm = controlNP->getPermission();
                }
            }
        }
    }

    DeviceDecorator::registerProperty(prop);
}

void CCD::removeProperty(const QString &name)
{
    if (name == "CCD_WEBSOCKET_SETTINGS")
    {
        m_Media->disconnectServer();
    }

    DeviceDecorator::removeProperty(name);
}

void CCD::processLight(ILightVectorProperty *lvp)
{
    DeviceDecorator::processLight(lvp);
}

void CCD::processNumber(INumberVectorProperty *nvp)
{
    if (!strcmp(nvp->name, "CCD_EXPOSURE"))
    {
        INumber *np = IUFindNumber(nvp, "CCD_EXPOSURE_VALUE");
        if (np)
            emit newExposureValue(primaryChip.get(), np->value, nvp->s);
        if (nvp->s == IPS_ALERT)
            emit captureFailed();
    }
    else if (!strcmp(nvp->name, "CCD_TEMPERATURE"))
    {
        HasCooler   = true;
        INumber *np = IUFindNumber(nvp, "CCD_TEMPERATURE_VALUE");
        if (np)
            emit newTemperatureValue(np->value);
    }
    else if (!strcmp(nvp->name, "GUIDER_EXPOSURE"))
    {
        INumber *np = IUFindNumber(nvp, "GUIDER_EXPOSURE_VALUE");
        if (np)
            emit newExposureValue(guideChip.get(), np->value, nvp->s);
    }
    else if (!strcmp(nvp->name, "FPS"))
    {
        emit newFPS(nvp->np[0].value, nvp->np[1].value);
    }
    else if (!strcmp(nvp->name, "CCD_RAPID_GUIDE_DATA"))
    {
        double dx = -1, dy = -1, fit = -1;
        INumber *np = nullptr;

        if (nvp->s == IPS_ALERT)
        {
            emit newGuideStarData(primaryChip.get(), -1, -1, -1);
        }
        else
        {
            np = IUFindNumber(nvp, "GUIDESTAR_X");
            if (np)
                dx = np->value;
            np = IUFindNumber(nvp, "GUIDESTAR_Y");
            if (np)
                dy = np->value;
            np = IUFindNumber(nvp, "GUIDESTAR_FIT");
            if (np)
                fit = np->value;

            if (dx >= 0 && dy >= 0 && fit >= 0)
                emit newGuideStarData(primaryChip.get(), dx, dy, fit);
        }
    }
    else if (!strcmp(nvp->name, "GUIDER_RAPID_GUIDE_DATA"))
    {
        double dx = -1, dy = -1, fit = -1;
        INumber *np = nullptr;

        if (nvp->s == IPS_ALERT)
        {
            emit newGuideStarData(guideChip.get(), -1, -1, -1);
        }
        else
        {
            np = IUFindNumber(nvp, "GUIDESTAR_X");
            if (np)
                dx = np->value;
            np = IUFindNumber(nvp, "GUIDESTAR_Y");
            if (np)
                dy = np->value;
            np = IUFindNumber(nvp, "GUIDESTAR_FIT");
            if (np)
                fit = np->value;

            if (dx >= 0 && dy >= 0 && fit >= 0)
                emit newGuideStarData(guideChip.get(), dx, dy, fit);
        }
    }

    DeviceDecorator::processNumber(nvp);
}

void CCD::processSwitch(ISwitchVectorProperty *svp)
{
    if (!strcmp(svp->name, "CCD_COOLER"))
    {
        // Can turn cooling on/off
        HasCoolerControl = true;
        emit coolerToggled(svp->sp[0].s == ISS_ON);
    }
    else if (QString(svp->name).endsWith("VIDEO_STREAM"))
    {
        // If BLOB is not enabled for this camera, then ignore all VIDEO_STREAM calls.
        if (isBLOBEnabled() == false)
            return;

        HasVideoStream = true;

        if (!streamWindow && svp->sp[0].s == ISS_ON)
        {
            streamWindow.reset(new StreamWG(this));

            INumberVectorProperty *streamFrame = baseDevice->getNumber("CCD_STREAM_FRAME");
            INumber *w = nullptr, *h = nullptr;

            if (streamFrame)
            {
                w = IUFindNumber(streamFrame, "WIDTH");
                h = IUFindNumber(streamFrame, "HEIGHT");
            }

            if (w && h)
            {
                streamW = w->value;
                streamH = h->value;
            }
            else
            {
                // Only use CCD dimensions if we are receiving raw stream and not stream of images (i.e. mjpeg..etc)
                auto rawBP = baseDevice->getBLOB("CCD1");
                if (rawBP)
                {
                    int x = 0, y = 0, w = 0, h = 0;
                    int binx = 0, biny = 0;

                    primaryChip->getFrame(&x, &y, &w, &h);
                    primaryChip->getBinning(&binx, &biny);
                    streamW = w / binx;
                    streamH = h / biny;
                }
            }

            streamWindow->setSize(streamW, streamH);
        }

        if (streamWindow)
        {
            connect(streamWindow.get(), &StreamWG::hidden, this, &CCD::StreamWindowHidden, Qt::UniqueConnection);
            connect(streamWindow.get(), &StreamWG::imageChanged, this, &CCD::newVideoFrame, Qt::UniqueConnection);

            streamWindow->enableStream(svp->sp[0].s == ISS_ON);
            emit videoStreamToggled(svp->sp[0].s == ISS_ON);
        }
    }
    else if (!strcmp(svp->name, "CCD_TRANSFER_FORMAT"))
    {
        ISwitch *format = IUFindSwitch(svp, "FORMAT_NATIVE");

        if (format && format->s == ISS_ON)
            transferFormat = FORMAT_NATIVE;
        else
            transferFormat = FORMAT_FITS;
    }
    else if (!strcmp(svp->name, "RECORD_STREAM"))
    {
        ISwitch *recordOFF = IUFindSwitch(svp, "RECORD_OFF");

        if (recordOFF && recordOFF->s == ISS_ON)
        {
            emit videoRecordToggled(false);
            KSNotification::event(QLatin1String("IndiServerMessage"), i18n("Video Recording Stopped"), KSNotification::EVENT_INFO);
        }
        else
        {
            emit videoRecordToggled(true);
            KSNotification::event(QLatin1String("IndiServerMessage"), i18n("Video Recording Started"), KSNotification::EVENT_INFO);
        }
    }
    else if (!strcmp(svp->name, "TELESCOPE_TYPE"))
    {
        ISwitch *format = IUFindSwitch(svp, "TELESCOPE_PRIMARY");
        if (format && format->s == ISS_ON)
            telescopeType = TELESCOPE_PRIMARY;
        else
            telescopeType = TELESCOPE_GUIDE;
    }
    else if (!strcmp(svp->name, "CCD_EXPOSURE_LOOP"))
    {
        ISwitch *looping = IUFindSwitch(svp, "LOOP_ON");
        if (looping && looping->s == ISS_ON)
            IsLooping = true;
        else
            IsLooping = false;
    }
    else if (streamWindow && !strcmp(svp->name, "CONNECTION"))
    {
        ISwitch *dSwitch = IUFindSwitch(svp, "DISCONNECT");

        if (dSwitch && dSwitch->s == ISS_ON)
        {
            streamWindow->enableStream(false);
            emit videoStreamToggled(false);
            streamWindow->close();
            streamWindow.reset();
        }

        //emit switchUpdated(svp);
        //return;
    }

    DeviceDecorator::processSwitch(svp);
}

void CCD::processText(ITextVectorProperty *tvp)
{
    if (!strcmp(tvp->name, "CCD_FILE_PATH"))
    {
        IText *filepath = IUFindText(tvp, "FILE_PATH");
        if (filepath)
            emit newRemoteFile(QString(filepath->text));
    }

    DeviceDecorator::processText(tvp);
}

void CCD::setWSBLOB(const QByteArray &message, const QString &extension)
{
    if (!primaryCCDBLOB)
        return;

    primaryCCDBLOB->blob = const_cast<char *>(message.data());
    primaryCCDBLOB->size = message.size();
    strncpy(primaryCCDBLOB->format, extension.toLatin1().constData(), MAXINDIFORMAT - 1);
    processBLOB(primaryCCDBLOB);

    // Disassociate
    primaryCCDBLOB->blob = nullptr;
}

void CCD::processStream(IBLOB *bp)
{
    if (!streamWindow || streamWindow->isStreamEnabled() == false)
        return;

    INumberVectorProperty *streamFrame = baseDevice->getNumber("CCD_STREAM_FRAME");
    INumber *w = nullptr, *h = nullptr;

    if (streamFrame)
    {
        w = IUFindNumber(streamFrame, "WIDTH");
        h = IUFindNumber(streamFrame, "HEIGHT");
    }

    if (w && h)
    {
        streamW = w->value;
        streamH = h->value;
    }
    else
    {
        int x, y, w, h;
        int binx, biny;

        primaryChip->getFrame(&x, &y, &w, &h);
        primaryChip->getBinning(&binx, &biny);
        streamW = w / binx;
        streamH = h / biny;
    }

    streamWindow->setSize(streamW, streamH);

    streamWindow->show();
    streamWindow->newFrame(bp);
}

bool CCD::generateFilename(const QString &format, bool batch_mode, QString *filename)
{
    QDir currentDir;
    if (batch_mode)
        currentDir.setPath(fitsDir.isEmpty() ? Options::fitsDir() : fitsDir);
    else
        currentDir.setPath(KSPaths::writableLocation(QStandardPaths::TempLocation) + "/" + qAppName());

    currentDir.mkpath(".");

    // IS8601 contains colons but they are illegal under Windows OS, so replacing them with '-'
    // The timestamp is no longer ISO8601 but it should solve interoperality issues
    // between different OS hosts
    QString ts = QDateTime::currentDateTime().toString("yyyy-MM-ddThh-mm-ss");

    if (seqPrefix.contains("_ISO8601"))
    {
        QString finalPrefix = seqPrefix;
        finalPrefix.replace("ISO8601", ts);
        *filename = currentDir.filePath(finalPrefix +
                                        QString("_%1%2").arg(QString::asprintf("%03d", nextSequenceID), format));
    }
    else
        *filename = currentDir.filePath(seqPrefix + (seqPrefix.isEmpty() ? "" : "_") +
                                        QString("%1%2").arg(QString::asprintf("%03d", nextSequenceID), format));

    QFile test_file(*filename);
    if (!test_file.open(QIODevice::WriteOnly))
    {
        qCCritical(KSTARS_INDI) << "ISD:CCD Error: Unable to open " << test_file.fileName();
        return false;
    }
    test_file.flush();
    test_file.close();
    return true;
}

bool CCD::writeImageFile(const QString &filename, IBLOB *bp, bool is_fits)
{
    // TODO: Not yet threading the writes for non-fits files.
    // Would need to deal with the raw conversion, etc.
    if (is_fits)
    {
        // Check if the last write is still ongoing, and if so wait.
        // It is using the fileWriteBuffer.
        if (fileWriteThread.isRunning())
        {
            fileWriteThread.waitForFinished();
        }

        // Wait until the file is written before overwritting the filename.
        fileWriteFilename = filename;

        // Will write blob data in a separate thread, and can't depend on the blob
        // memory, so copy it first.

        // Check buffer size.
        if (fileWriteBufferSize != bp->size)
        {
            if (fileWriteBuffer != nullptr)
                delete [] fileWriteBuffer;
            fileWriteBufferSize = bp->size;
            fileWriteBuffer = new char[fileWriteBufferSize];
        }

        // Copy memory, and write file on a separate thread.
        // Probably too late to return an error if the file couldn't write.
        memcpy(fileWriteBuffer, bp->blob, bp->size);
        fileWriteThread = QtConcurrent::run(WriteImageFileInternal, fileWriteFilename, fileWriteBuffer, bp->size);
        //filter = "";
    }
    else
    {
        if (!WriteImageFileInternal(filename, static_cast<char*>(bp->blob), bp->size))
            return false;
    }
    return true;
}

// Get or Create FITSViewer if we are using FITSViewer
// or if capture mode is calibrate since for now we are forced to open the file in the viewer
// this should be fixed in the future and should only use FITSData
QPointer<FITSViewer> CCD::getFITSViewer()
{
    // if the FITS viewer exists, return it
    if (m_FITSViewerWindow != nullptr && ! m_FITSViewerWindow.isNull())
        return m_FITSViewerWindow;

    // otherwise, create it
    normalTabID = calibrationTabID = focusTabID = guideTabID = alignTabID = -1;

    m_FITSViewerWindow = KStars::Instance()->createFITSViewer();

    // Check if ONE tab of the viewer was closed.
    connect(m_FITSViewerWindow, &FITSViewer::closed, this, [this](int tabIndex)
    {
        if (tabIndex == normalTabID)
            normalTabID = -1;
        else if (tabIndex == calibrationTabID)
            calibrationTabID = -1;
        else if (tabIndex == focusTabID)
            focusTabID = -1;
        else if (tabIndex == guideTabID)
            guideTabID = -1;
        else if (tabIndex == alignTabID)
            alignTabID = -1;
    });

    // If FITS viewer was completed closed. Reset everything
    connect(m_FITSViewerWindow, &FITSViewer::destroyed, this, [this]()
    {
        normalTabID = -1;
        calibrationTabID = -1;
        focusTabID = -1;
        guideTabID = -1;
        alignTabID = -1;
        m_FITSViewerWindow.clear();
    });

    return m_FITSViewerWindow;
}

void CCD::processBLOB(IBLOB *bp)
{
    // Ignore write-only BLOBs since we only receive it for state-change
    if (bp->bvp->p == IP_WO || bp->size == 0)
        return;

    BType = BLOB_OTHER;

    QString format = QString(bp->format).toLower();

    // If stream, process it first
    if (format.contains("stream") && streamWindow)
    {
        processStream(bp);
        return;
    }

    // Format without leading . (.jpg --> jpg)
    QString shortFormat = format.mid(1);

    // If it's not FITS or an image, don't process it.
    if ((QImageReader::supportedImageFormats().contains(shortFormat.toLatin1())))
        BType = BLOB_IMAGE;
    else if (format.contains("fits"))
        BType = BLOB_FITS;
    else if (RAWFormats.contains(shortFormat))
        BType = BLOB_RAW;

    if (BType == BLOB_OTHER)
    {
        DeviceDecorator::processBLOB(bp);
        return;
    }

    CCDChip *targetChip = nullptr;

    if (!strcmp(bp->name, "CCD2"))
        targetChip = guideChip.get();
    else
    {
        targetChip = primaryChip.get();
        qCDebug(KSTARS_INDI) << "Image received. Mode:" << getFITSModeStringString(targetChip->getCaptureMode()) << "Size:" <<
                             bp->size;
    }

    // Create temporary name if ANY of the following conditions are met:
    // 1. file is preview or batch mode is not enabled
    // 2. file type is not FITS_NORMAL (focus, guide..etc)
    QString filename;
#if 0

    if (targetChip->isBatchMode() == false || targetChip->getCaptureMode() != FITS_NORMAL)
    {
        if (!writeTempImageFile(format, static_cast<char *>(bp->blob), bp->size, &filename))
        {
            emit BLOBUpdated(nullptr);
            return;
        }
        if (BType == BLOB_FITS)
            addFITSKeywords(filename, filter);

    }
#endif
    // Create file name for sequences.
    if (targetChip->isBatchMode())
    {
        // If either generating file name or writing the image file fails
        // then return
        if (!generateFilename(format, targetChip->isBatchMode(), &filename) ||
                !writeImageFile(filename, bp, BType == BLOB_FITS))
        {
            emit BLOBUpdated(nullptr);
            return;
        }
    }
    else
        filename = QDir::tempPath() + QDir::separator() + "image" + format;

    // store file name
    //    strncpy(BLOBFilename, filename.toLatin1(), MAXINDIFILENAME);
    //    bp->aux0 = targetChip;
    //    bp->aux1 = &BType;
    //    bp->aux2 = BLOBFilename;

    if (targetChip->getCaptureMode() == FITS_NORMAL && targetChip->isBatchMode() == true)
    {
        KStars::Instance()->statusBar()->showMessage(i18n("%1 file saved to %2", shortFormat.toUpper(), filename), 0);
        qCInfo(KSTARS_INDI) << shortFormat.toUpper() << "file saved to" << filename;
    }

    // Don't spam, just one notification per 3 seconds
    if (QDateTime::currentDateTime().secsTo(m_LastNotificationTS) <= -3)
    {
        KNotification::event(QLatin1String("FITSReceived"), i18n("Image file is received"));
        m_LastNotificationTS = QDateTime::currentDateTime();
    }

    // Check if we need to process RAW or regular image. Anything but FITS.
#if 0
    if (BType == BLOB_IMAGE || BType == BLOB_RAW)
    {
        bool useFITSViewer = Options::autoImageToFITS() &&
                             (Options::useFITSViewer() || (Options::useDSLRImageViewer() == false && targetChip->isBatchMode() == false));
        bool useDSLRViewer = (Options::useDSLRImageViewer() || targetChip->isBatchMode() == false);
        // For raw image, we only process them to JPG if we need to open them in the image viewer
        if (BType == BLOB_RAW && (useFITSViewer || useDSLRViewer))
        {
            QString rawFileName  = filename;
            rawFileName          = rawFileName.remove(0, rawFileName.lastIndexOf(QLatin1String("/")));

            QString templateName = QString("%1/%2.XXXXXX").arg(QDir::tempPath(), rawFileName);
            QTemporaryFile imgPreview(templateName);

            imgPreview.setAutoRemove(false);
            imgPreview.open();
            imgPreview.close();
            QString preview_filename = imgPreview.fileName();
            QString errorMessage;

            if (KSUtils::RAWToJPEG(filename, preview_filename, errorMessage) == false)
            {
                KStars::Instance()->statusBar()->showMessage(errorMessage);
                emit BLOBUpdated(bp);
                return;
            }

            // Remove tempeorary CR2 files
            if (targetChip->isBatchMode() == false)
                QFile::remove(filename);

            filename = preview_filename;
            format = ".jpg";
            shortFormat = "jpg";
        }

        // Convert to FITS if checked.
        QString output;
        if (useFITSViewer && (FITSData::ImageToFITS(filename, shortFormat, output)))
        {
            if (BType == BLOB_RAW || targetChip->isBatchMode() == false)
                QFile::remove(filename);

            filename = output;
            BType = BLOB_FITS;

            emit previewFITSGenerated(output);

            FITSData *blob_fits_data = new FITSData(targetChip->getCaptureMode());

            QFuture<bool> fitsloader = blob_fits_data->loadFromFile(filename, false);
            fitsloader.waitForFinished();
            if (!fitsloader.result())
            {
                // If reading the blob fails, we treat it the same as exposure failure
                // and recapture again if possible
                delete (blob_fits_data);
                qCCritical(KSTARS_INDI) << "failed reading FITS memory buffer";
                emit newExposureValue(targetChip, 0, IPS_ALERT);
                return;
            }
            displayFits(targetChip, filename, bp, blob_fits_data);
            return;
        }
        else if (useDSLRViewer)
        {
            if (m_ImageViewerWindow.isNull())
                m_ImageViewerWindow = new ImageViewer(getDeviceName(), KStars::Instance());

            m_ImageViewerWindow->loadImage(filename);

            emit previewJPEGGenerated(filename, m_ImageViewerWindow->metadata());
        }
    }
#endif

    // Load FITS if either:
    // #1 FITS Viewer is set to enabled.
    // #2 This is a preview, so we MUST open FITS Viewer even if disabled.
    //    if (BType == BLOB_FITS)
    //    {
    // Don't display image if the following conditions are met:
    // 1. Mode is NORMAL or CALIBRATE; and
    // 2. FITS Viewer is disabled; and
    // 3. Batch mode is enabled.
    // 4. Summary view is false.
    if ((targetChip->getCaptureMode() == FITS_NORMAL || targetChip->getCaptureMode() == FITS_CALIBRATE) &&
            Options::useFITSViewer() == false &&
            Options::useSummaryPreview() == false &&
            targetChip->isBatchMode())
    {
        emit BLOBUpdated(bp);
        emit newImage(nullptr);
        return;
    }

    QSharedPointer<FITSData> blob_data;
    QByteArray buffer = QByteArray::fromRawData(reinterpret_cast<char *>(bp->blob), bp->size);
    blob_data.reset(new FITSData(targetChip->getCaptureMode()), &QObject::deleteLater);
    if (!blob_data->loadFromBuffer(buffer, shortFormat, filename, false))
    {
        // If reading the blob fails, we treat it the same as exposure failure
        // and recapture again if possible
        qCCritical(KSTARS_INDI) << "failed reading FITS memory buffer";
        emit newExposureValue(targetChip, 0, IPS_ALERT);
        return;
    }

    handleImage(targetChip, filename, bp, blob_data);
    //    else
    //        emit BLOBUpdated(bp);
}

void CCD::handleImage(CCDChip *targetChip, const QString &filename, IBLOB *bp, QSharedPointer<FITSData> data)
{
    FITSMode captureMode = targetChip->getCaptureMode();

    // Add metadata
    data->setProperty("device", getDeviceName());
    data->setProperty("blobVector", bp->bvp->name);
    data->setProperty("blobElement", bp->name);
    data->setProperty("chip", targetChip->getType());

    switch (captureMode)
    {
        case FITS_NORMAL:
        case FITS_CALIBRATE:
        {
            if (Options::useFITSViewer() || targetChip->isBatchMode() == false)
            {
                // No need to wait until the image is loaded in the view, but emit AFTER checking
                // batch mode, since newImage() may change it
                emit BLOBUpdated(bp);
                emit newImage(data);

                bool success = false;
                int tabIndex = -1;
                int *tabID = (captureMode == FITS_NORMAL) ? &normalTabID : &calibrationTabID;
                QUrl fileURL = QUrl::fromLocalFile(filename);
                FITSScale captureFilter = targetChip->getCaptureFilter();
                if (*tabID == -1 || Options::singlePreviewFITS() == false)
                {
                    // If image is preview and we should display all captured images in a
                    // single tab called "Preview", then set the title to "Preview",
                    // Otherwise, the title will be the captured image name
                    QString previewTitle;
                    if (targetChip->isBatchMode() == false && Options::singlePreviewFITS())
                    {
                        // If we are displaying all images from all cameras in a single FITS
                        // Viewer window, then we prefix the camera name to the "Preview" string
                        if (Options::singleWindowCapturedFITS())
                            previewTitle = i18n("%1 Preview", getDeviceName());
                        else
                            // Otherwise, just use "Preview"
                            previewTitle = i18n("Preview");
                    }

                    success = getFITSViewer()->loadData(data, fileURL, &tabIndex, captureMode, captureFilter, previewTitle);
                }
                else
                    success = getFITSViewer()->updateData(data, fileURL, *tabID, &tabIndex, captureFilter);

                if (!success)
                {
                    // If opening file fails, we treat it the same as exposure failure
                    // and recapture again if possible
                    qCCritical(KSTARS_INDI) << "error adding/updating FITS";
                    emit newExposureValue(targetChip, 0, IPS_ALERT);
                    return;
                }
                *tabID = tabIndex;
                targetChip->setImageView(getFITSViewer()->getView(tabIndex), captureMode);
                if (Options::focusFITSOnNewImage())
                    getFITSViewer()->raise();
            }
            else
            {
                emit BLOBUpdated(bp);
                emit newImage(data);
            }
        }
        break;

        case FITS_FOCUS:
        case FITS_GUIDE:
        case FITS_ALIGN:
            loadImageInView(targetChip, data);
            emit BLOBUpdated(bp);
            emit newImage(data);
            break;
    }
}

void CCD::loadImageInView(ISD::CCDChip *targetChip, const QSharedPointer<FITSData> &data)
{
    FITSMode mode = targetChip->getCaptureMode();
    FITSView *view = targetChip->getImageView(mode);
    //QString filename = QString(static_cast<const char *>(bp->aux2));

    if (view)
    {
        view->setFilter(targetChip->getCaptureFilter());
        //if (!view->loadFITSFromData(data, filename))
        if (!view->loadData(data))
        {
            emit newExposureValue(targetChip, 0, IPS_ALERT);
            return;

        }
        // FITSViewer is shown if:
        // Image in preview mode, or useFITSViewer is true; AND
        // Image type is either NORMAL or CALIBRATION since the rest have their dedicated windows.
        // NORMAL is used for raw INDI drivers without Ekos.
        if ( (Options::useFITSViewer() || targetChip->isBatchMode() == false) &&
                (mode == FITS_NORMAL || mode == FITS_CALIBRATE))
            getFITSViewer()->show();
    }
}

CCD::TransferFormat CCD::getTargetTransferFormat() const
{
    return targetTransferFormat;
}

void CCD::setTargetTransferFormat(const TransferFormat &value)
{
    targetTransferFormat = value;
}

//void CCD::FITSViewerDestroyed()
//{
//    normalTabID = calibrationTabID = focusTabID = guideTabID = alignTabID = -1;
//}

void CCD::StreamWindowHidden()
{
    if (baseDevice->isConnected())
    {
        // We can have more than one *_VIDEO_STREAM property active so disable them all
        auto streamSP = baseDevice->getSwitch("CCD_VIDEO_STREAM");
        if (streamSP)
        {
            streamSP->reset();
            streamSP->at(0)->setState(ISS_OFF);
            streamSP->at(1)->setState(ISS_ON);
            streamSP->setState(IPS_IDLE);
            clientManager->sendNewSwitch(streamSP);
        }

        streamSP = baseDevice->getSwitch("VIDEO_STREAM");
        if (streamSP)
        {
            streamSP->reset();
            streamSP->at(0)->setState(ISS_OFF);
            streamSP->at(1)->setState(ISS_ON);
            streamSP->setState(IPS_IDLE);
            clientManager->sendNewSwitch(streamSP);
        }

        streamSP = baseDevice->getSwitch("AUX_VIDEO_STREAM");
        if (streamSP)
        {
            streamSP->reset();
            streamSP->at(0)->setState(ISS_OFF);
            streamSP->at(1)->setState(ISS_ON);
            streamSP->setState(IPS_IDLE);
            clientManager->sendNewSwitch(streamSP);
        }
    }

    if (streamWindow)
        streamWindow->disconnect();
}

bool CCD::hasGuideHead()
{
    return HasGuideHead;
}

bool CCD::hasCooler()
{
    return HasCooler;
}

bool CCD::hasCoolerControl()
{
    return HasCoolerControl;
}

bool CCD::setCoolerControl(bool enable)
{
    if (HasCoolerControl == false)
        return false;

    auto coolerSP = baseDevice->getSwitch("CCD_COOLER");

    if (!coolerSP)
        return false;

    // Cooler ON/OFF
    auto coolerON  = coolerSP->findWidgetByName("COOLER_ON");
    auto coolerOFF = coolerSP->findWidgetByName("COOLER_OFF");
    if (!coolerON || !coolerOFF)
        return false;

    coolerON->setState(enable ? ISS_ON : ISS_OFF);
    coolerOFF->setState(enable ? ISS_OFF : ISS_ON);
    clientManager->sendNewSwitch(coolerSP);

    return true;
}

CCDChip *CCD::getChip(CCDChip::ChipType cType)
{
    switch (cType)
    {
        case CCDChip::PRIMARY_CCD:
            return primaryChip.get();

        case CCDChip::GUIDE_CCD:
            return guideChip.get();
    }

    return nullptr;
}

bool CCD::setRapidGuide(CCDChip *targetChip, bool enable)
{
    ISwitchVectorProperty *rapidSP = nullptr;
    ISwitch *enableS               = nullptr;

    if (targetChip == primaryChip.get())
        rapidSP = baseDevice->getSwitch("CCD_RAPID_GUIDE");
    else
        rapidSP = baseDevice->getSwitch("GUIDER_RAPID_GUIDE");

    if (rapidSP == nullptr)
        return false;

    enableS = IUFindSwitch(rapidSP, "ENABLE");

    if (enableS == nullptr)
        return false;

    // Already updated, return OK
    if ((enable && enableS->s == ISS_ON) || (!enable && enableS->s == ISS_OFF))
        return true;

    IUResetSwitch(rapidSP);
    rapidSP->sp[0].s = enable ? ISS_ON : ISS_OFF;
    rapidSP->sp[1].s = enable ? ISS_OFF : ISS_ON;

    clientManager->sendNewSwitch(rapidSP);

    return true;
}

bool CCD::configureRapidGuide(CCDChip *targetChip, bool autoLoop, bool sendImage, bool showMarker)
{
    ISwitchVectorProperty *rapidSP = nullptr;
    ISwitch *autoLoopS = nullptr, *sendImageS = nullptr, *showMarkerS = nullptr;

    if (targetChip == primaryChip.get())
        rapidSP = baseDevice->getSwitch("CCD_RAPID_GUIDE_SETUP");
    else
        rapidSP = baseDevice->getSwitch("GUIDER_RAPID_GUIDE_SETUP");

    if (rapidSP == nullptr)
        return false;

    autoLoopS   = IUFindSwitch(rapidSP, "AUTO_LOOP");
    sendImageS  = IUFindSwitch(rapidSP, "SEND_IMAGE");
    showMarkerS = IUFindSwitch(rapidSP, "SHOW_MARKER");

    if (!autoLoopS || !sendImageS || !showMarkerS)
        return false;

    // If everything is already set, let's return.
    if (((autoLoop && autoLoopS->s == ISS_ON) || (!autoLoop && autoLoopS->s == ISS_OFF)) &&
            ((sendImage && sendImageS->s == ISS_ON) || (!sendImage && sendImageS->s == ISS_OFF)) &&
            ((showMarker && showMarkerS->s == ISS_ON) || (!showMarker && showMarkerS->s == ISS_OFF)))
        return true;

    autoLoopS->s   = autoLoop ? ISS_ON : ISS_OFF;
    sendImageS->s  = sendImage ? ISS_ON : ISS_OFF;
    showMarkerS->s = showMarker ? ISS_ON : ISS_OFF;

    clientManager->sendNewSwitch(rapidSP);

    return true;
}

void CCD::updateUploadSettings(const QString &remoteDir)
{
    QString filename = seqPrefix + (seqPrefix.isEmpty() ? "" : "_") + QString("XXX");

    ITextVectorProperty *uploadSettingsTP = nullptr;
    IText *uploadT                        = nullptr;

    uploadSettingsTP = baseDevice->getText("UPLOAD_SETTINGS");
    if (uploadSettingsTP)
    {
        uploadT = IUFindText(uploadSettingsTP, "UPLOAD_DIR");
        if (uploadT && remoteDir.isEmpty() == false)
            IUSaveText(uploadT, remoteDir.toLatin1().constData());

        uploadT = IUFindText(uploadSettingsTP, "UPLOAD_PREFIX");
        if (uploadT)
            IUSaveText(uploadT, filename.toLatin1().constData());

        clientManager->sendNewText(uploadSettingsTP);
    }
}

CCD::UploadMode CCD::getUploadMode()
{
    ISwitchVectorProperty *uploadModeSP = nullptr;

    uploadModeSP = baseDevice->getSwitch("UPLOAD_MODE");

    if (uploadModeSP == nullptr)
    {
        qWarning() << "No UPLOAD_MODE in CCD driver. Please update driver to INDI compliant CCD driver.";
        return UPLOAD_CLIENT;
    }

    if (uploadModeSP)
    {
        ISwitch *modeS = nullptr;

        modeS = IUFindSwitch(uploadModeSP, "UPLOAD_CLIENT");
        if (modeS && modeS->s == ISS_ON)
            return UPLOAD_CLIENT;
        modeS = IUFindSwitch(uploadModeSP, "UPLOAD_LOCAL");
        if (modeS && modeS->s == ISS_ON)
            return UPLOAD_LOCAL;
        modeS = IUFindSwitch(uploadModeSP, "UPLOAD_BOTH");
        if (modeS && modeS->s == ISS_ON)
            return UPLOAD_BOTH;
    }

    // Default
    return UPLOAD_CLIENT;
}

bool CCD::setUploadMode(UploadMode mode)
{
    ISwitch *modeS = nullptr;

    auto uploadModeSP = baseDevice->getSwitch("UPLOAD_MODE");

    if (!uploadModeSP)
    {
        qWarning() << "No UPLOAD_MODE in CCD driver. Please update driver to INDI compliant CCD driver.";
        return false;
    }

    switch (mode)
    {
        case UPLOAD_CLIENT:
            modeS = uploadModeSP->findWidgetByName("UPLOAD_CLIENT");
            if (!modeS)
                return false;
            if (modeS->s == ISS_ON)
                return true;
            break;

        case UPLOAD_BOTH:
            modeS = uploadModeSP->findWidgetByName("UPLOAD_BOTH");
            if (!modeS)
                return false;
            if (modeS->s == ISS_ON)
                return true;
            break;

        case UPLOAD_LOCAL:
            modeS = uploadModeSP->findWidgetByName("UPLOAD_LOCAL");
            if (!modeS)
                return false;
            if (modeS->s == ISS_ON)
                return true;
            break;
    }

    uploadModeSP->reset();
    modeS->s = ISS_ON;

    clientManager->sendNewSwitch(uploadModeSP);

    return true;
}

bool CCD::getTemperature(double *value)
{
    if (HasCooler == false)
        return false;

    auto temperatureNP = baseDevice->getNumber("CCD_TEMPERATURE");

    if (!temperatureNP)
        return false;

    *value = temperatureNP->at(0)->getValue();

    return true;
}

bool CCD::setTemperature(double value)
{
    auto nvp = baseDevice->getNumber("CCD_TEMPERATURE");

    if (!nvp)
        return false;

    auto np = nvp->findWidgetByName("CCD_TEMPERATURE_VALUE");

    if (!np)
        return false;

    np->setValue(value);

    clientManager->sendNewNumber(nvp);

    return true;
}

bool CCD::setTransformFormat(CCD::TransferFormat format)
{
    if (format == transferFormat)
        return true;

    auto svp = baseDevice->getSwitch("CCD_TRANSFER_FORMAT");

    if (!svp)
        return false;

    auto formatFITS   = svp->findWidgetByName("FORMAT_FITS");
    auto formatNative = svp->findWidgetByName("FORMAT_NATIVE");

    if (!formatFITS || !formatNative)
        return false;

    transferFormat = format;

    formatFITS->setState(transferFormat == FORMAT_FITS ? ISS_ON : ISS_OFF);
    formatNative->setState(transferFormat == FORMAT_FITS ? ISS_OFF : ISS_ON);

    clientManager->sendNewSwitch(svp);

    return true;
}

bool CCD::setTelescopeType(TelescopeType type)
{
    if (type == telescopeType)
        return true;

    auto svp = baseDevice->getSwitch("TELESCOPE_TYPE");

    if (!svp)
        return false;

    auto typePrimary = svp->findWidgetByName("TELESCOPE_PRIMARY");
    auto typeGuide   = svp->findWidgetByName("TELESCOPE_GUIDE");

    if (!typePrimary || !typeGuide)
        return false;

    telescopeType = type;

    if ( (telescopeType == TELESCOPE_PRIMARY && typePrimary->getState() == ISS_OFF) ||
            (telescopeType == TELESCOPE_GUIDE && typeGuide->getState() == ISS_OFF))
    {
        typePrimary->setState(telescopeType == TELESCOPE_PRIMARY ? ISS_ON : ISS_OFF);
        typeGuide->setState(telescopeType == TELESCOPE_PRIMARY ? ISS_OFF : ISS_ON);
        clientManager->sendNewSwitch(svp);
        setConfig(SAVE_CONFIG);
    }

    return true;
}

bool CCD::setVideoStreamEnabled(bool enable)
{
    if (HasVideoStream == false)
        return false;

    auto svp = baseDevice->getSwitch("CCD_VIDEO_STREAM");

    if (!svp)
        return false;

    // If already on and enable is set or vice versa no need to change anything we return true
    if ((enable && svp->at(0)->getState() == ISS_ON) || (!enable && svp->at(1)->getState() == ISS_ON))
        return true;

    svp->at(0)->setState(enable ? ISS_ON : ISS_OFF);
    svp->at(1)->setState(enable ? ISS_OFF : ISS_ON);

    clientManager->sendNewSwitch(svp);

    return true;
}

bool CCD::resetStreamingFrame()
{
    auto frameProp = baseDevice->getNumber("CCD_STREAM_FRAME");

    if (!frameProp)
        return false;

    auto xarg = frameProp->findWidgetByName("X");
    auto yarg = frameProp->findWidgetByName("Y");
    auto warg = frameProp->findWidgetByName("WIDTH");
    auto harg = frameProp->findWidgetByName("HEIGHT");

    if (xarg && yarg && warg && harg)
    {
        if (!std::fabs(xarg->getValue() - xarg->getMin()) &&
                !std::fabs(yarg->getValue() - yarg->getMin()) &&
                !std::fabs(warg->getValue() - warg->getMax()) &&
                !std::fabs(harg->getValue() - harg->getMax()))
            return false;

        xarg->setValue(xarg->getMin());
        yarg->setValue(yarg->getMin());
        warg->setValue(warg->getMax());
        harg->setValue(harg->getMax());

        clientManager->sendNewNumber(frameProp);
        return true;
    }

    return false;
}

bool CCD::setStreamLimits(uint16_t maxBufferSize, uint16_t maxPreviewFPS)
{
    auto limitsProp = baseDevice->getNumber("LIMITS");

    if (!limitsProp)
        return false;

    auto bufferMax = limitsProp->findWidgetByName("LIMITS_BUFFER_MAX");
    auto previewFPS = limitsProp->findWidgetByName("LIMITS_PREVIEW_FPS");

    if (bufferMax && previewFPS)
    {
        if(std::fabs(bufferMax->getValue() - maxBufferSize) > 0 || std::fabs(previewFPS->getValue() - maxPreviewFPS) > 0)
        {
            bufferMax->setValue(maxBufferSize);
            previewFPS->setValue(maxPreviewFPS);
            clientManager->sendNewNumber(limitsProp);
        }

        return true;
    }

    return false;
}

bool CCD::setStreamingFrame(int x, int y, int w, int h)
{
    auto frameProp = baseDevice->getNumber("CCD_STREAM_FRAME");

    if (!frameProp)
        return false;

    auto xarg = frameProp->findWidgetByName("X");
    auto yarg = frameProp->findWidgetByName("Y");
    auto warg = frameProp->findWidgetByName("WIDTH");
    auto harg = frameProp->findWidgetByName("HEIGHT");

    if (xarg && yarg && warg && harg)
    {
        if (!std::fabs(xarg->getValue() - x) &&
                !std::fabs(yarg->getValue() - y) &&
                !std::fabs(warg->getValue() - w) &&
                !std::fabs(harg->getValue() - h))
            return true;

        // N.B. We add offset since the X, Y are relative to whatever streaming frame is currently active
        xarg->value = qBound(xarg->getMin(), static_cast<double>(x) + xarg->getValue(), xarg->getMax());
        yarg->value = qBound(yarg->getMin(), static_cast<double>(y) + yarg->getValue(), yarg->getMax());
        warg->value = qBound(warg->getMin(), static_cast<double>(w), warg->getMax());
        harg->value = qBound(harg->getMin(), static_cast<double>(h), harg->getMax());

        clientManager->sendNewNumber(frameProp);
        return true;
    }

    return false;
}

bool CCD::isStreamingEnabled()
{
    if (HasVideoStream == false || !streamWindow)
        return false;

    return streamWindow->isStreamEnabled();
}

bool CCD::setSERNameDirectory(const QString &filename, const QString &directory)
{
    auto tvp = baseDevice->getText("RECORD_FILE");

    if (!tvp)
        return false;

    auto filenameT = tvp->findWidgetByName("RECORD_FILE_NAME");
    auto dirT      = tvp->findWidgetByName("RECORD_FILE_DIR");

    if (!filenameT || !dirT)
        return false;

    filenameT->setText(filename.toLatin1().data());
    dirT->setText(directory.toLatin1().data());

    clientManager->sendNewText(tvp);

    return true;
}

bool CCD::getSERNameDirectory(QString &filename, QString &directory)
{
    auto tvp = baseDevice->getText("RECORD_FILE");

    if (!tvp)
        return false;

    auto filenameT = tvp->findWidgetByName("RECORD_FILE_NAME");
    auto dirT      = tvp->findWidgetByName("RECORD_FILE_DIR");

    if (!filenameT || !dirT)
        return false;

    filename  = QString(filenameT->getText());
    directory = QString(dirT->getText());

    return true;
}

bool CCD::startRecording()
{
    auto svp = baseDevice->getSwitch("RECORD_STREAM");

    if (!svp)
        return false;

    auto recordON = svp->findWidgetByName("RECORD_ON");

    if (!recordON)
        return false;

    if (recordON->getState() == ISS_ON)
        return true;

    svp->reset();
    recordON->setState(ISS_ON);

    clientManager->sendNewSwitch(svp);

    return true;
}

bool CCD::startDurationRecording(double duration)
{
    auto nvp = baseDevice->getNumber("RECORD_OPTIONS");

    if (!nvp)
        return false;

    auto durationN = nvp->findWidgetByName("RECORD_DURATION");

    if (!durationN)
        return false;

    auto svp = baseDevice->getSwitch("RECORD_STREAM");

    if (!svp)
        return false;

    auto recordON = svp->findWidgetByName("RECORD_DURATION_ON");

    if (!recordON)
        return false;

    if (recordON->getState() == ISS_ON)
        return true;

    durationN->setValue(duration);
    clientManager->sendNewNumber(nvp);

    svp->reset();
    recordON->setState(ISS_ON);

    clientManager->sendNewSwitch(svp);

    return true;
}

bool CCD::startFramesRecording(uint32_t frames)
{
    auto nvp = baseDevice->getNumber("RECORD_OPTIONS");

    if (!nvp)
        return false;

    auto frameN = nvp->findWidgetByName("RECORD_FRAME_TOTAL");
    auto svp = baseDevice->getSwitch("RECORD_STREAM");

    if (!frameN || !svp)
        return false;

    auto recordON = svp->findWidgetByName("RECORD_FRAME_ON");

    if (!recordON)
        return false;

    if (recordON->getState() == ISS_ON)
        return true;

    frameN->setValue(frames);
    clientManager->sendNewNumber(nvp);

    svp->reset();
    recordON->setState(ISS_ON);

    clientManager->sendNewSwitch(svp);

    return true;
}

bool CCD::stopRecording()
{
    auto svp = baseDevice->getSwitch("RECORD_STREAM");

    if (!svp)
        return false;

    auto recordOFF = svp->findWidgetByName("RECORD_OFF");

    if (!recordOFF)
        return false;

    // If already set
    if (recordOFF->getState() == ISS_ON)
        return true;

    svp->reset();
    recordOFF->setState(ISS_ON);

    clientManager->sendNewSwitch(svp);

    return true;
}

bool CCD::setFITSHeader(const QMap<QString, QString> &values)
{
    auto tvp = baseDevice->getText("FITS_HEADER");

    if (!tvp)
        return false;

    QMapIterator<QString, QString> i(values);

    while (i.hasNext())
    {
        i.next();

        auto headerT = tvp->findWidgetByName(i.key().toLatin1().data());

        if (!headerT)
            continue;

        headerT->setText(i.value().toLatin1().data());
    }

    clientManager->sendNewText(tvp);

    return true;
}

bool CCD::setGain(double value)
{
    if (!gainN)
        return false;

    gainN->value = value;
    clientManager->sendNewNumber(gainN->nvp);
    return true;
}

bool CCD::getGain(double *value)
{
    if (!gainN)
        return false;

    *value = gainN->value;

    return true;
}

bool CCD::getGainMinMaxStep(double *min, double *max, double *step)
{
    if (!gainN)
        return false;

    *min  = gainN->min;
    *max  = gainN->max;
    *step = gainN->step;

    return true;
}

bool CCD::setOffset(double value)
{
    if (!offsetN)
        return false;

    offsetN->value = value;
    clientManager->sendNewNumber(offsetN->nvp);
    return true;
}

bool CCD::getOffset(double *value)
{
    if (!offsetN)
        return false;

    *value = offsetN->value;

    return true;
}

bool CCD::getOffsetMinMaxStep(double *min, double *max, double *step)
{
    if (!offsetN)
        return false;

    *min  = offsetN->min;
    *max  = offsetN->max;
    *step = offsetN->step;

    return true;
}

bool CCD::isBLOBEnabled()
{
    return (clientManager->isBLOBEnabled(getDeviceName(), "CCD1"));
}

bool CCD::setBLOBEnabled(bool enable, const QString &prop)
{
    clientManager->setBLOBEnabled(enable, getDeviceName(), prop);

    return true;
}

bool CCD::setExposureLoopingEnabled(bool enable)
{
    // Set value immediately
    IsLooping = enable;

    auto svp = baseDevice->getSwitch("CCD_EXPOSURE_LOOP");

    if (!svp)
        return false;

    svp->at(0)->setState(enable ? ISS_ON : ISS_OFF);
    svp->at(1)->setState(enable ? ISS_OFF : ISS_ON);
    clientManager->sendNewSwitch(svp);

    return true;
}

bool CCD::setExposureLoopCount(uint32_t count)
{
    auto nvp = baseDevice->getNumber("CCD_EXPOSURE_LOOP_COUNT");

    if (!nvp)
        return false;

    nvp->at(0)->setValue(count);

    clientManager->sendNewNumber(nvp);

    return true;
}

bool CCD::setStreamExposure(double duration)
{
    auto nvp = baseDevice->getNumber("STREAMING_EXPOSURE");

    if (!nvp)
        return false;

    nvp->at(0)->setValue(duration);

    clientManager->sendNewNumber(nvp);

    return true;
}

bool CCD::getStreamExposure(double *duration)
{
    auto nvp = baseDevice->getNumber("STREAMING_EXPOSURE");

    if (!nvp)
        return false;

    *duration = nvp->at(0)->getValue();

    return true;
}

bool CCD::isCoolerOn()
{
    auto svp = baseDevice->getSwitch("CCD_COOLER");

    if (!svp)
        return false;

    return (svp->at(0)->getState() == ISS_ON);
}

bool CCD::getTemperatureRegulation(double &ramp, double &threshold)
{
    auto regulation = baseDevice->getProperty("CCD_TEMP_RAMP");
    if (!regulation->isValid())
        return false;

    ramp = regulation.getNumber()->at(0)->getValue();
    threshold = regulation.getNumber()->at(1)->getValue();
    return true;
}

bool CCD::setTemperatureRegulation(double ramp, double threshold)
{
    auto regulation = baseDevice->getProperty("CCD_TEMP_RAMP");
    if (!regulation->isValid())
        return false;

    regulation.getNumber()->at(0)->setValue(ramp);
    regulation.getNumber()->at(1)->setValue(threshold);
    clientManager->sendNewNumber(regulation->getNumber());
    return true;
}
}
