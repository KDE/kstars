/*
    SPDX-FileCopyrightText: 2004 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Some code fragments were adapted from Peter Kirchgessner's FITS plugin
    SPDX-FileCopyrightText: Peter Kirchgessner <http://members.aol.com/pkirchg>
*/

#pragma once

#include "config-kstars.h"

// From StellarSolver
#ifdef HAVE_STELLARSOLVER
#include <structuredefinitions.h>
#else
#include "structuredefinitions.h"
#endif

#include "kstarsdatetime.h"
#include "bayer.h"
#include "skybackground.h"
#include "fitscommon.h"
#include "fitsstardetector.h"
#include "auxiliary/imagemask.h"

#ifdef WIN32
// This header must be included before fitsio.h to avoid compiler errors with Visual Studio
#include <windows.h>
#endif

#include <fitsio.h>

#include <QFuture>
#include <QObject>
#include <QRect>
#include <QVariant>
#include <QTemporaryFile>

#ifndef KSTARS_LITE
#include <kxmlguiwindow.h>
#ifdef HAVE_WCSLIB
#include <wcs.h>
#endif
#endif

#include "fitsskyobject.h"

class QProgressDialog;

class SkyPoint;
class FITSHistogramData;
class Edge;

class FITSData : public QObject
{
        Q_OBJECT

        // Name of FITS file
        Q_PROPERTY(QString filename READ filename)
        // Extension
        Q_PROPERTY(QString extension READ extension)
        // Size of file in bytes
        Q_PROPERTY(qint64 size READ size)
        // Width in pixels
        Q_PROPERTY(quint16 width READ width)
        // Height in pixels
        Q_PROPERTY(quint16 height READ height)
        // FITS MODE --> Normal, Focus, Guide..etc
        Q_PROPERTY(FITSMode mode MEMBER m_Mode)
        // 1 channel (grayscale) or 3 channels (RGB)
        Q_PROPERTY(quint8 channels READ channels)
        // Bits per pixel
        Q_PROPERTY(quint8 bpp READ bpp)
        // Does FITS have WSC header?
        Q_PROPERTY(bool hasWCS READ hasWCS)
        // Does FITS have bayer data?
        Q_PROPERTY(bool hasDebayer READ hasDebayer)

    public:
        explicit FITSData(FITSMode fitsMode = FITS_NORMAL);
        explicit FITSData(const QSharedPointer<FITSData> &other);
        ~FITSData() override;

        /** Object to hold FITS Header records */
        struct Record
        {
            Record() = default;
            Record(QString k, QString v, QString c) : key(k), value(v), comment(c) {}
            QString key;      /** FITS Header Key */
            QVariant value;   /** FITS Header Value */
            QString comment;  /** FITS Header Comment, if any */
        };

        typedef enum
        {
            Idle,
            Busy,
            Success,
            Failure
        } WCSState;

        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        /// Read and Write file/buffer Functions.
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        /**
         * @brief loadFITS Loading FITS file asynchronously.
         * @param inFilename Path to FITS file (or compressed fits.gz)
         * @return A QFuture that can be watched until the async operation is complete.
         */
        QFuture<bool> loadFromFile(const QString &inFilename);

        /**
         * @brief loadFITSFromMemory Loading FITS from memory buffer.
         * @param buffer The memory buffer containing the fits data.
         * @param extension file extension (e.g. "jpg", "fits", "cr2"...etc)
         * @param inFilename Set filename metadata, does not load from file.
         * @return bool indicating success or failure.
         */
        bool loadFromBuffer(const QByteArray &buffer, const QString &extension, const QString &inFilename = QString());

        /**
         * @brief parseSolution Parse the WCS solution information from the header into the given struct.
         * @param solution Solution structure to fill out.
         * @return True if parsing successful, false otherwise.
         */
        bool parseSolution(FITSImage::Solution &solution) const;

        /* Save FITS or JPG/PNG*/
        bool saveImage(const QString &newFilename);

        // Access functions
        void clearImageBuffers();
        void setImageBuffer(uint8_t *buffer);
        uint8_t const *getImageBuffer() const;
        uint8_t *getWritableImageBuffer();

        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        /// Statistics Functions.
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        // Calculate stats
        void calculateStats(bool refresh = false, bool roi = false);
        void saveStatistics(FITSImage::Statistic &other);
        void restoreStatistics(FITSImage::Statistic &other);
        FITSImage::Statistic const &getStatistics() const
        {
            return m_Statistics;
        }

        uint16_t width(bool roi = false) const
        {
            return roi ?  m_ROIStatistics.width : m_Statistics.width;
        }
        uint16_t height(bool roi = false) const
        {
            return roi ?  m_ROIStatistics.height : m_Statistics.height;
        }
        int64_t size(bool roi = false) const
        {
            return roi ?  m_ROIStatistics.size : m_Statistics.size;
        }
        int channels() const
        {
            return m_Statistics.channels;
        }
        uint32_t samplesPerChannel(bool roi = false) const
        {
            return roi ?  m_ROIStatistics.samples_per_channel : m_Statistics.samples_per_channel;
        }
        uint32_t dataType() const
        {
            return m_Statistics.dataType;
        }
        double getMin(uint8_t channel = 0, bool roi = false) const
        {
            return roi ?  m_ROIStatistics.min[channel] : m_Statistics.min[channel];
        }
        double getMax(uint8_t channel = 0, bool roi = false) const
        {
            return roi ?  m_ROIStatistics.max[channel] : m_Statistics.max[channel];

        }
        void setMinMax(double newMin, double newMax, uint8_t channel = 0);
        void getMinMax(double *min, double *max, uint8_t channel = 0) const
        {
            *min = m_Statistics.min[channel];
            *max = m_Statistics.max[channel];
        }
        void setStdDev(double value, uint8_t channel = 0)
        {
            m_Statistics.stddev[channel] = value;
        }
        double getStdDev(uint8_t channel = 0, bool roi = false ) const
        {
            return roi ? m_ROIStatistics.stddev[channel] :  m_Statistics.stddev[channel];
        }
        double getAverageStdDev(bool roi = false) const;
        void setMean(double value, uint8_t channel = 0)
        {
            m_Statistics.mean[channel] = value;
        }
        double getMean(uint8_t channel = 0, bool roi = false) const
        {
            return roi ? m_ROIStatistics.mean[channel] :  m_Statistics.mean[channel];
        }
        // for single channel, just return the mean for channel zero
        // for color, return the average
        double getAverageMean(bool roi = false) const;
        void setMedian(double val, uint8_t channel = 0)
        {
            m_Statistics.median[channel] = val;
        }
        // for single channel, just return the median for channel zero
        // for color, return the average
        double getAverageMedian(bool roi = false) const;
        double getMedian(uint8_t channel = 0, bool roi = false) const
        {
            return roi ? m_ROIStatistics.median[channel] :  m_Statistics.median[channel];
        }

        int getBytesPerPixel() const
        {
            return m_Statistics.bytesPerPixel;
        }
        void setSNR(double val)
        {
            m_Statistics.SNR = val;
        }
        double getSNR() const
        {
            return m_Statistics.SNR;
        }
        uint32_t bpp() const
        {
            switch(m_Statistics.dataType)
            {
                case TBYTE:
                    return 8;
                case TSHORT:
                case TUSHORT:
                    return 16;
                case TLONG:
                case TULONG:
                case TFLOAT:
                    return 32;
                case TLONGLONG:
                case TDOUBLE:
                    return 64;
                default:
                    return 8;
            }
        }
        double getADU() const;

        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        /// FITS Header Functions.
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        // FITS Record
        bool getRecordValue(const QString &key, QVariant &value) const;
        const QList<Record> &getRecords() const
        {
            return m_HeaderRecords;
        }

        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        /// Star Search & HFR Functions.
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        // Star Detection - Native KStars implementation
        void setStarAlgorithm(StarAlgorithm algorithm)
        {
            starAlgorithm = algorithm;
        }
        int getDetectedStars() const
        {
            return starCenters.count();
        }
        bool areStarsSearched() const
        {
            return starsSearched;
        }
        void appendStar(Edge *newCenter)
        {
            starCenters.append(newCenter);
        }
        const QList<Edge *> &getStarCenters() const
        {
            return starCenters;
        }
        QList<Edge *> getStarCentersInSubFrame(QRect subFrame) const;

        void setStarCenters(const QList<Edge*> &centers)
        {
            qDeleteAll(starCenters);
            starCenters = centers;
        }
        QFuture<bool> findStars(StarAlgorithm algorithm = ALGORITHM_CENTROID, const QRect &trackingBox = QRect());

        void setSkyBackground(const SkyBackground &bg)
        {
            m_SkyBackground = bg;
        }
        const SkyBackground &getSkyBackground() const
        {
            return m_SkyBackground;
        }
        const QVariantMap &getSourceExtractorSettings() const
        {
            return m_SourceExtractorSettings;
        }
        void setSourceExtractorSettings(const QVariantMap &settings)
        {
            m_SourceExtractorSettings = settings;
        }
        // Use SEP (Sextractor Library) to find stars
        template <typename T>
        void getFloatBuffer(float *buffer, int x, int y, int w, int h) const;
        //int findSEPStars(QList<Edge*> &, const int8_t &boundary = int8_t()) const;

        // filter all stars that are visible through the given mask
        int filterStars(QSharedPointer<ImageMask> mask);

        // Half Flux Radius
        const Edge &getSelectedHFRStar() const
        {
            return m_SelectedHFRStar;
        }

        // Calculates the median star eccentricity.
        double getEccentricity();

        double getHFR(HFRType type = HFR_AVERAGE);
        double getHFR(int x, int y, double scale = 1.0);

        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        /// Date & Time (WCS) Functions.
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////

        const KStarsDateTime &getDateTime() const
        {
            return m_DateTime;
        }

        // Set the time, for testing (doesn't set header field)
        void setDateTime(const KStarsDateTime &t)
        {
            m_DateTime = t;
        }

        const QPoint getRoiCenter() const
        {
            return roiCenter;
        }

        void setRoiCenter(QPoint c)
        {
            roiCenter = c;
        }

        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        /// World Coordinate System (WCS) Functions.
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        // Check if a particular point exists within the image
        bool contains(const QPointF &point) const;
        // Does image have valid WCS?
        bool hasWCS()
        {
            return HasWCS;
        }
        // Load WCS data
        bool loadWCS();
        // Get WCS State
        WCSState getWCSState() const
        {
            return m_WCSState;
        }

        /**
             * @brief wcsToPixel Given J2000 (RA0,DE0) coordinates. Find in the image the corresponding pixel coordinates.
             * @param wcsCoord Coordinates of target
             * @param wcsPixelPoint Return XY FITS coordinates
             * @param wcsImagePoint Return XY Image coordinates
             * @return True if conversion is successful, false otherwise.
             */
        bool wcsToPixel(const SkyPoint &wcsCoord, QPointF &wcsPixelPoint, QPointF &wcsImagePoint);

        /**
             * @brief pixelToWCS Convert Pixel coordinates to J2000 world coordinates
             * @param wcsPixelPoint Pixel coordinates in XY Image space.
             * @param wcsCoord Store back WCS world coordinate in wcsCoord
             * @return True if successful, false otherwise.
             */
        bool pixelToWCS(const QPointF &wcsPixelPoint, SkyPoint &wcsCoord);

        /**
             * @brief injectWCS Add WCS keywords
             * @param orientation Solver orientation, degrees E of N.
             * @param ra J2000 Right Ascension
             * @param dec J2000 Declination
             * @param pixscale Pixel scale in arcsecs per pixel
             * @param eastToTheRight if true, then when the image is rotated so that north is up, then east would be to the right on the image.
             */
        void injectWCS(double orientation, double ra, double dec, double pixscale, bool eastToTheRight);

        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        /// Debayering Functions
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////

        // Debayer
        bool hasDebayer()
        {
            return HasDebayer;
        }

        /**
         * @brief debayer the 1-channel data to 3-channel RGB using the default debayer pattern detected in the FITS header.
         * @param reload If true, it will read the image again from disk before performing debayering. This is necessary to attempt
         * subsequent debayering processes on an already debayered image.
         */
        bool debayer(bool reload = false);
        bool debayer_8bit();
        bool debayer_16bit();
        void getBayerParams(BayerParams *param);
        void setBayerParams(BayerParams *param);

        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        /// Public Histogram Functions
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////

        void resetHistogram()
        {
            m_HistogramConstructed = false;
        }
        double getHistogramBinWidth(int channel = 0)
        {
            return m_HistogramBinWidth[channel];
        }

        // Returns a vector with the counts (y-axis values) for the histogram.
        const QVector<uint32_t> &getCumulativeFrequency(uint8_t channel = 0) const
        {
            return m_CumulativeFrequency[channel];
        }
        // Returns a vector with the values (x-axis values) for the histogram.
        // The value returned is the low end of the histogram interval.
        // The high end is this intensity plus the value returned by getHistogramBinWidth().
        const QVector<double> &getHistogramIntensity(uint8_t channel = 0) const
        {
            return m_HistogramIntensity[channel];
        }
        const QVector<double> &getHistogramFrequency(uint8_t channel = 0) const
        {
            return m_HistogramFrequency[channel];
        }
        int getHistogramBinCount() const
        {
            return m_HistogramBinCount;
        }

        // Returns the histogram bin for the pixel at location x,y in the given channel.
        int32_t histogramBin(int x, int y, int channel) const;

        /**
         * @brief getJMIndex Overall contrast of the image used in find centeroid algorithm. i.e. is the image diffuse?
         * @return Value of JMIndex
         */
        double getJMIndex() const
        {
            return m_JMIndex;
        }

        bool isHistogramConstructed() const
        {
            return m_HistogramConstructed;
        }
        void constructHistogram();

        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        /// Filters and Rotations Functions.
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        // Filter
        void applyFilter(FITSScale type, uint8_t *image = nullptr, QVector<double> *targetMin = nullptr,
                         QVector<double> *targetMax = nullptr);

        // Rotation counter. We keep count to rotate WCS keywords on save
        int getRotCounter() const;
        void setRotCounter(int value);

        // Filename
        const QString &filename() const
        {
            return m_Filename;
        }
        const QString &compressedFilename() const
        {
            return m_compressedFilename;
        }
        bool isCompressed() const
        {
            return m_isCompressed;
        }
        // Extension
        const QString &extension() const
        {
            return m_Extension;
        }

        // Horizontal flip counter. We keep count to rotate WCS keywords on save
        int getFlipHCounter() const;
        void setFlipHCounter(int value);

        // Horizontal flip counter. We keep count to rotate WCS keywords on save
        int getFlipVCounter() const;
        void setFlipVCounter(int value);

        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        /// Object Search Functions.
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
#ifndef KSTARS_LITE
#ifdef HAVE_WCSLIB
        bool searchObjects();
        bool findObjectsInImage(SkyPoint startPoint, SkyPoint endPoint);
        bool findWCSBounds(double &minRA, double &maxRA, double &minDec, double &maxDec);
#endif
#endif
        const QList<FITSSkyObject *> &getSkyObjects() const
        {
            return m_SkyObjects;
        }

        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        /// Image Conversion Functions.
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        // Create autostretch image from FITS File
        static QImage FITSToImage(const QString &filename);

        /**
         * @brief ImageToFITS Convert an image file with supported format to a FITS file.
         * @param filename full path to filename without extension
         * @param format file extension. Supported formats are whatever supported by Qt (e.g. PNG, JPG,..etc)
         * @param output Output temporary file path. The created file is generated by the function and store in output.
         * @return True if conversion is successful, false otherwise.
         */
        static bool ImageToFITS(const QString &filename, const QString &format, QString &output);

        QString getLastError() const;

        static bool readableFilename(const QString &filename);

    signals:
        void converted(QImage);

        /**
         * @brief histogramReady Sends signal when histogram construction is complete.
         */
        void histogramReady();

        /**
         * @brief dataChanged Send signal when undelying raw data buffer data changed.
         */
        void dataChanged();
    public slots:
        void makeRoiBuffer(QRect roi);

    private:
        void loadCommon(const QString &inFilename);
        /**
         * @brief privateLoad Load an image (FITS, RAW, or images supported by Qt like jpeg, png).
         * @param Buffer pointer to image data. If buffer is emtpy, read from disk (m_Filename).
         * @return true if successfully loaded, false otherwise.
         */
        bool privateLoad(const QByteArray &buffer);

        // Load Qt-supported images.
        bool loadCanonicalImage(const QByteArray &buffer);
        // Load FITS images.
        bool loadFITSImage(const QByteArray &buffer, const bool isCompressed = false);
        // Load XISF images.
        bool loadXISFImage(const QByteArray &buffer);
        // Save XISF images.
        bool saveXISFImage(const QString &newFilename);
        // Load RAW images.
        bool loadRAWImage(const QByteArray &buffer);

        void rotWCSFITS(int angle, int mirror);
        void calculateMinMax(bool refresh = false, bool roi = false);
        void calculateMedian(bool refresh = false, bool roi = false);
        bool checkDebayer();
        void readWCSKeys();

        // Record last FITS error
        void recordLastError(int errorCode);
        void logOOMError(uint32_t requiredMemory = 0);

        // FITS Record
        bool parseHeader();
        //int getFITSRecord(QString &recordList, int &nkeys);

        // Templated functions
        template <typename T>
        bool debayer();

        template <typename T>
        bool rotFITS(int rotate, int mirror);

        // Apply Filter
        template <typename T>
        void applyFilter(FITSScale type, uint8_t *targetImage, QVector<double> * min = nullptr, QVector<double> * max = nullptr);

        template <typename T>
        void calculateMinMax(bool roi = false);
        template <typename T>
        void calculateMedian(bool roi = false);

        template <typename T>
        QPair<T, T> getParitionMinMax(uint32_t start, uint32_t stride, bool roi);

        /* Calculate the Gaussian blur matrix and apply it to the image using the convolution filter */
        QVector<double> createGaussianKernel(int size, double sigma);
        template <typename T>
        void convolutionFilter(const QVector<double> &kernel, int kernelSize);
        template <typename T>
        void gaussianBlur(int kernelSize, double sigma);

        /* Calculate running average & standard deviation using Welford’s method for computing variance */
        template <typename T>
        void runningAverageStdDev( bool roi = false );
        template <typename T>
        QPair<double, double> getSquaredSumAndMean(uint32_t start, uint32_t stride, bool roi = false);

        template <typename T>
        void convertToQImage(double dataMin, double dataMax, double scale, double zero, QImage &image);

        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        /// Private Histogram Functions.
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        template <typename T>  void constructHistogramInternal();
        template <typename T> int32_t histogramBinInternal(T value, int channel) const;
        template <typename T> int32_t histogramBinInternal(int x, int y, int channel) const;

        /// Pointer to CFITSIO FITS file struct
        fitsfile *fptr { nullptr };
        /// Generic data image buffer
        uint8_t *m_ImageBuffer { nullptr };
        /// Above buffer size in bytes
        uint32_t m_ImageBufferSize { 0 };
        /// Image Buffer if Selection is to be done
        uint8_t *m_ImageRoiBuffer { nullptr };
        /// Above buffer size in bytes
        uint32_t m_ImageRoiBufferSize { 0 };
        /// Is this a temporary file or one loaded from disk?
        bool m_isTemporary { false };
        /// is this file compress (.fits.fz)?
        bool m_isCompressed { false };
        /// Did we search for stars yet?
        bool starsSearched { false };
        ///Star Selection Algorithm
        StarAlgorithm starAlgorithm { ALGORITHM_GRADIENT };
        /// Do we have WCS keywords in this FITS data?
        bool HasWCS { false };        /// Do we have WCS keywords in this FITS data?
        /// Is the image debayarable?
        bool HasDebayer { false };
        /// Buffer to hold fpack uncompressed data
        uint8_t *m_PackBuffer {nullptr};

        /// Our very own file name
        QString m_Filename, m_compressedFilename, m_Extension;
        /// FITS Mode (Normal, WCS, Guide, Focus..etc)
        FITSMode m_Mode;
        // FITS Observed UTC date time
        KStarsDateTime m_DateTime;

        /// How many times the image was rotated? Useful for WCS keywords rotation on save.
        int rotCounter { 0 };
        /// How many times the image was flipped horizontally?
        int flipHCounter { 0 };
        /// How many times the image was flipped vertically?
        int flipVCounter { 0 };

        /// WCS Struct
        struct wcsprm *m_WCSHandle
        {
            nullptr
        };
        /// Number of coordinate representations found.
        int m_nwcs {0};
        WCSState m_WCSState { Idle };
        /// All the stars we detected, if any.
        QList<Edge *> starCenters;
        QList<Edge *> localStarCenters;
        /// The biggest fattest star in the image.
        Edge m_SelectedHFRStar;

        /// Bayer parameters
        BayerParams debayerParams;
        QTemporaryFile m_TemporaryDataFile;

        /// Data type of fits pixel in the image. Used when saving FITS again.
        /// There is bit depth and also data type. They're not the same.
        /// 16bit can be either SHORT_IMG or USHORT_IMG, so m_FITSBITPIX specifies which is
        int m_FITSBITPIX {USHORT_IMG};
        FITSImage::Statistic m_Statistics;
        FITSImage::Statistic m_ROIStatistics;

        // A list of header records
        QList<Record> m_HeaderRecords;

        QList<FITSSkyObject *> m_SkyObjects;
        bool m_ObjectsSearched {false};

        QString m_LastError;

        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        /// Histogram Variables
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        QVector<QVector<uint32_t>> m_CumulativeFrequency;
        QVector<QVector<double>> m_HistogramIntensity;
        QVector<QVector<double>> m_HistogramFrequency;
        QVector<double> m_HistogramBinWidth;
        uint16_t m_HistogramBinCount { 0 };
        double m_JMIndex { 1 };
        bool m_HistogramConstructed { false };

        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        /// Star Detector
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        // Sky Background
        SkyBackground m_SkyBackground;
        // Detector Settings
        QVariantMap m_SourceExtractorSettings;
        QFuture<bool> m_StarFindFuture;
        QScopedPointer<FITSStarDetector, QScopedPointerDeleteLater> m_StarDetector;

        // Cached values for hfr and eccentricity computations
        double cacheHFR { -1 };
        HFRType cacheHFRType { HFR_AVERAGE };
        double cacheEccentricity { -1 };
        QPoint roiCenter;
};
