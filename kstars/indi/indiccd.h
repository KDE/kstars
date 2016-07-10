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
#include <QPointer>

#include <fitsviewer/fitsviewer.h>
#include <fitsviewer/fitsdata.h>

#include <auxiliary/imageviewer.h>

class FITSView;

class StreamWG;

/**
 * \namespace ISD
 *
 *  ISD is a collection of INDI Standard Devices. It encapsulates common types of INDI devices such as telescopes and CCDs.
 *
 */
namespace ISD
{

/**
 * @class CCDChip
 * CCDChip class controls a particular chip in CCD device. While most amateur CCDs only have a single chip on the CCD, some
 * CCDs have additional chips targetted for guiding purposes.
 */
class CCDChip
{
public:
    typedef enum { PRIMARY_CCD, GUIDE_CCD } ChipType;

    CCDChip(INDI::BaseDevice *bDevice, ClientManager *cManager, ChipType cType);

    FITSView * getImage(FITSMode imageType);
    void setImage(FITSView *image, FITSMode imageType);
    void setCaptureMode(FITSMode mode) { captureMode = mode; }
    void setCaptureFilter(FITSScale fType) { captureFilter = fType; }

    // Common commands
    bool getFrame(int *x, int *y, int *w, int *h);
    bool getFrameMinMax(int *minX, int *maxX, int *minY, int *maxY, int *minW, int *maxW, int *minH, int *maxH);
    bool setFrame(int x, int y, int w, int h);
    bool getFocusFrame(int *x, int *y, int *w, int *h);
    bool setFocusFrame(int x, int y, int w, int h);
    bool resetFrame();
    bool capture(double exposure);
    bool setFrameType(CCDFrameType fType);
    bool setFrameType(const QString & name);    
    CCDFrameType getFrameType();
    bool setBinning(int bin_x, int bin_y);
    bool setBinning(CCDBinType binType);
    CCDBinType getBinning();
    bool getBinning(int *bin_x, int *bin_y);
    bool getMaxBin(int *max_xbin, int *max_ybin);
    ChipType getType() const { return type; }

    bool isCapturing();
    bool abortExposure();

    FITSMode getCaptureMode() const { return captureMode;}
    FITSScale getCaptureFilter() const { return captureFilter; }
    bool isBatchMode() const { return batchMode; }
    void setBatchMode(bool enable) { batchMode = enable; }
    QStringList getFrameTypes() const { return frameTypes; }
    void addFrameLabel(const QString & label) { frameTypes << label; }
    void clearFrameTypes() { frameTypes.clear();}

    bool canBin() const;
    void setCanBin(bool value);

    bool canSubframe() const;
    void setCanSubframe(bool value);

    bool canAbort() const;
    void setCanAbort(bool value);

    FITSData *getImageData() const;
    void setImageData(FITSData *value);

    int getISOIndex() const;
    bool setISOIndex(int value);

    QStringList getISOList() const;

private:
    QPointer<FITSView> normalImage, focusImage, guideImage, calibrationImage;
    FITSData *imageData;
    FITSMode captureMode;
    FITSScale captureFilter;
    INDI::BaseDevice *baseDevice;
    ClientManager *clientManager;
    ChipType type;
    bool batchMode;
    bool displayFITS;
    QStringList frameTypes;
    bool CanBin;
    bool CanSubframe;
    bool CanAbort;    
    int fx,fy,fw,fh;

};

/**
 * @class CCD
 * CCD class controls an INDI CCD device. It can be used to issue and abort capture commands, receive and process BLOBs,
 * and return information on the capabilities of the CCD.
 *
 * @author Jasem Mutlaq
 */
class CCD : public DeviceDecorator
{
    Q_OBJECT

public:

    CCD(GDInterface *iPtr);
    ~CCD();

    typedef enum { UPLOAD_CLIENT, UPLOAD_LOCAL, UPLOAD_BOTH } UploadMode;
    void registerProperty(INDI::Property *prop);
    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty* tvp);
    void processNumber(INumberVectorProperty *nvp);
    void processLight(ILightVectorProperty *lvp);
    void processBLOB(IBLOB *bp);

    DeviceFamily getType() { return dType;}
    bool hasGuideHead();
    bool hasCooler();
    bool hasCoolerControl();
    bool setCoolerControl(bool enable);

    // Utitlity functions
    bool getTemperature(double *value);
    bool setTemperature(double value);
    void setISOMode(bool enable) { ISOMode = enable; }
    void setSeqPrefix(const QString &preFix) { seqPrefix = preFix; }
    void setNextSequenceID(int count) { nextSequenceID = count; }
    void setFilter(const QString & newFilter) { filter = newFilter;}
    bool configureRapidGuide(CCDChip *targetChip, bool autoLoop, bool sendImage=false, bool showMarker=false);
    bool setRapidGuide(CCDChip *targetChip, bool enable);
    void updateUploadSettings();
    UploadMode getUploadMode();
    bool setUploadMode(UploadMode mode);

    FITSViewer *getViewer() { return fv;}
    CCDChip * getChip(CCDChip::ChipType cType);
    void setFITSDir(const QString &dir) { fitsDir = dir;}

public slots:
    void FITSViewerDestroyed();
    void StreamWindowHidden();

signals:
    //void FITSViewerClosed();
    void newTemperatureValue(double value);
    void newExposureValue(ISD::CCDChip *chip, double value, IPState state);
    void newGuideStarData(ISD::CCDChip *chip, double dx, double dy, double fit);
    void newRemoteFile(QString);
    void newImage(QImage *image);

private:
    void addFITSKeywords(QString filename);
    QString filter;

    bool ISOMode;
    bool HasGuideHead;
    bool HasCooler;
    bool HasCoolerControl;
    QString		seqPrefix;
    QString     fitsDir;
    char BLOBFilename[MAXINDIFILENAME];
    int nextSequenceID;
    StreamWG *streamWindow;
    int streamW, streamH;
    ISD::ST4 *ST4Driver;
    int normalTabID, calibrationTabID, focusTabID, guideTabID;
    CCDChip *primaryChip, *guideChip;

    QPointer<FITSViewer> fv;
    QPointer<ImageViewer> imageViewer;


};

}
#endif // INDICCD_H
