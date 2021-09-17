/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indistd.h"
#include "wsmedia.h"
#include "auxiliary/imageviewer.h"
#include "fitsviewer/fitscommon.h"
#include "fitsviewer/fitsview.h"
#include "fitsviewer/fitsviewer.h"

#include <QStringList>
#include <QPointer>
#include <QtConcurrent>

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
        void setCaptureMode(FITSMode mode)
        {
            captureMode = mode;
        }
        void setCaptureFilter(FITSScale fType)
        {
            captureFilter = fType;
        }

        // Common commands
        bool getFrame(int *x, int *y, int *w, int *h);
        bool getFrameMinMax(int *minX, int *maxX, int *minY, int *maxY, int *minW, int *maxW, int *minH, int *maxH);
        bool setFrame(int x, int y, int w, int h, bool force = false);

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
        ChipType getType() const
        {
            return type;
        }
        ISD::CCD *getCCD()
        {
            return parentCCD;
        }

        // Set Image Info
        bool setImageInfo(uint16_t width, uint16_t height, double pixelX, double pixelY, uint8_t bitdepth);
        // Get Image Info
        bool getImageInfo(uint16_t &width, uint16_t &height, double &pixelX, double &pixelY, uint8_t &bitdepth);
        // Bayer Info
        bool getBayerInfo(uint16_t &offsetX, uint16_t &offsetY, QString &pattern);

        bool isCapturing();
        bool abortExposure();

        FITSMode getCaptureMode() const
        {
            return captureMode;
        }
        FITSScale getCaptureFilter() const
        {
            return captureFilter;
        }
        bool isBatchMode() const
        {
            return batchMode;
        }
        void setBatchMode(bool enable)
        {
            batchMode = enable;
        }
        QStringList getFrameTypes() const
        {
            return frameTypes;
        }
        void addFrameLabel(const QString &label)
        {
            frameTypes << label;
        }
        void clearFrameTypes()
        {
            frameTypes.clear();
        }

        bool canBin() const;
        void setCanBin(bool value);

        bool canSubframe() const;
        void setCanSubframe(bool value);

        bool canAbort() const;
        void setCanAbort(bool value);

        const QSharedPointer<FITSData> &getImageData() const;
        void setImageData(const QSharedPointer<FITSData> &data)
        {
            imageData = data;
        }

        int getISOIndex() const;
        bool setISOIndex(int value);

        QStringList getISOList() const;

    private:
        QPointer<FITSView> normalImage, focusImage, guideImage, calibrationImage, alignImage;
        QSharedPointer<FITSData> imageData { nullptr };
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
        virtual ~CCD() override;

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

        void registerProperty(INDI::Property prop) override;
        void removeProperty(const QString &name) override;
        void processSwitch(ISwitchVectorProperty *svp) override;
        void processText(ITextVectorProperty *tvp) override;
        void processNumber(INumberVectorProperty *nvp) override;
        void processLight(ILightVectorProperty *lvp) override;
        void processBLOB(IBLOB *bp) override;

        DeviceFamily getType() override
        {
            return dType;
        }

        // Does it an on-chip dedicated guide head?
        bool hasGuideHead();
        // Does it report temperature?
        bool hasCooler();
        // Can temperature be controlled?
        bool canCool()
        {
            return CanCool;
        }
        // Does it have active cooler on/off controls?
        bool hasCoolerControl();
        bool setCoolerControl(bool enable);
        bool isCoolerOn();
        // Does it have a video stream?
        bool hasVideoStream()
        {
            return HasVideoStream;
        }

        // Temperature controls
        bool getTemperature(double *value);
        bool setTemperature(double value);

        // Temperature Regulation
        bool getTemperatureRegulation(double &ramp, double &threshold);
        bool setTemperatureRegulation(double ramp, double threshold);

        // Utility functions
        void setISOMode(bool enable)
        {
            ISOMode = enable;
        }
        void setSeqPrefix(const QString &preFix)
        {
            seqPrefix = preFix;
        }
        void setNextSequenceID(int count)
        {
            nextSequenceID = count;
        }
        //        void setFilter(const QString &newFilter)
        //        {
        //            filter = newFilter;
        //        }

        // Gain controls
        bool hasGain()
        {
            return gainN != nullptr;
        }
        bool getGain(double *value);
        IPerm getGainPermission() const
        {
            return gainPerm;
        }
        bool setGain(double value);
        bool getGainMinMaxStep(double *min, double *max, double *step);

        // offset controls
        bool hasOffset()
        {
            return offsetN != nullptr;
        }
        bool getOffset(double *value);
        IPerm getOffsetPermission() const
        {
            return offsetPerm;
        }
        bool setOffset(double value);
        bool getOffsetMinMaxStep(double *min, double *max, double *step);

        // Rapid Guide
        bool configureRapidGuide(CCDChip *targetChip, bool autoLoop, bool sendImage = false, bool showMarker = false);
        bool setRapidGuide(CCDChip *targetChip, bool enable);

        // Upload Settings
        void updateUploadSettings(const QString &remoteDir);
        UploadMode getUploadMode();
        bool setUploadMode(UploadMode mode);

        // Transfer Format
        TransferFormat getTransferFormat()
        {
            return transferFormat;
        }
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
        bool setStreamLimits(uint16_t maxBufferSize, uint16_t maxPreviewFPS);

        // Video Recording
        bool setSERNameDirectory(const QString &filename, const QString &directory);
        bool getSERNameDirectory(QString &filename, QString &directory);
        bool startRecording();
        bool startDurationRecording(double duration);
        bool startFramesRecording(uint32_t frames);
        bool stopRecording();

        // Telescope type
        TelescopeType getTelescopeType()
        {
            return telescopeType;
        }
        bool setTelescopeType(TelescopeType type);

        // Update FITS Header
        bool setFITSHeader(const QMap<QString, QString> &values);

        CCDChip *getChip(CCDChip::ChipType cType);
        void setFITSDir(const QString &dir)
        {
            fitsDir = dir;
        }

        TransferFormat getTargetTransferFormat() const;
        void setTargetTransferFormat(const TransferFormat &value);

        bool setExposureLoopingEnabled(bool enable);
        bool isLooping() const
        {
            return IsLooping;
        }
        bool setExposureLoopCount(uint32_t count);

        const QMap<QString, double> &getExposurePresets() const
        {
            return m_ExposurePresets;
        }
        const QPair<double, double> getExposurePresetsMinMax() const
        {
            return m_ExposurePresetsMinMax;
        }

    public slots:
        //void FITSViewerDestroyed();
        void StreamWindowHidden();
        // Blob manager
        void setBLOBManager(const char *device, INDI::Property prop);

    protected slots:
        void setWSBLOB(const QByteArray &message, const QString &extension);

    signals:
        void newTemperatureValue(double value);
        void newExposureValue(ISD::CCDChip *chip, double value, IPState state);
        void newGuideStarData(ISD::CCDChip *chip, double dx, double dy, double fit);
        void newBLOBManager(INDI::Property prop);
        void newRemoteFile(QString);
        void videoStreamToggled(bool enabled);
        void videoRecordToggled(bool enabled);
        void newFPS(double instantFPS, double averageFPS);
        void newVideoFrame(const QSharedPointer<QImage> &frame);
        void coolerToggled(bool enabled);
        void ready();
        void captureFailed();
        void newImage(const QSharedPointer<FITSData> &data);

    private:
        void processStream(IBLOB *bp);
        void loadImageInView(ISD::CCDChip *targetChip, const QSharedPointer<FITSData> &data);
        bool generateFilename(const QString &format, bool batch_mode, QString *filename);
        // Saves an image to disk on a separate thread.
        bool writeImageFile(const QString &filename, IBLOB *bp, bool is_fits);
        // Creates or finds the FITSViewer.
        QPointer<FITSViewer> getFITSViewer();
        void handleImage(CCDChip *targetChip, const QString &filename, IBLOB *bp, QSharedPointer<FITSData> data);

        //QString filter;
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

        //char BLOBFilename[MAXINDIFILENAME + 1];
        IBLOB *primaryCCDBLOB { nullptr };

        std::unique_ptr<QTimer> readyTimer;
        std::unique_ptr<CCDChip> primaryChip;
        std::unique_ptr<CCDChip> guideChip;
        std::unique_ptr<WSMedia> m_Media;
        TransferFormat transferFormat { FORMAT_FITS };
        TransferFormat targetTransferFormat { FORMAT_FITS };
        TelescopeType telescopeType { TELESCOPE_UNKNOWN };

        // Gain, since it is spread among different vector properties, let's try to find the property itself.
        INumber *gainN { nullptr };
        IPerm gainPerm { IP_RO };

        INumber *offsetN { nullptr };
        IPerm offsetPerm { IP_RO };

        QPointer<FITSViewer> m_FITSViewerWindow;
        QPointer<ImageViewer> m_ImageViewerWindow;

        QDateTime m_LastNotificationTS;

        // Typically for DSLRs
        QMap<QString, double> m_ExposurePresets;
        QPair<double, double> m_ExposurePresetsMinMax;

        // Used when writing the image fits file to disk in a separate thread.
        char *fileWriteBuffer { nullptr };
        int fileWriteBufferSize { 0 };
        QString fileWriteFilename;
        QFuture<void> fileWriteThread;
};
}
