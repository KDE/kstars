/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "fitsviewer/fitsdata.h"
#include "indiconcretedevice.h"
#include "indicamerachip.h"

#include "wsmedia.h"
#include "auxiliary/imageviewer.h"
#include "fitsviewer/fitsdata.h"

#include <QStringList>
#include <QPointer>
#include <QtConcurrent>

#include <memory>

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
class CameraChip;

/**
 * @class Camera
 * Camera class controls an INDI Camera device. It can be used to issue and abort capture commands, receive and process BLOBs,
 * and return information on the capabilities of the camera.
 *
 * @author Jasem Mutlaq
 */
class Camera : public ConcreteDevice
{
        Q_OBJECT
        Q_PROPERTY(bool StreamingEnabled MEMBER m_StreamingEnabled)

    public:
        explicit Camera(ISD::GenericDevice *parent);
        virtual ~Camera() override;

        typedef enum { UPLOAD_CLIENT, UPLOAD_REMOTE, UPLOAD_BOTH } UploadMode;
        enum BlobType
        {
            BLOB_IMAGE,
            BLOB_FITS,
            BLOB_XISF,
            BLOB_RAW,
            BLOB_OTHER
        } BType;
        typedef enum { TELESCOPE_PRIMARY, TELESCOPE_GUIDE, TELESCOPE_UNKNOWN } TelescopeType;
        typedef enum
        {
            ERROR_CAPTURE,              /** INDI Camera error */
            ERROR_SAVE,                 /** Saving to disk error */
            ERROR_LOAD,                 /** Loading image buffer error */
            ERROR_VIEWER                /** Loading in FITS Viewer Error */
        } ErrorType;

        void registerProperty(INDI::Property prop) override;
        void removeProperty(INDI::Property prop) override;

        void processSwitch(INDI::Property prop) override;
        void processText(INDI::Property prop) override;
        void processNumber(INDI::Property prop) override;
        bool processBLOB(INDI::Property prop) override;

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

        bool setScopeInfo(double focalLength, double aperture);

        // Utility functions
        void setSeqPrefix(const QString &preFix)
        {
            seqPrefix = preFix;
        }
        void setNextSequenceID(int count)
        {
            nextSequenceID = count;
        }

        // Gain controls
        bool hasGain()
        {
            return m_GainSource != GAIN_SOURCE_UNKNOWN;
        }
        bool getGain(double *value);
        IPerm getGainPermission() const
        {
            return m_GainPermission;
        }
        bool setGain(double value);
        bool getGainMinMaxStep(double *min, double *max, double *step);

        // offset controls
        bool hasOffset()
        {
            return m_OffsetSource != OFFSET_SOURCE_UNKNOWN;
        }
        bool getOffset(double *value);
        IPerm getOffsetPermission() const
        {
            return m_OffsetPermission;
        }
        bool setOffset(double value);
        bool getOffsetMinMaxStep(double *min, double *max, double *step);

        // Upload Settings
        void updateUploadSettings(const QString &uploadDirectory, const QString &uploadFile);
        UploadMode getUploadMode();
        bool setUploadMode(UploadMode mode);

        // Encoding Format
        const QString &getEncodingFormat() const
        {
            return m_EncodingFormat;
        }
        bool setEncodingFormat(const QString &value);
        const QStringList &getEncodingFormats() const
        {
            return m_EncodingFormats;
        }
        const QString &getStreamEncoding() const
        {
            return m_StreamEncoding;
        }
        bool setStreamEncoding(const QString &value);
        const QStringList &getStreamEncodings() const
        {
            return m_StreamEncodings;
        }
        const QString &getStreamRecording() const
        {
            return m_StreamRecording;
        }
        bool setStreamRecording(const QString &value);
        const QStringList &getVideoFormats() const
        {
            return m_VideoFormats;
        }

        // Capture Format
        const QStringList &getCaptureFormats() const
        {
            return m_CaptureFormats;
        }
        QString getCaptureFormat() const;
        bool setCaptureFormat(const QString &format);

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

        // Update FITS Header
        bool setFITSHeaders(const QList<FITSData::Record> &values);

        CameraChip *getChip(CameraChip::ChipType cType);

        bool setFastExposureEnabled(bool enable);
        bool isFastExposureEnabled() const
        {
            return m_FastExposureEnabled;
        }
        bool setFastCount(uint32_t count);

        const QMap<QString, double> &getExposurePresets() const
        {
            return m_ExposurePresets;
        }
        const QPair<double, double> getExposurePresetsMinMax() const
        {
            return m_ExposurePresetsMinMax;
        }

        /**
         * @brief saveCurrentImage save the image that is currently in the image data buffer
         * @return true if saving succeeded
         */
        bool saveCurrentImage(QString &filename);


    public slots:
        void StreamWindowHidden();
        // Blob manager
        void setBLOBManager(const char *device, INDI::Property prop);

    protected slots:
        void setWSBLOB(const QByteArray &message, const QString &extension);

    signals:
        void newTemperatureValue(double value);
        void newExposureValue(ISD::CameraChip *chip, double value, IPState state);
        void newGuideStarData(ISD::CameraChip *chip, double dx, double dy, double fit);
        void newBLOBManager(INDI::Property prop);
        void newRemoteFile(QString);
        void coolerToggled(bool enabled);
        void error(ErrorType type);
        // Video
        void updateVideoWindow(int width, int height, bool streamEnabled);
        void closeVideoWindow();
        void videoStreamToggled(bool enabled);
        void showVideoFrame(INDI::Property prop, int width, int height);
        void videoRecordToggled(bool enabled);
        void newFPS(double instantFPS, double averageFPS);
        void newVideoFrame(const QSharedPointer<QImage> &frame);
        // Data
        void newImage(const QSharedPointer<FITSData> &data, const QString &extension = "");
        // View
        void newView(const QSharedPointer<FITSView> &view);

    private:
        void processStream(INDI::Property prop);
        bool WriteImageFileInternal(const QString &filename, char *buffer, const size_t size);

        bool HasGuideHead { false };
        bool HasCooler { false };
        bool CanCool { false };
        bool HasCoolerControl { false };
        bool HasVideoStream { false };
        bool m_isStreamEnabled { false };
        bool m_FastExposureEnabled { false };
        QString seqPrefix;

        int nextSequenceID { 0 };
        int streamW { 0 };
        int streamH { 0 };
        int normalTabID { -1 };
        int calibrationTabID { -1 };
        int focusTabID { -1 };
        int guideTabID { -1 };
        int alignTabID { -1 };

        INDI::Property primaryCCDBLOB;

        std::unique_ptr<CameraChip> primaryChip;
        std::unique_ptr<CameraChip> guideChip;
        std::unique_ptr<WSMedia> m_Media;
        QString m_EncodingFormat {"FITS"};
        QString m_StreamEncoding {"RAW"};
        QString m_StreamRecording {"SER"};
        QStringList m_EncodingFormats;
        QStringList m_StreamEncodings;
        QStringList m_CaptureFormats;
        QStringList m_VideoFormats;
        bool m_StreamingEnabled {true};
        int m_CaptureFormatIndex {-1};
        TelescopeType telescopeType { TELESCOPE_UNKNOWN };

        enum GainSource
        {
            GAIN_SOURCE_UNKNOWN,
            GAIN_SOURCE_STANDALONE, // e.g., CCD_GAIN property
            GAIN_SOURCE_CONTROLS    // e.g., CCD_CONTROLS property with "gain" label
        };

        enum OffsetSource
        {
            OFFSET_SOURCE_UNKNOWN,
            OFFSET_SOURCE_STANDALONE, // e.g., CCD_OFFSET property
            OFFSET_SOURCE_CONTROLS    // e.g., CCD_CONTROLS property with "offset" label
        };

        // Gain properties
        GainSource m_GainSource { GAIN_SOURCE_UNKNOWN };
        INDI::Property m_GainProperty;
        IPerm m_GainPermission { IP_RO };

        // Offset properties
        OffsetSource m_OffsetSource { OFFSET_SOURCE_UNKNOWN };
        INDI::Property m_OffsetProperty;
        IPerm m_OffsetPermission { IP_RO };

        QPointer<ImageViewer> m_ImageViewerWindow;

        QDateTime m_LastNotificationTS;

        // Typically for DSLRs
        QMap<QString, double> m_ExposurePresets;
        QPair<double, double> m_ExposurePresetsMinMax;

        // Used when writing the image fits file to disk in a separate thread.
        void updateFileBuffer(INDI::Property prop, bool is_fits);
        char *fileWriteBuffer { nullptr };
        int fileWriteBufferSize { 0 };
        QString fileWriteFilename;
        QFuture<void> fileWriteThread;
};
}
