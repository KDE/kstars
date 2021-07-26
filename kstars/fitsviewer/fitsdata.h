/***************************************************************************
                          fitsimage.cpp  -  FITS Image
                             -------------------
    begin                : Tue Feb 24 2004
    copyright            : (C) 2004 by Jasem Mutlaq
    email                : mutlaqja@ikarustech.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Some code fragments were adapted from Peter Kirchgessner's FITS plugin*
 *   See http://members.aol.com/pkirchg for more details.                  *
 ***************************************************************************/

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
        ~FITSData();

        /** Structure to hold FITS Header records */
        typedef struct
        {
            QString key;      /** FITS Header Key */
            QVariant value;   /** FITS Header Value */
            QString comment;  /** FITS Header Comment, if any */
        } Record;

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
         * @param silent If set, error messages are ignored. If set to false, the error message will get displayed in a popup.
         * @return A QFuture that can be watched until the async operation is complete.
         */
        QFuture<bool> loadFromFile(const QString &inFilename, bool silent = true);

        /**
         * @brief loadFITSFromMemory Loading FITS from memory buffer.
         * @param buffer The memory buffer containing the fits data.
         * @param extension file extension (e.g. "jpg", "fits", "cr2"...etc)
         * @param inFilename Set filename metadata, does not load from file.
         * @param silent If set, error messages are ignored. If set to false, the error message will get displayed in a popup.
         * @return bool indicating success or failure.
         */
        bool loadFromBuffer(const QByteArray &buffer, const QString &extension, const QString &inFilename = QString(),
                            bool silent = true);

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
        void calculateStats(bool refresh = false);
        void saveStatistics(FITSImage::Statistic &other);
        void restoreStatistics(FITSImage::Statistic &other);
        FITSImage::Statistic const &getStatistics() const
        {
            return m_Statistics;
        };

        uint16_t width() const
        {
            return m_Statistics.width;
        }
        uint16_t height() const
        {
            return m_Statistics.height;
        }
        int64_t size() const
        {
            return m_Statistics.size;
        }
        int channels() const
        {
            return m_Statistics.channels;
        }
        uint32_t samplesPerChannel() const
        {
            return m_Statistics.samples_per_channel;
        }
        uint32_t dataType() const
        {
            return m_Statistics.dataType;
        }
        double getMin(uint8_t channel = 0) const
        {
            return m_Statistics.min[channel];
        }
        double getMax(uint8_t channel = 0) const
        {
            return m_Statistics.max[channel];
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
        double getStdDev(uint8_t channel = 0) const
        {
            return m_Statistics.stddev[channel];
        }
        double getAverageStdDev() const;
        void setMean(double value, uint8_t channel = 0)
        {
            m_Statistics.mean[channel] = value;
        }
        double getMean(uint8_t channel = 0) const
        {
            return m_Statistics.mean[channel];
        }
        // for single channel, just return the mean for channel zero
        // for color, return the average
        double getAverageMean() const;
        void setMedian(double val, uint8_t channel = 0)
        {
            m_Statistics.median[channel] = val;
        }
        // for single channel, just return the median for channel zero
        // for color, return the average
        double getAverageMedian() const;
        double getMedian(uint8_t channel = 0) const
        {
            return m_Statistics.median[channel];
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
                    break;
                case TSHORT:
                case TUSHORT:
                    return 16;
                    break;
                case TLONG:
                case TULONG:
                case TFLOAT:
                    return 32;
                    break;
                case TLONGLONG:
                case TDOUBLE:
                    return 64;
                    break;
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
        //int findSEPStars(QList<Edge*> &, const QRect &boundary = QRect()) const;

        // Apply ring filter to searched stars
        int filterStars(const float innerRadius, const float outerRadius);

        // Half Flux Radius
        Edge *getSelectedHFRStar() const
        {
            return m_SelectedHFRStar;
        }

        // Calculates the median star eccentricity.
        double getEccentricity();

        double getHFR(HFRType type = HFR_AVERAGE);
        double getHFR(int x, int y);

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

        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        /// World Coordinate System (WCS) Functions.
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        // Check if a particular point exists within the image
        bool contains(const QPointF &point) const;
        // Check if image has valid WCS header information and set HasWCS accordingly. Call in loadFITS()
        bool checkForWCS();
        // Does image have valid WCS?
        bool hasWCS()
        {
            return HasWCS;
        }
        // The WCS can be loaded without pre-computing each pixel's position. This can make certain
        // operations slow. FullWCS() is true if the pixel positions are pre-calculated.
        bool fullWCS()
        {
            return FullWCS;
        }
        // Load WCS data
        bool loadWCS(bool extras = true);
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
             * @brief injectWCS Add WCS keywords to file
             * @param orientation Solver orientation, degrees E of N.
             * @param ra J2000 Right Ascension
             * @param dec J2000 Declination
             * @param pixscale Pixel scale in arcsecs per pixel
             * @param eastToTheRight if true, then when the image is rotated so that north is up, then east would be to the right on the image.
             * @return  True if file is successfully updated with WCS info.
             */
        bool injectWCS(double orientation, double ra, double dec, double pixscale, bool eastToTheRight);

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

        const QVector<uint32_t> &getCumulativeFrequency(uint8_t channel = 0) const
        {
            return m_CumulativeFrequency[channel];
        };
        const QVector<double> &getHistogramIntensity(uint8_t channel = 0) const
        {
            return m_HistogramIntensity[channel];
        }
        const QVector<double> &getHistogramFrequency(uint8_t channel = 0) const
        {
            return m_HistogramFrequency[channel];
        }

        /**
         * @brief getJMIndex Overall contrast of the image used in find centeroid algorithm. i.e. is the image diffuse?
         * @return Value of JMIndex
         */
        double getJMIndex() const
        {
            return m_JMIndex;
        };

        bool isHistogramConstructed()
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
        void findObjectsInImage(SkyPoint startPoint, SkyPoint endPoint);
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

    private:
        void loadCommon(const QString &inFilename);
        /**
         * @brief privateLoad Load an image (FITS, RAW, or images supported by Qt like jpeg, png).
         * @param Buffer pointer to image data. If buffer is emtpy, read from disk (m_Filename).
         * @param silent If true, suppress any messages.
         * @return true if successfully loaded, false otherwise.
         */
        bool privateLoad(const QByteArray &buffer, const QString &extension, bool silent);

        // Load Qt-supported images.
        bool loadCanonicalImage(const QByteArray &buffer, const QString &extension, bool silent);
        // Load FITS images.
        bool loadFITSImage(const QByteArray &buffer, const QString &extension, bool silent);
        // Load RAW images.
        bool loadRAWImage(const QByteArray &buffer, const QString &extension, bool silent);

        void rotWCSFITS(int angle, int mirror);
        void calculateMinMax(bool refresh = false);
        void calculateMedian(bool refresh = false);
        bool checkDebayer();
        void readWCSKeys();

        // Record last FITS error
        void recordLastError(int errorCode);

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
        void calculateMinMax();
        template <typename T>
        void calculateMedian();

        template <typename T>
        QPair<T, T> getParitionMinMax(uint32_t start, uint32_t stride);

        /* Calculate the Gaussian blur matrix and apply it to the image using the convolution filter */
        QVector<double> createGaussianKernel(int size, double sigma);
        template <typename T>
        void convolutionFilter(const QVector<double> &kernel, int kernelSize);
        template <typename T>
        void gaussianBlur(int kernelSize, double sigma);

        /* Calculate running average & standard deviation using Welford’s method for computing variance */
        template <typename T>
        void runningAverageStdDev();
        template <typename T>
        QPair<double, double> getSquaredSumAndMean(uint32_t start, uint32_t stride);

        template <typename T>
        void convertToQImage(double dataMin, double dataMax, double scale, double zero, QImage &image);

        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        /// Private Histogram Functions.
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        template <typename T>  void constructHistogramInternal();

        /// Pointer to CFITSIO FITS file struct
        fitsfile *fptr { nullptr };
        /// Generic data image buffer
        uint8_t *m_ImageBuffer { nullptr };
        /// Above buffer size in bytes
        uint32_t m_ImageBufferSize { 0 };
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
        /// we can initialize wcs without computing all the image positions.
        bool FullWCS { false };
        /// Is the image debayarable?
        bool HasDebayer { false };

        /// Our very own file name
        QString m_Filename, m_compressedFilename;
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
        Edge *m_SelectedHFRStar { nullptr };

        /// Bayer parameters
        BayerParams debayerParams;
        QTemporaryFile m_TemporaryDataFile;

        /// Data type of fits pixel in the image. Used when saving FITS again.
        /// There is bit depth and also data type. They're not the same.
        /// 16bit can be either SHORT_IMG or USHORT_IMG, so m_FITSBITPIX specifies which is
        int m_FITSBITPIX {USHORT_IMG};
        FITSImage::Statistic m_Statistics;

        // A list of header records
        QList<Record> m_HeaderRecords;

        // Sky Background
        SkyBackground m_SkyBackground;

        // Detector Settings
        QVariantMap m_SourceExtractorSettings;

        QFuture<bool> m_StarFindFuture;

        QList<FITSSkyObject *> m_SkyObjects;

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
};
