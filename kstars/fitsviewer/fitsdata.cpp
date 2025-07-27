/*
    SPDX-FileCopyrightText: 2004 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Some code fragments were adapted from Peter Kirchgessner's FITS plugin
    SPDX-FileCopyrightText: Peter Kirchgessner <http://members.aol.com/pkirchg>
*/

#include "fitsdata.h"
#include "fitsbahtinovdetector.h"
#include "fitsthresholddetector.h"
#include "fitsgradientdetector.h"
#include "fitscentroiddetector.h"
#include "fitssepdetector.h"

#include "fpack.h"

#include "kstarsdata.h"
#include "ksutils.h"
#include "kspaths.h"
#include "Options.h"
#include "skymapcomposite.h"
#include "auxiliary/ksnotification.h"
#include "auxiliary/robuststatistics.h"

#include <KFormat>
#include <QApplication>
#include <QImage>
#include <QtConcurrent>
#include <QImageReader>
#include <QUrl>
#include <QNetworkAccessManager>

#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)
#include <wcshdr.h>
#include <wcsfix.h>
#endif

#if !defined(KSTARS_LITE) && defined(HAVE_LIBRAW)
#include <libraw/libraw.h>
#endif

#ifdef HAVE_XISF
#include <libxisf.h>
#endif

#include <cfloat>
#include <cmath>

#include <fits_debug.h>

#define ZOOM_DEFAULT   100.0
#define ZOOM_MIN       10
#define ZOOM_MAX       400
#define ZOOM_LOW_INCR  10
#define ZOOM_HIGH_INCR 50

QString getTemporaryPath()
{
    return QDir(KSPaths::writableLocation(QStandardPaths::TempLocation) + "/" +
                qAppName()).path();
}

const QStringList RAWFormats = { "cr2", "cr3", "crw", "nef", "raf", "dng", "arw", "orf" };

bool FITSData::readableFilename(const QString &filename)
{
    QFileInfo info(filename);
    QString extension = info.completeSuffix().toLower();
    return extension.contains("fit") || extension.contains("fz") ||
           extension.contains("xisf") ||
           QImageReader::supportedImageFormats().contains(extension.toLatin1()) ||
           RAWFormats.contains(extension);
}

FITSData::FITSData(FITSMode fitsMode): m_Mode(fitsMode)
{
    static const QRegularExpression re("[-{}]");

    qRegisterMetaType<FITSMode>("FITSMode");

    debayerParams.method  = DC1394_BAYER_METHOD_NEAREST;
    debayerParams.filter  = DC1394_COLOR_FILTER_RGGB;
    debayerParams.offsetX = debayerParams.offsetY = 0;

    // Reserve 3 channels
    m_CumulativeFrequency.resize(3);
    m_HistogramBinWidth.resize(3);
    m_HistogramFrequency.resize(3);
    m_HistogramIntensity.resize(3);

    // Set UUID for each view
    QString uuid = QUuid::createUuid().toString();
    uuid = uuid.remove(re);
    setObjectName(uuid);
}

FITSData::FITSData(const QSharedPointer<FITSData> &other)
{
    static const QRegularExpression re("[-{}]");
    qRegisterMetaType<FITSMode>("FITSMode");

    debayerParams.method  = DC1394_BAYER_METHOD_NEAREST;
    debayerParams.filter  = DC1394_COLOR_FILTER_RGGB;
    debayerParams.offsetX = debayerParams.offsetY = 0;

    m_TemporaryDataFile.setFileTemplate("fits_memory_XXXXXX");

    this->m_Mode = other->m_Mode;
    this->m_Statistics.channels = other->m_Statistics.channels;
    memcpy(&m_Statistics, &(other->m_Statistics), sizeof(m_Statistics));
    m_ImageBuffer = new uint8_t[m_Statistics.samples_per_channel * m_Statistics.channels * m_Statistics.bytesPerPixel];
    memcpy(m_ImageBuffer, other->m_ImageBuffer,
           m_Statistics.samples_per_channel * m_Statistics.channels * m_Statistics.bytesPerPixel);

    // Set UUID for each view
    QString uuid = QUuid::createUuid().toString();
    uuid = uuid.remove(re);
    setObjectName(uuid);
}

FITSData::~FITSData()
{
    int status = 0;

    if (m_StarFindFuture.isRunning())
        m_StarFindFuture.waitForFinished();

    clearImageBuffers();

#ifdef HAVE_WCSLIB
    if (m_WCSHandle != nullptr)
    {
        wcsvfree(&m_nwcs, &m_WCSHandle);
        m_WCSHandle = nullptr;
        m_nwcs = 0;
    }
#endif

    if (starCenters.count() > 0)
        qDeleteAll(starCenters);
    starCenters.clear();

    if (m_SkyObjects.count() > 0)
        qDeleteAll(m_SkyObjects);
    m_SkyObjects.clear();

    m_CatObjects.clear();

    if (fptr != nullptr)
    {
        fits_flush_file(fptr, &status);
        fits_close_file(fptr, &status);
        free(m_PackBuffer);
        m_PackBuffer = nullptr;
        fptr = nullptr;
    }

    // Live Stacking
    if (m_StackImageBuffer != nullptr)
    {
        delete[] m_StackImageBuffer;
        m_StackImageBuffer = nullptr;
        m_StackImageBufferSize = 0;
    }
#ifdef HAVE_WCSLIB
    if (m_StackWCSHandle != nullptr)
    {
        wcsvfree(&m_Stacknwcs, &m_StackWCSHandle);
        m_StackWCSHandle = nullptr;
        m_Stacknwcs = 0;
    }
#endif
    if (m_Stackfptr != nullptr)
    {
        status = 0;
        fits_flush_file(m_Stackfptr, &status);
        status = 0;
        fits_close_file(m_Stackfptr, &status);
        m_Stackfptr = nullptr;
    }
    m_StackFITSWatcher.waitForFinished();
    m_StackWatcher.waitForFinished();
}

void FITSData::loadCommon(const QString &inFilename)
{
    int status = 0;
    qDeleteAll(starCenters);
    starCenters.clear();

    if (fptr != nullptr)
    {
        fits_flush_file(fptr, &status);
        fits_close_file(fptr, &status);
        free(m_PackBuffer);
        m_PackBuffer = nullptr;
        fptr = nullptr;
    }

    m_Filename = inFilename;
}

bool FITSData::loadFromBuffer(const QByteArray &buffer)
{
    loadCommon("");
    qCDebug(KSTARS_FITS) << "Reading file buffer (" << KFormat().formatByteSize(buffer.size()) << ")";
    return privateLoad(buffer);
}

QFuture<bool> FITSData::loadFromFile(const QString &inFilename)
{
    loadCommon(inFilename);
    QFileInfo info(m_Filename);
    m_Extension = info.completeSuffix().toLower();
    qCDebug(KSTARS_FITS) << "Loading file " << m_Filename;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QtConcurrent::run(&FITSData::privateLoad, this, QByteArray());
#else
    return QtConcurrent::run(this, &FITSData::privateLoad, QByteArray());
#endif
}

#if !defined (KSTARS_LITE) && defined (HAVE_WCSLIB) && defined (HAVE_OPENCV)
bool FITSData::loadStack(const QString &inDir, const LiveStackData &params)
{
    m_StackDir = inDir;
    m_Stack.reset(new FITSStack(this, params));
    connect(m_Stack.get(), &FITSStack::stackChanged, this, [this]()
    {
        QByteArray buffer = m_Stack->getStackedImage();
        loadFromBuffer(buffer);
        emit dataChanged();
        emit stackReady();
    });

    // Clear the work queue
    m_StackQ.clear();

    // Setup directory watcher on the stack directory
    m_StackDirWatcher.reset(new FITSDirWatcher(this));
    connect(m_StackDirWatcher.get(), &FITSDirWatcher::newFilesDetected, this, &FITSData::newStackSubs);

    m_StackDirWatcher->watchDir(m_StackDir);
    QStringList subs = m_StackDirWatcher->getCurrentFiles();

    auto stackData = m_Stack->getStackData();

    // Make sure the future watchers aren't still active
    m_StackWatcher.waitForFinished();
    m_StackFITSWatcher.waitForFinished();
    connect(&m_StackWatcher, &QFutureWatcher<bool>::finished, this, &FITSData::stackProcessDone);
    connect(&m_StackFITSWatcher, &QFutureWatcher<bool>::finished, this, &FITSData::stackFITSLoaded);

    // Choose and alignment master if we can
    m_AlignMasterChosen = true;
    QString alignMaster = stackData.alignMaster;
    if (alignMaster.isEmpty())
    {
        // No align master chosen so use the first sub
        if (subs.size() > 0)
            emit alignMasterChosen(subs[0]);
        else
            m_AlignMasterChosen = false;
    }
    else
    {
        // An alignment master has been specified so make this the first sub to be processed
        int pos = subs.indexOf(alignMaster);
        if (pos >= 0 && pos < subs.size())
            // Align Master is in the directory of subs to be processed so remove it from its current position
            subs.takeAt(pos);
        subs.insert(0, alignMaster);
    }

    int subsToProcess = m_Stack->getStackData().numInMem;
    for (int i = 0; i < subs.size(); i++)
    {
        // Process the first subsToProcess and put the rest on a Q for processing later
        if (i < subsToProcess)
            m_StackSubs.push_back(subs[i]);
        else
            m_StackQ.enqueue(subs[i]);
    }

    if (m_StackSubs.size() == 0)
    {
        // No subs in the selected directory
        emit stackReady();
        return true;
    }

    // Set the control variables
    m_StackSubPos = -1;
    m_Stack->setInitalStackDone(false);
    m_Stack->setStackInProgress(true);
    m_DarkLoaded = false;
    m_FlatLoaded = false;
    m_CancelRequest = false;
    setStackSubSolution(0.0, 0.0, 0.0, -1, -1);
    nextStackAction();
    return true;
}

// Called when user requested to cancel the in-flight stacking operation
void FITSData::cancelStack()
{
    // If there is a stack in progress then no file processing will be happening
    m_CancelRequest = !m_Stack->getStackInProgress();

    if (m_StackFITSWatcher.isRunning())
    {
        m_StackFITSWatcherCancel = true;
        m_StackFITSWatcher.cancel();
        qCDebug(KSTARS_FITS) << "Cancelling stack file loading thread...";
    }

    if (m_StackWatcher.isRunning())
    {
        m_StackWatcherCancel = true;
        m_StackWatcher.cancel();
        qCDebug(KSTARS_FITS) << "Cancelling stacking threads...";
    }
}

// Monitor progress of the user's cancel stack request. When all 3 phases are done
// update other objects
void FITSData::checkCancelStack()
{
    if (!m_CancelRequest && !m_StackFITSWatcherCancel && !m_StackWatcherCancel)
    {
        qCDebug(KSTARS_FITS) << "Cancel stack request completed";

        // Stop watching for more files
        disconnect(m_StackDirWatcher.get(), &FITSDirWatcher::newFilesDetected, this, &FITSData::newStackSubs);
        m_StackDirWatcher->stopWatching();

        // Reset the image to noimage
        m_Stack->resetStackedImage();

        // Drain any subs in the work Q
        m_StackQ.clear();

        emit stackReady();
    }
}

// Called when 1 (or more) new files added to the watched stack directory
void FITSData::newStackSubs(const QStringList &newFiles)
{
    for (const QString &file : newFiles)
    {
        qCDebug(KSTARS_FITS) << file;
        m_StackQ.enqueue(file);
    }
    incrementalStack();
}

// Add 1 or more new subs to an existing stack
void FITSData::incrementalStack()
{
    // Check whether a user cancel request has been received
    if (m_CancelRequest)
    {
        m_CancelRequest = false;
        checkCancelStack();
        return;
    }

    if (m_StackQ.isEmpty())
        // Nothing to do
        return;

    // If processing of other subs is still in progress we must wait for it to complete
    bool hasUnprocessedSubs = !m_StackSubs.empty() && m_StackSubPos < m_StackSubs.size();
    if (m_Stack->getStackInProgress() || hasUnprocessedSubs)
        return;

    m_Stack->setStackInProgress(true);
    emit stackInProgress();
    int subsToProcess = m_Stack->getStackData().numInMem;
    m_StackSubs.clear();
    m_StackSubPos = -1;
    for (int i = 0; i < subsToProcess; i++)
    {
        m_StackSubs.push_back(m_StackQ.dequeue());
        if (m_StackQ.isEmpty())
            break;
    }
    nextStackAction();
}

QFuture<bool> FITSData::loadStackBuffer()
{
    m_Extension = "fits";
    QByteArray buffer = m_Stack->getStackedImage();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QtConcurrent::run(&FITSData::loadFromBuffer, this, buffer);
#else
    return QtConcurrent::run(this, &FITSData::loadFromBuffer, buffer);
#endif
}

bool FITSData::processNextSub(QString &sub)
{
    m_Stack->setupNextSub();
    m_StackFITSAsync = stackFITSSub;
    qCDebug(KSTARS_FITS) << "Loading sub" << sub;

    // Lambda to load the sub in the background
    QFuture<bool> future = QtConcurrent::run([this, sub]() -> bool
    {
        bool ok = stackLoadFITSImage(sub, false);
        if (!ok)
            qCDebug(KSTARS_FITS) << QString("Unable to load sub %1").arg(sub);
        else
        {
            bool plateSolving = (m_Stack->getStackData().alignMethod == LS_ALIGNMENT_PLATE_SOLVE);
            if (plateSolving && m_StackSubIndex <= 0)
            {
                // 1st time solving, or solving had a problem so use WCS from sub header
                if ((ok = stackLoadWCS()))
                    setStackSubSolution(m_StackWCSHandle->crval[0], m_StackWCSHandle->crval[1],
                                        std::fabs(m_StackWCSHandle->cdelt[0]) * 3600.0, -1, -1);
            }

            if (ok)
                ok = m_Stack->addSub((void *) m_StackImageBuffer, m_StackStatistics.cvType,
                                     m_StackStatistics.stats.width, m_StackStatistics.stats.height,
                                     m_StackStatistics.stats.bytesPerPixel);
        }
        return ok;
    });

    m_StackFITSWatcher.setFuture(future);
    return true;
}

void FITSData::processMasters()
{
    // Dark
    if (!m_DarkLoaded)
    {
        QString dark = m_Stack->getStackData().masterDark;
        if (dark.isEmpty())
            m_DarkLoaded = true;
        else
        {
            m_StackFITSAsync = stackFITSDark;
            qCDebug(KSTARS_FITS) << "Loading master dark" << dark;

            // Lambda to load the dark in the background
            QFuture<bool> future = QtConcurrent::run([this, dark]() -> bool
            {
                bool load = stackLoadFITSImage(dark, false);
                if (!load)
                    qCDebug(KSTARS_FITS) << QString("Unable to load master dark");
                else
                {
                    int width = m_StackStatistics.stats.width;
                    int height = m_StackStatistics.stats.height;
                    int bytesPerPixel = m_StackStatistics.stats.bytesPerPixel;
                    int cvType = m_StackStatistics.cvType;
                    m_Stack->addMaster(true, (void *) m_StackImageBuffer, width, height, bytesPerPixel, cvType);
                }
                return load;
            });

            m_StackFITSWatcher.setFuture(future);
            return;
        }
    }

    // Flat
    if (!m_FlatLoaded)
    {
        QString flat = m_Stack->getStackData().masterFlat;
        if (flat.isEmpty())
            m_FlatLoaded = true;
        else
        {
            m_StackFITSAsync = stackFITSFlat;
            qCDebug(KSTARS_FITS) << "Loading master flat" << flat;

            // Lambda to load the dark in the background
            QFuture<bool> future = QtConcurrent::run([this, flat]() -> bool
            {
                bool load = stackLoadFITSImage(flat, false);
                if (!load)
                    qCDebug(KSTARS_FITS) << QString("Unable to load master flat");
                else
                {
                    int width = m_StackStatistics.stats.width;
                    int height = m_StackStatistics.stats.height;
                    int bytesPerPixel = m_StackStatistics.stats.bytesPerPixel;
                    int cvType = m_StackStatistics.cvType;
                    m_Stack->addMaster(false, (void *) m_StackImageBuffer, width, height, bytesPerPixel, cvType);
                }
                return load;
            });

            m_StackFITSWatcher.setFuture(future);
            return;
        }
    }
    nextStackAction();
}

// Callback when a FITS file has been asynchronously loaded. This happens when:
// 1. A new sub is loaded
// 2. A dark or flat master is loaded
// This function checks the result and takes the next action
void FITSData::stackFITSLoaded()
{
    StackFITSAsyncType action = m_StackFITSAsync;
    m_StackFITSAsync = stackFITSNone;

    // Check for user cancel request
    if (m_StackFITSWatcher.isCanceled())
    {
        qCDebug(KSTARS_FITS) << "Cancelled stack file loading thread";
        m_StackFITSWatcherCancel = false;
        checkCancelStack();
        return;
    }

    bool plateSolving = (m_Stack->getStackData().alignMethod == LS_ALIGNMENT_PLATE_SOLVE);
    switch (action)
    {
        case stackFITSDark:
            m_DarkLoaded = true;
            processMasters();
            break;

        case stackFITSFlat:
            m_FlatLoaded = true;
            processMasters();
            break;

        case stackFITSSub:
            if (m_StackFITSWatcher.result())
            {
                if (plateSolving)
                {
                    // Next step in the chain is to plate solve
                    qCDebug(KSTARS_FITS) << "Starting to plate solve sub...";
                    emit plateSolveSub(m_StackSubRa, m_StackSubDec, m_StackSubPixscale, m_StackSubIndex,
                                       m_StackSubHealpix, m_Stack->getStackData().weighting);
                    return;
                }
                // We're not plate solving so update sub status to good and emit stats
                m_Stack->addSubStatus(true);
                emit stackUpdateStats(true, m_StackSubPos, m_StackDirWatcher->getCurrentFiles().size(), m_Stack->getMeanSubSNR(),
                                      m_Stack->getMinSubSNR(), m_Stack->getMaxSubSNR());
            }
            else
            {
                // Something has gone wrong with this sub so mark it failed and move to the next action
                qCDebug(KSTARS_FITS) << "Failed to process sub.";
                m_Stack->addSubStatus(false);
                emit stackUpdateStats(false, m_StackSubPos, m_StackDirWatcher->getCurrentFiles().size(),
                                      m_Stack->getMeanSubSNR(), m_Stack->getMinSubSNR(), m_Stack->getMaxSubSNR());
            }
            nextStackAction();
            break;
        default:
            qCDebug(KSTARS_FITS) << QString("%1 Unknown m_StackFITSAsync %2").arg(__FUNCTION__).arg(m_StackFITSAsync);
    }
}

// Update plate solving status
void FITSData::solverDone(const bool timedOut, const bool success, const double hfr, const int numStars)
{
    qCDebug(KSTARS_FITS) << "Plate solve complete";
    bool ok = m_Stack->solverDone(m_StackWCSHandle, timedOut, success, hfr, numStars);

    emit stackUpdateStats(ok, m_StackSubPos, m_StackDirWatcher->getCurrentFiles().size(), m_Stack->getMeanSubSNR(),
                          m_Stack->getMinSubSNR(), m_Stack->getMaxSubSNR());
    nextStackAction();
}

// Current stack action is complete so do next action... either process next sub or stack
void FITSData::nextStackAction()
{
    bool done = false;
    while (!done)
    {
        // Check whether a user cancel request has been received
        if (m_CancelRequest)
        {
            m_CancelRequest = false;
            checkCancelStack();
            return;
        }

        m_StackSubPos++;
        if (m_StackSubs.size() > m_StackSubPos)
            done = processNextSub(m_StackSubs[m_StackSubPos]);
        else
        {
            // All subs have been processed so load calibration masters (if any)
            done = true;
            if (!m_DarkLoaded || !m_FlatLoaded)
            {
                processMasters();
                return;
            }

            // If we haven't already chosen an alignment master use the first sub
            if (!m_AlignMasterChosen)
            {
                m_AlignMasterChosen = true;
                emit alignMasterChosen(m_StackSubs[0]);
            }

            // Stack... either an initial stack or add 1 or more subs to an existing stack
            QFuture<bool> future;
            if (m_Stack->getInitialStackDone())
            {
                qCDebug(KSTARS_FITS) << "Starting incremental stack...";
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                future = QtConcurrent::run(&FITSStack::stackn, m_Stack.get());
#else
                future = QtConcurrent::run(m_Stack.get(), &FITSStack::stackn);
#endif
            }
            else
            {
                qCDebug(KSTARS_FITS) << "Starting initial stack...";
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                future = QtConcurrent::run(&FITSStack::stack, m_Stack.get());
#else
                future = QtConcurrent::run(m_Stack.get(), &FITSStack::stack);
#endif
            }
            m_StackWatcher.setFuture(future);
            return;
        }
    }
}

// Callback when an asynchronous stacking operation completes. This happens on:
// 1. Initial stack
// 2. Incremental stack
void FITSData::stackProcessDone()
{
    m_Stack->setStackInProgress(false);

    if (m_StackWatcher.isCanceled())
    {
        qCDebug(KSTARS_FITS) << "Stacking threads cancelled";
        m_StackWatcherCancel = false;
        checkCancelStack();
    }
    else
    {
        if (!m_StackWatcher.result())
           qCDebug(KSTARS_FITS) << QString("Stacking operation failed");

        emit stackReady();
    }
}

void FITSData::stackSetupWCS()
{
    if (m_WCSHandle != nullptr)
    {
        wcsvfree(&m_nwcs, &m_WCSHandle);
        m_nwcs = 0;
        m_WCSHandle = nullptr;
    }

    if (!m_Stack)
    {
        qCDebug(KSTARS_FITS) << QString("%1 called but no m_Stack").arg(__FUNCTION__);
        return;
    }

    const struct wcsprm * wcsRef = m_Stack->getWCSStackImage();
    if (!wcsRef)
    {
        qCDebug(KSTARS_FITS) << QString("%1 stack returned nullptr to image WCS").arg(__FUNCTION__);
        return;
    }

    // Take a deep copy of the WCS state
    m_WCSHandle = new struct wcsprm;
    m_WCSHandle->flag = -1; // Allocate space
    int status = 0;
    if ((status = wcssub(1, wcsRef, 0x0, 0x0, m_WCSHandle)) != 0)
    {
        qCDebug(KSTARS_FITS) << QString("%1 wcssub error processing %1 %2").arg(__FUNCTION__).arg(status)
                                            .arg(wcs_errmsg[status]);
        delete m_WCSHandle;
        m_WCSHandle = nullptr;
        return;
    }
    if ((status = wcsset(m_WCSHandle)) != 0)
    {
        qCDebug(KSTARS_FITS) << QString("%1 wcsset error processing %1 %2").arg(__FUNCTION__).arg(status)
                                            .arg(wcs_errmsg[status]);
        wcsvfree(&m_nwcs, &m_WCSHandle);
        m_nwcs = 0;
        m_WCSHandle = nullptr;
        return;
    }

    // Keep header consistent with changes to WCS structure - needed if we save the image
    updateRecordValue("EQUINOX", m_WCSHandle->equinox, "EQUINOX");
    updateRecordValue("CRVAL1", m_WCSHandle->crval[0], "CRVAL1");
    updateRecordValue("CRVAL2", m_WCSHandle->crval[1], "CRVAL2");

    updateRecordValue("RADECSYS", QString(m_WCSHandle->radesys), "RADECSYS");
    updateRecordValue("CTYPE1", QString(m_WCSHandle->ctype[0]), "CTYPE1");
    updateRecordValue("CTYPE2", QString(m_WCSHandle->ctype[1]), "CTYPE2");

    updateRecordValue("CRPIX1", m_WCSHandle->crpix[0], "CRPIX1");
    updateRecordValue("CRPIX2", m_WCSHandle->crpix[1], "CRPIX2");

    updateRecordValue("CDELT1", m_WCSHandle->cdelt[0], "CDELT1");
    updateRecordValue("CDELT2", m_WCSHandle->cdelt[1], "CDELT2");

    updateRecordValue("CROTA1", m_WCSHandle->crota[0], "CROTA1");
    updateRecordValue("CROTA2", m_WCSHandle->crota[1], "CROTA2");

    m_ObjectsSearched = false;
    m_CatObjectsSearched = false;
    m_WCSState = Success;
    HasWCS = true;
}
#endif // #if !KSTARS_LITE, HAVE_WCSLIB, HAVE_OPENCV

namespace
{
// Common code for reporting fits read errors. Always returns false.
QString fitsErrorToString(int status)
{
    char error_status[512] = {0};
    fits_report_error(stderr, status);
    fits_get_errstatus(status, error_status);
    QString message = error_status;
    return message;
}
}

bool FITSData::privateLoad(const QByteArray &buffer)
{
    m_isTemporary = m_Filename.startsWith(KSPaths::writableLocation(QStandardPaths::TempLocation));
    cacheHFR = -1;
    cacheEccentricity = -1;

    if (m_Extension.contains("fit") || m_Extension.contains("fz"))
        return loadFITSImage(buffer);
    if (m_Extension.contains("xisf"))
        return loadXISFImage(buffer);
    if (QImageReader::supportedImageFormats().contains(m_Extension.toLatin1()))
        return loadCanonicalImage(buffer);
    else if (RAWFormats.contains(m_Extension))
        return loadRAWImage(buffer);

    return false;
}

bool FITSData::loadFITSImage(const QByteArray &buffer, const bool isCompressed)
{
    int status = 0, anynull = 0;
    long naxes[3];

    m_HistogramConstructed = false;

    if (m_Extension.contains(".fz") || isCompressed)
    {
        fpstate fpvar;
        fp_init (&fpvar);
        bool rc = false;

        if (buffer.isEmpty())
        {
            // Store so we don't lose.
            m_compressedFilename = m_Filename;

            QString uncompressedFile = QDir::tempPath() + QString("/%1").arg(QUuid::createUuid().toString().remove(
                                           QRegularExpression("[-{}]")));

            rc = fp_unpack_file_to_fits(m_Filename.toLocal8Bit().data(), &fptr, fpvar) == 0;
            if (rc)
            {
                m_Filename = uncompressedFile;
            }
        }
        else
        {
            size_t m_PackBufferSize = 100000;
            free(m_PackBuffer);
            m_PackBuffer = (uint8_t *)malloc(m_PackBufferSize);
            rc = fp_unpack_data_to_data(buffer.data(), buffer.size(), &m_PackBuffer, &m_PackBufferSize, fpvar) == 0;

            if (rc)
            {
                void *data = reinterpret_cast<void *>(m_PackBuffer);
                if (fits_open_memfile(&fptr, m_Filename.toLocal8Bit().data(), READONLY, &data, &m_PackBufferSize, 0,
                                      nullptr, &status))
                {
                    m_LastError = i18n("Error reading fits buffer: %1.", fitsErrorToString(status));
                    return false;
                }

                m_Statistics.size = m_PackBufferSize;
            }
            //rc = fp_unpack_data_to_fits(buffer.data(), buffer.size(), &fptr, fpvar) == 0;
        }

        if (rc == false)
        {
            free(m_PackBuffer);
            m_PackBuffer = nullptr;
            m_LastError = i18n("Failed to unpack compressed fits");
            qCCritical(KSTARS_FITS) << m_LastError;
            return false;
        }

        m_isTemporary = true;
        m_isCompressed = true;
        m_Statistics.size = fptr->Fptr->logfilesize;

    }
    else if (buffer.isEmpty())
    {
        // Use open diskfile as it does not use extended file names which has problems opening
        // files with [ ] or ( ) in their names.
        if (fits_open_diskfile(&fptr, m_Filename.toLocal8Bit(), READONLY, &status))
        {
            m_LastError = i18n("Error opening fits file %1 : %2", m_Filename, fitsErrorToString(status));
        }

        m_Statistics.size = QFile(m_Filename).size();
    }
    else
    {
        // Read the FITS file from a memory buffer.
        void *temp_buffer = const_cast<void *>(reinterpret_cast<const void *>(buffer.data()));
        size_t temp_size = buffer.size();
        if (fits_open_memfile(&fptr, m_Filename.toLocal8Bit().data(), READONLY,
                              &temp_buffer, &temp_size, 0, nullptr, &status))
        {
            m_LastError = i18n("Error reading fits buffer: %1", fitsErrorToString(status));
            return false;
        }

        m_Statistics.size = temp_size;
    }

    if (fits_movabs_hdu(fptr, 1, IMAGE_HDU, &status))
    {

        free(m_PackBuffer);
        m_PackBuffer = nullptr;
        m_LastError = i18n("Could not locate image HDU: %1", fitsErrorToString(status));
    }

    if (fits_get_img_param(fptr, 3, &m_FITSBITPIX, &(m_Statistics.ndim), naxes, &status))
    {
        free(m_PackBuffer);
        m_PackBuffer = nullptr;
        m_LastError = i18n("FITS file open error (fits_get_img_param): %1", fitsErrorToString(status));
        return false;
    }

    // Reload if it is transparently compressed.
    if ((fits_is_compressed_image(fptr, &status) || m_Statistics.ndim <= 0) && !isCompressed)
    {
        loadCommon(m_Filename);
        qCDebug(KSTARS_FITS) << "Image is compressed. Reloading...";
        return loadFITSImage(buffer, true);
    }

    if (m_Statistics.ndim < 2)
    {
        m_LastError = i18n("1D FITS images are not supported in KStars.");
        qCCritical(KSTARS_FITS) << m_LastError;
        free(m_PackBuffer);
        m_PackBuffer = nullptr;
        return false;
    }

    switch (m_FITSBITPIX)
    {
        case BYTE_IMG:
            m_Statistics.dataType      = TBYTE;
            m_Statistics.bytesPerPixel = sizeof(uint8_t);
            break;
        case SHORT_IMG:
            // Read SHORT image as USHORT
            m_FITSBITPIX               = USHORT_IMG;
            m_Statistics.dataType      = TUSHORT;
            m_Statistics.bytesPerPixel = sizeof(int16_t);
            break;
        case USHORT_IMG:
            m_Statistics.dataType      = TUSHORT;
            m_Statistics.bytesPerPixel = sizeof(uint16_t);
            break;
        case LONG_IMG:
            // Read LONG image as ULONG
            m_FITSBITPIX               = ULONG_IMG;
            m_Statistics.dataType      = TULONG;
            m_Statistics.bytesPerPixel = sizeof(int32_t);
            break;
        case ULONG_IMG:
            m_Statistics.dataType      = TULONG;
            m_Statistics.bytesPerPixel = sizeof(uint32_t);
            break;
        case FLOAT_IMG:
            m_Statistics.dataType      = TFLOAT;
            m_Statistics.bytesPerPixel = sizeof(float);
            break;
        case LONGLONG_IMG:
            m_Statistics.dataType      = TLONGLONG;
            m_Statistics.bytesPerPixel = sizeof(int64_t);
            break;
        case DOUBLE_IMG:
            m_Statistics.dataType      = TDOUBLE;
            m_Statistics.bytesPerPixel = sizeof(double);
            break;
        default:
            m_LastError = i18n("Bit depth %1 is not supported.", m_FITSBITPIX);
            qCCritical(KSTARS_FITS) << m_LastError;
            return false;
    }

    if (m_Statistics.ndim < 3)
        naxes[2] = 1;

    if (naxes[0] == 0 || naxes[1] == 0)
    {
        m_LastError = i18n("Image has invalid dimensions %1x%2", naxes[0], naxes[1]);
        qCCritical(KSTARS_FITS) << m_LastError;
        free(m_PackBuffer);
        m_PackBuffer = nullptr;
        return false;
    }

    m_Statistics.width               = naxes[0];
    m_Statistics.height              = naxes[1];
    m_Statistics.samples_per_channel = m_Statistics.width * m_Statistics.height;
    roiCenter.setX(m_Statistics.width / 2);
    roiCenter.setY(m_Statistics.height / 2);
    if(m_Statistics.width % 2)
        roiCenter.setX(roiCenter.x() + 1);
    if(m_Statistics.height % 2)
        roiCenter.setY(roiCenter.y() + 1);

    clearImageBuffers();

    m_Statistics.channels = naxes[2];

    // Channels always set to #1 if we are not required to process 3D Cubes
    // Or if mode is not FITS_NORMAL (guide, focus..etc)
    if ( (m_Mode != FITS_NORMAL && m_Mode != FITS_CALIBRATE && m_Mode != FITS_LIVESTACKING) || !Options::auto3DCube())
        m_Statistics.channels = 1;

    m_ImageBufferSize = m_Statistics.samples_per_channel * m_Statistics.channels * m_Statistics.bytesPerPixel;
    m_ImageBuffer = new uint8_t[m_ImageBufferSize];
    if (m_ImageBuffer == nullptr)
    {
        qCWarning(KSTARS_FITS) << "FITSData: Not enough memory for image_buffer channel. Requested: "
                               << m_ImageBufferSize << " bytes.";
        clearImageBuffers();
        free(m_PackBuffer);
        m_PackBuffer = nullptr;
        return false;
    }

    rotCounter     = 0;
    flipHCounter   = 0;
    flipVCounter   = 0;
    long nelements = m_Statistics.samples_per_channel * m_Statistics.channels;

    if (fits_read_img(fptr, m_Statistics.dataType, 1, nelements, nullptr, m_ImageBuffer, &anynull, &status))
    {
        m_LastError = i18n("Error reading image: %1", fitsErrorToString(status));
        return false;
    }

    parseHeader();

    // Get UTC date time
    QVariant value;
    if (getRecordValue("DATE-OBS", value) && value.isValid())
    {
        QDateTime ts = value.toDateTime();
        m_DateTime = KStarsDateTime(ts.date(), ts.time());
    }

    // Only check for debayed IF the original naxes[2] is 1
    // which is for single channels.
    if (naxes[2] == 1 && m_Statistics.channels == 1 && Options::autoDebayer() && checkDebayer())
    {
        // Save bayer image on disk in case we need to save it later since debayer destorys this data
        if (m_isTemporary && m_TemporaryDataFile.open())
        {
            m_TemporaryDataFile.write(buffer);
            m_TemporaryDataFile.close();
            m_Filename = m_TemporaryDataFile.fileName();
        }

        if (debayer())
            calculateStats(false, false);
    }
    else
        calculateStats(false, false);

    if (m_Mode == FITS_NORMAL || m_Mode == FITS_ALIGN)
        loadWCS();
#if !defined (KSTARS_LITE) && defined (HAVE_WCSLIB) && defined (HAVE_OPENCV)
    else if (m_Mode == FITS_LIVESTACKING && m_Stack->getStackData().alignMethod == LS_ALIGNMENT_PLATE_SOLVE)
        stackSetupWCS();
#endif // !KSTARS_LITE, HAVE_WCSLIB, HAVE_OPENCV

    starsSearched = false;

    return true;
}

// Load a FITS image temporarily for stacking so no need to setup all the global
// variables required for a "normal" load
#if !defined (KSTARS_LITE) && defined (HAVE_WCSLIB) && defined (HAVE_OPENCV)
bool FITSData::stackLoadFITSImage(QString filename, const bool isCompressed)
{
    int status = 0, anynull = 0;

    // We're done with the previous file now so we can close it, discarding changes
    if (m_Stackfptr != nullptr)
    {
        fits_close_file(m_Stackfptr, &status);
        if (status != 0)
            qCDebug(KSTARS_FITS) << QString("Error %1 closing file %2").arg(fitsErrorToString(status)).arg(filename);
        m_Stackfptr = nullptr;
        status = 0;
    }

    QFileInfo info(filename);
    QString extension = info.completeSuffix().toLower();

    if (extension.contains(".fz") || isCompressed)
    {
        fpstate fpvar;
        fp_init (&fpvar);
        bool rc = false;

        QString uncompressedFile = QDir::tempPath() + QString("/%1").arg(QUuid::createUuid().toString().remove(
                                                              QRegularExpression("[-{}]")));

        rc = fp_unpack_file_to_fits(filename.toLocal8Bit().data(), &m_Stackfptr, fpvar) == 0;
        if (rc)
            filename = uncompressedFile;
        else
        {
            qCDebug(KSTARS_FITS) << QString("Failed to unpack compressed fits file %1").arg(filename);
            return false;
        }
    }
    else
    {
        // Use open diskfile as it does not use extended file names which has problems opening
        // files with [ ] or ( ) in their names.
        if (fits_open_diskfile(&m_Stackfptr, filename.toLocal8Bit(), READONLY, &status))
        {
            qCDebug(KSTARS_FITS) << QString("Error %1 opening fits file %2").arg(fitsErrorToString(status)).arg(filename);
            return false;
        }
    }

    if (fits_movabs_hdu(m_Stackfptr, 1, IMAGE_HDU, &status))
    {
        qCDebug(KSTARS_FITS) << QString("Error %1 locating image HDU in %2").arg(fitsErrorToString(status)).arg(filename);
        return false;
    }

    int FITSBITPIX;
    long naxes[3];
    if (fits_get_img_param(m_Stackfptr, 3, &FITSBITPIX, &m_StackStatistics.stats.ndim, naxes, &status))
    {
        qCDebug(KSTARS_FITS) << QString("Error %1 getting image params for %2").arg(fitsErrorToString(status)).arg(filename);
        return false;
    }

    // Reload if it is transparently compressed.
    if ((fits_is_compressed_image(m_Stackfptr, &status) || m_StackStatistics.stats.ndim <= 0) && !isCompressed)
    {
        qCDebug(KSTARS_FITS) << "Image is compressed. Reloading...";
        return stackLoadFITSImage(filename, true);
    }

    if (m_StackStatistics.stats.ndim < 2)
    {
        qCDebug(KSTARS_FITS) << QString("1D FITS image %1 detected. These are not supported").arg(filename);
        return false;
    }

    if (m_StackStatistics.stats.ndim < 3)
        naxes[2] = 1;

    if (naxes[0] == 0 || naxes[1] == 0)
    {
        qCDebug(KSTARS_FITS) << QString("Image %1 has invalid dimensions %2x%3")
                                    .arg(filename).arg(naxes[0]).arg(naxes[1]);
        return false;
    }

    m_StackStatistics.stats.width               = naxes[0];
    m_StackStatistics.stats.height              = naxes[1];
    m_StackStatistics.stats.channels            = naxes[2];
    m_StackStatistics.stats.samples_per_channel = m_StackStatistics.stats.width * m_StackStatistics.stats.height;

    switch (FITSBITPIX)
    {
        case BYTE_IMG:
            m_StackStatistics.stats.dataType      = TBYTE;
            m_StackStatistics.stats.bytesPerPixel = sizeof(uint8_t);
            m_StackStatistics.cvType              = CV_MAKETYPE(CV_8U, m_StackStatistics.stats.channels);
            break;
        case SHORT_IMG:
            // Process a signed short image as unsigned
            m_StackStatistics.stats.dataType      = TUSHORT;
            m_StackStatistics.stats.bytesPerPixel = sizeof(int16_t);
            m_StackStatistics.cvType              = CV_MAKETYPE(CV_16U, m_StackStatistics.stats.channels);
            break;
        case USHORT_IMG:
            m_StackStatistics.stats.dataType      = TUSHORT;
            m_StackStatistics.stats.bytesPerPixel = sizeof(uint16_t);
            m_StackStatistics.cvType              = CV_MAKETYPE(CV_16U, m_StackStatistics.stats.channels);
            break;
        case LONG_IMG:
            // Process a signed long image as unsigned
            m_StackStatistics.stats.dataType      = TULONG;
            m_StackStatistics.stats.bytesPerPixel = sizeof(int32_t);
            m_StackStatistics.cvType              = -1;
            qCDebug(KSTARS_FITS) << "OpenCV does not support unsigned long datatype";
            return false;
        case ULONG_IMG:
            m_StackStatistics.stats.dataType      = TULONG;
            m_StackStatistics.stats.bytesPerPixel = sizeof(uint32_t);
            m_StackStatistics.cvType              = -1;
            qCDebug(KSTARS_FITS) << "OpenCV does not support unsigned long datatype";
            return false;
        case FLOAT_IMG:
            m_StackStatistics.stats.dataType      = TFLOAT;
            m_StackStatistics.stats.bytesPerPixel = sizeof(float);
            m_StackStatistics.cvType              = CV_MAKETYPE(CV_32F, m_StackStatistics.stats.channels);
            break;
        case LONGLONG_IMG:
            m_StackStatistics.stats.dataType      = TLONGLONG;
            m_StackStatistics.stats.bytesPerPixel = sizeof(int64_t);
            m_StackStatistics.cvType              = -1;
            qCDebug(KSTARS_FITS) << "OpenCV does not support long long datatype";
            return false;
        case DOUBLE_IMG:
            m_StackStatistics.stats.dataType      = TDOUBLE;
            m_StackStatistics.stats.bytesPerPixel = sizeof(double);
            m_StackStatistics.cvType              = CV_MAKETYPE(CV_64F, m_StackStatistics.stats.channels);
            break;
        default:
            m_StackStatistics.cvType              = -1;
            qCDebug(KSTARS_FITS) << QString("File %1 has bit depth %2 This is not supported")
                                        .arg(filename).arg(FITSBITPIX);
            return false;
    }

    uint32_t stackImageBufferSize = m_StackStatistics.stats.samples_per_channel * m_StackStatistics.stats.channels *
                                    m_StackStatistics.stats.bytesPerPixel;
    if (stackImageBufferSize != m_StackImageBufferSize)
    {
        if (m_StackImageBuffer != nullptr)
            delete[] m_StackImageBuffer;
        m_StackImageBuffer = new uint8_t[stackImageBufferSize];

        if (m_StackImageBuffer != nullptr)
            m_StackImageBufferSize = stackImageBufferSize;
        else
        {
            qCDebug(KSTARS_FITS) << "FITSData: Not enough memory for stack_image_buffer channel. Requested: "
                                 << stackImageBufferSize << " bytes.";
            m_StackImageBufferSize = 0;
            return false;
        }
    }

    long nelements = m_StackStatistics.stats.samples_per_channel * m_StackStatistics.stats.channels;
    if (fits_read_img(m_Stackfptr, m_StackStatistics.stats.dataType, 1, nelements, nullptr, m_StackImageBuffer, &anynull, &status))
    {
        qCDebug(KSTARS_FITS) << QString("Error %1 reading image: %2").arg(fitsErrorToString(status)).arg(filename);
        return false;
    }

    if (!parseHeader(true))
    {
        qCDebug(KSTARS_FITS) << QString("Error parsing FITS header in %1").arg(filename);
        return false;
    }

    // Debayer if necessary
    BayerParams bayerParams;
    bayerParams.method  = DC1394_BAYER_METHOD_NEAREST;
    bayerParams.filter  = DC1394_COLOR_FILTER_RGGB;
    bayerParams.offsetX = bayerParams.offsetY = 0;

    if (naxes[2] == 1 && m_StackStatistics.stats.channels == 1 && Options::autoDebayer() &&
                                             stackCheckDebayer(bayerParams))
    {
        if (m_StackStatistics.stats.dataType == TUSHORT)
            stackDebayer<uint16_t>(bayerParams);
        else if (m_StackStatistics.stats.dataType == TBYTE)
            stackDebayer<uint8_t>(bayerParams);
        else
            qCDebug(KSTARS_FITS) << QString("Unsupported bit depth for debayering: %1 bytes per pixel")
                                        .arg(m_StackStatistics.stats.bytesPerPixel);
    }
    return true;
}

bool FITSData::stackLoadWCS()
{
    if (m_StackWCSHandle != nullptr)
    {
        wcsvfree(&m_Stacknwcs, &m_StackWCSHandle);
        m_Stacknwcs = 0;
        m_StackWCSHandle = nullptr;
    }

    // Get the FITS header in a format to do WCS processing...
    // We hold m_StackHeaderRecords with strings without quotes because when writing a FITS
    // header for example, CFITSIO quotes strings automatically. However, here the WCS function
    // wcspih expects strings to be quoted else it fails silently so we must add quotes
    int status = 0, nreject = 0, nkeyrec = 1;
    QByteArray header_str;
    static const QSet<QString> wcsKeysNeedingQuotes = { "CTYPE1", "CTYPE2", "RADECSYS" };

    for (auto &fitsKeyword : m_StackHeaderRecords)
    {
        QByteArray rec;
        rec.append(fitsKeyword.key.leftJustified(8, ' ').toLatin1());
        rec.append("= ");

        QString valueStr = fitsKeyword.value.toString();
        if (wcsKeysNeedingQuotes.contains(fitsKeyword.key))
        {
            // Quote only if not already quoted
            if (!valueStr.startsWith('\'') || !valueStr.endsWith('\''))
                valueStr = '\'' + valueStr + '\'';
        }

        rec.append(valueStr.toLatin1());
        rec.append(" / ");
        rec.append(fitsKeyword.comment.toLatin1());
        header_str.append(rec.leftJustified(80, ' ', true));
        nkeyrec++;
    }

    header_str.append(QByteArray("END").leftJustified(80));

    // Do the WCS processing...
    if ((status = wcspih(header_str.data(), nkeyrec, WCSHDR_all, 0, &nreject, &m_Stacknwcs, &m_StackWCSHandle)) != 0)
    {
        wcsvfree(&m_Stacknwcs, &m_StackWCSHandle);
        m_StackWCSHandle = nullptr;
        m_Stacknwcs = 0;
        qCDebug(KSTARS_FITS) << QString("Error %1 getting WCS header")
                                    .arg(fitsErrorToString(status));
        return false;
    }

    if (m_StackWCSHandle == nullptr)
    {
        qCDebug(KSTARS_FITS) << QString("No world coordinate systems found");
        return false;
    }

    // FIXME: Call above goes through EVEN if no WCS is present, so we're adding this to return for now.
    if (m_StackWCSHandle->crpix[0] == 0)
    {
        wcsvfree(&m_Stacknwcs, &m_StackWCSHandle);
        m_StackWCSHandle = nullptr;
        m_Stacknwcs = 0;
        qCDebug(KSTARS_FITS) << QString("No world coordinate systems found (check)");
        return false;
    }

    cdfix(m_StackWCSHandle);
    if ((status = wcsset(m_StackWCSHandle)) != 0)
    {
        wcsvfree(&m_Stacknwcs, &m_StackWCSHandle);
        m_StackWCSHandle = nullptr;
        m_Stacknwcs = 0;
        qCDebug(KSTARS_FITS) << QString("wcsset error %1").arg(wcs_errmsg[status]);
        return false;
    }
    return true;
}
#endif // !KSTARS_LITE, HAVE_WCSLIB, HAVE_OPENCV

bool FITSData::loadXISFImage(const QByteArray &buffer)
{
    m_HistogramConstructed = false;
    clearImageBuffers();

#ifdef HAVE_XISF
    try
    {
        LibXISF::XISFReader xisfReader;
        if (buffer.isEmpty())
        {
            xisfReader.open(m_Filename.toLocal8Bit().data());
        }
        else
        {
            LibXISF::ByteArray byteArray(buffer.constData(), buffer.size());
            xisfReader.open(byteArray);
        }

        if (xisfReader.imagesCount() == 0)
        {
            m_LastError = i18n("File contain no images");
            return false;
        }

        const LibXISF::Image &image = xisfReader.getImage(0);

        switch (image.sampleFormat())
        {
            case LibXISF::Image::UInt8:
                m_Statistics.dataType = TBYTE;
                m_Statistics.bytesPerPixel = sizeof(LibXISF::UInt8);
                m_FITSBITPIX = TBYTE;
                break;
            case LibXISF::Image::UInt16:
                m_Statistics.dataType = TUSHORT;
                m_Statistics.bytesPerPixel = sizeof(LibXISF::UInt16);
                m_FITSBITPIX = TUSHORT;
                break;
            case LibXISF::Image::UInt32:
                m_Statistics.dataType = TULONG;
                m_Statistics.bytesPerPixel = sizeof(LibXISF::UInt32);
                m_FITSBITPIX = TULONG;
                break;
            case LibXISF::Image::Float32:
                m_Statistics.dataType = TFLOAT;
                m_Statistics.bytesPerPixel = sizeof(LibXISF::Float32);
                m_FITSBITPIX = TFLOAT;
                break;
            default:
                m_LastError = i18n("Sample format %1 is not supported.", LibXISF::Image::sampleFormatString(image.sampleFormat()).c_str());
                qCCritical(KSTARS_FITS) << m_LastError;
                return false;
        }

        m_Statistics.width = image.width();
        m_Statistics.height = image.height();
        m_Statistics.samples_per_channel = m_Statistics.width * m_Statistics.height;
        m_Statistics.channels = image.channelCount();
        m_Statistics.size = buffer.size();
        roiCenter.setX(m_Statistics.width / 2);
        roiCenter.setY(m_Statistics.height / 2);
        if(m_Statistics.width % 2)
            roiCenter.setX(roiCenter.x() + 1);
        if(m_Statistics.height % 2)
            roiCenter.setY(roiCenter.y() + 1);

        m_HeaderRecords.clear();
        auto &fitsKeywords = image.fitsKeywords();
        for(auto &fitsKeyword : fitsKeywords)
        {
            m_HeaderRecords.push_back({QString::fromStdString(fitsKeyword.name), QString::fromStdString(fitsKeyword.value), QString::fromStdString(fitsKeyword.comment)});
        }

        setupWCSParams();

        m_ImageBufferSize = image.imageDataSize();
        m_ImageBuffer = new uint8_t[m_ImageBufferSize];
        std::memcpy(m_ImageBuffer, image.imageData(), m_ImageBufferSize);

        calculateStats(false, false);
        loadWCS();
    }
    catch (LibXISF::Error &error)
    {
        m_LastError = i18n("XISF file open error: ") + error.what();
        return false;
    }
    return true;
#else
    Q_UNUSED(buffer)
    return false;
#endif

}

bool FITSData::saveXISFImage(const QString &newFilename)
{
#ifdef HAVE_XISF
    try
    {
        LibXISF::XISFWriter xisfWriter;
        LibXISF::Image image;
        image.setGeometry(m_Statistics.width, m_Statistics.height, m_Statistics.channels);
        if (m_Statistics.channels > 1)
            image.setColorSpace(LibXISF::Image::RGB);

        switch (m_FITSBITPIX)
        {
            case BYTE_IMG:
                image.setSampleFormat(LibXISF::Image::UInt8);
                break;
            case USHORT_IMG:
                image.setSampleFormat(LibXISF::Image::UInt16);
                break;
            case ULONG_IMG:
                image.setSampleFormat(LibXISF::Image::UInt32);
                break;
            case FLOAT_IMG:
                image.setSampleFormat(LibXISF::Image::Float32);
                break;
            default:
                m_LastError = i18n("Bit depth %1 is not supported.", m_FITSBITPIX);
                qCCritical(KSTARS_FITS) << m_LastError;
                return false;
        }

        std::memcpy(image.imageData(), m_ImageBuffer, m_ImageBufferSize);
        for (auto &fitsKeyword : m_HeaderRecords)
            image.addFITSKeyword({fitsKeyword.key.toUtf8().data(), fitsKeyword.value.toString().toUtf8().data(), fitsKeyword.comment.toUtf8().data()});

        xisfWriter.writeImage(image);
        xisfWriter.save(newFilename.toLocal8Bit().data());
        m_Filename = newFilename;
    }
    catch (LibXISF::Error &err)
    {
        m_LastError = i18n("Error saving XISF image") + err.what();
        return false;
    }
    return true;
#else
    Q_UNUSED(newFilename)
    return false;
#endif
}

bool FITSData::loadCanonicalImage(const QByteArray &buffer)
{
    QImage imageFromFile;
    if (!buffer.isEmpty())
    {
        if(!imageFromFile.loadFromData(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size()))
        {
            qCCritical(KSTARS_FITS) << "Failed to open image.";
            return false;
        }

        m_Statistics.size = buffer.size();
    }
    else
    {
        if(!imageFromFile.load(m_Filename.toLocal8Bit()))
        {
            qCCritical(KSTARS_FITS) << "Failed to open image.";
            return false;
        }

        m_Statistics.size = QFileInfo(m_Filename).size();
    }

    //imageFromFile = imageFromFile.convertToFormat(QImage::Format_RGB32);

    // Note: This will need to be changed.  I think QT only loads 8 bpp images.
    // Also the depth method gives the total bits per pixel in the image not just the bits per
    // pixel in each channel.
    m_FITSBITPIX = BYTE_IMG;
    switch (m_FITSBITPIX)
    {
        case BYTE_IMG:
            m_Statistics.dataType      = TBYTE;
            m_Statistics.bytesPerPixel = sizeof(uint8_t);
            break;
        case SHORT_IMG:
            // Read SHORT image as USHORT
            m_Statistics.dataType      = TUSHORT;
            m_Statistics.bytesPerPixel = sizeof(int16_t);
            break;
        case USHORT_IMG:
            m_Statistics.dataType      = TUSHORT;
            m_Statistics.bytesPerPixel = sizeof(uint16_t);
            break;
        case LONG_IMG:
            // Read LONG image as ULONG
            m_Statistics.dataType      = TULONG;
            m_Statistics.bytesPerPixel = sizeof(int32_t);
            break;
        case ULONG_IMG:
            m_Statistics.dataType      = TULONG;
            m_Statistics.bytesPerPixel = sizeof(uint32_t);
            break;
        case FLOAT_IMG:
            m_Statistics.dataType      = TFLOAT;
            m_Statistics.bytesPerPixel = sizeof(float);
            break;
        case LONGLONG_IMG:
            m_Statistics.dataType      = TLONGLONG;
            m_Statistics.bytesPerPixel = sizeof(int64_t);
            break;
        case DOUBLE_IMG:
            m_Statistics.dataType      = TDOUBLE;
            m_Statistics.bytesPerPixel = sizeof(double);
            break;
        default:
            m_LastError = QString("Bit depth %1 is not supported.").arg(m_FITSBITPIX);
            qCCritical(KSTARS_FITS) << m_LastError;
            return false;
    }

    m_Statistics.width = static_cast<uint16_t>(imageFromFile.width());
    m_Statistics.height = static_cast<uint16_t>(imageFromFile.height());
    m_Statistics.channels = imageFromFile.format() == QImage::Format_Grayscale8 ? 1 : 3;
    m_Statistics.samples_per_channel = m_Statistics.width * m_Statistics.height;
    clearImageBuffers();
    m_ImageBufferSize = m_Statistics.samples_per_channel * m_Statistics.channels * static_cast<uint16_t>
                        (m_Statistics.bytesPerPixel);
    m_ImageBuffer = new uint8_t[m_ImageBufferSize];
    if (m_ImageBuffer == nullptr)
    {
        m_LastError = i18n("FITSData: Not enough memory for image_buffer channel. Requested: %1 bytes ", m_ImageBufferSize);
        qCCritical(KSTARS_FITS) << m_LastError;
        clearImageBuffers();
        return false;
    }

    if (m_Statistics.channels == 1)
    {
        memcpy(m_ImageBuffer, imageFromFile.bits(), m_ImageBufferSize);
    }
    else
    {

        auto debayered_buffer = reinterpret_cast<uint8_t *>(m_ImageBuffer);
        auto * original_bayered_buffer = reinterpret_cast<uint8_t *>(imageFromFile.bits());

        // Data in RGBA, with bytes in the order of B,G,R we need to copy them into 3 layers for FITS
        uint8_t * rBuff = debayered_buffer;
        uint8_t * gBuff = debayered_buffer + (m_Statistics.width * m_Statistics.height);
        uint8_t * bBuff = debayered_buffer + (m_Statistics.width * m_Statistics.height * 2);

        int imax = m_Statistics.samples_per_channel * 4 - 4;
        for (int i = 0; i <= imax; i += 4)
        {
            *rBuff++ = original_bayered_buffer[i + 2];
            *gBuff++ = original_bayered_buffer[i + 1];
            *bBuff++ = original_bayered_buffer[i + 0];
        }
    }

    m_HeaderRecords.clear();
    setupWCSParams();

    calculateStats(false, false);
    loadWCS();
    return true;
}

bool FITSData::loadRAWImage(const QByteArray &buffer)
{
#if !defined(KSTARS_LITE) && !defined(HAVE_LIBRAW)
    m_LastError = i18n("Unable to find dcraw and cjpeg. Please install the required tools to convert CR2/NEF to JPEG.");
    return false;
#else

    int ret = 0;
    // Creation of image processing object
    LibRaw RawProcessor;

    // Let us open the file/buffer
    if (buffer.isEmpty())
    {
        // Open file
        if ((ret = RawProcessor.open_file(m_Filename.toLocal8Bit().constData())) != LIBRAW_SUCCESS)
        {
            m_LastError = i18n("Cannot open file %1: %2", m_Filename, libraw_strerror(ret));
            RawProcessor.recycle();
            return false;
        }

        m_Statistics.size = QFileInfo(m_Filename).size();
    }
    // Open Buffer
    else
    {
        if ((ret = RawProcessor.open_buffer(const_cast<void *>(reinterpret_cast<const void *>(buffer.data())),
                                            buffer.size())) != LIBRAW_SUCCESS)
        {
            m_LastError = i18n("Cannot open buffer: %1", libraw_strerror(ret));
            RawProcessor.recycle();
            return false;
        }

        m_Statistics.size = buffer.size();
    }

    // Let us unpack the thumbnail
    if ((ret = RawProcessor.unpack()) != LIBRAW_SUCCESS)
    {
        m_LastError = i18n("Cannot unpack_thumb: %1", libraw_strerror(ret));
        RawProcessor.recycle();
        return false;
    }

    if ((ret = RawProcessor.dcraw_process()) != LIBRAW_SUCCESS)
    {
        m_LastError = i18n("Cannot dcraw_process: %1", libraw_strerror(ret));
        RawProcessor.recycle();
        return false;
    }

    libraw_processed_image_t *image = RawProcessor.dcraw_make_mem_image(&ret);
    if (ret != LIBRAW_SUCCESS)
    {
        m_LastError = i18n("Cannot load to memory: %1", libraw_strerror(ret));
        RawProcessor.recycle();
        return false;
    }

    RawProcessor.recycle();

    m_Statistics.bytesPerPixel = image->bits / 8;
    // We only support two types now
    if (m_Statistics.bytesPerPixel == 1)
        m_Statistics.dataType = TBYTE;
    else
        m_Statistics.dataType = TUSHORT;
    m_Statistics.width = image->width;
    m_Statistics.height = image->height;
    m_Statistics.channels = image->colors;
    m_Statistics.samples_per_channel = m_Statistics.width * m_Statistics.height;
    clearImageBuffers();
    m_ImageBufferSize = m_Statistics.samples_per_channel * m_Statistics.channels * m_Statistics.bytesPerPixel;
    m_ImageBuffer = new uint8_t[m_ImageBufferSize];
    if (m_ImageBuffer == nullptr)
    {
        m_LastError = i18n("FITSData: Not enough memory for image_buffer channel. Requested: %1 bytes ", m_ImageBufferSize);
        qCCritical(KSTARS_FITS) << m_LastError;
        libraw_dcraw_clear_mem(image);
        clearImageBuffers();
        return false;
    }

    auto destination_buffer = reinterpret_cast<uint8_t *>(m_ImageBuffer);
    auto source_buffer = reinterpret_cast<uint8_t *>(image->data);

    // For mono, we memcpy directly
    if (image->colors == 1)
    {
        memcpy(destination_buffer, source_buffer, m_ImageBufferSize);
    }
    else
    {
        // Data in RGB24, with bytes in the order of R,G,B. We copy them copy them into 3 layers for FITS
        uint8_t * rBuff = destination_buffer;
        uint8_t * gBuff = destination_buffer + (m_Statistics.width * m_Statistics.height);
        uint8_t * bBuff = destination_buffer + (m_Statistics.width * m_Statistics.height * 2);

        int imax = m_Statistics.samples_per_channel * 3 - 3;
        for (int i = 0; i <= imax; i += 3)
        {
            *rBuff++ = source_buffer[i + 0];
            *gBuff++ = source_buffer[i + 1];
            *bBuff++ = source_buffer[i + 2];
        }
    }
    libraw_dcraw_clear_mem(image);

    m_HeaderRecords.clear();
    setupWCSParams();

    calculateStats(false, false);
    loadWCS();
    return true;
#endif
}

bool FITSData::saveImage(const QString &newFilename)
{
    if (newFilename == m_Filename)
        return true;

    const QString ext = QFileInfo(newFilename).suffix();

    if (ext == "jpg" || ext == "png")
    {
        double min, max;
        QImage fitsImage = FITSToImage(newFilename);
        getMinMax(&min, &max);

        if (min == max)
        {
            fitsImage.fill(Qt::white);
        }
        else if (channels() == 1)
        {
            fitsImage = QImage(width(), height(), QImage::Format_Indexed8);

            fitsImage.setColorCount(256);
            for (int i = 0; i < 256; i++)
                fitsImage.setColor(i, qRgb(i, i, i));
        }
        else
        {
            fitsImage = QImage(width(), height(), QImage::Format_RGB32);
        }

        double dataMin = m_Statistics.mean[0] - m_Statistics.stddev[0];
        double dataMax = m_Statistics.mean[0] + m_Statistics.stddev[0] * 3;

        double bscale = 255. / (dataMax - dataMin);
        double bzero  = (-dataMin) * (255. / (dataMax - dataMin));

        // Long way to do this since we do not want to use templated functions here
        switch (m_Statistics.dataType)
        {
            case TBYTE:
                convertToQImage<uint8_t>(dataMin, dataMax, bscale, bzero, fitsImage);
                break;

            case TSHORT:
                convertToQImage<int16_t>(dataMin, dataMax, bscale, bzero, fitsImage);
                break;

            case TUSHORT:
                convertToQImage<uint16_t>(dataMin, dataMax, bscale, bzero, fitsImage);
                break;

            case TLONG:
                convertToQImage<int32_t>(dataMin, dataMax, bscale, bzero, fitsImage);
                break;

            case TULONG:
                convertToQImage<uint32_t>(dataMin, dataMax, bscale, bzero, fitsImage);
                break;

            case TFLOAT:
                convertToQImage<float>(dataMin, dataMax, bscale, bzero, fitsImage);
                break;

            case TLONGLONG:
                convertToQImage<int64_t>(dataMin, dataMax, bscale, bzero, fitsImage);
                break;

            case TDOUBLE:
                convertToQImage<double>(dataMin, dataMax, bscale, bzero, fitsImage);
                break;

            default:
                break;
        }

        fitsImage.save(newFilename, ext.toLatin1().constData());

        m_Filename = newFilename;
        qCInfo(KSTARS_FITS) << "Saved image file:" << m_Filename;
        return true;
    }

    if (m_isCompressed)
    {
        m_LastError = i18n("Saving compressed files is not supported.");
        return false;
    }

    int status = 0;
    long nelements;
    fitsfile * new_fptr;

    if (ext == "xisf")
    {
        if(fptr)
        {
            fits_close_file(fptr, &status);
            fptr = nullptr;
        }
        rotCounter = flipHCounter = flipVCounter = 0;
        return saveXISFImage(newFilename);
    }

    //    if (HasDebayer && m_Filename.isEmpty() == false)
    //    {
    //        fits_flush_file(fptr, &status);
    //        /* close current file */
    //        if (fits_close_file(fptr, &status))
    //        {
    //            recordLastError(status);
    //            return status;
    //        }

    //        // Skip "!" in the beginning of the new file name
    //        QString finalFileName(newFilename);

    //        finalFileName.remove('!');

    //        // Remove first otherwise copy will fail below if file exists
    //        QFile::remove(finalFileName);

    //        if (!QFile::copy(m_Filename, finalFileName))
    //        {
    //            qCCritical(KSTARS_FITS()) << "FITS: Failed to copy " << m_Filename << " to " << finalFileName;
    //            fptr = nullptr;
    //            return false;
    //        }

    //        m_Filename = finalFileName;

    //        // Use open diskfile as it does not use extended file names which has problems opening
    //        // files with [ ] or ( ) in their names.
    //        fits_open_diskfile(&fptr, m_Filename.toLocal8Bit(), READONLY, &status);
    //        fits_movabs_hdu(fptr, 1, IMAGE_HDU, &status);

    //        return true;
    //    }

    // Read the image back into buffer in case we debyayed
    nelements = m_Statistics.samples_per_channel * m_Statistics.channels;

    /* close current file */
    if (fptr && fits_close_file(fptr, &status))
        // We can continue if the close fails, e.g. on a memory file
        status = 0;

    /* Create a new File, overwriting existing*/
    if (fits_create_file(&new_fptr, QString("!%1").arg(newFilename).toLocal8Bit(), &status))
    {
        m_LastError = i18n("Failed to create file: %1", fitsErrorToString(status));
        return status;
    }

    status = 0;

    fptr = new_fptr;

    // Create image
    long naxis = m_Statistics.channels == 1 ? 2 : 3;
    long naxes[3] = {m_Statistics.width, m_Statistics.height, naxis};

    // JM 2020-12-28: Here we to use bitpix values
    if (fits_create_img(fptr, m_FITSBITPIX, naxis, naxes, &status))
    {
        m_LastError = i18n("Failed to create image: %1", fitsErrorToString(status));
        return false;
    }

    /* Write keywords */

    // Minimum
    if (fits_update_key(fptr, TDOUBLE, "DATAMIN", &(m_Statistics.min), "Minimum value", &status))
    {
        m_LastError = i18n("Failed to update key: %1", fitsErrorToString(status));
        return false;
    }

    // Maximum
    if (fits_update_key(fptr, TDOUBLE, "DATAMAX", &(m_Statistics.max), "Maximum value", &status))
    {
        m_LastError = i18n("Failed to update key: %1", fitsErrorToString(status));
        return false;
    }

    // KStars Min, for 3 channels
    fits_write_key(fptr, TDOUBLE, "MIN1", &m_Statistics.min[0], "Min Channel 1", &status);
    if (channels() > 1)
    {
        fits_write_key(fptr, TDOUBLE, "MIN2", &m_Statistics.min[1], "Min Channel 2", &status);
        fits_write_key(fptr, TDOUBLE, "MIN3", &m_Statistics.min[2], "Min Channel 3", &status);
    }

    // KStars max, for 3 channels
    fits_write_key(fptr, TDOUBLE, "MAX1", &m_Statistics.max[0], "Max Channel 1", &status);
    if (channels() > 1)
    {
        fits_write_key(fptr, TDOUBLE, "MAX2", &m_Statistics.max[1], "Max Channel 2", &status);
        fits_write_key(fptr, TDOUBLE, "MAX3", &m_Statistics.max[2], "Max Channel 3", &status);
    }

    // Mean
    if (m_Statistics.mean[0] > 0)
    {
        fits_write_key(fptr, TDOUBLE, "MEAN1", &m_Statistics.mean[0], "Mean Channel 1", &status);
        if (channels() > 1)
        {
            fits_write_key(fptr, TDOUBLE, "MEAN2", &m_Statistics.mean[1], "Mean Channel 2", &status);
            fits_write_key(fptr, TDOUBLE, "MEAN3", &m_Statistics.mean[2], "Mean Channel 3", &status);
        }
    }

    // Median
    if (m_Statistics.median[0] > 0)
    {
        fits_write_key(fptr, TDOUBLE, "MEDIAN1", &m_Statistics.median[0], "Median Channel 1", &status);
        if (channels() > 1)
        {
            fits_write_key(fptr, TDOUBLE, "MEDIAN2", &m_Statistics.median[1], "Median Channel 2", &status);
            fits_write_key(fptr, TDOUBLE, "MEDIAN3", &m_Statistics.median[2], "Median Channel 3", &status);
        }
    }

    // Standard Deviation
    if (m_Statistics.stddev[0] > 0)
    {
        fits_write_key(fptr, TDOUBLE, "STDDEV1", &m_Statistics.stddev[0], "Standard Deviation Channel 1", &status);
        if (channels() > 1)
        {
            fits_write_key(fptr, TDOUBLE, "STDDEV2", &m_Statistics.stddev[1], "Standard Deviation Channel 2", &status);
            fits_write_key(fptr, TDOUBLE, "STDDEV3", &m_Statistics.stddev[2], "Standard Deviation Channel 3", &status);
        }
    }

    // Skip first 10 standard records and copy the rest.
    for (int i = 10; i < m_HeaderRecords.count(); i++)
    {
        QString key = m_HeaderRecords[i].key;
        const char *comment = m_HeaderRecords[i].comment.toLatin1().constBegin();
        QVariant value = m_HeaderRecords[i].value;

        // Handle common WCS numeric keywords explicitly to ensure they are saved correctly
        if (key == "CRPIX1" || key == "CRPIX2" ||
            key == "CRVAL1" || key == "CRVAL2" ||
            key == "CDELT1" || key == "CDELT2" ||
            key == "CROTA1" || key == "CROTA2")
        {
            double number = value.toDouble();
            fits_write_key(fptr, TDOUBLE, key.toLatin1().constData(), &number, comment, &status);
        }
        else
        {
           switch (value.type())
           {
               case QVariant::Int:
               {
                   int number = value.toInt();
                   fits_write_key(fptr, TINT, key.toLatin1().constData(), &number, comment, &status);
               }
               break;

               case QVariant::Double:
               {
                    double number = value.toDouble();
                    fits_write_key(fptr, TDOUBLE, key.toLatin1().constData(), &number, comment, &status);
                }
                break;

                case QVariant::String:
                default:
                {
                    char valueBuffer[256] = {0};
                    strncpy(valueBuffer, value.toString().toLatin1().constData(), 256 - 1);
                    fits_write_key(fptr, TSTRING, key.toLatin1().constData(), valueBuffer, comment, &status);
                }
            }
        }
    }

    // ISO Date
    if (fits_write_date(fptr, &status))
    {
        m_LastError = i18n("Failed to update date: %1", fitsErrorToString(status));
        return false;
    }

    QString history =
        QString("Modified by KStars on %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"));
    // History
    if (fits_write_history(fptr, history.toLatin1(), &status))
    {
        m_LastError = i18n("Failed to update history: %1", fitsErrorToString(status));
        return false;
    }

    int rot = 0, mirror = 0;
    if (rotCounter > 0)
    {
        rot = (90 * rotCounter) % 360;
        if (rot < 0)
            rot += 360;
    }
    if (flipHCounter % 2 != 0 || flipVCounter % 2 != 0)
        mirror = 1;

    if ((rot != 0) || (mirror != 0))
        rotWCSFITS(rot, mirror);

    rotCounter = flipHCounter = flipVCounter = 0;

    // Here we need to use the actual data type
    if (fits_write_img(fptr, m_Statistics.dataType, 1, nelements, m_ImageBuffer, &status))
    {
        m_LastError = i18n("Failed to write image: %1", fitsErrorToString(status));
        return false;
    }

    m_Filename = newFilename;

    fits_flush_file(fptr, &status);

    qCInfo(KSTARS_FITS) << "Saved FITS file:" << m_Filename;

    return true;
}

void FITSData::clearImageBuffers()
{
    delete[] m_ImageBuffer;
    m_ImageBuffer = nullptr;
    if(m_ImageRoiBuffer != nullptr )
    {
        delete[] m_ImageRoiBuffer;
        m_ImageRoiBuffer = nullptr;

    }
    //m_BayerBuffer = nullptr;
}

void FITSData::makeRoiBuffer(QRect roi)
{
    if (!roi.isValid())
        return;

    uint32_t channelSize = roi.height() * roi.width();
    if(channelSize  > m_Statistics.samples_per_channel || channelSize == 1)
    {
        return;
    }
    if(m_ImageRoiBuffer != nullptr )
    {
        delete[] m_ImageRoiBuffer;
        m_ImageRoiBuffer = nullptr;
    }
    int xoffset = roi.topLeft().x() - 1;
    int yoffset = roi.topLeft().y() - 1;
    uint32_t bpp = m_Statistics.bytesPerPixel;
    m_ImageRoiBuffer = new uint8_t[roi.height()*roi.width()*m_Statistics.channels * m_Statistics.bytesPerPixel]();
    for(int n = 0 ; n < m_Statistics.channels ; n++)
    {
        for(int i = 0; i < roi.height(); i++)
        {
            size_t i1 = n * channelSize * bpp +  i * roi.width() * bpp;
            size_t i2 = n * m_Statistics.samples_per_channel * bpp + (yoffset + i) * width() * bpp + xoffset * bpp;
            memcpy(&m_ImageRoiBuffer[i1],
                   &m_ImageBuffer[i2],
                   roi.width() * bpp);
        }

    }
    memcpy(&m_ROIStatistics, &m_Statistics, sizeof(FITSImage::Statistic));
    m_ROIStatistics.samples_per_channel = roi.height() * roi.width();
    m_ROIStatistics.width = roi.width();
    m_ROIStatistics.height = roi.height();
    calculateStats(false, true);
}

void FITSData::setupWCSParams()
{
    FITSImage::Solution solution;
    if (parseSolution(solution))
    {
        const bool eastToTheRight = solution.parity == FITSImage::POSITIVE ? false : true;
        updateWCSHeaderData(solution.orientation, solution.ra, solution.dec, solution.pixscale, eastToTheRight);

        QVariant value;
        bool validObservationDate = false;

        if (getRecordValue("DATE-OBS", value))
        {
            QString tsString(value.toString());
            tsString = tsString.remove('\'').trimmed();
            // Add Zulu time to indicate UTC
            tsString += "Z";

            QDateTime ts = QDateTime::fromString(tsString, Qt::ISODate);

            if (ts.isValid())
            {
                validObservationDate = true;
                m_DateTime = KStarsDateTime(ts.date(), ts.time());
            }
        }

        // Set to current datetime if no valid observation datetime. Not 100% accurate but close enough in most cases
        if (!validObservationDate)
            m_DateTime = KStarsDateTime::currentDateTimeUtc();
    }
}

void FITSData::calculateStats(bool refresh, bool roi)
{
    // Calculate min max
    if(roi == false)
    {
        calculateMinMax(refresh);
        calculateMedian(refresh);

        // Try to read mean/median/stddev if in file
        if (refresh == false && fptr)
        {
            int status = 0;
            int nfound = 0;
            if (fits_read_key_dbl(fptr, "MEAN1", &m_Statistics.mean[0], nullptr, &status) == 0)
                nfound++;
            // NB. These could fail if missing, which is OK.
            fits_read_key_dbl(fptr, "MEAN2", & m_Statistics.mean[1], nullptr, &status);
            fits_read_key_dbl(fptr, "MEAN3", &m_Statistics.mean[2], nullptr, &status);

            status = 0;
            if (fits_read_key_dbl(fptr, "STDDEV1", &m_Statistics.stddev[0], nullptr, &status) == 0)
                nfound++;
            // NB. These could fail if missing, which is OK.
            fits_read_key_dbl(fptr, "STDDEV2", &m_Statistics.stddev[1], nullptr, &status);
            fits_read_key_dbl(fptr, "STDDEV3", &m_Statistics.stddev[2], nullptr, &status);

            // If all is OK, we're done
            if (nfound == 2)
                return;
        }

        // Get standard deviation and mean in one run
        switch (m_Statistics.dataType)
        {
            case TBYTE:
                calculateStdDev<uint8_t>();
                break;

            case TSHORT:
                calculateStdDev<int16_t>();
                break;

            case TUSHORT:
                calculateStdDev<uint16_t>();
                break;

            case TLONG:
                calculateStdDev<int32_t>();
                break;

            case TULONG:
                calculateStdDev<uint32_t>();
                break;

            case TFLOAT:
                calculateStdDev<float>();
                break;

            case TLONGLONG:
                calculateStdDev<int64_t>();
                break;

            case TDOUBLE:
                calculateStdDev<double>();
                break;

            default:
                return;
        }

        // FIXME That's not really SNR, must implement a proper solution for this value
        m_Statistics.SNR = m_Statistics.mean[0] / m_Statistics.stddev[0];
    }
    else
    {
        calculateMinMax(refresh, roi);
        calculateMedian(refresh, roi);

        switch (m_ROIStatistics.dataType)
        {
            case TBYTE:
                calculateStdDev<uint8_t>(roi);
                break;

            case TSHORT:
                calculateStdDev<int16_t>(roi);
                break;

            case TUSHORT:
                calculateStdDev<uint16_t>(roi);
                break;

            case TLONG:
                calculateStdDev<int32_t>(roi);
                break;

            case TULONG:
                calculateStdDev<uint32_t>(roi);
                break;

            case TFLOAT:
                calculateStdDev<float>(roi);
                break;

            case TLONGLONG:
                calculateStdDev<int64_t>(roi);
                break;

            case TDOUBLE:
                calculateStdDev<double>(roi);
                break;

            default:
                return;
        }
    }
}

void FITSData::calculateMinMax(bool refresh, bool roi)
{
    if(roi == false)
    {
        int status, nfound = 0;

        status = 0;

        // Only fetch from header if we have a single channel
        // Otherwise, calculate manually.
        if (fptr != nullptr && !refresh)
        {

            status = 0;

            if (fits_read_key_dbl(fptr, "DATAMIN", &(m_Statistics.min[0]), nullptr, &status) == 0)
                nfound++;
            else if (fits_read_key_dbl(fptr, "MIN1", &(m_Statistics.min[0]), nullptr, &status) == 0)
                nfound++;

            // NB. These could fail if missing, which is OK.
            fits_read_key_dbl(fptr, "MIN2", &m_Statistics.min[1], nullptr, &status);
            fits_read_key_dbl(fptr, "MIN3", &m_Statistics.min[2], nullptr, &status);

            status = 0;

            if (fits_read_key_dbl(fptr, "DATAMAX", &(m_Statistics.max[0]), nullptr, &status) == 0)
                nfound++;
            else if (fits_read_key_dbl(fptr, "MAX1", &(m_Statistics.max[0]), nullptr, &status) == 0)
                nfound++;

            // NB. These could fail if missing, which is OK.
            fits_read_key_dbl(fptr, "MAX2", &m_Statistics.max[1], nullptr, &status);
            fits_read_key_dbl(fptr, "MAX3", &m_Statistics.max[2], nullptr, &status);

            // If we found both keywords, no need to calculate them, unless they are both zeros
            if (nfound == 2 && !(m_Statistics.min[0] == 0 && m_Statistics.max[0] == 0))
                return;
        }

        m_Statistics.min[0] = 1.0E30;
        m_Statistics.max[0] = -1.0E30;

        m_Statistics.min[1] = 1.0E30;
        m_Statistics.max[1] = -1.0E30;

        m_Statistics.min[2] = 1.0E30;
        m_Statistics.max[2] = -1.0E30;

        switch (m_Statistics.dataType)
        {
            case TBYTE:
                calculateMinMax<uint8_t>();
                break;

            case TSHORT:
                calculateMinMax<int16_t>();
                break;

            case TUSHORT:
                calculateMinMax<uint16_t>();
                break;

            case TLONG:
                calculateMinMax<int32_t>();
                break;

            case TULONG:
                calculateMinMax<uint32_t>();
                break;

            case TFLOAT:
                calculateMinMax<float>();
                break;

            case TLONGLONG:
                calculateMinMax<int64_t>();
                break;

            case TDOUBLE:
                calculateMinMax<double>();
                break;

            default:
                break;
        }
    }
    else
    {
        m_ROIStatistics.min[0] = 1.0E30;
        m_ROIStatistics.max[0] = -1.0E30;

        m_ROIStatistics.min[1] = 1.0E30;
        m_ROIStatistics.max[1] = -1.0E30;

        m_ROIStatistics.min[2] = 1.0E30;
        m_ROIStatistics.max[2] = -1.0E30;

        switch (m_Statistics.dataType)
        {
            case TBYTE:
                calculateMinMax<uint8_t>(roi);
                break;

            case TSHORT:
                calculateMinMax<int16_t>(roi);
                break;

            case TUSHORT:
                calculateMinMax<uint16_t>(roi);
                break;

            case TLONG:
                calculateMinMax<int32_t>(roi);
                break;

            case TULONG:
                calculateMinMax<uint32_t>(roi);
                break;

            case TFLOAT:
                calculateMinMax<float>(roi);
                break;

            case TLONGLONG:
                calculateMinMax<int64_t>(roi);
                break;

            case TDOUBLE:
                calculateMinMax<double>(roi);
                break;

            default:
                break;
        }

    }
}

void FITSData::calculateMedian(bool refresh, bool roi)
{
    if(!roi)
    {
        int status, nfound = 0;

        status = 0;

        // Only fetch from header if we have a single channel
        // Otherwise, calculate manually.
        if (fptr != nullptr && !refresh)
        {
            status = 0;
            if (fits_read_key_dbl(fptr, "MEDIAN1", &m_Statistics.median[0], nullptr, &status) == 0)
                nfound++;

            // NB. These could fail if missing, which is OK.
            fits_read_key_dbl(fptr, "MEDIAN2", &m_Statistics.median[1], nullptr, &status);
            fits_read_key_dbl(fptr, "MEDIAN3", &m_Statistics.median[2], nullptr, &status);

            if (nfound == 1)
                return;
        }

        m_Statistics.median[RED_CHANNEL] = 0;
        m_Statistics.median[GREEN_CHANNEL] = 0;
        m_Statistics.median[BLUE_CHANNEL] = 0;

        switch (m_Statistics.dataType)
        {
            case TBYTE:
                calculateMedian<uint8_t>();
                break;

            case TSHORT:
                calculateMedian<int16_t>();
                break;

            case TUSHORT:
                calculateMedian<uint16_t>();
                break;

            case TLONG:
                calculateMedian<int32_t>();
                break;

            case TULONG:
                calculateMedian<uint32_t>();
                break;

            case TFLOAT:
                calculateMedian<float>();
                break;

            case TLONGLONG:
                calculateMedian<int64_t>();
                break;

            case TDOUBLE:
                calculateMedian<double>();
                break;

            default:
                break;
        }
    }
    else
    {
        m_ROIStatistics.median[RED_CHANNEL] = 0;
        m_ROIStatistics.median[GREEN_CHANNEL] = 0;
        m_ROIStatistics.median[BLUE_CHANNEL] = 0;

        switch (m_ROIStatistics.dataType)
        {
            case TBYTE:
                calculateMedian<uint8_t>(roi);
                break;

            case TSHORT:
                calculateMedian<int16_t>(roi);
                break;

            case TUSHORT:
                calculateMedian<uint16_t>(roi);
                break;

            case TLONG:
                calculateMedian<int32_t>(roi);
                break;

            case TULONG:
                calculateMedian<uint32_t>(roi);
                break;

            case TFLOAT:
                calculateMedian<float>(roi);
                break;

            case TLONGLONG:
                calculateMedian<int64_t>(roi);
                break;

            case TDOUBLE:
                calculateMedian<double>(roi);
                break;

            default:
                break;
        }

    }
}

template <typename T>
void FITSData::calculateMedian(bool roi)
{
    auto * buffer = reinterpret_cast<T *>(roi ? m_ImageRoiBuffer : m_ImageBuffer);
    const uint32_t maxMedianSize = 500000;
    uint32_t medianSize = roi ? m_ROIStatistics.samples_per_channel : m_Statistics.samples_per_channel;
    uint8_t downsample = 1;
    if (medianSize > maxMedianSize)
    {
        downsample = (static_cast<double>(medianSize) / maxMedianSize) + 0.999;
        medianSize /= downsample;
    }
    // Ideally samples would be declared like this...
    // std::vector<T> samples;
    // Unfortunately this doesn't compile on Mac - see the comments in robuststatistics.cpp for more details
    // So for now declare samples like this...
    std::vector<int32_t> samples;
    samples.reserve(medianSize);

    for (uint8_t n = 0; n < m_Statistics.channels; n++)
    {
        auto *oneChannel = buffer + n * (roi ? m_ROIStatistics.samples_per_channel : m_Statistics.samples_per_channel);
        for (uint32_t upto = 0; upto < (roi ? m_ROIStatistics.samples_per_channel : m_Statistics.samples_per_channel);
                upto += downsample)
            samples.push_back(oneChannel[upto]);
        auto median = Mathematics::RobustStatistics::ComputeLocation(Mathematics::RobustStatistics::LOCATION_MEDIAN, samples);
        roi ? m_ROIStatistics.median[n] = median : m_Statistics.median[n] = median;
    }
}

template <typename T>
QPair<T, T> FITSData::getParitionMinMax(uint32_t start, uint32_t stride, bool roi)
{
    auto * buffer = reinterpret_cast<T *>(roi ? m_ImageRoiBuffer : m_ImageBuffer);
    T min = std::numeric_limits<T>::max();
    T max = std::numeric_limits<T>::min();

    uint32_t end = start + stride;

    for (uint32_t i = start; i < end; i++)
    {
        min = qMin(buffer[i], min);
        max = qMax(buffer[i], max);
        //        if (buffer[i] < min)
        //            min = buffer[i];
        //        else if (buffer[i] > max)
        //            max = buffer[i];
    }

    return qMakePair(min, max);
}

template <typename T>
void FITSData::calculateMinMax(bool roi)
{
    T min = std::numeric_limits<T>::max();
    T max = std::numeric_limits<T>::min();


    // Create N threads
    const uint8_t nThreads = 16;

    for (int n = 0; n < m_Statistics.channels; n++)
    {
        uint32_t cStart = n * (roi ? m_ROIStatistics.samples_per_channel : m_Statistics.samples_per_channel);

        // Calculate how many elements we process per thread
        uint32_t tStride = (roi ? m_ROIStatistics.samples_per_channel : m_Statistics.samples_per_channel) / nThreads;

        // Calculate the final stride since we can have some left over due to division above
        uint32_t fStride = tStride + ((roi ? m_ROIStatistics.samples_per_channel : m_Statistics.samples_per_channel) -
                                      (tStride * nThreads));

        // Start location for inspecting elements
        uint32_t tStart = cStart;

        // List of futures
        QList<QFuture<QPair<T, T>>> futures;

        for (int i = 0; i < nThreads; i++)
        {
            // Run threads
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            futures.append(QtConcurrent::run(&FITSData::getParitionMinMax<T>, this, tStart, (i == (nThreads - 1)) ? fStride : tStride,
                                             roi));
#else
            futures.append(QtConcurrent::run(this, &FITSData::getParitionMinMax<T>, tStart, (i == (nThreads - 1)) ? fStride : tStride,
                                             roi));
#endif
            tStart += tStride;
        }

        // Now wait for results
        for (int i = 0; i < nThreads; i++)
        {
            QPair<T, T> result = futures[i].result();
            min = qMin(result.first, min);
            max = qMax(result.second, max);
        }

        if(!roi)
        {
            m_Statistics.min[n] = min;
            m_Statistics.max[n] = max;
        }
        else
        {
            m_ROIStatistics.min[n] = min;
            m_ROIStatistics.max[n] = max;
        }
    }

}

// This struct is used when returning results from the threaded getSumAndSquaredSum calculations
// used to compute the mean and variance of the image.
struct SumData
{
    double sum;
    double squaredSum;
    double numSamples;
    SumData(double s, double sq, int n) : sum(s), squaredSum(sq), numSamples(n) {}
    SumData() : sum(0), squaredSum(0), numSamples(0) {}
};

template <typename T>
SumData getSumAndSquaredSum(uint32_t start, uint32_t stride, uint8_t *buff)
{
    auto * buffer = reinterpret_cast<T *>(buff);
    const uint32_t end = start + stride;
    double sum = 0;
    double squaredSum = 0;
    for (uint32_t i = start; i < end; i++)
    {
        double sample = buffer[i];
        sum += sample;
        squaredSum += sample * sample;
    }
    const double numSamples = end - start;
    return SumData(sum, squaredSum, numSamples);
}

template <typename T>
void FITSData::calculateStdDev(bool roi )
{
    // Create N threads
    const uint8_t nThreads = 16;

    for (int n = 0; n < m_Statistics.channels; n++)
    {
        uint32_t cStart = n * (roi ? m_ROIStatistics.samples_per_channel : m_Statistics.samples_per_channel);

        // Calculate how many elements we process per thread
        uint32_t tStride = (roi ? m_ROIStatistics.samples_per_channel : m_Statistics.samples_per_channel) / nThreads;

        // Calculate the final stride since we can have some left over due to division above
        uint32_t fStride = tStride + ((roi ? m_ROIStatistics.samples_per_channel : m_Statistics.samples_per_channel) -
                                      (tStride * nThreads));

        // Start location for inspecting elements
        uint32_t tStart = cStart;

        // List of futures
        QList<QFuture<SumData>> futures;

        for (int i = 0; i < nThreads; i++)
        {
            // Run threads
            uint8_t *buff = roi ? m_ImageRoiBuffer : m_ImageBuffer;
            futures.append(QtConcurrent::run(&getSumAndSquaredSum<T>, tStart,
                                             (i == (nThreads - 1)) ? fStride : tStride, buff));
            tStart += tStride;
        }

        // Now wait for results
        double sum = 0, squared_sum = 0;
        double numSamples = 0;
        for (int i = 0; i < nThreads; i++)
        {
            const SumData result = futures[i].result();
            sum += result.sum;
            squared_sum += result.squaredSum;
            numSamples += result.numSamples;

        }
        if (numSamples <= 0) continue;
        const double mean = sum / numSamples;
        const double variance = squared_sum / numSamples - mean * mean;
        if(!roi)
        {
            m_Statistics.mean[n]   = mean;
            m_Statistics.stddev[n] = sqrt(variance);
        }
        else
        {
            m_ROIStatistics.mean[n] = mean;
            m_ROIStatistics.stddev[n] = sqrt(variance);
        }
    }
}

QVector<double> FITSData::createGaussianKernel(int size, double sigma)
{
    QVector<double> kernel(size * size);
    kernel.fill(0.0, size * size);

    double kernelSum = 0.0;
    int fOff = (size - 1) / 2;
    double normal = 1.0 / (2.0 * M_PI * sigma * sigma);
    for (int y = -fOff; y <= fOff; y++)
    {
        for (int x = -fOff; x <= fOff; x++)
        {
            double distance = ((y * y) + (x * x)) / (2.0 * sigma * sigma);
            int index = (y + fOff) * size + (x + fOff);
            kernel[index] = normal * qExp(-distance);
            kernelSum += kernel.at(index);
        }
    }
    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            int index = y * size + x;
            kernel[index] = kernel.at(index) * 1.0 / kernelSum;
        }
    }

    return kernel;
}

template <typename T>
void FITSData::convolutionFilter(const QVector<double> &kernel, int kernelSize)
{
    T * imagePtr = reinterpret_cast<T *>(m_ImageBuffer);

    // Create variable for pixel data for each kernel
    T gt = 0;

    // This is how much your center pixel is offset from the border of your kernel
    int fOff = (kernelSize - 1) / 2;

    // Start with the pixel that is offset fOff from top and fOff from the left side
    // this is so entire kernel is on your image
    for (int offsetY = 0; offsetY < m_Statistics.height; offsetY++)
    {
        for (int offsetX = 0; offsetX < m_Statistics.width; offsetX++)
        {
            // reset gray value to 0
            gt = 0;
            // position of the kernel center pixel
            int byteOffset = offsetY * m_Statistics.width + offsetX;

            // kernel calculations
            for (int filterY = -fOff; filterY <= fOff; filterY++)
            {
                for (int filterX = -fOff; filterX <= fOff; filterX++)
                {
                    if ((offsetY + filterY) >= 0 && (offsetY + filterY) < m_Statistics.height
                            && ((offsetX + filterX) >= 0 && (offsetX + filterX) < m_Statistics.width ))
                    {

                        int calcOffset = byteOffset + filterX + filterY * m_Statistics.width;
                        int index = (filterY + fOff) * kernelSize + (filterX + fOff);
                        double kernelValue = kernel.at(index);
                        gt += (imagePtr[calcOffset]) * kernelValue;
                    }
                }
            }

            // set new data in the other byte array for your image data
            imagePtr[byteOffset] = gt;
        }
    }
}

template <typename T>
void FITSData::gaussianBlur(int kernelSize, double sigma)
{
    // Size must be an odd number!
    if (kernelSize % 2 == 0)
    {
        kernelSize--;
        qCInfo(KSTARS_FITS) << "Warning, size must be an odd number, correcting size to " << kernelSize;
    }
    // Edge must be a positive number!
    if (kernelSize < 1)
    {
        kernelSize = 1;
    }

    QVector<double> gaussianKernel = createGaussianKernel(kernelSize, sigma);
    convolutionFilter<T>(gaussianKernel, kernelSize);
}

void FITSData::setMinMax(double newMin, double newMax, uint8_t channel)
{
    m_Statistics.min[channel] = newMin;
    m_Statistics.max[channel] = newMax;
}

bool FITSData::parseHeader(const bool stack)
{
    char * header = nullptr;
    int status = 0, nkeys = 0;

    if (fits_hdr2str(stack ? m_Stackfptr : fptr, 0, nullptr, 0, &header, &nkeys, &status))
    {
        fits_report_error(stderr, status);
        free(header);
        return false;
    }

    stack ? m_StackHeaderRecords.clear() : m_HeaderRecords.clear();
    QString recordList = QString(header);

    for (int i = 0; i < nkeys; i++)
    {
        Record oneRecord;
        // Quotes cause issues for simplified below so we're removing them.
        QString record = recordList.mid(i * 80, 80).remove("'");
        QStringList properties = record.split(QRegularExpression("[=/]"));
        // If it is only a comment
        if (properties.size() == 1)
        {
            oneRecord.key = properties[0].mid(0, 7);
            oneRecord.comment = properties[0].mid(8).simplified();
        }
        else
        {
            oneRecord.key = properties[0].simplified();
            oneRecord.value = properties[1].simplified();
            if (properties.size() > 2)
                oneRecord.comment = properties[2].simplified();

            // Try to guess the value.
            // Test for integer & double. If neither, then leave it as "string".
            bool ok = false;

            // Is it Integer?
            oneRecord.value.toInt(&ok);
            if (ok)
                oneRecord.value.convert(QMetaType::Int);
            else
            {
                // Is it double?
                oneRecord.value.toDouble(&ok);
                if (ok)
                    oneRecord.value.convert(QMetaType::Double);
            }
        }

        stack ? m_StackHeaderRecords.append(oneRecord) : m_HeaderRecords.append(oneRecord);
    }

    free(header);

    return true;
}

bool FITSData::getRecordValue(const QString &key, QVariant &value) const
{
    auto result = std::find_if(m_HeaderRecords.begin(), m_HeaderRecords.end(), [&key](const Record & oneRecord)
    {
        return (oneRecord.key == key && oneRecord.value.isValid());
    });

    if (result != m_HeaderRecords.end())
    {
        value = (*result).value;
        return true;
    }
    return false;
}

void FITSData::updateRecordValue(const QString &key, QVariant value, const QString &comment, const bool stack)
{
    auto &headerRecords = (stack) ? m_StackHeaderRecords : m_HeaderRecords;
    auto result = std::find_if(headerRecords.begin(), headerRecords.end(), [&key](const Record & oneRecord)
    {
        return (oneRecord.key == key && oneRecord.value.isValid());
    });

    if (result != headerRecords.end())
    {
        (*result).value = value;
    }
    else
    {
        // Add item as penultimate entry (END is usually the last one).
        FITSData::Record record = {key, value.toString(), comment};
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        headerRecords.insert(std::max(0LL, headerRecords.size() - 1), record);
#else
        headerRecords.insert(std::max(0, headerRecords.size() - 1), record);
#endif
    }
}

bool FITSData::parseSolution(FITSImage::Solution &solution) const
{
    dms angleValue;
    bool raOK = false, deOK = false, coordOK = false, scaleOK = false;
    QVariant value;

    // Reset all
    solution.fieldWidth = solution.fieldHeight = solution.pixscale = solution.ra = solution.dec = -1;

    // RA
    if (getRecordValue("OBJCTRA", value))
    {
        angleValue = dms::fromString(value.toString(), false);
        solution.ra = angleValue.Degrees();
        raOK = true;
    }
    else if (getRecordValue("RA", value))
    {
        solution.ra = value.toDouble(&raOK);
    }

    // DE
    if (getRecordValue("OBJCTDEC", value))
    {
        angleValue = dms::fromString(value.toString(), true);
        solution.dec = angleValue.Degrees();
        deOK = true;
    }
    else if (getRecordValue("DEC", value))
    {
        solution.dec = value.toDouble(&deOK);
    }

    coordOK = raOK && deOK;

    // PixScale
    double scale = -1;
    if (getRecordValue("SCALE", value))
    {
        scale = value.toDouble();
    }

    double focal_length = -1;
    if (getRecordValue("FOCALLEN", value))
    {
        focal_length = value.toDouble();
    }

    double pixsize1 = -1, pixsize2 = -1;
    // Pixel Size 1
    if (getRecordValue("PIXSIZE1", value))
    {
        pixsize1 = value.toDouble();
    }
    else if (getRecordValue("XPIXSZ", value))
    {
        pixsize1 = value.toDouble();
    }
    // Pixel Size 2
    if (getRecordValue("PIXSIZE2", value))
    {
        pixsize2 = value.toDouble();
    }
    else if (getRecordValue("YPIXSZ", value))
    {
        pixsize2 = value.toDouble();
    }

    int binx = 1, biny = 1;
    // Binning X
    if (getRecordValue("XBINNING", value))
    {
        binx = value.toDouble();
    }
    // Binning Y
    if (getRecordValue("YBINNING", value))
    {
        biny = value.toDouble();
    }

    if (pixsize1 > 0 && pixsize2 > 0)
    {
        // If we have scale, then that's it
        if (scale > 0)
        {
            // Arcsecs per pixel
            solution.pixscale = scale;
            // Arcmins
            solution.fieldWidth = (m_Statistics.width * scale) / 60.0;
            // Arcmins, and account for pixel ratio if non-squared.
            solution.fieldHeight = (m_Statistics.height * scale * (pixsize2 / pixsize1)) / 60.0;
        }
        else if (focal_length > 0)
        {
            // Arcmins
            solution.fieldWidth = ((206264.8062470963552 * m_Statistics.width * (pixsize1 / 1000.0)) / (focal_length * binx)) / 60.0;
            // Arsmins
            solution.fieldHeight = ((206264.8062470963552 * m_Statistics.height * (pixsize2 / 1000.0)) / (focal_length * biny)) / 60.0;
            // Arcsecs per pixel
            solution.pixscale = (206264.8062470963552 * (pixsize1 / 1000.0)) / (focal_length * binx);
        }

        scaleOK = true;
    }

    return (coordOK || scaleOK);
}

QFuture<bool> FITSData::findStars(StarAlgorithm algorithm, const QRect &trackingBox)
{
    if (m_StarFindFuture.isRunning())
        m_StarFindFuture.waitForFinished();

    starAlgorithm = algorithm;
    qDeleteAll(starCenters);
    starCenters.clear();
    starsSearched = true;

    switch (algorithm)
    {
        case ALGORITHM_SEP:
        {
            m_StarDetector.reset(new FITSSEPDetector(this));
            m_StarDetector->setSettings(m_SourceExtractorSettings);
            if (m_Mode == FITS_NORMAL && trackingBox.isNull())
            {
                if (Options::quickHFR())
                {
                    //Just finds stars in the center 25% of the image.
                    const int w = getStatistics().width;
                    const int h = getStatistics().height;
                    QRect middle(static_cast<int>(w * 0.25), static_cast<int>(h * 0.25), w / 2, h / 2);
                    m_StarFindFuture = m_StarDetector->findSources(middle);
                    return m_StarFindFuture;
                }
            }
            m_StarFindFuture = m_StarDetector->findSources(trackingBox);
            return m_StarFindFuture;
        }

        case ALGORITHM_GRADIENT:
        default:
        {
            m_StarDetector.reset(new FITSGradientDetector(this));
            m_StarDetector->setSettings(m_SourceExtractorSettings);
            m_StarFindFuture = m_StarDetector->findSources(trackingBox);
            return m_StarFindFuture;
        }

        case ALGORITHM_CENTROID:
        {
#ifndef KSTARS_LITE
            m_StarDetector.reset(new FITSCentroidDetector(this));
            m_StarDetector->setSettings(m_SourceExtractorSettings);
            // We need JMIndex calculated from histogram
            if (!isHistogramConstructed())
                constructHistogram();
            m_StarDetector->configure("JMINDEX", m_JMIndex);
            m_StarFindFuture = m_StarDetector->findSources(trackingBox);
            return m_StarFindFuture;
        }
#else
            {
                m_StarDetector.reset(new FITSCentroidDetector(this));
                m_StarFindFuture = starDetector->findSources(trackingBox);
                return m_StarFindFuture;
            }
#endif

        case ALGORITHM_THRESHOLD:
        {
            m_StarDetector.reset(new FITSThresholdDetector(this));
            m_StarDetector->setSettings(m_SourceExtractorSettings);
            m_StarDetector->configure("THRESHOLD_PERCENTAGE", Options::focusThreshold());
            m_StarFindFuture =  m_StarDetector->findSources(trackingBox);
            return m_StarFindFuture;
        }

        case ALGORITHM_BAHTINOV:
        {
            m_StarDetector.reset(new FITSBahtinovDetector(this));
            m_StarDetector->setSettings(m_SourceExtractorSettings);
            m_StarDetector->configure("NUMBER_OF_AVERAGE_ROWS", Options::focusMultiRowAverage());
            m_StarFindFuture = m_StarDetector->findSources(trackingBox);
            return m_StarFindFuture;
        }
    }
}

int FITSData::filterStars(QSharedPointer<ImageMask> mask)
{
    if (mask.isNull() == false)
    {
        starCenters.erase(std::remove_if(starCenters.begin(), starCenters.end(),
                                         [&](Edge * edge)
        {
            return (mask->isVisible(edge->x, edge->y) == false);
        }), starCenters.end());
    }

    return starCenters.count();

}

double FITSData::getHFR(HFRType type)
{
    if (starCenters.empty())
        return -1;

    if (cacheHFR >= 0 && cacheHFRType == type)
        return cacheHFR;

    m_SelectedHFRStar.invalidate();

    if (type == HFR_MAX)
    {

        int maxVal   = 0;
        int maxIndex = 0;
        for (int i = 0; i < starCenters.count(); i++)
        {
            if (starCenters[i]->HFR > maxVal)
            {
                maxIndex = i;
                maxVal   = starCenters[i]->HFR;
            }
        }

        m_SelectedHFRStar = *starCenters[maxIndex];
        cacheHFR = starCenters[maxIndex]->HFR;
        cacheHFRType = type;
        return cacheHFR;
    }
    else if (type == HFR_HIGH)
    {
        // Reject all stars within 10% of border
        int minX = width() / 10;
        int minY = height() / 10;
        int maxX = width() - minX;
        int maxY = height() - minY;
        starCenters.erase(std::remove_if(starCenters.begin(), starCenters.end(), [minX, minY, maxX, maxY](Edge * oneStar)
        {
            return (oneStar->x < minX || oneStar->x > maxX || oneStar->y < minY || oneStar->y > maxY);
        }), starCenters.end());
        // Top 5%
        if (starCenters.empty())
            return -1;

        m_SelectedHFRStar = *starCenters[static_cast<int>(starCenters.size() * 0.05)];
        cacheHFR = m_SelectedHFRStar.HFR;
        cacheHFRType = type;
        return cacheHFR;
    }
    else if (type == HFR_MEDIAN)
    {
        std::nth_element(starCenters.begin(), starCenters.begin() + starCenters.size() / 2, starCenters.end());
        m_SelectedHFRStar = *starCenters[starCenters.size() / 2];

        cacheHFR = m_SelectedHFRStar.HFR;
        cacheHFRType = type;
        return cacheHFR;
    }

    // We may remove saturated stars from the HFR calculation, if we have enough stars.
    // No real way to tell the scale, so only remove saturated stars with range 0 -> 2**16
    // for non-byte types. Unsigned types and floating types, or really any pixels whose
    // range isn't 0-64 (or 0-255 for bytes) won't have their saturated stars removed.
    const int saturationValue = m_Statistics.dataType == TBYTE ? 250 : 50000;
    int numSaturated = 0;
    for (auto center : starCenters)
        if (center->val > saturationValue)
            numSaturated++;
    const bool removeSaturatedStars = starCenters.size() - numSaturated > 20;
    if (removeSaturatedStars && numSaturated > 0)
        qCDebug(KSTARS_FITS) << "Removing " << numSaturated << " stars from HFR calculation";

    std::vector<double> HFRs;

    for (auto center : starCenters)
    {
        if (removeSaturatedStars && center->val > saturationValue) continue;

        if (type == HFR_AVERAGE)
            HFRs.push_back(center->HFR);
        else
        {
            // HFR_ADJ_AVERAGE - so adjust the HFR based on the stars brightness
            // HFRadj = HFR.erf(sqrt(ln(peak/background)))/(1 - background/peak)
            // Sanity check inputs to prevent equation blowing up
            if (m_SkyBackground.mean <= 0.0 || center->val < m_SkyBackground.mean)
            {
                HFRs.push_back(center->HFR);
                qCDebug(KSTARS_FITS) << "HFR Adj, sky background " << m_SkyBackground.mean << " star peak " << center->val <<
                                     " not adjusting";
            }
            else
            {
                const double a_div_b = center->val / m_SkyBackground.mean;
                const double factor = erf(sqrt(log(a_div_b))) / (1 - (1 / a_div_b));
                HFRs.push_back(center->HFR * factor);
                qCDebug(KSTARS_FITS) << "HFR Adj, brightness adjusted from " << center->HFR << " to " << center->HFR * factor;
            }
        }
    }

    auto m = Mathematics::RobustStatistics::ComputeLocation(Mathematics::RobustStatistics::LOCATION_SIGMACLIPPING,
             HFRs, 2);

    cacheHFR = m;
    cacheHFRType = HFR_AVERAGE;
    return cacheHFR;
}

double FITSData::getHFR(int x, int y, double scale)
{
    if (starCenters.empty())
        return -1;

    for (int i = 0; i < starCenters.count(); i++)
    {
        const int maxDist = std::max(2, static_cast<int>(0.5 + 2 * starCenters[i]->width / scale));
        const int dx = std::fabs(starCenters[i]->x - x);
        const int dy = std::fabs(starCenters[i]->y - y);
        if (dx <= maxDist && dy <= maxDist)
        {
            m_SelectedHFRStar = *starCenters[i];
            return starCenters[i]->HFR;
        }
    }

    return -1;
}

double FITSData::getEccentricity()
{
    if (starCenters.empty())
        return -1;
    if (cacheEccentricity >= 0)
        return cacheEccentricity;
    std::vector<float> eccs;
    for (const auto &s : starCenters)
        eccs.push_back(s->ellipticity);
    int middle = eccs.size() / 2;
    std::nth_element(eccs.begin(), eccs.begin() + middle, eccs.end());
    float medianEllipticity = eccs[middle];

    // SEP gives us the ellipticity (flattening) directly.
    // To get the eccentricity, use this formula:
    // e = sqrt(ellipticity * (2 - ellipticity))
    // https://en.wikipedia.org/wiki/Eccentricity_(mathematics)#Ellipses
    const float eccentricity = sqrt(medianEllipticity * (2 - medianEllipticity));
    cacheEccentricity = eccentricity;
    return eccentricity;
}

// Pass a star extraction abort request along to the extraction process
void FITSData::abortStarDetection()
{
    if (m_StarDetector)
        m_StarDetector->abort();
}

void FITSData::applyFilter(FITSScale type, uint8_t * image, QVector<double> * min, QVector<double> * max)
{
    if (type == FITS_NONE)
        return;

    QVector<double> dataMin(3);
    QVector<double> dataMax(3);

    if (min)
        dataMin = *min;
    if (max)
        dataMax = *max;

    switch (type)
    {
        case FITS_AUTO_STRETCH:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = m_Statistics.mean[i] - m_Statistics.stddev[i];
                dataMax[i] = m_Statistics.mean[i] + m_Statistics.stddev[i] * 3;
            }
        }
        break;

        case FITS_HIGH_CONTRAST:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = m_Statistics.mean[i] + m_Statistics.stddev[i];
                dataMax[i] = m_Statistics.mean[i] + m_Statistics.stddev[i] * 3;
            }
        }
        break;

        case FITS_HIGH_PASS:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = m_Statistics.mean[i];
            }
        }
        break;

        default:
            break;
    }

    switch (m_Statistics.dataType)
    {
        case TBYTE:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = dataMin[i] < 0 ? 0 : dataMin[i];
                dataMax[i] = dataMax[i] > UINT8_MAX ? UINT8_MAX : dataMax[i];
            }
            applyFilter<uint8_t>(type, image, &dataMin, &dataMax);
        }
        break;

        case TSHORT:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = dataMin[i] < INT16_MIN ? INT16_MIN : dataMin[i];
                dataMax[i] = dataMax[i] > INT16_MAX ? INT16_MAX : dataMax[i];
            }
            applyFilter<uint16_t>(type, image, &dataMin, &dataMax);
        }

        break;

        case TUSHORT:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = dataMin[i] < 0 ? 0 : dataMin[i];
                dataMax[i] = dataMax[i] > UINT16_MAX ? UINT16_MAX : dataMax[i];
            }
            applyFilter<uint16_t>(type, image, &dataMin, &dataMax);
        }
        break;

        case TLONG:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = dataMin[i] < INT_MIN ? INT_MIN : dataMin[i];
                dataMax[i] = dataMax[i] > INT_MAX ? INT_MAX : dataMax[i];
            }
            applyFilter<uint16_t>(type, image, &dataMin, &dataMax);
        }
        break;

        case TULONG:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = dataMin[i] < 0 ? 0 : dataMin[i];
                dataMax[i] = dataMax[i] > UINT_MAX ? UINT_MAX : dataMax[i];
            }
            applyFilter<uint16_t>(type, image, &dataMin, &dataMax);
        }
        break;

        case TFLOAT:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = dataMin[i] < FLT_MIN ? FLT_MIN : dataMin[i];
                dataMax[i] = dataMax[i] > FLT_MAX ? FLT_MAX : dataMax[i];
            }
            applyFilter<float>(type, image, &dataMin, &dataMax);
        }
        break;

        case TLONGLONG:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = dataMin[i] < LLONG_MIN ? LLONG_MIN : dataMin[i];
                dataMax[i] = dataMax[i] > LLONG_MAX ? LLONG_MAX : dataMax[i];
            }

            applyFilter<long>(type, image, &dataMin, &dataMax);
        }
        break;

        case TDOUBLE:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = dataMin[i] < DBL_MIN ? DBL_MIN : dataMin[i];
                dataMax[i] = dataMax[i] > DBL_MAX ? DBL_MAX : dataMax[i];
            }
            applyFilter<double>(type, image, &dataMin, &dataMax);
        }

        break;

        default:
            return;
    }

    if (min != nullptr)
        *min = dataMin;
    if (max != nullptr)
        *max = dataMax;

    emit dataChanged();
}

template <typename T>
void FITSData::applyFilter(FITSScale type, uint8_t * targetImage, QVector<double> * targetMin, QVector<double> * targetMax)
{
    bool calcStats = false;
    T * image = nullptr;

    if (targetImage)
        image = reinterpret_cast<T *>(targetImage);
    else
    {
        image     = reinterpret_cast<T *>(m_ImageBuffer);
        calcStats = true;
    }

    T min[3], max[3];
    for (int i = 0; i < 3; i++)
    {
        min[i] = (*targetMin)[i] < std::numeric_limits<T>::min() ? std::numeric_limits<T>::min() : (*targetMin)[i];
        max[i] = (*targetMax)[i] > std::numeric_limits<T>::max() ? std::numeric_limits<T>::max() : (*targetMax)[i];
    }


    // Create N threads
    const uint8_t nThreads = 16;

    uint32_t width  = m_Statistics.width;
    uint32_t height = m_Statistics.height;

    //QTime timer;
    //timer.start();
    switch (type)
    {
        case FITS_AUTO:
        case FITS_LINEAR:
        case FITS_AUTO_STRETCH:
        case FITS_HIGH_CONTRAST:
        case FITS_LOG:
        case FITS_SQRT:
        case FITS_HIGH_PASS:
        {
            // List of futures
            QList<QFuture<void>> futures;
            QVector<double> coeff(3);

            if (type == FITS_LOG)
            {
                for (int i = 0; i < 3; i++)
                    coeff[i] = max[i] / std::log(1 + max[i]);
            }
            else if (type == FITS_SQRT)
            {
                for (int i = 0; i < 3; i++)
                    coeff[i] = max[i] / sqrt(max[i]);
            }

            for (int n = 0; n < m_Statistics.channels; n++)
            {
                if (type == FITS_HIGH_PASS)
                    min[n] = m_Statistics.mean[n];

                uint32_t cStart = n * m_Statistics.samples_per_channel;

                // Calculate how many elements we process per thread
                uint32_t tStride = m_Statistics.samples_per_channel / nThreads;

                // Calculate the final stride since we can have some left over due to division above
                uint32_t fStride = tStride + (m_Statistics.samples_per_channel - (tStride * nThreads));

                T * runningBuffer = image + cStart;

                if (type == FITS_LOG)
                {
                    for (int i = 0; i < nThreads; i++)
                    {
                        // Run threads
                        futures.append(QtConcurrent::map(runningBuffer, (runningBuffer + ((i == (nThreads - 1)) ? fStride : tStride)), [min, max,
                                                              coeff, n](T & a)
                        {
                            a = qBound(min[n], static_cast<T>(round(coeff[n] * std::log(1 + qBound(min[n], a, max[n])))), max[n]);
                        }));

                        runningBuffer += tStride;
                    }
                }
                else if (type == FITS_SQRT)
                {
                    for (int i = 0; i < nThreads; i++)
                    {
                        // Run threads
                        futures.append(QtConcurrent::map(runningBuffer, (runningBuffer + ((i == (nThreads - 1)) ? fStride : tStride)), [min, max,
                                                              coeff, n](T & a)
                        {
                            a = qBound(min[n], static_cast<T>(round(coeff[n] * a)), max[n]);
                        }));
                    }

                    runningBuffer += tStride;
                }
                else
                {
                    for (int i = 0; i < nThreads; i++)
                    {
                        // Run threads
                        futures.append(QtConcurrent::map(runningBuffer, (runningBuffer + ((i == (nThreads - 1)) ? fStride : tStride)), [min, max,
                                                              n](T & a)
                        {
                            a = qBound(min[n], a, max[n]);
                        }));
                        runningBuffer += tStride;
                    }
                }
            }

            for (int i = 0; i < nThreads * m_Statistics.channels; i++)
                futures[i].waitForFinished();

            if (calcStats)
            {
                for (int i = 0; i < 3; i++)
                {
                    m_Statistics.min[i] = min[i];
                    m_Statistics.max[i] = max[i];
                }
                calculateStdDev<T>();
            }
        }
        break;

        case FITS_EQUALIZE:
        {
#ifndef KSTARS_LITE
            if (!isHistogramConstructed())
                constructHistogram();

            T bufferVal                    = 0;

            double coeff = 255.0 / (height * width);
            uint32_t row = 0;
            uint32_t index = 0;

            for (int i = 0; i < m_Statistics.channels; i++)
            {
                uint32_t offset = i * m_Statistics.samples_per_channel;
                for (uint32_t j = 0; j < height; j++)
                {
                    row = offset + j * width;
                    for (uint32_t k = 0; k < width; k++)
                    {
                        index     = k + row;
                        bufferVal = (image[index] - min[i]) / m_HistogramBinWidth[i];

                        if (bufferVal >= m_CumulativeFrequency[i].size())
                            bufferVal = m_CumulativeFrequency[i].size() - 1;

                        image[index] = qBound(min[i], static_cast<T>(round(coeff * m_CumulativeFrequency[i][bufferVal])), max[i]);
                    }
                }
            }
#endif
        }
        if (calcStats)
            calculateStats(true, false);
        break;

        // Based on http://www.librow.com/articles/article-1
        case FITS_MEDIAN:
        {
            uint8_t BBP      = m_Statistics.bytesPerPixel;
            auto * extension = new T[(width + 2) * (height + 2)];
            //   Check memory allocation
            if (!extension)
                return;
            //   Create image extension
            for (uint32_t ch = 0; ch < m_Statistics.channels; ch++)
            {
                uint32_t offset = ch * m_Statistics.samples_per_channel;
                uint32_t N = width, M = height;

                for (uint32_t i = 0; i < M; ++i)
                {
                    memcpy(extension + (N + 2) * (i + 1) + 1, image + (N * i) + offset, N * BBP);
                    extension[(N + 2) * (i + 1)]     = image[N * i + offset];
                    extension[(N + 2) * (i + 2) - 1] = image[N * (i + 1) - 1 + offset];
                }
                //   Fill first line of image extension
                memcpy(extension, extension + N + 2, (N + 2) * BBP);
                //   Fill last line of image extension
                memcpy(extension + (N + 2) * (M + 1), extension + (N + 2) * M, (N + 2) * BBP);
                //   Call median filter implementation

                N = width + 2;
                M = height + 2;

                //   Move window through all elements of the image
                for (uint32_t m = 1; m < M - 1; ++m)
                    for (uint32_t n = 1; n < N - 1; ++n)
                    {
                        //   Pick up window elements
                        int k = 0;
                        float window[9];

                        memset(&window[0], 0, 9 * sizeof(float));
                        for (uint32_t j = m - 1; j < m + 2; ++j)
                            for (uint32_t i = n - 1; i < n + 2; ++i)
                                window[k++] = extension[j * N + i];
                        //   Order elements (only half of them)
                        for (uint32_t j = 0; j < 5; ++j)
                        {
                            //   Find position of minimum element
                            int mine = j;
                            for (uint32_t l = j + 1; l < 9; ++l)
                                if (window[l] < window[mine])
                                    mine = l;
                            //   Put found minimum element in its place
                            const float temp = window[j];
                            window[j]        = window[mine];
                            window[mine]     = temp;
                        }
                        //   Get result - the middle element
                        image[(m - 1) * (N - 2) + n - 1 + offset] = window[4];
                    }
            }

            //   Free memory
            delete[] extension;

            if (calcStats)
                calculateStdDev<T>();
        }
        break;

        case FITS_GAUSSIAN:
            gaussianBlur<T>(Options::focusGaussianKernelSize(), Options::focusGaussianSigma());
            if (calcStats)
                calculateStats(true, false);
            break;

        case FITS_ROTATE_CW:
            rotFITS<T>(90, 0);
            rotCounter++;
            break;

        case FITS_ROTATE_CCW:
            rotFITS<T>(270, 0);
            rotCounter--;
            break;

        case FITS_MOUNT_FLIP_H:
            rotFITS<T>(0, 1);
            flipHCounter++;
            break;

        case FITS_MOUNT_FLIP_V:
            rotFITS<T>(0, 2);
            flipVCounter++;
            break;

        default:
            break;
    }
}

QList<Edge *> FITSData::getStarCentersInSubFrame(QRect subFrame) const
{
    QList<Edge *> starCentersInSubFrame;
    for (int i = 0; i < starCenters.count(); i++)
    {
        int x = static_cast<int>(starCenters[i]->x);
        int y = static_cast<int>(starCenters[i]->y);
        if(subFrame.contains(x, y))
        {
            starCentersInSubFrame.append(starCenters[i]);
        }
    }
    return starCentersInSubFrame;
}

bool FITSData::loadWCS()
{
#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)

    if (m_WCSState == Success)
    {
        qCWarning(KSTARS_FITS) << "WCS data already loaded";
        return true;
    }

    if (m_WCSHandle != nullptr)
    {
        wcsvfree(&m_nwcs, &m_WCSHandle);
        m_nwcs = 0;
        m_WCSHandle = nullptr;
    }

    qCDebug(KSTARS_FITS) << "Started WCS Data Processing...";

    QByteArray header_str;
    int status = 0;
    int nkeyrec = 0, nreject = 0;
    if (fptr)
    {
        char *header = nullptr;
        if (fits_hdr2str(fptr, 1, nullptr, 0, &header, &nkeyrec, &status))
        {
            char errmsg[512];
            fits_get_errstatus(status, errmsg);
            m_LastError = errmsg;
            m_WCSState = Failure;
            return false;
        }
        header_str = QByteArray(header);
        fits_free_memory(header, &status);
    }
    else
    {
        nkeyrec = 1;
        for(auto &fitsKeyword : m_HeaderRecords)
        {
            QByteArray rec;
            rec.append(fitsKeyword.key.leftJustified(8, ' ').toLatin1());
            rec.append("= ");
            rec.append(fitsKeyword.value.toByteArray());
            rec.append(" / ");
            rec.append(fitsKeyword.comment.toLatin1());
            header_str.append(rec.leftJustified(80, ' ', true));
            nkeyrec++;
        }
        header_str.append(QByteArray("END").leftJustified(80));
    }

    if ((status = wcspih(header_str.data(), nkeyrec, WCSHDR_all, 0, &nreject, &m_nwcs, &m_WCSHandle)) != 0)
    {
        wcsvfree(&m_nwcs, &m_WCSHandle);
        m_WCSHandle = nullptr;
        m_nwcs = 0;
        m_LastError = QString("wcspih ERROR %1: %2.").arg(status).arg(wcshdr_errmsg[status]);
        m_WCSState = Failure;
        return false;
    }

    if (m_WCSHandle == nullptr)
    {
        m_LastError = i18n("No world coordinate systems found.");
        m_WCSState = Failure;
        return false;
    }

    // FIXME: Call above goes through EVEN if no WCS is present, so we're adding this to return for now.
    if (m_WCSHandle->crpix[0] == 0)
    {
        wcsvfree(&m_nwcs, &m_WCSHandle);
        m_WCSHandle = nullptr;
        m_nwcs = 0;
        m_LastError = i18n("No world coordinate systems found.");
        m_WCSState = Failure;
        return false;
    }

    cdfix(m_WCSHandle);
    if ((status = wcsset(m_WCSHandle)) != 0)
    {
        wcsvfree(&m_nwcs, &m_WCSHandle);
        m_WCSHandle = nullptr;
        m_nwcs = 0;
        m_LastError = QString("wcsset error %1: %2.").arg(status).arg(wcs_errmsg[status]);
        m_WCSState = Failure;
        return false;
    }

    m_ObjectsSearched = false;
    m_CatObjectsSearched = false;
    m_WCSState = Success;
    HasWCS = true;

    qCDebug(KSTARS_FITS) << "Finished WCS Data processing...";

    return true;
#else
    return false;
#endif
}

bool FITSData::wcsToPixel(const SkyPoint &wcsCoord, QPointF &wcsPixelPoint, QPointF &wcsImagePoint)
{
#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)
    int status, stat[NWCSFIX];
    double imgcrd[NWCSFIX], phi, pixcrd[NWCSFIX], theta, worldcrd[NWCSFIX];

    if (m_WCSHandle == nullptr)
    {
        m_LastError = i18n("No world coordinate systems found.");
        return false;
    }

    worldcrd[0] = wcsCoord.ra0().Degrees();
    worldcrd[1] = wcsCoord.dec0().Degrees();

    if ((status = wcss2p(m_WCSHandle, 1, 2, worldcrd, &phi, &theta, imgcrd, pixcrd, stat)) != 0)
    {
        m_LastError = QString("wcss2p error %1: %2.").arg(status).arg(wcs_errmsg[status]);
        return false;
    }

    wcsImagePoint.setX(imgcrd[0]);
    wcsImagePoint.setY(imgcrd[1]);

    wcsPixelPoint.setX(pixcrd[0]);
    wcsPixelPoint.setY(pixcrd[1]);

    return true;
#else
    Q_UNUSED(wcsCoord);
    Q_UNUSED(wcsPixelPoint);
    Q_UNUSED(wcsImagePoint);
    return false;
#endif
}

bool FITSData::pixelToWCS(const QPointF &wcsPixelPoint, SkyPoint &wcsCoord)
{
#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)
    int status, stat[NWCSFIX];
    double imgcrd[NWCSFIX], phi, pixcrd[NWCSFIX], theta, world[NWCSFIX];

    if (m_WCSHandle == nullptr)
    {
        m_LastError = i18n("No world coordinate systems found.");
        return false;
    }

    pixcrd[0] = wcsPixelPoint.x();
    pixcrd[1] = wcsPixelPoint.y();

    if ((status = wcsp2s(m_WCSHandle, 1, 2, pixcrd, imgcrd, &phi, &theta, world, stat)) != 0)
    {
        m_LastError = QString("wcsp2s error %1: %2.").arg(status).arg(wcs_errmsg[status]);
        return false;
    }
    else
    {
        wcsCoord.setRA0(world[0] / 15.0);
        wcsCoord.setDec0(world[1]);
    }

    return true;
#else
    Q_UNUSED(wcsPixelPoint);
    Q_UNUSED(wcsCoord);
    return false;
#endif
}

#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)
bool FITSData::searchObjects()
{
    return (Options::fitsCatalog() == CAT_SKYMAP) ? searchSkyMapObjects() : searchCatObjects();
}

bool FITSData::searchSkyMapObjects()
{
    if (m_ObjectsSearched)
        return true;

    m_ObjectsSearched = true;

    SkyPoint startPoint;
    SkyPoint endPoint;

    pixelToWCS(QPointF(0, 0), startPoint);
    pixelToWCS(QPointF(width() - 1, height() - 1), endPoint);

    return findObjectsInImage(startPoint, endPoint);
}

bool FITSData::searchCatObjects()
{
    if (m_CatObjectsSearched)
        return true;

    m_CatObjectsSearched = true;

    SkyPoint searchCenter;
    double radius;
    QPoint pt;
    bool ok = true;
    if (catROIRadius() > 0)
    {
        // A ROI has been set so this is a request where the user set a ROI
        pt = catROIPt();
        QPoint edgePt = QPoint(pt.x() + catROIRadius(), pt.y());
        SkyPoint searchEdge;
        ok = pixelToWCS(pt, searchCenter);
        if (ok)
            ok = pixelToWCS(edgePt, searchEdge);
        if (ok)
        {
            QVariant date;
            KSNumbers * num = nullptr;

            if (getRecordValue("DATE-OBS", date))
            {
                QString tsString(date.toString());
                tsString = tsString.remove('\'').trimmed();
                // Add Zulu time to indicate UTC
                tsString += "Z";

                QDateTime ts = QDateTime::fromString(tsString, Qt::ISODate);

                if (ts.isValid())
                    num = new KSNumbers(KStarsDateTime(ts).djd());
            }

            //Set to current time if the above does not work.
            if (num == nullptr)
                num = new KSNumbers(KStarsData::Instance()->ut().djd());

            searchCenter.updateCoordsNow(num);
            searchEdge.updateCoordsNow(num);
            radius = searchCenter.angularDistanceTo(&searchEdge).Degrees() * 60; // Arc minutes

            delete num;
        }
    }
    else
    {
        // No ROI setup so this is a first call to use a circle of 0.5 arcmins in the center of the image
        // Lets calculate the number of pixels that correspond to the 0.5 arcmin radius
        pt = QPoint((width() / 2) - 1, (height() / 2) - 1);
        ok = pixelToWCS(pt, searchCenter);
        if (ok)
        {
            radius = 1.0; // Use 1.0 arcmins as a starting point... not too big to avoid getting swamped with objects
            double raEdge = searchCenter.ra0().Degrees() + (radius / 60.0);
            double decEdge = searchCenter.dec0().Degrees();
            SkyPoint searchEdge(raEdge / 15.0, decEdge);
            QPointF edgePoint, pEdge;
            ok = wcsToPixel(searchEdge, pEdge, edgePoint);
            if (ok)
            {
                // Set the ROI which will be drawn
                const double radiusPix = std::hypot((pt.x() - pEdge.x()), pt.y() - pEdge.y());
                setCatSearchROI(pt, radiusPix);
            }
        }
    }
    if (!ok)
    {
        qCDebug(KSTARS_FITS) << "Unable to process Catalog Object request...";
        return false;
    }
    if (Options::fitsCatalog() == CAT_SIMBAD)
        return findSimbadObjectsInImage(searchCenter, radius);
    else
        return false;
}

bool FITSData::findWCSBounds(double &minRA, double &maxRA, double &minDec, double &maxDec)
{
    if (m_WCSHandle == nullptr)
    {
        m_LastError = i18n("No world coordinate systems found.");
        return false;
    }

    maxRA  = -1000;
    minRA  = 1000;
    maxDec = -1000;
    minDec = 1000;

    auto updateMinMax = [&](int x, int y)
    {
        int stat[2];
        double imgcrd[2], phi, pixcrd[2], theta, world[2];

        pixcrd[0] = x;
        pixcrd[1] = y;

        if (wcsp2s(m_WCSHandle, 1, 2, &pixcrd[0], &imgcrd[0], &phi, &theta, &world[0], &stat[0]))
            return;

        minRA = std::min(minRA, world[0]);
        maxRA = std::max(maxRA, world[0]);
        minDec = std::min(minDec, world[1]);
        maxDec = std::max(maxDec, world[1]);
    };

    // Find min and max values from edges
    for (int y = 0; y < height(); y++)
    {
        updateMinMax(0, y);
        updateMinMax(width() - 1, y);
    }

    for (int x = 1; x < width() - 1; x++)
    {
        updateMinMax(x, 0);
        updateMinMax(x, height() - 1);
    }

    // Check if either pole is in the image
    SkyPoint NCP(0, 90);
    SkyPoint SCP(0, -90);
    QPointF imagePoint, pPoint;
    if (wcsToPixel(NCP, pPoint, imagePoint))
    {
        if (pPoint.x() > 0 && pPoint.x() < width() && pPoint.y() > 0 && pPoint.y() < height())
        {
            maxDec = 90;
        }
    }
    if (wcsToPixel(SCP, pPoint, imagePoint))
    {
        if (pPoint.x() > 0 && pPoint.x() < width() && pPoint.y() > 0 && pPoint.y() < height())
        {
            minDec = -90;
        }
    }
    return true;
}
#endif

#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)
bool FITSData::findObjectsInImage(SkyPoint startPoint, SkyPoint endPoint)
{
    if (KStarsData::Instance() == nullptr)
        return false;

    int w = width();
    int h = height();
    QVariant date;
    KSNumbers * num = nullptr;

    if (getRecordValue("DATE-OBS", date))
    {
        QString tsString(date.toString());
        tsString = tsString.remove('\'').trimmed();
        // Add Zulu time to indicate UTC
        tsString += "Z";

        QDateTime ts = QDateTime::fromString(tsString, Qt::ISODate);

        if (ts.isValid())
            num = new KSNumbers(KStarsDateTime(ts).djd());
    }

    //Set to current time if the above does not work.
    if (num == nullptr)
        num = new KSNumbers(KStarsData::Instance()->ut().djd());

    startPoint.updateCoordsNow(num);
    endPoint.updateCoordsNow(num);

    m_SkyObjects.clear();

    QList<SkyObject *> list = KStarsData::Instance()->skyComposite()->findObjectsInArea(startPoint, endPoint);
    list.erase(std::remove_if(list.begin(), list.end(), [](SkyObject * oneObject)
    {
        int type = oneObject->type();
        return (type == SkyObject::STAR || type == SkyObject::PLANET || type == SkyObject::ASTEROID ||
                type == SkyObject::COMET || type == SkyObject::SUPERNOVA || type == SkyObject::MOON ||
                type == SkyObject::SATELLITE);
    }), list.end());

    double world[2], phi, theta, imgcrd[2], pixcrd[2];
    int stat[2];
    for (auto &object : list)
    {
        world[0] = object->ra0().Degrees();
        world[1] = object->dec0().Degrees();

        if (wcss2p(m_WCSHandle, 1, 2, &world[0], &phi, &theta, &imgcrd[0], &pixcrd[0], &stat[0]) == 0)
        {
            //The X and Y are set to the found position if it does work.
            int x = pixcrd[0];
            int y = pixcrd[1];
            if (x > 0 && y > 0 && x < w && y < h)
                m_SkyObjects.append(new FITSSkyObject(object, x, y));
        }
    }

    delete (num);
    return true;
}

// See https://simbad.u-strasbg.fr/Pages/guide/sim-q.htx and
//     https://simbad.u-strasbg.fr/Pages/guide/sim-url.htx
// for details of how to query Simbad and the various options
//
// Set the epoch from the datetime of the image and ensure that the reply is in this same epoch
bool FITSData::findSimbadObjectsInImage(SkyPoint searchCenter, double radius)
{
    m_CatObjects.clear();
    m_CatUpdateTable = false;

    // Query Simbad
    QUrl simbadURL = QUrl("https://simbad.cds.unistra.fr/simbad/sim-coo");
    QUrlQuery simbadQuery(simbadURL.query());

    QString coord = QString("%1 %2").arg(searchCenter.ra0().toHMSString(true, true))
                    .arg(searchCenter.dec0().toDMSString(true, true, true));

    QString radiusStr = QString("%1").arg(radius * 60.0, 0, 'f', 5);
    m_CatObjQuery = QString("%1 %2").arg(coord).arg(radiusStr);

    coord.replace("+", "%2B"); // Need to replace the + otherwise it gets lost by URL processing

    QString epoch = QString("J%1").arg(m_DateTime.epoch());

    simbadQuery.addQueryItem("Coord", coord);
    simbadQuery.addQueryItem("Radius", radiusStr);
    simbadQuery.addQueryItem("Radius.unit", "arcsec");
    simbadQuery.addQueryItem("CooFrame", "ICRS");
    simbadQuery.addQueryItem("CooEpoch", epoch);
    simbadQuery.addQueryItem("output.format", "ASCII");
    simbadQuery.addQueryItem("output.max", QString("%1").arg(10000));
    simbadQuery.addQueryItem("list.otypesel", "on");
    simbadQuery.addQueryItem("otypedisp", "3"); // Use Simbad's 3 char object type
    simbadQuery.addQueryItem("list.coo1", "on"); // Display coord 1
    simbadQuery.addQueryItem("frame1", "ICRS");
    simbadQuery.addQueryItem("epoch1", "J2000");
    simbadQuery.addQueryItem("equi1", epoch);
    simbadQuery.addQueryItem("list.spsel", "off"); // Spectral Type
    simbadQuery.addQueryItem("list.sizesel", "on"); // Angular size
    simbadQuery.addQueryItem("list.bibsel", "off"); // Bibliography

    simbadURL.setQuery(simbadQuery);
    qCDebug(KSTARS_FITS) << "Simbad query:" << simbadURL;

    m_NetworkAccessManager.reset(new QNetworkAccessManager(this));
    connect(m_NetworkAccessManager.get(), &QNetworkAccessManager::finished, this, &FITSData::simbadResponseReady);
    QNetworkReply *response = m_NetworkAccessManager->get(QNetworkRequest(simbadURL));
    if (response->error() != QNetworkReply::NoError)
    {
        qCDebug(KSTARS_FITS) << "Error:" << response->errorString() << " occured querying SIMBAD with" << simbadURL;
        m_CatQueryInProgress = false;
        emit catalogQueryFailed(i18n("Error querying Simbad"));
        return false;
    }

    m_CatQueryInProgress = true;
    m_CatQueryTimer.setSingleShot(true);
    connect(&m_CatQueryTimer, &QTimer::timeout, this, &FITSData::catTimeout);
    m_CatQueryTimer.start(60 * 1000);
    emit loadingCatalogData();
    return true;
}
#endif

#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)
void FITSData::catTimeout()
{
    m_CatQueryInProgress = false;
    QString text = i18n("Simbad query timed out");
    qCDebug(KSTARS_FITS) << text;
    emit catalogQueryFailed(text);
}
#endif

// There are 3 types of responses... zero objects, 1 object and >1 objects. The formats are as follows:
// (Note that Line xx isn't part of the reply, these are added for convenience)
//
// Zero Objects
// Line 0 "!! No astronomical object found : "
//
// 1 Object
// Line 0 "C.D.S.  -  SIMBAD4 rel 1.8  -  2024.09.30CEST14:07:24"
// Line 1 ""
// Line 2 "coord 21:36:49 +57:31:23 (ICRS, J2024.7, 2000.0), radius: 5.71083 arcsec"
// Line 3 "------------------------------------------------------------------------"
// Line 4 ""
// Line 5 "Object EM* LkHA 349C  ---  RS*  ---  OID=@157406   (@@122902,11)  ---  coobox=594"
// Line 6 ""
// Line 7 "Coordinates(ICRS,ep=J2000,eq=J2024.7): 21 36 49.4008740691  +57 31 21.834541600 (Opt ) A [0.0658 0.0673 90] 2020yCat.1350....0G"
// Line 8 "Coordinates(FK4,ep=B1950,eq=1950): 21 35 17.0484337158  +57 17 51.458438447"
// Line 9 "Coordinates(Gal,ep=J2000,eq=2000): 099.0986092951232  +03.9548388170815"
// Line 10 "hierarchy counts: #parents=2, #children=0, #siblings=0"
// Line 11 "Proper motions: -2.232 -5.540 [0.087 0.086 90] A 2020yCat.1350....0G"
// Line 12 "Parallax: 1.0825 [0.0718] A 2020yCat.1350....0G"
// Line 13 "Radial Velocity: ~ [~ ~ ] ~ ~                  "
// Line 14 "Redshift: ~ [~ ~ ] ~ ~                  "
// Line 15 "cz: ~ [~ ~ ] ~ ~                  "
// Line 16 "Flux U : 19.219 [0.083] C 2010ApJ...710..597S"
// Line 17 "Flux V : 15.81 [0.01] B 2012MNRAS.426.2917G"
// Line 18 "Flux G : 14.948605 [0.007309] C 2020yCat.1350....0G"
// Line 19 "Flux R : 14.508 [0.030] C 2010ApJ...710..597S"
// Line 20 "Flux I : 13.094 [0.003] C 2010ApJ...710..597S"
// Line 21 "Flux J : 11.918 [0.022] C 2003yCat.2246....0C"
// Line 22 "Flux H : 10.914 [0.029] C 2003yCat.2246....0C"
// Line 23 "Flux K : 10.355 [0.021] C 2003yCat.2246....0C"
// Line 24 "Flux g : 16.702 [~] D 2020ApJS..249...18C"
// Line 25 "Flux r : 15.057 [~] D 2020ApJS..249...18C"
// Line 26 "Flux i : 14.84 [~] D 2012AJ....143...61N"
// Line 27 "Spectral type: K6 D 2006AJ....132.2135S"
// Line 28 "Morphological type: ~ ~ ~"
// Line 29 "Angular size:     ~     ~   ~ (~)  ~ ~"
// Line 30 ""
// Line 31 "Identifiers (12):"
// Line 32 "   ZTF J213649.40+573121.9              Gaia DR3 2178442322028512512         TIC 469533750                      "
// Line 33 "   CoKu LkHA 349 c                      EM* LkHA 349C                        2MASS J21364941+5731220            "
// Line 34 "   [SHB2004] Trumpler 37 14-141         [MSR2009] IC1396A-29                 [NSW2012] 125                      "
// Line 35 "   CXOIC1396A J213649.39+573122.1       [GFS2012] 173                        Gaia DR2 2178442322028512512       "
// Line 36 ""
// Line 37 "Bibcodes  1850-2024 () (20):"
// Line 38 "  2024MNRAS.532.2108S  2020ApJS..249...18C  2019ApJ...878....7M  2018MNRAS.478.5091F"
// Line 39 "  2012AJ....143...61N  2012MNRAS.426.2917G  2011ApJ...742...39S  2010ApJ...710..597S"
// Line 40 "  2009ApJ...690..683R  2009ApJ...702.1507M  2006AJ....132.2135S  2006ApJ...638..897S"
// Line 41 "  2005A&A...443..535V  2005AJ....130..188S  2004AJ....128..805S  2004ApJS..154..385R"
// Line 42 "  1997A&A...325.1001S  1996ApJ...463L.105M  1995A&A...299..464H  1979ApJS...41..743C"
// Line 43 ""
// Line 44 "Measures (distance:1  PLX:2  PM:2  ROT:1  V*:3  velocities:1  ):"
// Line 45 "distance:1PLX:2PM:2ROT:1V*:3velocities:1"
// Line 46 ""
// Line 47 "Notes (0) :"
// Line 48 ""
// Line 49 "================================================================================"
// Line 50 ""
// Line 51 ""
//
// >1 Objects
// Line 0 "C.D.S.  -  SIMBAD4 rel 1.8  -  2024.09.30CEST14:13:23"
// Line 1 ""
// Line 2 "coord 21:36:51 +57:31:13 (ICRS, J2024.7, 2000.0), radius: 11.42166 arcsec"
// Line 3 "-------------------------------------------------------------------------"
// Line 4 ""
// Line 5 "Number of objects : 2"
// Line 6 ""
// Line 7 "#|dist(asec)|            identifier             |typ|      coord1 (ICRS,J2000/J2024.7)      |Mag U |  Mag B  |  Mag V  |  Mag R  |  Mag I  |   ang. size   |#not"
// Line 8 "-|----------|-----------------------------------|---|---------------------------------------|------|---------|---------|---------|---------|---------------|----"
// Line 9 "1|      2.62|DOBASHI 3187                       |DNe|21 36 51.3 +57 31 12                   |     ~|     ~   |     ~   |     ~   |     ~   |    ~     ~    |   1"
// Line 10 "2|      3.19|EM* LkHA 349                       |Or*|21 36 50.7149106104 +57 31 10.580260473|     ~|15.13    |13.45    |12.88    |10.91    |    ~     ~    |   1"
// Line 11 "================================================================================"
// Line 12 ""
// Line 13 ""

#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)
void FITSData::simbadResponseReady(QNetworkReply * simbadReply)
{
    m_CatQueryTimer.stop();
    m_CatQueryInProgress = false;
    if (simbadReply->error() != QNetworkReply::NoError)
    {
        qCDebug(KSTARS_FITS) << "Error:" << simbadReply->errorString() << " occured in SIMBAD query reply";
        m_CatQueryInProgress = false;
        emit catalogQueryFailed(i18n("Error querying Simbad"));
        return;
    }

    auto data = simbadReply->readAll();
    QString dataStr = QString(data);

    // Break the line into comma-separated components
    int numObjs = 0;
    bool ok;
    QString name, type, coord, sizeStr;
    double dist = 0, magnitude = 0;
    int num = 0;
    QStringList replyLines = dataStr.split(QLatin1Char('\n'));
    int dataRow = -1;
    for (int i = 0; i < replyLines.size(); i++)
    {
        // qCDebug(KSTARS_FITS) << "Line" << i << replyLines[i];
        if (i == 0 && replyLines[i].contains("No astronomical object found : "))
            // No objects in the reply so we can just ignore the rest of the data
            break;
        if (i == 2)
        {
            // Check that the query coord & radius match the reply - otherwise ignore...
            QString replyStr;
            QStringList replyData = replyLines[i].split(QLatin1Char(' '));
            if (replyData.size() == 9 && replyData[0] == "coord" && replyData[6] == "radius:")
                replyStr = QString("%1 %2 %3").arg(replyData[1]).arg(replyData[2]).arg(replyData[7]);
            if (replyStr != m_CatObjQuery)
            {
                qCDebug(KSTARS_FITS) << "Simbad query:" << m_CatObjQuery << "Reply:" << replyStr << ". Ignoring...";
                break;
            }
        }
        else if (i == 5)
        {
            if (replyLines[i].contains("Number of objects : "))
                numObjs = QString(replyLines[i].mid(20)).toInt(&ok);
            else if (replyLines[i].startsWith("Object "))
            {
                // Dealing with a single object
                num = numObjs = 1;
                const int firstBreak = replyLines[i].indexOf("---");
                const int secondBreak = replyLines[i].indexOf("---", firstBreak + 3);
                name = replyLines[i].mid(7, firstBreak - 7).trimmed();
                type = replyLines[i].mid(firstBreak + 3, secondBreak - firstBreak - 3).trimmed();
            }
            else
            {
                qCDebug(KSTARS_FITS) << "Bad Simbad Reply, Ignoring...";
                break;
            }
        }
        else if (numObjs == 1 && i >= 7)
        {
            if (replyLines[i].startsWith("Coordinates(ICRS"))
            {
                QStringList replyData = replyLines[i].split(QLatin1Char(' '));
                if (replyData.size() >= 8)
                    coord = replyData[1] + " " + replyData[2] + " " + replyData[3] + " " +
                            replyData[5] + " " + replyData[6] + " " + replyData[7];
            }
            else if (replyLines[i].startsWith("Flux V :"))
            {
                QStringList replyData = replyLines[i].split(QLatin1Char(' '));
                if (replyData.size() >= 4)
                    magnitude = QString(replyData[3]).toDouble(&ok);
            }
            else if (replyLines[i].startsWith("Angular size:"))
                sizeStr = replyLines[i].mid(13, replyLines[i].indexOf("~") - 13).trimmed();
            else if (replyLines[i].startsWith("=========================================="))
            {
                // End of data so add the object
                if (addCatObject(num, name, type, coord, dist, magnitude, sizeStr))
                    dataRow++;
            }
        }
        else if (numObjs > 1 && i >= 9)
        {
            // These are the data lines - 1 per object
            QStringList replyData = replyLines[i].split(QLatin1Char('|'));
            num = 0;
            name = type = coord = sizeStr = "";
            magnitude = dist = 0.0;
            for (int j = 0; j < replyData.size(); j++)
            {
                if (j == 0)
                    num = QString(replyData[j]).toInt(&ok);
                else if (j == 1)
                    dist = QString(replyData[j]).toDouble(&ok) / 60.0; // Convert from arcsecs to arcmins
                else if (j == 2)
                    name = (QString(replyData[j]).trimmed());
                else if (j == 3)
                    type = QString(replyData[j]).trimmed();
                else if (j == 4)
                    coord = QString(replyData[j]).trimmed();
                else if (j == 7) // Mag V
                    magnitude = QString(replyData[j]).toDouble(&ok);
                else if (j == 10) // Angular size
                    sizeStr = QString(replyData[j]).trimmed();
            }

            if (num == 0 || name.isEmpty() || type.isEmpty() || coord.isEmpty())
                continue;

            if (addCatObject(num, name, type, coord, dist, magnitude, sizeStr))
                dataRow++;
        }
    }
    if (numObjs != ++dataRow)
        qCDebug(KSTARS_FITS) << "Simbad Header rows:" << numObjs << ". Data rows:" << dataRow;
    // Signal fitsview to update based on new data. Fitsview will signal fitstab
    m_CatObjectsSearched = false;
    // Signal FITSView that there is new data to process
    emit dataChanged();
    // Signal FITSTab that there is new data to process
    emit loadedCatalogData();
    simbadReply->deleteLater();
}
#endif

bool FITSData::addCatObject(const int num, const QString name, const QString type, const QString coord, const double dist,
                            const double magnitude, const QString sizeStr)
{
    bool ok;
    dms r(0.0), d(0.0);

    QStringList split = coord.split(QLatin1Char(' '));
    if (split.size() != 6)
    {
        qCDebug(KSTARS_FITS) << "Coordinates for " << name << "invalid: " << coord;
        return false;
    }

    QString raStr = QString("%1 %2 %3").arg(split[0]).arg(split[1]).arg(split[2]);
    QString decStr = QString("%1 %2 %3").arg(split[3]).arg(split[4]).arg(split[5]);
    ok = r.setFromString(raStr, false); // angle in hours
    if (ok)
        ok = d.setFromString(decStr, true); // angle in degrees
    if (!ok)
    {
        qCDebug(KSTARS_FITS) << "Coordinates for " << name << "invalid: " << coord;
        return false;
    }

    double size = 0.0;
    if (!sizeStr.contains("~"))
    {
        QStringList sizeSplit = sizeStr.split(QLatin1Char(' '));
        if (sizeSplit.size() >= 1)
            // Take the first (major axis) size. Note sometimes there's a double space between axes so
            // number of elements can be 3.
            size = QString(sizeSplit[0]).toDouble(&ok);
        else
            qCDebug(KSTARS_FITS) << "Angular size for " << name << "invalid: " << sizeStr;
    }
    CatObject catObject;
    catObject.num = num;
    catObject.catType = CAT_SIMBAD;
    catObject.name = name;
    catObject.typeCode = type;
    catObject.typeLabel = getCatObjectLabel(type);
    catObject.r = r;
    catObject.d = d;
    catObject.dist = dist;
    catObject.magnitude = magnitude;
    catObject.size = size;
    catObject.highlight = false;
    catObject.show = getCatObjectFilter(type);

    double world[2], phi, theta, imgcrd[2], pixcrd[2];
    int stat[2];

    world[0] = r.Degrees();
    world[1] = d.Degrees();

    int status = wcss2p(m_WCSHandle, 1, 2, &world[0], &phi, &theta, &imgcrd[0], &pixcrd[0], &stat[0]);
    if (status != 0)
    {
        qCDebug(KSTARS_FITS) << QString("wcss2p error processing object %1 %2: %3.").arg(name).arg(status).arg(wcs_errmsg[status]);
        return false;
    }

    //X and Y are set to the found position if in image
    double x = pixcrd[0];
    double y = pixcrd[1];
    if (x > 0 && y > 0 && x < width() && y < height())
    {
        catObject.x = x;
        catObject.y = y;
        m_CatObjects.append(catObject);
    }
    return true;
}

void FITSData::setCatObjectsFilters(const QStringList filters)
{
    m_CatObjectsFilters = filters;
}

#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)
void FITSData::setCatSearchROI(const QPoint searchCenter, const int radius)
{
    m_CatROIPt = searchCenter;
    m_CatROIRadius = radius;
    Q_UNUSED(searchCenter);
    Q_UNUSED(radius);
}
#endif

#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)
QString FITSData::getCatObjectLabel(const QString code) const
{
    QString label;
    if (code != "")
    {
        for (int i = 0; i < MAX_CAT_OBJ_TYPES; i++)
        {
            if (code == catObjTypes[i].code)
            {
                label = catObjTypes[i].label;
                break;
            }
            else if (code == catObjTypes[i].candidateCode)
            {
                label = catObjTypes[i].label + "_Cand";
                break;
            }
        }
    }
    return label;
}
#endif

#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)
bool FITSData::getCatObjectFilter(const QString type) const
{
    if (m_CatObjectsFilters.isEmpty() || m_CatObjectsFilters.contains(type))
        return true;
    return false;
}
#endif

void FITSData::filterCatObjects()
{
#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)
    bool changed = false;
    for (int i = 0; i < m_CatObjects.size(); i++)
    {
        bool showState = getCatObjectFilter(m_CatObjects[i].typeCode);
        if (m_CatObjects[i].show != showState)
        {
            changed = true;
            m_CatObjects[i].show = showState;
        }
    }
    if (changed)
        emit dataChanged();
#endif
}

bool FITSData::highlightCatObject(const int hilite, const int lolite)
{
#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)
    if (hilite < 0 || hilite >= m_CatObjects.size())
        return false;

    // Lowlight previous highlighted catalog object if passed
    if (lolite >= 0 && lolite < m_CatObjects.size())
        m_CatObjects[lolite].highlight = false;
    else
    {
        // Loop through data to find current highlighted item
        for (int i = 0; i < m_CatObjects.size(); i++)
        {
            if (m_CatObjects[i].highlight)
            {
                m_CatObjects[i].highlight = false;
                break;
            }
        }
    }

    // Highlight the catalog object
    m_CatObjects[hilite].highlight = true;
#endif
    return true;
}

int FITSData::getFlipVCounter() const
{
    return flipVCounter;
}

void FITSData::setFlipVCounter(int value)
{
    flipVCounter = value;
}

int FITSData::getFlipHCounter() const
{
    return flipHCounter;
}

void FITSData::setFlipHCounter(int value)
{
    flipHCounter = value;
}

int FITSData::getRotCounter() const
{
    return rotCounter;
}

void FITSData::setRotCounter(int value)
{
    rotCounter = value;
}

/* Rotate an image by 90, 180, or 270 degrees, with an optional
 * reflection across the vertical or horizontal axis.
 * verbose generates extra info on stdout.
 * return nullptr if successful or rotated image.
 */
template <typename T>
bool FITSData::rotFITS(int rotate, int mirror)
{
    int ny, nx;
    int x1, y1, x2, y2;
    uint8_t * rotimage = nullptr;
    int offset        = 0;

    if (rotate == 1)
        rotate = 90;
    else if (rotate == 2)
        rotate = 180;
    else if (rotate == 3)
        rotate = 270;
    else if (rotate < 0)
        rotate = rotate + 360;

    nx = m_Statistics.width;
    ny = m_Statistics.height;

    int BBP = m_Statistics.bytesPerPixel;

    /* Allocate buffer for rotated image */
    rotimage = new uint8_t[m_Statistics.samples_per_channel * m_Statistics.channels * BBP];

    if (rotimage == nullptr)
    {
        qWarning() << "Unable to allocate memory for rotated image buffer!";
        return false;
    }

    auto * rotBuffer = reinterpret_cast<T *>(rotimage);
    auto * buffer    = reinterpret_cast<T *>(m_ImageBuffer);

    /* Mirror image without rotation */
    if (rotate < 45 && rotate > -45)
    {
        if (mirror == 1)
        {
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
                for (x1 = 0; x1 < nx; x1++)
                {
                    x2 = nx - x1 - 1;
                    for (y1 = 0; y1 < ny; y1++)
                        rotBuffer[(y1 * nx) + x2 + offset] = buffer[(y1 * nx) + x1 + offset];
                }
            }
        }
        else if (mirror == 2)
        {
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    y2 = ny - y1 - 1;
                    for (x1 = 0; x1 < nx; x1++)
                        rotBuffer[(y2 * nx) + x1 + offset] = buffer[(y1 * nx) + x1 + offset];
                }
            }
        }
        else
        {
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    for (x1 = 0; x1 < nx; x1++)
                        rotBuffer[(y1 * nx) + x1 + offset] = buffer[(y1 * nx) + x1 + offset];
                }
            }
        }
    }

    /* Rotate by 90 degrees */
    else if (rotate >= 45 && rotate < 135)
    {
        if (mirror == 1)
        {
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    x2 = ny - y1 - 1;
                    for (x1 = 0; x1 < nx; x1++)
                    {
                        y2                                 = nx - x1 - 1;
                        rotBuffer[(y2 * ny) + x2 + offset] = buffer[(y1 * nx) + x1 + offset];
                    }
                }
            }
        }
        else if (mirror == 2)
        {
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    for (x1 = 0; x1 < nx; x1++)
                        rotBuffer[(x1 * ny) + y1 + offset] = buffer[(y1 * nx) + x1 + offset];
                }
            }
        }
        else
        {
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    x2 = ny - y1 - 1;
                    for (x1 = 0; x1 < nx; x1++)
                    {
                        y2                                 = x1;
                        rotBuffer[(y2 * ny) + x2 + offset] = buffer[(y1 * nx) + x1 + offset];
                    }
                }
            }
        }

        m_Statistics.width  = ny;
        m_Statistics.height = nx;
    }

    /* Rotate by 180 degrees */
    else if (rotate >= 135 && rotate < 225)
    {
        if (mirror == 1)
        {
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    y2 = ny - y1 - 1;
                    for (x1 = 0; x1 < nx; x1++)
                        rotBuffer[(y2 * nx) + x1 + offset] = buffer[(y1 * nx) + x1 + offset];
                }
            }
        }
        else if (mirror == 2)
        {
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
                for (x1 = 0; x1 < nx; x1++)
                {
                    x2 = nx - x1 - 1;
                    for (y1 = 0; y1 < ny; y1++)
                        rotBuffer[(y1 * nx) + x2 + offset] = buffer[(y1 * nx) + x1 + offset];
                }
            }
        }
        else
        {
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    y2 = ny - y1 - 1;
                    for (x1 = 0; x1 < nx; x1++)
                    {
                        x2                                 = nx - x1 - 1;
                        rotBuffer[(y2 * nx) + x2 + offset] = buffer[(y1 * nx) + x1 + offset];
                    }
                }
            }
        }
    }

    /* Rotate by 270 degrees */
    else if (rotate >= 225 && rotate < 315)
    {
        if (mirror == 1)
        {
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    for (x1 = 0; x1 < nx; x1++)
                        rotBuffer[(x1 * ny) + y1 + offset] = buffer[(y1 * nx) + x1 + offset];
                }
            }
        }
        else if (mirror == 2)
        {
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    x2 = ny - y1 - 1;
                    for (x1 = 0; x1 < nx; x1++)
                    {
                        y2                                 = nx - x1 - 1;
                        rotBuffer[(y2 * ny) + x2 + offset] = buffer[(y1 * nx) + x1 + offset];
                    }
                }
            }
        }
        else
        {
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    x2 = y1;
                    for (x1 = 0; x1 < nx; x1++)
                    {
                        y2                                 = nx - x1 - 1;
                        rotBuffer[(y2 * ny) + x2 + offset] = buffer[(y1 * nx) + x1 + offset];
                    }
                }
            }
        }

        m_Statistics.width  = ny;
        m_Statistics.height = nx;
    }

    /* If rotating by more than 315 degrees, assume top-bottom reflection */
    else if (rotate >= 315 && mirror)
    {
        for (int i = 0; i < m_Statistics.channels; i++)
        {
            offset = m_Statistics.samples_per_channel * i;
            for (y1 = 0; y1 < ny; y1++)
            {
                for (x1 = 0; x1 < nx; x1++)
                {
                    x2                                 = y1;
                    y2                                 = x1;
                    rotBuffer[(y2 * ny) + x2 + offset] = buffer[(y1 * nx) + x1 + offset];
                }
            }
        }
    }

    delete[] m_ImageBuffer;
    m_ImageBuffer = rotimage;

    return true;
}

void FITSData::rotWCSFITS(int angle, int mirror)
{
    int status = 0;
    char comment[100];
    double ctemp1, ctemp2, ctemp3, ctemp4, naxis1, naxis2;
    int WCS_DECIMALS = 6;

    naxis1 = m_Statistics.width;
    naxis2 = m_Statistics.height;

    if (fits_read_key_dbl(fptr, "CD1_1", &ctemp1, comment, &status))
    {
        // No WCS keywords
        return;
    }

    /* Reset CROTAn and CD matrix if axes have been exchanged */
    if (angle == 90)
    {
        if (!fits_read_key_dbl(fptr, "CROTA1", &ctemp1, comment, &status))
            fits_update_key_dbl(fptr, "CROTA1", ctemp1 + 90.0, WCS_DECIMALS, comment, &status);

        if (!fits_read_key_dbl(fptr, "CROTA2", &ctemp1, comment, &status))
            fits_update_key_dbl(fptr, "CROTA2", ctemp1 + 90.0, WCS_DECIMALS, comment, &status);
    }

    status = 0;

    /* Negate rotation angle if mirrored */
    if (mirror != 0)
    {
        if (!fits_read_key_dbl(fptr, "CROTA1", &ctemp1, comment, &status))
            fits_update_key_dbl(fptr, "CROTA1", -ctemp1, WCS_DECIMALS, comment, &status);

        if (!fits_read_key_dbl(fptr, "CROTA2", &ctemp1, comment, &status))
            fits_update_key_dbl(fptr, "CROTA2", -ctemp1, WCS_DECIMALS, comment, &status);

        status = 0;

        if (!fits_read_key_dbl(fptr, "LTM1_1", &ctemp1, comment, &status))
            fits_update_key_dbl(fptr, "LTM1_1", -ctemp1, WCS_DECIMALS, comment, &status);

        status = 0;

        if (!fits_read_key_dbl(fptr, "CD1_1", &ctemp1, comment, &status))
            fits_update_key_dbl(fptr, "CD1_1", -ctemp1, WCS_DECIMALS, comment, &status);

        if (!fits_read_key_dbl(fptr, "CD1_2", &ctemp1, comment, &status))
            fits_update_key_dbl(fptr, "CD1_2", -ctemp1, WCS_DECIMALS, comment, &status);

        if (!fits_read_key_dbl(fptr, "CD2_1", &ctemp1, comment, &status))
            fits_update_key_dbl(fptr, "CD2_1", -ctemp1, WCS_DECIMALS, comment, &status);
    }

    status = 0;

    /* Unbin CRPIX and CD matrix */
    if (!fits_read_key_dbl(fptr, "LTM1_1", &ctemp1, comment, &status))
    {
        if (ctemp1 != 1.0)
        {
            if (!fits_read_key_dbl(fptr, "LTM2_2", &ctemp2, comment, &status))
                if (ctemp1 == ctemp2)
                {
                    double ltv1 = 0.0;
                    double ltv2 = 0.0;
                    status      = 0;
                    if (!fits_read_key_dbl(fptr, "LTV1", &ltv1, comment, &status))
                        fits_delete_key(fptr, "LTV1", &status);
                    if (!fits_read_key_dbl(fptr, "LTV2", &ltv2, comment, &status))
                        fits_delete_key(fptr, "LTV2", &status);

                    status = 0;

                    if (!fits_read_key_dbl(fptr, "CRPIX1", &ctemp3, comment, &status))
                        fits_update_key_dbl(fptr, "CRPIX1", (ctemp3 - ltv1) / ctemp1, WCS_DECIMALS, comment, &status);

                    if (!fits_read_key_dbl(fptr, "CRPIX2", &ctemp3, comment, &status))
                        fits_update_key_dbl(fptr, "CRPIX2", (ctemp3 - ltv2) / ctemp1, WCS_DECIMALS, comment, &status);

                    status = 0;

                    if (!fits_read_key_dbl(fptr, "CD1_1", &ctemp3, comment, &status))
                        fits_update_key_dbl(fptr, "CD1_1", ctemp3 / ctemp1, WCS_DECIMALS, comment, &status);

                    if (!fits_read_key_dbl(fptr, "CD1_2", &ctemp3, comment, &status))
                        fits_update_key_dbl(fptr, "CD1_2", ctemp3 / ctemp1, WCS_DECIMALS, comment, &status);

                    if (!fits_read_key_dbl(fptr, "CD2_1", &ctemp3, comment, &status))
                        fits_update_key_dbl(fptr, "CD2_1", ctemp3 / ctemp1, WCS_DECIMALS, comment, &status);

                    if (!fits_read_key_dbl(fptr, "CD2_2", &ctemp3, comment, &status))
                        fits_update_key_dbl(fptr, "CD2_2", ctemp3 / ctemp1, WCS_DECIMALS, comment, &status);

                    status = 0;

                    fits_delete_key(fptr, "LTM1_1", &status);
                    fits_delete_key(fptr, "LTM1_2", &status);
                }
        }
    }

    status = 0;

    /* Reset CRPIXn */
    if (!fits_read_key_dbl(fptr, "CRPIX1", &ctemp1, comment, &status) &&
            !fits_read_key_dbl(fptr, "CRPIX2", &ctemp2, comment, &status))
    {
        if (mirror != 0)
        {
            if (angle == 0)
                fits_update_key_dbl(fptr, "CRPIX1", naxis1 - ctemp1, WCS_DECIMALS, comment, &status);
            else if (angle == 90)
            {
                fits_update_key_dbl(fptr, "CRPIX1", naxis2 - ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CRPIX2", naxis1 - ctemp1, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 180)
            {
                fits_update_key_dbl(fptr, "CRPIX1", ctemp1, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CRPIX2", naxis2 - ctemp2, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 270)
            {
                fits_update_key_dbl(fptr, "CRPIX1", ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CRPIX2", ctemp1, WCS_DECIMALS, comment, &status);
            }
        }
        else
        {
            if (angle == 90)
            {
                fits_update_key_dbl(fptr, "CRPIX1", naxis2 - ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CRPIX2", ctemp1, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 180)
            {
                fits_update_key_dbl(fptr, "CRPIX1", naxis1 - ctemp1, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CRPIX2", naxis2 - ctemp2, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 270)
            {
                fits_update_key_dbl(fptr, "CRPIX1", ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CRPIX2", naxis1 - ctemp1, WCS_DECIMALS, comment, &status);
            }
        }
    }

    status = 0;

    /* Reset CDELTn (degrees per pixel) */
    if (!fits_read_key_dbl(fptr, "CDELT1", &ctemp1, comment, &status) &&
            !fits_read_key_dbl(fptr, "CDELT2", &ctemp2, comment, &status))
    {
        if (mirror != 0)
        {
            if (angle == 0)
                fits_update_key_dbl(fptr, "CDELT1", -ctemp1, WCS_DECIMALS, comment, &status);
            else if (angle == 90)
            {
                fits_update_key_dbl(fptr, "CDELT1", -ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CDELT2", -ctemp1, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 180)
            {
                fits_update_key_dbl(fptr, "CDELT1", ctemp1, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CDELT2", -ctemp2, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 270)
            {
                fits_update_key_dbl(fptr, "CDELT1", ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CDELT2", ctemp1, WCS_DECIMALS, comment, &status);
            }
        }
        else
        {
            if (angle == 90)
            {
                fits_update_key_dbl(fptr, "CDELT1", -ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CDELT2", ctemp1, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 180)
            {
                fits_update_key_dbl(fptr, "CDELT1", -ctemp1, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CDELT2", -ctemp2, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 270)
            {
                fits_update_key_dbl(fptr, "CDELT1", ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CDELT2", -ctemp1, WCS_DECIMALS, comment, &status);
            }
        }
    }

    /* Reset CD matrix, if present */
    ctemp1 = 0.0;
    ctemp2 = 0.0;
    ctemp3 = 0.0;
    ctemp4 = 0.0;
    status = 0;
    if (!fits_read_key_dbl(fptr, "CD1_1", &ctemp1, comment, &status))
    {
        fits_read_key_dbl(fptr, "CD1_2", &ctemp2, comment, &status);
        fits_read_key_dbl(fptr, "CD2_1", &ctemp3, comment, &status);
        fits_read_key_dbl(fptr, "CD2_2", &ctemp4, comment, &status);
        status = 0;
        if (mirror != 0)
        {
            if (angle == 0)
            {
                fits_update_key_dbl(fptr, "CD1_2", -ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_1", -ctemp3, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 90)
            {
                fits_update_key_dbl(fptr, "CD1_1", -ctemp4, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD1_2", -ctemp3, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_1", -ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_2", -ctemp1, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 180)
            {
                fits_update_key_dbl(fptr, "CD1_1", ctemp1, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD1_2", ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_1", -ctemp3, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_2", -ctemp4, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 270)
            {
                fits_update_key_dbl(fptr, "CD1_1", ctemp4, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD1_2", ctemp3, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_1", ctemp3, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_2", ctemp1, WCS_DECIMALS, comment, &status);
            }
        }
        else
        {
            if (angle == 90)
            {
                fits_update_key_dbl(fptr, "CD1_1", -ctemp4, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD1_2", -ctemp3, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_1", ctemp1, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_2", ctemp1, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 180)
            {
                fits_update_key_dbl(fptr, "CD1_1", -ctemp1, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD1_2", -ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_1", -ctemp3, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_2", -ctemp4, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 270)
            {
                fits_update_key_dbl(fptr, "CD1_1", ctemp4, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD1_2", ctemp3, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_1", -ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_2", -ctemp1, WCS_DECIMALS, comment, &status);
            }
        }
    }

    /* Delete any polynomial solution */
    /* (These could maybe be switched, but I don't want to work them out yet */
    status = 0;
    if (!fits_read_key_dbl(fptr, "CO1_1", &ctemp1, comment, &status))
    {
        int i;
        char keyword[16];

        for (i = 1; i < 13; i++)
        {
            sprintf(keyword, "CO1_%d", i);
            fits_delete_key(fptr, keyword, &status);
        }
        for (i = 1; i < 13; i++)
        {
            sprintf(keyword, "CO2_%d", i);
            fits_delete_key(fptr, keyword, &status);
        }
    }

}

uint8_t * FITSData::getWritableImageBuffer()
{
    return m_ImageBuffer;
}

uint8_t const * FITSData::getImageBuffer() const
{
    return m_ImageBuffer;
}

void FITSData::setImageBuffer(uint8_t * buffer)
{
    delete[] m_ImageBuffer;
    m_ImageBuffer = buffer;
}

bool FITSData::checkDebayer()
{
    int status = 0;
    char bayerPattern[64], roworder[64];

    // Let's search for BAYERPAT keyword, if it's not found we return as there is no bayer pattern in this image
    if (fits_read_keyword(fptr, "BAYERPAT", bayerPattern, nullptr, &status))
        return false;

    if (m_Statistics.dataType != TUSHORT && m_Statistics.dataType != TBYTE)
    {
        m_LastError = i18n("Only 8 and 16 bits bayered images supported.");
        return false;
    }
    QString pattern(bayerPattern);
    pattern = pattern.remove('\'').trimmed();

    QString order(roworder);
    order = order.remove('\'').trimmed();

    if (order == "BOTTOM-UP" && !(m_Statistics.height % 2))
    {
        if (pattern == "RGGB")
            pattern = "GBRG";
        else if (pattern == "GBRG")
            pattern = "RGGB";
        else if (pattern == "GRBG")
            pattern = "BGGR";
        else if (pattern == "BGGR")
            pattern = "GRBG";
        else return false;
    }

    if (pattern == "RGGB")
        debayerParams.filter = DC1394_COLOR_FILTER_RGGB;
    else if (pattern == "GBRG")
        debayerParams.filter = DC1394_COLOR_FILTER_GBRG;
    else if (pattern == "GRBG")
        debayerParams.filter = DC1394_COLOR_FILTER_GRBG;
    else if (pattern == "BGGR")
        debayerParams.filter = DC1394_COLOR_FILTER_BGGR;
    // We return unless we find a valid pattern
    else
    {
        m_LastError = i18n("Unsupported bayer pattern %1.", pattern);
        return false;
    }

    fits_read_key(fptr, TINT, "XBAYROFF", &debayerParams.offsetX, nullptr, &status);
    fits_read_key(fptr, TINT, "YBAYROFF", &debayerParams.offsetY, nullptr, &status);

    if (debayerParams.offsetX == 1)
    {
        // This may leave odd values in the 0th column if the color filter is not there
        // in the sensor, but otherwise should process the offset correctly.
        // Only offsets of 0 or 1 are implemented in debayer_8bit() and debayer_16bit().
        switch (debayerParams.filter)
        {
            case DC1394_COLOR_FILTER_RGGB:
                debayerParams.filter = DC1394_COLOR_FILTER_GRBG;
                break;
            case DC1394_COLOR_FILTER_GBRG:
                debayerParams.filter = DC1394_COLOR_FILTER_BGGR;
                break;
            case DC1394_COLOR_FILTER_GRBG:
                debayerParams.filter = DC1394_COLOR_FILTER_RGGB;
                break;
            case DC1394_COLOR_FILTER_BGGR:
                debayerParams.filter = DC1394_COLOR_FILTER_GBRG;
                break;
        }
        debayerParams.offsetX = 0;
    }
    if (debayerParams.offsetX != 0 || debayerParams.offsetY > 1 || debayerParams.offsetY < 0)
    {
        m_LastError = i18n("Unsupported bayer offsets %1 %2.", debayerParams.offsetX, debayerParams.offsetY);
        return false;
    }

    HasDebayer = true;

    return true;
}

// Check whether a stack sub needs debayering
#if !defined (KSTARS_LITE) && defined (HAVE_WCSLIB) && defined (HAVE_OPENCV)
bool FITSData::stackCheckDebayer(BayerParams bayerParams)
{
    int status = 0;
    char bayerPattern[64], roworder[64];

    // Let's search for BAYERPAT keyword, if it's not found we return as there is no bayer pattern in this image
    if (fits_read_keyword(m_Stackfptr, "BAYERPAT", bayerPattern, nullptr, &status))
        return false;

    if (m_StackStatistics.stats.dataType != TUSHORT && m_StackStatistics.stats.dataType != TBYTE)
    {
        qCDebug(KSTARS_FITS) << QString("Only 8 and 16 bit bayered images supported. Continuing as greyscale...");
        return false;
    }
    QString pattern(bayerPattern);
    pattern = pattern.remove('\'').trimmed();

    QString order(roworder);
    order = order.remove('\'').trimmed();

    if (order == "BOTTOM-UP" && !(m_StackStatistics.stats.height % 2))
    {
        if (pattern == "RGGB")
            pattern = "GBRG";
        else if (pattern == "GBRG")
            pattern = "RGGB";
        else if (pattern == "GRBG")
            pattern = "BGGR";
        else if (pattern == "BGGR")
            pattern = "GRBG";
        else return false;
    }

    if (pattern == "RGGB")
        bayerParams.filter = DC1394_COLOR_FILTER_RGGB;
    else if (pattern == "GBRG")
        bayerParams.filter = DC1394_COLOR_FILTER_GBRG;
    else if (pattern == "GRBG")
        bayerParams.filter = DC1394_COLOR_FILTER_GRBG;
    else if (pattern == "BGGR")
        bayerParams.filter = DC1394_COLOR_FILTER_BGGR;
    else
    {
        qCDebug(KSTARS_FITS) << QString("Unsupported bayer pattern %1.").arg(pattern);
        return false;
    }

    fits_read_key(m_Stackfptr, TINT, "XBAYROFF", &bayerParams.offsetX, nullptr, &status);
    fits_read_key(m_Stackfptr, TINT, "YBAYROFF", &bayerParams.offsetY, nullptr, &status);

    if (bayerParams.offsetX == 1)
    {
        // This may leave odd values in the 0th column if the color filter is not there
        // in the sensor, but otherwise should process the offset correctly.
        // Only offsets of 0 or 1 are implemented in debayer_8bit() and debayer_16bit().
        switch (bayerParams.filter)
        {
            case DC1394_COLOR_FILTER_RGGB:
                bayerParams.filter = DC1394_COLOR_FILTER_GRBG;
                pattern = "GRBG";
                break;
            case DC1394_COLOR_FILTER_GBRG:
                bayerParams.filter = DC1394_COLOR_FILTER_BGGR;
                pattern = "BGGR";
                break;
            case DC1394_COLOR_FILTER_GRBG:
                bayerParams.filter = DC1394_COLOR_FILTER_RGGB;
                pattern = "RGGB";
                break;
            case DC1394_COLOR_FILTER_BGGR:
                bayerParams.filter = DC1394_COLOR_FILTER_GBRG;
                pattern = "GBRG";
                break;
        }
        bayerParams.offsetX = 0;
    }
    if (bayerParams.offsetX != 0 || bayerParams.offsetY > 1 || bayerParams.offsetY < 0)
    {
        qCDebug(KSTARS_FITS) << QString("Unsupported bayer offsets %1 %2.").arg(bayerParams.offsetX).arg(bayerParams.offsetY);
        return false;
    }
    if (m_Stack)
        m_Stack->setBayerPattern(pattern, bayerParams.offsetX, bayerParams.offsetY);
    return true;
}
#endif // !KSTARS_LITE, HAVE_WCSLIB, HAVE_OPENCV

void FITSData::getBayerParams(BayerParams * param)
{
    param->method  = debayerParams.method;
    param->filter  = debayerParams.filter;
    param->offsetX = debayerParams.offsetX;
    param->offsetY = debayerParams.offsetY;
}

void FITSData::setBayerParams(BayerParams * param)
{
    debayerParams.method  = param->method;
    debayerParams.filter  = param->filter;
    debayerParams.offsetX = param->offsetX;
    debayerParams.offsetY = param->offsetY;
}

bool FITSData::debayer(bool reload)
{
    if (reload)
    {
        int anynull = 0, status = 0;

        if (fits_read_img(fptr, m_Statistics.dataType, 1, m_Statistics.samples_per_channel, nullptr, m_ImageBuffer,
                          &anynull, &status))
        {
            //                char errmsg[512];
            //                fits_get_errstatus(status, errmsg);
            //                KSNotification::error(i18n("Error reading image: %1", QString(errmsg)), i18n("Debayer error"));
            return false;
        }
    }

    switch (m_Statistics.dataType)
    {
        case TBYTE:
            return debayer_8bit();

        case TUSHORT:
            return debayer_16bit();

        default:
            return false;
    }
}

bool FITSData::debayer_8bit()
{
    dc1394error_t error_code;

    uint32_t rgb_size = m_Statistics.samples_per_channel * 3 * m_Statistics.bytesPerPixel;
    uint8_t * destinationBuffer = nullptr;

    try
    {
        destinationBuffer = new uint8_t[rgb_size];
    }
    catch (const std::bad_alloc &e)
    {
        logOOMError(rgb_size);
        m_LastError = i18n("Unable to allocate memory for temporary bayer buffer: %1", e.what());
        return false;
    }

    auto * bayer_source_buffer      = reinterpret_cast<uint8_t *>(m_ImageBuffer);
    auto * bayer_destination_buffer = reinterpret_cast<uint8_t *>(destinationBuffer);

    if (bayer_destination_buffer == nullptr)
    {
        logOOMError(rgb_size);
        m_LastError = i18n("Unable to allocate memory for temporary bayer buffer.");
        return false;
    }

    int ds1394_height = m_Statistics.height;
    auto dc1394_source = bayer_source_buffer;

    if (debayerParams.offsetY == 1)
    {
        dc1394_source += m_Statistics.width;
        ds1394_height--;
    }
    // offsetX == 1 is handled in checkDebayer() and should be 0 here.

    error_code = dc1394_bayer_decoding_8bit(dc1394_source, bayer_destination_buffer, m_Statistics.width, ds1394_height,
                                            debayerParams.filter,
                                            debayerParams.method);

    if (error_code != DC1394_SUCCESS)
    {
        m_LastError = i18n("Debayer failed (%1)", error_code);
        m_Statistics.channels = 1;
        delete[] destinationBuffer;
        return false;
    }

    if (m_ImageBufferSize != rgb_size)
    {
        delete[] m_ImageBuffer;
        try
        {
            m_ImageBuffer = new uint8_t[rgb_size];
        }
        catch (const std::bad_alloc &e)
        {
            delete[] destinationBuffer;
            logOOMError(rgb_size);
            m_LastError = i18n("Unable to allocate memory for temporary bayer buffer: %1", e.what());
            return false;
        }

        m_ImageBufferSize = rgb_size;
    }

    auto bayered_buffer = reinterpret_cast<uint8_t *>(m_ImageBuffer);

    // Data in R1G1B1, we need to copy them into 3 layers for FITS

    uint8_t * rBuff = bayered_buffer;
    uint8_t * gBuff = bayered_buffer + (m_Statistics.width * m_Statistics.height);
    uint8_t * bBuff = bayered_buffer + (m_Statistics.width * m_Statistics.height * 2);

    int imax = m_Statistics.samples_per_channel * 3 - 3;
    for (int i = 0; i <= imax; i += 3)
    {
        *rBuff++ = bayer_destination_buffer[i];
        *gBuff++ = bayer_destination_buffer[i + 1];
        *bBuff++ = bayer_destination_buffer[i + 2];
    }

    // TODO Maybe all should be treated the same
    // Doing single channel saves lots of memory though for non-essential
    // frames
    m_Statistics.channels = (m_Mode == FITS_NORMAL || m_Mode == FITS_CALIBRATE || m_Mode == FITS_LIVESTACKING) ? 3 : 1;
    m_Statistics.dataType = TBYTE;
    delete[] destinationBuffer;
    return true;
}

bool FITSData::debayer_16bit()
{
    dc1394error_t error_code;

    uint32_t rgb_size = m_Statistics.samples_per_channel * 3 * m_Statistics.bytesPerPixel;
    uint8_t *destinationBuffer = nullptr;
    try
    {
        destinationBuffer = new uint8_t[rgb_size];
    }
    catch (const std::bad_alloc &e)
    {
        logOOMError(rgb_size);
        m_LastError = i18n("Unable to allocate memory for temporary bayer buffer: %1", e.what());
        return false;
    }

    auto * bayer_source_buffer      = reinterpret_cast<uint16_t *>(m_ImageBuffer);
    auto * bayer_destination_buffer = reinterpret_cast<uint16_t *>(destinationBuffer);

    if (bayer_destination_buffer == nullptr)
    {
        logOOMError(rgb_size);
        m_LastError = i18n("Unable to allocate memory for temporary bayer buffer.");
        return false;
    }

    int ds1394_height = m_Statistics.height;
    auto dc1394_source = bayer_source_buffer;

    if (debayerParams.offsetY == 1)
    {
        dc1394_source += m_Statistics.width;
        ds1394_height--;
    }
    // offsetX == 1 is handled in checkDebayer() and should be 0 here.

    error_code = dc1394_bayer_decoding_16bit(dc1394_source, bayer_destination_buffer, m_Statistics.width, ds1394_height,
                 debayerParams.filter,
                 debayerParams.method, 16);

    if (error_code != DC1394_SUCCESS)
    {
        m_LastError = i18n("Debayer failed (%1)");
        m_Statistics.channels = 1;
        delete[] destinationBuffer;
        return false;
    }

    if (m_ImageBufferSize != rgb_size)
    {
        delete[] m_ImageBuffer;
        try
        {
            m_ImageBuffer = new uint8_t[rgb_size];
        }
        catch (const std::bad_alloc &e)
        {
            logOOMError(rgb_size);
            delete[] destinationBuffer;
            m_LastError = i18n("Unable to allocate memory for temporary bayer buffer: %1", e.what());
            return false;
        }

        m_ImageBufferSize = rgb_size;
    }

    auto bayered_buffer = reinterpret_cast<uint16_t *>(m_ImageBuffer);

    // Data in R1G1B1, we need to copy them into 3 layers for FITS

    uint16_t * rBuff = bayered_buffer;
    uint16_t * gBuff = bayered_buffer + (m_Statistics.width * m_Statistics.height);
    uint16_t * bBuff = bayered_buffer + (m_Statistics.width * m_Statistics.height * 2);

    int imax = m_Statistics.samples_per_channel * 3 - 3;
    for (int i = 0; i <= imax; i += 3)
    {
        *rBuff++ = bayer_destination_buffer[i];
        *gBuff++ = bayer_destination_buffer[i + 1];
        *bBuff++ = bayer_destination_buffer[i + 2];
    }

    m_Statistics.channels = (m_Mode == FITS_NORMAL || m_Mode == FITS_CALIBRATE || m_Mode == FITS_LIVESTACKING) ? 3 : 1;
    m_Statistics.dataType = TUSHORT;
    delete[] destinationBuffer;
    return true;
}

// Template function to handle both 8-bit and 16-bit debayering
#if !defined (KSTARS_LITE) && defined (HAVE_WCSLIB) && defined (HAVE_OPENCV)
template <typename T>
bool FITSData::stackDebayer(BayerParams &bayerParams)
{
    //static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t>,
    //              "Template parameter must be uint8_t or uint16_t");

    dc1394error_t error_code;
    uint32_t rgb_size = m_StackStatistics.stats.samples_per_channel * 3 * m_StackStatistics.stats.bytesPerPixel;
    uint8_t *destinationBuffer = nullptr;

    try
    {
        destinationBuffer = new uint8_t[rgb_size];
    }
    catch (const std::bad_alloc &e)
    {
        qCDebug(KSTARS_FITS) << QString("Unable to allocate memory for temporary bayer buffer: %1").arg(e.what());
        return false;
    }

    auto * bayer_source_buffer = reinterpret_cast<T *>(m_StackImageBuffer);
    auto * bayer_destination_buffer = reinterpret_cast<T *>(destinationBuffer);

    if (bayer_destination_buffer == nullptr)
    {
        qCDebug(KSTARS_FITS) << QString("Unable to allocate memory for temporary bayer buffer");
        return false;
    }

    int ds1394_height = m_StackStatistics.stats.height;
    auto dc1394_source = bayer_source_buffer;

    if (bayerParams.offsetY == 1)
    {
        dc1394_source += m_StackStatistics.stats.width;
        ds1394_height--;
    }

    // Call appropriate debayering function based on template type
    if constexpr (std::is_same_v<T, uint16_t>)
    {
        error_code = dc1394_bayer_decoding_16bit(dc1394_source, bayer_destination_buffer,
                                                 m_StackStatistics.stats.width, ds1394_height,
                                                 bayerParams.filter, bayerParams.method, 16);
    }
    else // uint8_t
    {
        error_code = dc1394_bayer_decoding_8bit(dc1394_source, bayer_destination_buffer,
                                                m_StackStatistics.stats.width, ds1394_height,
                                                bayerParams.filter, bayerParams.method);
    }

    if (error_code != DC1394_SUCCESS)
    {
        qCDebug(KSTARS_FITS) << QString("Debayer failed %1").arg(error_code);
        delete[] destinationBuffer;
        return false;
    }

    if (m_StackImageBufferSize != rgb_size)
    {
        delete[] m_StackImageBuffer;
        try
        {
            m_StackImageBuffer = new uint8_t[rgb_size];
        }
        catch (const std::bad_alloc &e)
        {
            qCDebug(KSTARS_FITS) << QString("Unable to allocate memory for temporary bayer buffer: %1").arg(error_code);
            delete[] destinationBuffer;
            return false;
        }
        m_StackImageBufferSize = rgb_size;
    }

    auto bayered_buffer = reinterpret_cast<T *>(m_StackImageBuffer);

    // Data in R1G1B1, we need to copy them into 3 layers for FITS
    T * rBuff = bayered_buffer;
    T * gBuff = bayered_buffer + (m_StackStatistics.stats.width * m_StackStatistics.stats.height);
    T * bBuff = bayered_buffer + (m_StackStatistics.stats.width * m_StackStatistics.stats.height * 2);

    int imax = m_StackStatistics.stats.samples_per_channel * 3 - 3;
    for (int i = 0; i <= imax; i += 3)
    {
        *rBuff++ = bayer_destination_buffer[i];
        *gBuff++ = bayer_destination_buffer[i + 1];
        *bBuff++ = bayer_destination_buffer[i + 2];
    }

    // Now we've debayered update the channels from 1 to 3
    int type = CV_MAT_DEPTH(m_StackStatistics.cvType);
    int channels = 3;
    m_StackStatistics.stats.channels = 3;
    m_StackStatistics.cvType = CV_MAKETYPE(type, channels);

    delete[] destinationBuffer;
    return true;
}
#endif // !KSTARS_LITE, HAVE_WCSLIB, HAVE_OPENCV

void FITSData::logOOMError(uint32_t requiredMemory)
{
    qCCritical(KSTARS_FITS) << "Debayed memory allocation failure. Required Memory:" << KFormat().formatByteSize(requiredMemory)
                            << "Available system memory:" << KSUtils::getAvailableRAM();
}

double FITSData::getADU() const
{
    double adu = 0;
    for (int i = 0; i < m_Statistics.channels; i++)
        adu += m_Statistics.mean[i];

    return (adu / static_cast<double>(m_Statistics.channels));
}

QString FITSData::getLastError() const
{
    return m_LastError;
}

template <typename T>
void FITSData::convertToQImage(double dataMin, double dataMax, double scale, double zero, QImage &image)
{
    const auto * buffer = reinterpret_cast<const T *>(getImageBuffer());
    const T limit   = std::numeric_limits<T>::max();
    T bMin    = dataMin < 0 ? 0 : dataMin;
    T bMax    = dataMax > limit ? limit : dataMax;
    uint16_t w    = width();
    uint16_t h    = height();
    uint32_t size = w * h;
    double val;

    if (channels() == 1)
    {
        /* Fill in pixel values using indexed map, linear scale */
        for (int j = 0; j < h; j++)
        {
            unsigned char * scanLine = image.scanLine(j);

            for (int i = 0; i < w; i++)
            {
                val         = qBound(bMin, buffer[j * w + i], bMax);
                val         = val * scale + zero;
                scanLine[i] = qBound<unsigned char>(0, static_cast<uint8_t>(val), 255);
            }
        }
    }
    else
    {
        double rval = 0, gval = 0, bval = 0;
        QRgb value;
        /* Fill in pixel values using indexed map, linear scale */
        for (int j = 0; j < h; j++)
        {
            auto * scanLine = reinterpret_cast<QRgb *>((image.scanLine(j)));

            for (int i = 0; i < w; i++)
            {
                rval = qBound(bMin, buffer[j * w + i], bMax);
                gval = qBound(bMin, buffer[j * w + i + size], bMax);
                bval = qBound(bMin, buffer[j * w + i + size * 2], bMax);

                value = qRgb(rval * scale + zero, gval * scale + zero, bval * scale + zero);

                scanLine[i] = value;
            }
        }
    }
}

QImage FITSData::FITSToImage(const QString &filename)
{
    QImage fitsImage;
    double min, max;

    FITSData data;

    QFuture<bool> future = data.loadFromFile(filename);

    // Wait synchronously
    future.waitForFinished();
    if (future.result() == false)
        return fitsImage;

    data.getMinMax(&min, &max);

    if (min == max)
    {
        fitsImage.fill(Qt::white);
        return fitsImage;
    }

    if (data.channels() == 1)
    {
        fitsImage = QImage(data.width(), data.height(), QImage::Format_Indexed8);

        fitsImage.setColorCount(256);
        for (int i = 0; i < 256; i++)
            fitsImage.setColor(i, qRgb(i, i, i));
    }
    else
    {
        fitsImage = QImage(data.width(), data.height(), QImage::Format_RGB32);
    }

    double dataMin = data.m_Statistics.mean[0] - data.m_Statistics.stddev[0];
    double dataMax = data.m_Statistics.mean[0] + data.m_Statistics.stddev[0] * 3;

    double bscale = 255. / (dataMax - dataMin);
    double bzero  = (-dataMin) * (255. / (dataMax - dataMin));

    // Long way to do this since we do not want to use templated functions here
    switch (data.m_Statistics.dataType)
    {
        case TBYTE:
            data.convertToQImage<uint8_t>(dataMin, dataMax, bscale, bzero, fitsImage);
            break;

        case TSHORT:
            data.convertToQImage<int16_t>(dataMin, dataMax, bscale, bzero, fitsImage);
            break;

        case TUSHORT:
            data.convertToQImage<uint16_t>(dataMin, dataMax, bscale, bzero, fitsImage);
            break;

        case TLONG:
            data.convertToQImage<int32_t>(dataMin, dataMax, bscale, bzero, fitsImage);
            break;

        case TULONG:
            data.convertToQImage<uint32_t>(dataMin, dataMax, bscale, bzero, fitsImage);
            break;

        case TFLOAT:
            data.convertToQImage<float>(dataMin, dataMax, bscale, bzero, fitsImage);
            break;

        case TLONGLONG:
            data.convertToQImage<int64_t>(dataMin, dataMax, bscale, bzero, fitsImage);
            break;

        case TDOUBLE:
            data.convertToQImage<double>(dataMin, dataMax, bscale, bzero, fitsImage);
            break;

        default:
            break;
    }

    return fitsImage;
}

bool FITSData::ImageToFITS(const QString &filename, const QString &format, QString &output)
{
    if (QImageReader::supportedImageFormats().contains(format.toLatin1()) == false)
    {
        qCCritical(KSTARS_FITS) << "Failed to convert" << filename << "to FITS since format" << format << "is not supported in Qt";
        return false;
    }

    QImage input;

    if (input.load(filename, format.toLatin1()) == false)
    {
        qCCritical(KSTARS_FITS) << "Failed to open image" << filename;
        return false;
    }

    output = QDir(getTemporaryPath()).filePath(filename + ".fits");

    //This section sets up the FITS File
    fitsfile *fptr = nullptr;
    int status = 0;
    long  fpixel = 1, naxis = input.allGray() ? 2 : 3, nelements, exposure;
    long naxes[3] = { input.width(), input.height(), naxis == 3 ? 3 : 1 };
    char error_status[512] = {0};

    if (fits_create_file(&fptr, QString('!' + output).toLocal8Bit().data(), &status))
    {
        fits_get_errstatus(status, error_status);
        qCCritical(KSTARS_FITS) << "Failed to create FITS file. Error:" << error_status;
        return false;
    }

    if (fits_create_img(fptr, BYTE_IMG, naxis, naxes, &status))
    {
        qCWarning(KSTARS_FITS) << "fits_create_img failed:" << error_status;
        status = 0;
        fits_flush_file(fptr, &status);
        fits_close_file(fptr, &status);
        return false;
    }

    exposure = 1;
    fits_update_key(fptr, TLONG, "EXPOSURE", &exposure, "Total Exposure Time", &status);

    // Gray image
    if (naxis == 2)
    {
        nelements = naxes[0] * naxes[1];
        if (fits_write_img(fptr, TBYTE, fpixel, nelements, input.bits(), &status))
        {
            fits_get_errstatus(status, error_status);
            qCWarning(KSTARS_FITS) << "fits_write_img GRAY failed:" << error_status;
            status = 0;
            fits_flush_file(fptr, &status);
            fits_close_file(fptr, &status);
            return false;
        }
    }
    // RGB image, we have to convert from ARGB format to R G B for each plane
    else
    {
        nelements = naxes[0] * naxes[1] * 3;

        uint8_t *srcBuffer = input.bits();
        // ARGB
        uint32_t srcBytes = naxes[0] * naxes[1] * 4 - 4;

        uint8_t *rgbBuffer = new uint8_t[nelements];
        if (rgbBuffer == nullptr)
        {
            qCWarning(KSTARS_FITS) << "Not enough memory for RGB buffer";
            fits_flush_file(fptr, &status);
            fits_close_file(fptr, &status);
            return false;
        }

        uint8_t *subR = rgbBuffer;
        uint8_t *subG = rgbBuffer + naxes[0] * naxes[1];
        uint8_t *subB = rgbBuffer + naxes[0] * naxes[1] * 2;
        for (uint32_t i = 0; i < srcBytes; i += 4)
        {
            *subB++ = srcBuffer[i];
            *subG++ = srcBuffer[i + 1];
            *subR++ = srcBuffer[i + 2];
        }

        if (fits_write_img(fptr, TBYTE, fpixel, nelements, rgbBuffer, &status))
        {
            fits_get_errstatus(status, error_status);
            qCWarning(KSTARS_FITS) << "fits_write_img RGB failed:" << error_status;
            status = 0;
            fits_flush_file(fptr, &status);
            fits_close_file(fptr, &status);
            delete [] rgbBuffer;
            return false;
        }

        delete [] rgbBuffer;
    }

    if (fits_flush_file(fptr, &status))
    {
        fits_get_errstatus(status, error_status);
        qCWarning(KSTARS_FITS) << "fits_flush_file failed:" << error_status;
        status = 0;
        fits_close_file(fptr, &status);
        return false;
    }

    if (fits_close_file(fptr, &status))
    {
        fits_get_errstatus(status, error_status);
        qCWarning(KSTARS_FITS) << "fits_close_file failed:" << error_status;
        return false;
    }

    return true;
}

void FITSData::injectWCS(double orientation, double ra, double dec, double pixscale, bool eastToTheRight)
{
    int status = 0;

    updateWCSHeaderData(orientation, ra, dec, pixscale, eastToTheRight);

    if (fptr)
    {
        char radec[16];
        strcpy(radec, dms(ra).toHMSString(true, false, ' ').toStdString().c_str());
        fits_update_key(fptr, TSTRING, "OBJCTRA", radec, "Object RA", &status);
        strcpy(radec, dms(dec).toDMSString(true, true, false, ' ').toStdString().c_str());
        fits_update_key(fptr, TSTRING, "OBJCTDEC", radec, "Object DEC", &status);
        int epoch = 2000;

        fits_update_key(fptr, TINT, "EQUINOX", &epoch, "Equinox", &status);

        fits_update_key(fptr, TDOUBLE, "CRVAL1", &ra, "CRVAL1", &status);
        fits_update_key(fptr, TDOUBLE, "CRVAL2", &dec, "CRVAL2", &status);

        char radecsys[8] = "FK5";
        char ctype1[16]  = "RA---TAN";
        char ctype2[16]  = "DEC--TAN";

        fits_update_key(fptr, TSTRING, "RADECSYS", radecsys, "RADECSYS", &status);
        fits_update_key(fptr, TSTRING, "CTYPE1", ctype1, "CTYPE1", &status);
        fits_update_key(fptr, TSTRING, "CTYPE2", ctype2, "CTYPE2", &status);

        double crpix1 = width() / 2.0;
        double crpix2 = height() / 2.0;

        fits_update_key(fptr, TDOUBLE, "CRPIX1", &crpix1, "CRPIX1", &status);
        fits_update_key(fptr, TDOUBLE, "CRPIX2", &crpix2, "CRPIX2", &status);

        // Arcsecs per Pixel
        double secpix1 = eastToTheRight ? pixscale : -pixscale;
        double secpix2 = pixscale;

        fits_update_key(fptr, TDOUBLE, "SECPIX1", &secpix1, "SECPIX1", &status);
        fits_update_key(fptr, TDOUBLE, "SECPIX2", &secpix2, "SECPIX2", &status);

        double degpix1 = secpix1 / 3600.0;
        double degpix2 = secpix2 / 3600.0;

        fits_update_key(fptr, TDOUBLE, "CDELT1", &degpix1, "CDELT1", &status);
        fits_update_key(fptr, TDOUBLE, "CDELT2", &degpix2, "CDELT2", &status);

        // Rotation is CW, we need to convert it to CCW per CROTA1 definition
        double rotation = 360 - orientation;
        if (rotation > 360)
            rotation -= 360;

        fits_update_key(fptr, TDOUBLE, "CROTA1", &rotation, "CROTA1", &status);
        fits_update_key(fptr, TDOUBLE, "CROTA2", &rotation, "CROTA2", &status);
    }

    emit headerChanged();
    m_WCSState = Idle;
}

void FITSData::injectStackWCS(double orientation, double ra, double dec, double pixscale, bool eastToTheRight)
{
    updateWCSHeaderData(orientation, ra, dec, pixscale, eastToTheRight, true);
}

void FITSData::setStackSubSolution(const double ra, const double dec, const double pixscale, const int index, const int healpix)
{
    m_StackSubRa = ra;
    m_StackSubDec = dec;
    m_StackSubPixscale = pixscale;
    m_StackSubIndex = index;
    m_StackSubHealpix = healpix;
}

// Update header records based on plate solver solution
void FITSData::updateWCSHeaderData(const double orientation, const double ra, const double dec, const double pixscale,
                                   const bool eastToTheRight, const bool stack)
{
    updateRecordValue("OBJCTRA", dms(ra).toHMSString(true, false, ' '), "Object RA", stack);
    updateRecordValue("OBJCTDEC", dms(dec).toDMSString(true, true, false, ' '), "Object DEC", stack);
    updateRecordValue("EQUINOX", 2000, "Equinox", stack);
    updateRecordValue("CRVAL1", ra, "CRVAL1", stack);
    updateRecordValue("CRVAL2", dec, "CRVAL2", stack);

    updateRecordValue("RADECSYS", "FK5", "RADECSYS", stack);
    updateRecordValue("CTYPE1", "RA---TAN", "CTYPE1", stack);
    updateRecordValue("CTYPE2", "DEC--TAN", "CTYPE2", stack);

    auto width = stack ? getStackStatistics().width : m_Statistics.width;
    auto height = stack ? getStackStatistics().height : m_Statistics.height;
    updateRecordValue("CRPIX1", width / 2.0, "CRPIX1", stack);
    updateRecordValue("CRPIX2", height / 2.0, "CRPIX2", stack);

    double secpix1 = eastToTheRight ? pixscale : -pixscale;
    double secpix2 = pixscale;

    double degpix1 = secpix1 / 3600.0;
    double degpix2 = secpix2 / 3600.0;
    updateRecordValue("CDELT1", degpix1, "CDELT1", stack);
    updateRecordValue("CDELT2", degpix2, "CDELT2", stack);

    // Rotation is CW, we need to convert it to CCW per CROTA1 definition
    double rotation = 360 - orientation;
    if (rotation > 360)
        rotation -= 360;
    updateRecordValue("CROTA1", rotation, "CROTA1", stack);
    updateRecordValue("CROTA2", rotation, "CROTA2", stack);
}

bool FITSData::contains(const QPointF &point) const
{
    return (point.x() >= 0 && point.y() >= 0 && point.x() <= m_Statistics.width && point.y() <= m_Statistics.height);
}

void FITSData::saveStatistics(FITSImage::Statistic &other)
{
    other = m_Statistics;
}

void FITSData::restoreStatistics(FITSImage::Statistic &other)
{
    m_Statistics = other;

    emit dataChanged();
}

void FITSData::constructHistogram()
{
    switch (m_Statistics.dataType)
    {
        case TBYTE:
            constructHistogramInternal<uint8_t>();
            break;

        case TSHORT:
            constructHistogramInternal<int16_t>();
            break;

        case TUSHORT:
            constructHistogramInternal<uint16_t>();
            break;

        case TLONG:
            constructHistogramInternal<int32_t>();
            break;

        case TULONG:
            constructHistogramInternal<uint32_t>();
            break;

        case TFLOAT:
            constructHistogramInternal<float>();
            break;

        case TLONGLONG:
            constructHistogramInternal<int64_t>();
            break;

        case TDOUBLE:
            constructHistogramInternal<double>();
            break;

        default:
            break;
    }
}

template <typename T> int32_t FITSData::histogramBinInternal(T value, int channel) const
{
    return qMax(static_cast<T>(0), qMin(static_cast<T>(m_HistogramBinCount),
                                        static_cast<T>(rint((value - m_Statistics.min[channel]) / m_HistogramBinWidth[channel]))));
}

template <typename T> int32_t FITSData::histogramBinInternal(int x, int y, int channel) const
{
    if (!m_ImageBuffer || !isHistogramConstructed())
        return 0;
    uint32_t samples = m_Statistics.width * m_Statistics.height;
    uint32_t offset = channel * samples;
    auto * const buffer = reinterpret_cast<T const *>(m_ImageBuffer);
    int index = y * m_Statistics.width + x;
    const T &sample = buffer[index + offset];
    return histogramBinInternal(sample, channel);
}

int32_t FITSData::histogramBin(int x, int y, int channel) const
{
    switch (m_Statistics.dataType)
    {
        case TBYTE:
            return histogramBinInternal<uint8_t>(x, y, channel);
            break;

        case TSHORT:
            return histogramBinInternal<int16_t>(x, y, channel);
            break;

        case TUSHORT:
            return histogramBinInternal<uint16_t>(x, y, channel);
            break;

        case TLONG:
            return histogramBinInternal<int32_t>(x, y, channel);
            break;

        case TULONG:
            return histogramBinInternal<uint32_t>(x, y, channel);
            break;

        case TFLOAT:
            return histogramBinInternal<float>(x, y, channel);
            break;

        case TLONGLONG:
            return histogramBinInternal<int64_t>(x, y, channel);
            break;

        case TDOUBLE:
            return histogramBinInternal<double>(x, y, channel);
            break;

        default:
            return 0;
            break;
    }
}

template <typename T> void FITSData::constructHistogramInternal()
{
    auto * const buffer = reinterpret_cast<T const *>(m_ImageBuffer);
    uint32_t samples = m_Statistics.width * m_Statistics.height;
    const uint32_t sampleBy = samples > 500000 ? samples / 500000 : 1;

    m_HistogramBinCount = qMax(0., qMin(m_Statistics.max[0] - m_Statistics.min[0], 256.0));
    if (m_HistogramBinCount <= 0)
        m_HistogramBinCount = 256;

    for (int n = 0; n < m_Statistics.channels; n++)
    {
        m_HistogramIntensity[n].fill(0, m_HistogramBinCount + 1);
        m_HistogramFrequency[n].fill(0, m_HistogramBinCount + 1);
        m_CumulativeFrequency[n].fill(0, m_HistogramBinCount + 1);
        // Distinguish between 0-1.0 ranges and ranges with integer values.
        const double minBinSize = (m_Statistics.max[n] > 1.1) ? 1.0 : .0001;
        m_HistogramBinWidth[n] = qMax(minBinSize, (m_Statistics.max[n] - m_Statistics.min[n]) / m_HistogramBinCount);
    }

    QVector<QFuture<void>> futures;

    for (int n = 0; n < m_Statistics.channels; n++)
    {
        futures.append(QtConcurrent::run([ = ]()
        {
            for (int i = 0; i < m_HistogramBinCount; i++)
                m_HistogramIntensity[n][i] = m_Statistics.min[n] + (m_HistogramBinWidth[n] * i);
        }));
    }

    for (int n = 0; n < m_Statistics.channels; n++)
    {
        futures.append(QtConcurrent::run([ = ]()
        {
            uint32_t offset = n * samples;

            for (uint32_t i = 0; i < samples; i += sampleBy)
            {
                int32_t id = histogramBinInternal<T>(buffer[i + offset], n);
                m_HistogramFrequency[n][id] += sampleBy;
            }
        }));
    }

    for (QFuture<void> future : futures)
        future.waitForFinished();

    futures.clear();

    for (int n = 0; n < m_Statistics.channels; n++)
    {
        futures.append(QtConcurrent::run([ = ]()
        {
            uint32_t accumulator = 0;
            for (int i = 0; i < m_HistogramBinCount; i++)
            {
                accumulator += m_HistogramFrequency[n][i];
                m_CumulativeFrequency[n].replace(i, accumulator);
            }
        }));
    }

    for (QFuture<void> future : futures)
        future.waitForFinished();

    futures.clear();

    // Custom index to indicate the overall contrast of the image
    if (m_CumulativeFrequency[RED_CHANNEL][m_HistogramBinCount / 4] > 0)
        m_JMIndex = m_CumulativeFrequency[RED_CHANNEL][m_HistogramBinCount / 8] / static_cast<double>
                    (m_CumulativeFrequency[RED_CHANNEL][m_HistogramBinCount /
                            4]);
    else
        m_JMIndex = 1;

    qCDebug(KSTARS_FITS) << "FITHistogram: JMIndex " << m_JMIndex;

    m_HistogramConstructed = true;
    emit histogramReady();
}

void FITSData::recordLastError(int errorCode)
{
    char fitsErrorMessage[512] = {0};
    fits_get_errstatus(errorCode, fitsErrorMessage);
    m_LastError = fitsErrorMessage;
}

double FITSData::getAverageMean(bool roi) const
{
    const FITSImage::Statistic* ptr = (roi ? &m_ROIStatistics : &m_Statistics);
    if (ptr->channels == 1)
        return ptr->mean[0];
    else
        return (ptr->mean[0] + ptr->mean[1] + ptr->mean[2]) / 3.0;
}

double FITSData::getAverageMedian(bool roi) const
{
    const FITSImage::Statistic* ptr = (roi ? &m_ROIStatistics : &m_Statistics);
    if (ptr->channels == 1)
        return ptr->median[0];
    else
        return (ptr->median[0] + ptr->median[1] + ptr->median[2]) / 3.0;
}

double FITSData::getAverageStdDev(bool roi) const
{
    const FITSImage::Statistic* ptr = (roi ? &m_ROIStatistics : &m_Statistics);
    if (ptr->channels == 1)
        return ptr->stddev[0];
    else
        return (ptr->stddev[0] + ptr->stddev[1] + ptr->stddev[2]) / 3.0;
}
