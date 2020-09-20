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

#include "kstarsdatetime.h"
#include "bayer.h"
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

#ifndef KSTARS_LITE
#include <kxmlguiwindow.h>
#ifdef HAVE_WCSLIB
#include <wcs.h>
#endif
#endif

#include "fitsskyobject.h"

class QProgressDialog;

class SkyPoint;
class FITSHistogram;

typedef struct
{
    float ra;
    float dec;
} wcs_point;

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
        // Data type (BYTE, SHORT, INT..etc)
        Q_PROPERTY(quint32 dataType MEMBER m_DataType)
        // Bits per pixel
        Q_PROPERTY(quint8 bpp READ bpp WRITE setBPP)
        // Does FITS have WSC header?
        Q_PROPERTY(bool hasWCS READ hasWCS)
        // Does FITS have bayer data?
        Q_PROPERTY(bool hasDebyaer READ hasDebayer)

    public:
        explicit FITSData(FITSMode fitsMode = FITS_NORMAL);
        explicit FITSData(const FITSData *other);
        ~FITSData();

        /** Structure to hold FITS Header records */
        typedef struct
        {
            QString key;      /** FITS Header Key */
            QVariant value;   /** FITS Header Value */
            QString comment;  /** FITS Header Comment, if any */
        } Record;

        /// Stats struct to hold statisical data about the FITS data
        typedef struct
        {
            double min[3] = {0}, max[3] = {0};
            double mean[3] = {0};
            double stddev[3] = {0};
            double median[3] = {0};
            double SNR { 0 };
            int bitpix { 8 };
            int bytesPerPixel { 1 };
            int ndim { 2 };
            int64_t size { 0 };
            uint32_t samples_per_channel { 0 };
            uint16_t width { 0 };
            uint16_t height { 0 };
        } Statistic;

        /**
         * @brief loadFITS Loading FITS file asynchronously.
         * @param inFilename Path to FITS file (or compressed fits.gz)
         * @param silent If set, error messages are ignored. If set to false, the error message will get displayed in a popup.
         * @return A QFuture that can be watched until the async operation is complete.
         */
        QFuture<bool> loadFITS(const QString &inFilename, bool silent = true);

        /**
         * @brief loadFITSFromMemory Loading FITS from memory buffer.
         * @param inFilename Potential future path to FITS file (or compressed fits.gz), stored in a fitsdata class variable
         * @param fits_buffer The memory buffer containing the fits data.
         * @param fits_buffer_size The size in bytes of the buffer.
         * @param silent If set, error messages are ignored. If set to false, the error message will get displayed in a popup.
         * @return bool indicating success or failure.
         */
        bool loadFITSFromMemory(const QString &inFilename, void *fits_buffer,
                                size_t fits_buffer_size, bool silent);
        /* Save FITS or JPG/PNG*/
        bool saveImage(const QString &newFilename);
        /* Rescale image lineary from image_buffer, fit to window if desired */
        int rescale(FITSZoom type);
        /* Calculate stats */
        void calculateStats(bool refresh = false);
        /* Check if a particular point exists within the image */
        bool contains(const QPointF &point) const;

        // Access functions
        void clearImageBuffers();
        void setImageBuffer(uint8_t *buffer);
        uint8_t const *getImageBuffer() const;
        uint8_t *getWritableImageBuffer();

        // Statistics
        void saveStatistics(Statistic &other);
        void restoreStatistics(Statistic &other);
        Statistic const &getStatistics() const
        {
            return stats;
        };

        uint16_t width() const
        {
            return stats.width;
        }
        uint16_t height() const
        {
            return stats.height;
        }
        int64_t size() const
        {
            return stats.size;
        }
        int channels() const
        {
            return m_Channels;
        }
        double getMin(uint8_t channel = 0) const
        {
            return stats.min[channel];
        }
        double getMax(uint8_t channel = 0) const
        {
            return stats.max[channel];
        }
        void setMinMax(double newMin, double newMax, uint8_t channel = 0);
        void getMinMax(double *min, double *max, uint8_t channel = 0) const
        {
            *min = stats.min[channel];
            *max = stats.max[channel];
        }
        void setStdDev(double value, uint8_t channel = 0)
        {
            stats.stddev[channel] = value;
        }
        double getStdDev(uint8_t channel = 0) const
        {
            return stats.stddev[channel];
        }
        void setMean(double value, uint8_t channel = 0)
        {
            stats.mean[channel] = value;
        }
        double getMean(uint8_t channel = 0) const
        {
            return stats.mean[channel];
        }
        void setMedian(double val, uint8_t channel = 0)
        {
            stats.median[channel] = val;
        }
        double getMedian(uint8_t channel = 0) const
        {
            return stats.median[channel];
        }

        int getBytesPerPixel() const
        {
            return stats.bytesPerPixel;
        }
        void setSNR(double val)
        {
            stats.SNR = val;
        }
        double getSNR() const
        {
            return stats.SNR;
        }
        void setBPP(uint8_t value)
        {
            stats.bitpix = value;
        }
        uint32_t bpp() const
        {
            return stats.bitpix;
        }
        double getADU() const;

        // FITS Record
        bool getRecordValue(const QString &key, QVariant &value) const;
        const QList<Record*> &getRecords() const
        {
            return records;
        }

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
        QList<Edge *> getStarCenters() const
        {
            return starCenters;
        }
        QList<Edge *> getStarCentersInSubFrame(QRect subFrame) const;

        int findStars(StarAlgorithm algorithm = ALGORITHM_CENTROID, const QRect &trackingBox = QRect());

        // Use SEP (Sextractor Library) to find stars
        template <typename T>
        void getFloatBuffer(float *buffer, int x, int y, int w, int h) const;
        int findSEPStars(QList<Edge*> &, const QRect &boundary = QRect()) const;

        // Apply ring filter to searched stars
        int filterStars(const float innerRadius, const float outerRadius);

        // Half Flux Radius
        Edge *getMaxHFRStar() const
        {
            return maxHFRStar;
        }
        double getHFR(HFRType type = HFR_AVERAGE);
        double getHFR(int x, int y);

        const KStarsDateTime &getDateTime() const
        {
            return m_DateTime;
        }

        // WCS
        // Check if image has valid WCS header information and set HasWCS accordingly. Call in loadFITS()
        bool checkForWCS();
        // Does image have valid WCS?
        bool hasWCS()
        {
            return HasWCS;
        }
        // Load WCS data
        bool loadWCS();
        // Is WCS Image loaded?
        bool isWCSLoaded()
        {
            return WCSLoaded;
        }

        wcs_point *getWCSCoord()
        {
            return wcs_coord;
        }

        /**
             * @brief wcsToPixel Given J2000 (RA0,DE0) coordinates. Find in the image the corresponding pixel coordinates.
             * @param wcsCoord Coordinates of target
             * @param wcsPixelPoint Return XY FITS coordinates
             * @param wcsImagePoint Return XY Image coordinates
             * @return True if conversion is successful, false otherwise.
             */
        bool wcsToPixel(SkyPoint &wcsCoord, QPointF &wcsPixelPoint, QPointF &wcsImagePoint);

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
             * @return  True if file is successfully updated with WCS info.
             */
        bool injectWCS(double orientation, double ra, double dec, double pixscale);

        // Debayer
        bool hasDebayer()
        {
            return HasDebayer;
        }
        bool debayer();
        bool debayer_8bit();
        bool debayer_16bit();
        void getBayerParams(BayerParams *param);
        void setBayerParams(BayerParams *param);

        // Histogram
#ifndef KSTARS_LITE
        void setHistogram(FITSHistogram *inHistogram)
        {
            histogram = inHistogram;
        }
#endif

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
        bool isTempFile() const
        {
            return m_isTemporary;
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

#ifndef KSTARS_LITE
#ifdef HAVE_WCSLIB
        void findObjectsInImage(double world[], double phi, double theta, double imgcrd[], double pixcrd[], int stat[]);
#endif
#endif
        QList<FITSSkyObject *> getSkyObjects();
        QList<FITSSkyObject *> objList; //Does this need to be public??

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

        bool getAutoRemoveTemporaryFITS() const;
        void setAutoRemoveTemporaryFITS(bool value);

        QString getLastError() const;

    signals:
        void converted(QImage);

    private:
        void loadCommon(const QString &inFilename);
        bool privateLoad(void *fits_buffer, size_t fits_buffer_size, bool silent);
        void rotWCSFITS(int angle, int mirror);
        int calculateMinMax(bool refresh = false);
        bool checkDebayer();
        void readWCSKeys();

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

#ifndef KSTARS_LITE
        FITSHistogram *histogram { nullptr }; // Pointer to the FITS data histogram
#endif
        /// Pointer to CFITSIO FITS file struct
        fitsfile *fptr { nullptr };

        /// FITS image data type (TBYTE, TUSHORT, TINT, TFLOAT, TLONG, TDOUBLE)
        uint32_t m_DataType { 0 };
        /// Number of channels
        uint8_t m_Channels { 1 };
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
        bool HasWCS { false };
        /// Is the image debayarable?
        bool HasDebayer { false };
        /// Is WCS data loaded?
        bool WCSLoaded { false };

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

        /// Pointer to WCS coordinate data, if any.
        wcs_point *wcs_coord { nullptr };
        /// WCS Struct
        struct wcsprm *m_wcs
        {
            nullptr
        };
        int m_nwcs = 0;
        /// All the stars we detected, if any.
        QList<Edge *> starCenters;
        QList<Edge *> localStarCenters;
        /// The biggest fattest star in the image.
        Edge *maxHFRStar { nullptr };

        //uint8_t *m_BayerBuffer { nullptr };
        /// Bayer parameters
        BayerParams debayerParams;

        Statistic stats;

        // A list of header records
        QList<Record*> records;

        /// Remove temporary files after closing
        bool autoRemoveTemporaryFITS { true };

        QString lastError;

        static const QString m_TemporaryPath;
};
