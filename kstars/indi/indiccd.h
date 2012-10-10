/*  INDI CCD
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#ifndef INDICCD_H
#define INDICCD_H

#include "indistd.h"

#include <QStringList>

class FITSImage;

namespace ISD
{

class CCDChip
{
public:
    typedef enum { PRIMARY_CCD, GUIDE_CCD } ChipType;

    CCDChip(INDI::BaseDevice *bDevice, ClientManager *cManager, ChipType cType);

    FITSImage * getImage(FITSMode imageType);
    void setImage(FITSImage *image, FITSMode imageType);
    void setCaptureMode(FITSMode mode) { captureMode = mode; }
    void setCaptureFilter(FITSScale fType) { captureFilter = fType; }

    // Common commands
    bool getFrame(int *x, int *y, int *w, int *h);
    bool setFrame(int x, int y, int w, int h);
    bool capture(double exposure);
    bool setFrameType(CCDFrameType fType);
    bool setFrameType(const QString & name);
    CCDFrameType getFrameType();
    bool setBinning(int bin_x, int bin_y);
    bool setBinning(CCDBinType binType);
    CCDBinType getBinning();
    bool getBinning(int *bin_x, int *bin_y);

    FITSMode getCaptureMode() { return captureMode;}
    FITSScale getCaptureFilter() { return captureFilter; }

    bool isBatchMode() { return batchMode; }
    void setBatchMode(bool enable) { batchMode = enable; }
    QStringList getFrameTypes() { return frameTypes; }
    void addFrameLabel(const QString & label) { frameTypes << label; }

private:
    FITSImage *normalImage, *focusImage, *guideImage, *calibrationImage;
    FITSMode captureMode;
    FITSScale captureFilter;
    INDI::BaseDevice *baseDevice;
    ClientManager *clientManager;
    ChipType type;
    bool batchMode;
    QStringList frameTypes;
};

class CCD : public DeviceDecorator
{
    Q_OBJECT

public:

    CCD(GDInterface *iPtr);
    ~CCD();

    void registerProperty(INDI::Property *prop);
    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty* tvp);
    void processNumber(INumberVectorProperty *nvp);
    void processLight(ILightVectorProperty *lvp);
    void processBLOB(IBLOB *bp);

    DeviceFamily getType() { return dType;}
    bool hasGuideHead();

    // Utitlity functions
    void setISOMode(bool enable) { ISOMode = enable; }
    void setSeqPrefix(const QString &preFix) { seqPrefix = preFix; }
    void setSeqCount(int count) { seqCount = count; }

    FITSViewer *getViewer() { return fv;}


    CCDChip * getChip(CCDChip::ChipType cType);


public slots:
    void FITSViewerDestroyed();
    void StreamWindowDestroyed();

signals:
    void FITSViewerClosed();
    void newExposureValue(ISD::CCDChip *chip, double value);

private:    
    bool ISOMode;
    bool HasGuideHead;
    QString		seqPrefix;
    int seqCount;
    FITSViewer * fv;
    StreamWG *streamWindow;
    ISD::ST4 *ST4Driver;
    int normalTabID, calibrationTabID, focusTabID, guideTabID;


    CCDChip *primaryChip, *guideChip;

};

}
#endif // INDICCD_H
