/*  INDI CCD
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "indistd.h"
#include "media.h"
#include "auxiliary/imageviewer.h"
#include "fitsviewer/fitscommon.h"
#include "fitsviewer/fitsview.h"
#include "fitsviewer/fitsviewer.h"

#include <QStringList>
#include <QPointer>

#include <memory>

class FITSData;
class FITSView;
class QTimer;
class StreamWG;

/**
 * \namespace ISD
 *
 *  ISD is a collection of INDI Standard Devices. It encapsulates common types of INDI devices such as telescopes and CCDs.
 *
 */
namespace ISD
{
class CCD;

/**
 * @class CCDChip
 * CCDChip class controls a particular chip in CCD device. While most amateur CCDs only have a single chip on the CCD, some
 * CCDs have additional chips targetted for guiding purposes.
 */
class CCDChip
{
  public:
    typedef enum { PRIMARY_CCD, GUIDE_CCD } ChipType;

    CCDChip(ISD::CCD *ccd, ChipType cType);

    FITSView *getImageView(FITSMode imageType);
    void setImageView(FITSView *image, FITSMode imageType);
    void setCaptureMode(FITSMode mode) { captureMode = mode; }
    void setCaptureFilter(FITSScale fType) { captureFilter = fType; }

    // Common commands
    bool getFrame(int *x, int *y, int *w, int *h);
    bool getFrameMinMax(int *minX, int *maxX, int *minY, int *maxY, int *minW, int *maxW, int *minH, int *maxH);
    bool setFrame(int x, int y, int w, int h);

    bool resetFrame();
    bool capture(double exposure);
    bool setFrameType(CCDFrameType fType);
    bool setFrameType(const QString &name);
    CCDFrameType getFrameType();
    bool setBinning(int bin_x, int bin_y);
    bool setBinning(CCDBinType binType);
    CCDBinType getBinning();
    bool getBinning(int *bin_x, int *bin_y);
    bool getMaxBin(int *max_xbin, int *max_ybin);
    ChipType getType() const { return type; }
    ISD::CCD *getCCD() { return parentCCD; }

    // Set Image Info
    bool setImageInfo(uint16_t width, uint16_t height, double pixelX, double pixelY, uint8_t bitdepth);
    // Get Pixel size
    bool getPixelSize(double &x, double &y);

    bool isCapturing();
    bool abortExposure();

    FITSMode getCaptureMode() const { return captureMode; }
    FITSScale getCaptureFilter() const { return captureFilter; }
    bool isBatchMode() const { return batchMode; }
    void setBatchMode(bool enable) { batchMode = enable; }
    QStringList getFrameTypes() const { return frameTypes; }
    void addFrameLabel(const QString &label) { frameTypes << label; }
    void clearFrameTypes() { frameTypes.clear(); }

    bool canBin() const;
    void setCanBin(bool value);

    bool canSubframe() const;
    void setCanSubframe(bool value);

    bool canAbort() const;
    void setCanAbort(bool value);

    FITSData *getImageData() const;
    void setImageData(FITSData *data) { imageData = data; }

    int getISOIndex() const;
    bool setISOIndex(int value);

    QStringList getISOList() const;

private:
    QPointer<FITSView> normalImage, focusImage, guideImage, calibrationImage, alignImage;
    FITSData *imageData { nullptr };
    FITSMode captureMode { FITS_NORMAL };
    FITSScale captureFilter { FITS_NONE };
    INDI::BaseDevice *baseDevice { nullptr };
    ClientManager *clientManager { nullptr };
    ChipType type;
    bool batchMode { false };
    QStringList frameTypes;
    bool CanBin { false };
    bool CanSubframe { false };
    bool CanAbort { false };
    ISD::CCD *parentCCD { nullptr };
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
    explicit CCD(GDInterface *iPtr);

    typedef enum { UPLOAD_CLIENT, UPLOAD_LOCAL, UPLOAD_BOTH } UploadMode;
    typedef enum { FORMAT_FITS, FORMAT_NATIVE } TransferFormat;
    enum BlobType
    {
        BLOB_IMAGE,
        BLOB_FITS,
        BLOB_RAW,
        BLOB_OTHER
    } BType;
    typedef enum { TELESCOPE_PRIMARY, TELESCOPE_GUIDE, TELESCOPE_UNKNOWN } TelescopeType;

    void registerProperty(INDI::Property *prop);
    void removeProperty(INDI::Property *prop);
    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty *tvp);
    void processNumber(INumberVectorProperty *nvp);
    void processLight(ILightVectorProperty *lvp);
    void processBLOB(IBLOB *bp);

    DeviceFamily getType() { return dType; }

    // Does it an on-chip dedicated guide head?
    bool hasGuideHead();
    // Does it report temperature?
    bool hasCooler();
    // Can temperature be controlled?
    bool canCool() { return CanCool; }
    // Does it have active cooler on/off controls?
    bool hasCoolerControl();
    bool setCoolerControl(bool enable);
    bool isCoolerOn();
    // Does it have a video stream?
    bool hasVideoStream() { return HasVideoStream; }

    // Temperature controls
    bool getTemperature(double *value);
    bool setTemperature(double value);

    // Utility functions
    void setISOMode(bool enable) { ISOMode = enable; }
    void setSeqPrefix(const QString &preFix) { seqPrefix = preFix; }
    void setNextSequenceID(int count) { nextSequenceID = count; }
    void setFilter(const QString &newFilter) { filter = newFilter; }

    // Gain controls
    bool hasGain() { return gainN != nullptr; }
    bool getGain(double *value);
    IPerm getGainPermission() const {return gainPerm;}
    bool setGain(double value);
    bool getGainMinMaxStep(double *min, double *max, double *step);

    // Rapid Guide
    bool configureRapidGuide(CCDChip *targetChip, bool autoLoop, bool sendImage = false, bool showMarker = false);
    bool setRapidGuide(CCDChip *targetChip, bool enable);

    // Upload Settings
    void updateUploadSettings(const QString &remoteDir);
    UploadMode getUploadMode();
    bool setUploadMode(UploadMode mode);

    // Transfer Format
    TransferFormat getTransferFormat() { return transferFormat; }
    bool setTransformFormat(CCD::TransferFormat format);

    // BLOB control
    bool isBLOBEnabled();
    bool setBLOBEnabled(bool enable, const QString &prop = QString());

    // Video Stream
    bool setVideoStreamEnabled(bool enable);
    bool resetStreamingFrame();
    bool setStreamingFrame(int x, int y, int w, int h);
    bool isStreamingEnabled();
    bool setStreamExposure(double duration);
    bool getStreamExposure(double *duration);

    // Video Recording
    bool setSERNameDirectory(const QString &filename, const QString &directory);
    bool getSERNameDirectory(QString &filename, QString &directory);
    bool startRecording();
    bool startDurationRecording(double duration);
    bool startFramesRecording(uint32_t frames);
    bool stopRecording();

    // Telescope type
    TelescopeType getTelescopeType() { return telescopeType; }
    bool setTelescopeType(TelescopeType type);

    // Update FITS Header
    bool setFITSHeader(const QMap<QString, QString> &values);

    CCDChip *getChip(CCDChip::ChipType cType);
    void setFITSDir(const QString &dir) { fitsDir = dir; }

    TransferFormat getTargetTransferFormat() const;
    void setTargetTransferFormat(const TransferFormat &value);

    bool setExposureLoopingEnabled(bool enable);
    bool isLooping() const { return IsLooping; }
    bool setExposureLoopCount(uint32_t count);

  public slots:
    void FITSViewerDestroyed();
    void StreamWindowHidden();

  protected slots:
    void setWSBLOB(const QByteArray &message, const QString &extension);

  signals:
    //void FITSViewerClosed();
    void newTemperatureValue(double value);
    void newExposureValue(ISD::CCDChip *chip, double value, IPState state);
    void newGuideStarData(ISD::CCDChip *chip, double dx, double dy, double fit);
    void newBLOBManager(INDI::Property *prop);
    void newRemoteFile(QString);
    void videoStreamToggled(bool enabled);
    void videoRecordToggled(bool enabled);
    void newFPS(double instantFPS, double averageFPS);
    void newVideoFrame(std::unique_ptr<QImage> & frame);
    void coolerToggled(bool enabled);
    void ready();

  private:
    void addFITSKeywords(const QString& filename);
    void loadImageInView(IBLOB *bp, ISD::CCDChip *targetChip);

    QString filter;
    bool ISOMode { true };
    bool HasGuideHead { false };
    bool HasCooler { false };
    bool CanCool { false };
    bool HasCoolerControl { false };
    bool HasVideoStream { false };
    bool IsLooping { false };
    QString seqPrefix;
    QString fitsDir;
    int nextSequenceID { 0 };
    std::unique_ptr<StreamWG> streamWindow;
    int streamW { 0 };
    int streamH { 0 };
    int normalTabID { -1 };
    int calibrationTabID { -1 };
    int focusTabID { -1 };
    int guideTabID { -1 };
    int alignTabID { -1 };

    char BLOBFilename[MAXINDIFILENAME+1];
    IBLOB *primaryCCDBLOB { nullptr };

    std::unique_ptr<QTimer> readyTimer;
    std::unique_ptr<CCDChip> primaryChip;
    std::unique_ptr<CCDChip> guideChip;
    std::unique_ptr<Media> m_Media;
    TransferFormat transferFormat { FORMAT_FITS };
    TransferFormat targetTransferFormat { FORMAT_FITS };
    TelescopeType telescopeType { TELESCOPE_UNKNOWN };

    // Gain, since it is spread among different vector properties, let's try to find the property itself.
    INumber *gainN { nullptr };
    IPerm gainPerm { IP_RO };

    QPointer<FITSViewer> m_FITSViewerWindows;
    QPointer<ImageViewer> m_ImageViewerWindow;
};
}
