/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "darklibrary.h"
#include "auxiliary/ksmessagebox.h"
#include "auxiliary/ksnotification.h"

#include "Options.h"

#include "ekos/manager.h"
#include "ekos/capture/capture.h"
#include "ekos/capture/sequencejob.h"
#include "kstars.h"
#include "kspaths.h"
#include "kstarsdata.h"
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitsview.h"
#include "fitsviewer/fitshistogramview.h"

#include "ekos_debug.h"

#include <QDesktopServices>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QStatusBar>
#include <QtConcurrent>
#include <algorithm>
#include <array>

namespace Ekos
{
DarkLibrary *DarkLibrary::_DarkLibrary = nullptr;

DarkLibrary *DarkLibrary::Instance()
{
    if (_DarkLibrary == nullptr)
        _DarkLibrary = new DarkLibrary(Manager::Instance());

    return _DarkLibrary;
}

DarkLibrary::DarkLibrary(QWidget *parent) : QDialog(parent)
{
    setupUi(this);

    m_StatusBar = new QStatusBar(this);
    m_StatusLabel = new QLabel(i18n("Idle"), this);
    m_FileLabel = new QLabel(this);
    m_FileLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_StatusBar->insertPermanentWidget(0, m_StatusLabel);
    m_StatusBar->insertPermanentWidget(1, m_FileLabel, 1);
    mainLayout->addWidget(m_StatusBar);

    histogramView->setProperty("axesLabelEnabled", false);
    //histogramView->setProperty("linear", true);

    QDir writableDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation));
    writableDir.mkpath("darks");
    writableDir.mkpath("defectmaps");

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Dark Generation Connections
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    m_CurrentDarkFrame.reset(new FITSData(), &QObject::deleteLater);

    m_DarkCameras = Options::darkCameras();
    m_DefectCameras = Options::defectCameras();

    connect(darkHandlingButtonGroup, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::buttonToggled), [this]()
    {
        const QString device = m_CurrentCamera->getDeviceName();
        if (preferDarksRadio->isChecked())
        {
            m_DefectCameras.removeOne(device);
            if (!m_DarkCameras.contains(device))
                m_DarkCameras.append(device);
        }
        else
        {
            m_DarkCameras.removeOne(device);
            if (!m_DefectCameras.contains(device))
                m_DefectCameras.append(device);
        }

        Options::setDarkCameras(m_DarkCameras);
        Options::setDefectCameras(m_DefectCameras);
    });

    connect(darkTableView,  &QAbstractItemView::doubleClicked, this, &DarkLibrary::loadDarkFITS);
    connect(openDarksFolderB, &QPushButton::clicked, this, &DarkLibrary::openDarksFolder);
    connect(clearAllB, &QPushButton::clicked, this, &DarkLibrary::clearAll);
    connect(clearRowB, &QPushButton::clicked, this, &DarkLibrary::clearRow);
    connect(clearExpiredB, &QPushButton::clicked, this, &DarkLibrary::clearExpired);
    connect(refreshB, &QPushButton::clicked, this, &DarkLibrary::reloadDarksFromDatabase);

    connect(cameraS, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), [this]()
    {
        checkCamera();
        reloadDarksFromDatabase();
    });

    connect(&m_DarkFrameFutureWatcher, &QFutureWatcher<bool>::finished, [this]()
    {
        // If loading is successful, then set it in current dark view
        if (m_DarkFrameFutureWatcher.result())
        {
            m_DarkView->loadData(m_CurrentDarkFrame);
            loadCurrentMasterDefectMap();
            histogramView->setImageData(m_CurrentDarkFrame);
            if (!Options::nonLinearHistogram() && !m_CurrentDarkFrame->isHistogramConstructed())
                m_CurrentDarkFrame->constructHistogram();
            populateMasterMetedata();
        }
        else
            m_FileLabel->setText(i18n("Failed to load %1: %2",  m_MasterDarkFrameFilename, m_CurrentDarkFrame->getLastError()));

    });
    connect(m_CurrentDarkFrame.data(), &FITSData::histogramReady, [this]()
    {
        histogramView->setEnabled(true);
        histogramView->reset();
        histogramView->syncGUI();
    });

    connect(masterDarksCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int index)
    {
        DarkLibrary::loadCurrentMasterDark(cameraS->currentText(), index);
    });


    connect(minExposureSpin, &QDoubleSpinBox::editingFinished, this, &DarkLibrary::countDarkTotalTime);
    connect(maxExposureSpin, &QDoubleSpinBox::editingFinished, this, &DarkLibrary::countDarkTotalTime);
    connect(exposureStepSin, &QDoubleSpinBox::editingFinished, this, &DarkLibrary::countDarkTotalTime);

    connect(minTemperatureSpin, &QDoubleSpinBox::editingFinished, [this]()
    {
        maxTemperatureSpin->setMinimum(minTemperatureSpin->value());
        countDarkTotalTime();
    });
    connect(maxTemperatureSpin, &QDoubleSpinBox::editingFinished, [this]()
    {
        minTemperatureSpin->setMaximum(maxTemperatureSpin->value());
        countDarkTotalTime();
    });
    connect(temperatureStepSpin, &QDoubleSpinBox::editingFinished, [this]()
    {
        maxTemperatureSpin->setMinimum(minTemperatureSpin->value());
        minTemperatureSpin->setMaximum(maxTemperatureSpin->value());
        countDarkTotalTime();
    });

    connect(countSpin, &QDoubleSpinBox::editingFinished, this, &DarkLibrary::countDarkTotalTime);

    connect(binningButtonGroup, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::buttonToggled), this, [this](int,
            bool)
    {
        countDarkTotalTime();
    });

    connect(startB, &QPushButton::clicked, [this]()
    {
        generateDarkJobs();
        executeDarkJobs();
    });

    connect(stopB, &QPushButton::clicked, this, &DarkLibrary::stopDarkJobs);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Master Darks Database Connections
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    kcfg_DarkLibraryDuration->setValue(Options::darkLibraryDuration());
    connect(kcfg_DarkLibraryDuration, &QDoubleSpinBox::editingFinished, [this]()
    {
        Options::setDarkLibraryDuration(kcfg_DarkLibraryDuration->value());
    });

    kcfg_MaxDarkTemperatureDiff->setValue(Options::maxDarkTemperatureDiff());
    connect(kcfg_MaxDarkTemperatureDiff, &QDoubleSpinBox::editingFinished, [this]()
    {
        Options::setMaxDarkTemperatureDiff(kcfg_MaxDarkTemperatureDiff->value());
    });

    KStarsData::Instance()->userdb()->GetAllDarkFrames(m_DarkFramesDatabaseList);
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Defect Map Connections
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    connect(darkTabsWidget, &QTabWidget::currentChanged, [this](int index)
    {
        m_DarkView->setDefectMapEnabled(index == 1 && m_CurrentDefectMap);
    });
    connect(aggresivenessHotSlider, &QSlider::valueChanged, aggresivenessHotSpin, &QSpinBox::setValue);
    connect(aggresivenessColdSlider, &QSlider::valueChanged, aggresivenessColdSpin, &QSpinBox::setValue);
    connect(hotPixelsEnabled, &QCheckBox::toggled, [this](bool toggled)
    {
        if (m_CurrentDefectMap)
            m_CurrentDefectMap->setProperty("HotEnabled", toggled);
    });
    connect(coldPixelsEnabled, &QCheckBox::toggled, [this](bool toggled)
    {
        if (m_CurrentDefectMap)
            m_CurrentDefectMap->setProperty("ColdEnabled", toggled);
    });
    connect(generateMapB, &QPushButton::clicked, [this]()
    {
        if (m_CurrentDefectMap)
        {
            m_CurrentDefectMap->setProperty("HotPixelAggressiveness", aggresivenessHotSpin->value());
            m_CurrentDefectMap->setProperty("ColdPixelAggressiveness", aggresivenessColdSpin->value());
            m_CurrentDefectMap->filterPixels();
        }
    });
    connect(resetMapParametersB, &QPushButton::clicked, [this]()
    {
        if (m_CurrentDefectMap)
        {
            aggresivenessHotSlider->setValue(75);
            aggresivenessColdSlider->setValue(75);
            m_CurrentDefectMap->setProperty("HotPixelAggressiveness", 75);
            m_CurrentDefectMap->setProperty("ColdPixelAggressiveness", 75);
            m_CurrentDefectMap->filterPixels();
        }
    });
    connect(saveMapB, &QPushButton::clicked, this, &DarkLibrary::saveDefectMap);
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Settings & Initialization
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    m_RememberFITSViewer = Options::useFITSViewer();
    m_RememberSummaryView = Options::useSummaryPreview();
    initView();
}

DarkLibrary::~DarkLibrary()
{
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::refreshFromDB()
{
    KStarsData::Instance()->userdb()->GetAllDarkFrames(m_DarkFramesDatabaseList);
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
bool DarkLibrary::findDarkFrame(ISD::CCDChip *m_TargetChip, double duration, QSharedPointer<FITSData> &darkData)
{
    QVariantMap bestCandidate;
    for (auto &map : m_DarkFramesDatabaseList)
    {
        // First check CCD name matches and check if we are on the correct chip
        if (map["ccd"].toString() == m_TargetChip->getCCD()->getDeviceName() &&
                map["chip"].toInt() == static_cast<int>(m_TargetChip->getType()))
        {
            int binX, binY;
            m_TargetChip->getBinning(&binX, &binY);

            // Then check if binning is the same
            if (map["binX"].toInt() == binX && map["binY"].toInt() == binY)
            {
                // If camera has an active cooler, then we check temperature against the absolute threshold.
                if (m_TargetChip->getCCD()->hasCoolerControl())
                {
                    double temperature = 0;
                    m_TargetChip->getCCD()->getTemperature(&temperature);
                    double darkTemperature = map["temperature"].toDouble();
                    // If different is above threshold, it is completely rejected.
                    if (darkTemperature != INVALID_VALUE && fabs(darkTemperature - temperature) > Options::maxDarkTemperatureDiff())
                        continue;
                }

                if (bestCandidate.isEmpty())
                {
                    bestCandidate = map;
                    continue;
                }

                // We try to find the best frame
                // Frame closest in exposure duration wins
                // Frame with temperature closest to stored temperature wins (if temperature is reported)
                uint32_t thisMapScore = 0;
                uint32_t bestCandidateScore = 0;

                // Else we check for the closest passive temperature
                if (m_TargetChip->getCCD()->hasCooler())
                {
                    double temperature = 0;
                    m_TargetChip->getCCD()->getTemperature(&temperature);
                    double diffMap = std::fabs(temperature - map["temperature"].toDouble());
                    double diffBest = std::fabs(temperature - bestCandidate["temperature"].toDouble());
                    // Prefer temperatures closest to target
                    if (diffMap < diffBest)
                        thisMapScore++;
                    else if (diffBest < diffMap)
                        bestCandidateScore++;
                }

                // Duration has a higher score priority over temperature
                {
                    double diffMap = std::fabs(map["duration"].toDouble() - duration);
                    double diffBest = std::fabs(bestCandidate["duration"].toDouble() - duration);
                    if (diffMap < diffBest)
                        thisMapScore += 2;
                    else if (diffBest < diffMap)
                        bestCandidateScore += 2;
                }

                // More recent has a higher score than older.
                {
                    const QDateTime now = QDateTime::currentDateTime();
                    int64_t diffMap  = map["timestamp"].toDateTime().secsTo(now);
                    int64_t diffBest = bestCandidate["timestamp"].toDateTime().secsTo(now);
                    if (diffMap < diffBest)
                        thisMapScore += 2;
                    else if (diffBest < diffMap)
                        bestCandidateScore += 2;
                }

                // Find candidate with closest time in case we have multiple defect maps
                if (thisMapScore > bestCandidateScore)
                    bestCandidate = map;
            }
        }
    }

    if (bestCandidate.isEmpty())
        return false;

    if (fabs(bestCandidate["duration"].toDouble() - duration) > 3)
        emit i18n("Using available dark frame with %1 seconds exposure. Please take a dark frame with %1 seconds exposure for more accurate results.",
                  QString::number(bestCandidate["duration"].toDouble(), 'f', 1),
                  QString::number(duration, 'f', 1));

    QString filename = bestCandidate["filename"].toString();

    // Finally check if the duration is acceptable
    QDateTime frameTime = bestCandidate["timestamp"].toDateTime();
    if (frameTime.daysTo(QDateTime::currentDateTime()) > Options::darkLibraryDuration())
    {
        emit i18n("Dark frame %s is expired. Please create new master dark.", filename);
        return false;
    }

    if (m_CachedDarkFrames.contains(filename))
    {
        darkData = m_CachedDarkFrames[filename];
        return true;
    }

    // Finally we made it, let's put it in the hash
    if (cacheDarkFrameFromFile(filename))
    {
        darkData = m_CachedDarkFrames[filename];
        return true;
    }

    // Remove bad dark frame
    emit newLog(i18n("Removing bad dark frame file %1", filename));
    m_CachedDarkFrames.remove(filename);
    QFile::remove(filename);
    KStarsData::Instance()->userdb()->DeleteDarkFrame(filename);
    return false;

}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
bool DarkLibrary::findDefectMap(ISD::CCDChip *m_TargetChip, double duration, QSharedPointer<DefectMap> &defectMap)
{
    QVariantMap bestCandidate;
    for (auto &map : m_DarkFramesDatabaseList)
    {
        if (map["defectmap"].toString().isEmpty())
            continue;

        // First check CCD name matches and check if we are on the correct chip
        if (map["ccd"].toString() == m_TargetChip->getCCD()->getDeviceName() &&
                map["chip"].toInt() == static_cast<int>(m_TargetChip->getType()))
        {
            int binX, binY;
            m_TargetChip->getBinning(&binX, &binY);

            // Then check if binning is the same
            if (map["binX"].toInt() == binX && map["binY"].toInt() == binY)
            {
                if (bestCandidate.isEmpty())
                {
                    bestCandidate = map;
                    continue;
                }

                // We try to find the best frame
                // Frame closest in exposure duration wins
                // Frame with temperature closest to stored temperature wins (if temperature is reported)
                uint32_t thisMapScore = 0;
                uint32_t bestCandidateScore = 0;

                // Else we check for the closest passive temperature
                if (m_TargetChip->getCCD()->hasCooler())
                {
                    double temperature = 0;
                    m_TargetChip->getCCD()->getTemperature(&temperature);
                    double diffMap = std::fabs(temperature - map["temperature"].toDouble());
                    double diffBest = std::fabs(temperature - bestCandidate["temperature"].toDouble());
                    // Prefer temperatures closest to target
                    if (diffMap < diffBest)
                        thisMapScore++;
                    else if (diffBest < diffMap)
                        bestCandidateScore++;
                }

                // Duration has a higher score priority over temperature
                double diffMap = std::fabs(map["duration"].toDouble() - duration);
                double diffBest = std::fabs(bestCandidate["duration"].toDouble() - duration);
                if (diffMap < diffBest)
                    thisMapScore += 2;
                else if (diffBest < diffMap)
                    bestCandidateScore += 2;

                // Find candidate with closest time in case we have multiple defect maps
                if (thisMapScore > bestCandidateScore)
                    bestCandidate = map;
            }
        }
    }


    if (bestCandidate.isEmpty())
        return false;

    QString darkFilename = bestCandidate["filename"].toString();
    QString defectFilename = bestCandidate["defectmap"].toString();

    if (darkFilename.isEmpty() || defectFilename.isEmpty())
        return false;

    if (m_CachedDefectMaps.contains(darkFilename))
    {
        defectMap = m_CachedDefectMaps[darkFilename];
        return true;
    }

    // Finally we made it, let's put it in the hash
    if (cacheDefectMapFromFile(darkFilename, defectFilename))
    {
        defectMap = m_CachedDefectMaps[darkFilename];
        return true;
    }
    else
    {
        // Remove bad dark frame
        emit newLog(i18n("Failed to load defect map %1", defectFilename));
        return false;
    }
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
bool DarkLibrary::cacheDefectMapFromFile(const QString &key, const QString &filename)
{
    QSharedPointer<DefectMap> oneMap;
    oneMap.reset(new DefectMap());

    if (oneMap->load(filename))
    {
        oneMap->filterPixels();
        m_CachedDefectMaps[key] = oneMap;
        return true;
    }

    emit newLog(i18n("Failed to load defect map file %1", filename));
    return false;
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
bool DarkLibrary::cacheDarkFrameFromFile(const QString &filename)
{
    QFuture<bool> rc = m_CurrentDarkFrame->loadFromFile(filename);

    rc.waitForFinished();
    if (rc.result())
        m_CachedDarkFrames[filename] = m_CurrentDarkFrame;
    else
    {
        emit newLog(i18n("Failed to load dark frame file %1", filename));
    }

    return rc;
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::normalizeDefects(const QSharedPointer<DefectMap> &defectMap, const QSharedPointer<FITSData> &lightData,
                                   FITSScale filter, uint16_t offsetX, uint16_t offsetY)
{
    switch (lightData->dataType())
    {
        case TBYTE:
            normalizeDefectsInternal<uint8_t>(defectMap, lightData, filter, offsetX, offsetY);
            break;

        case TSHORT:
            normalizeDefectsInternal<int16_t>(defectMap, lightData, filter, offsetX, offsetY);
            break;

        case TUSHORT:
            normalizeDefectsInternal<uint16_t>(defectMap, lightData, filter, offsetX, offsetY);
            break;

        case TLONG:
            normalizeDefectsInternal<int32_t>(defectMap, lightData, filter, offsetX, offsetY);
            break;

        case TULONG:
            normalizeDefectsInternal<uint32_t>(defectMap, lightData, filter, offsetX, offsetY);
            break;

        case TFLOAT:
            normalizeDefectsInternal<float>(defectMap, lightData, filter, offsetX, offsetY);
            break;

        case TLONGLONG:
            normalizeDefectsInternal<int64_t>(defectMap, lightData, filter, offsetX, offsetY);
            break;

        case TDOUBLE:
            normalizeDefectsInternal<double>(defectMap, lightData, filter, offsetX, offsetY);
            break;

        default:
            break;
    }

}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
template <typename T>
void DarkLibrary::normalizeDefectsInternal(const QSharedPointer<DefectMap> &defectMap,
        const QSharedPointer<FITSData> &lightData, FITSScale filter, uint16_t offsetX, uint16_t offsetY)
{

    Q_UNUSED(filter);
    T *lightBuffer = reinterpret_cast<T *>(lightData->getWritableImageBuffer());
    const uint32_t width = lightData->width();

    // Account for offset X and Y
    // e.g. if we send a subframed light frame 100x100 pixels wide
    // but the source defect map covers 1000x1000 pixels array, then we need to only compensate
    // for the 100x100 region.
    for (BadPixelSet::const_iterator onePixel = defectMap->hotThreshold();
            onePixel != defectMap->hotPixels().cend(); ++onePixel)
    {
        const uint16_t x = (*onePixel).x;
        const uint16_t y = (*onePixel).y;

        if (x <= offsetX || y <= offsetY)
            continue;

        uint32_t offset = (x - offsetX) + (y - offsetY) * width;

        lightBuffer[offset] = median3x3Filter(x - offsetX, y - offsetY, width, lightBuffer);
    }

    for (BadPixelSet::const_iterator onePixel = defectMap->coldPixels().cbegin();
            onePixel != defectMap->coldThreshold(); ++onePixel)
    {
        const uint16_t x = (*onePixel).x;
        const uint16_t y = (*onePixel).y;

        if (x <= offsetX || y <= offsetY)
            continue;

        uint32_t offset = (x - offsetX) + (y - offsetY) * width;

        lightBuffer[offset] = median3x3Filter(x - offsetX, y - offsetY, width, lightBuffer);
    }

    lightData->calculateStats(true);
    emit darkFrameCompleted(true);

}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
template <typename T>
T DarkLibrary::median3x3Filter(uint16_t x, uint16_t y, uint32_t width, T *buffer)
{
    T *top = buffer + (y - 1) * width + (x - 1);
    T *mid = buffer + (y - 0) * width + (x - 1);
    T *bot = buffer + (y + 1) * width + (x - 1);

    std::array<T, 8> elements;

    // Top
    elements[0] = *(top + 0);
    elements[1] = *(top + 1);
    elements[2] = *(top + 2);
    // Mid
    elements[3] = *(mid + 0);
    // Mid+1 is the defective value, so we skip and go for + 2
    elements[4] = *(mid + 2);
    // Bottom
    elements[5] = *(bot + 0);
    elements[6] = *(bot + 1);
    elements[7] = *(bot + 2);

    std::sort(elements.begin(), elements.end());
    auto median = (elements[3] + elements[4]) / 2;
    return median;
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::subtractDarkData(const QSharedPointer<FITSData> &darkData, const QSharedPointer<FITSData> &lightData,
                                   FITSScale filter, uint16_t offsetX, uint16_t offsetY)
{
    switch (darkData->dataType())
    {
        case TBYTE:
            subtractInternal<uint8_t>(darkData, lightData, filter, offsetX, offsetY);
            break;

        case TSHORT:
            subtractInternal<int16_t>(darkData, lightData, filter, offsetX, offsetY);
            break;

        case TUSHORT:
            subtractInternal<uint16_t>(darkData, lightData, filter, offsetX, offsetY);
            break;

        case TLONG:
            subtractInternal<int32_t>(darkData, lightData, filter, offsetX, offsetY);
            break;

        case TULONG:
            subtractInternal<uint32_t>(darkData, lightData, filter, offsetX, offsetY);
            break;

        case TFLOAT:
            subtractInternal<float>(darkData, lightData, filter, offsetX, offsetY);
            break;

        case TLONGLONG:
            subtractInternal<int64_t>(darkData, lightData, filter, offsetX, offsetY);
            break;

        case TDOUBLE:
            subtractInternal<double>(darkData, lightData, filter, offsetX, offsetY);
            break;

        default:
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
template <typename T>
void DarkLibrary::subtractInternal(const QSharedPointer<FITSData> &darkData, const QSharedPointer<FITSData> &lightData,
                                   FITSScale filter, uint16_t offsetX, uint16_t offsetY)
{
    Q_UNUSED(filter);

    const uint32_t width = lightData->width();
    const uint32_t height = lightData->height();
    T *lightBuffer = reinterpret_cast<T *>(lightData->getWritableImageBuffer());

    const uint32_t darkStride = darkData->width();
    const uint32_t darkoffset = offsetX + offsetY * darkStride;
    T const *darkBuffer  = reinterpret_cast<T const*>(darkData->getImageBuffer()) + darkoffset;

    for (uint32_t y = 0; y < height; y++)
    {
        for (uint32_t x = 0; x < width; x++)
            lightBuffer[x] = (lightBuffer[x] > darkBuffer[x]) ? (lightBuffer[x] - darkBuffer[x]) : 0;

        lightBuffer += width;
        darkBuffer += darkStride;
    }

    lightData->calculateStats(true);
    emit darkFrameCompleted(true);
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::denoise(ISD::CCDChip *m_TargetChip, const QSharedPointer<FITSData> &targetData,
                          double duration, FITSScale filter, uint16_t offsetX, uint16_t offsetY)
{
    const QString device = m_TargetChip->getCCD()->getDeviceName();

    // Check if we have preference for defect map
    // If yes, check if defect map exists
    // If not, we check if we have regular dark frame as backup.
    if (m_DefectCameras.contains(device))
    {
        QSharedPointer<DefectMap> targetDefectMap;
        if (findDefectMap(m_TargetChip, duration, targetDefectMap))
        {
            normalizeDefects(targetDefectMap, targetData, filter, offsetX, offsetY);
            qCDebug(KSTARS_EKOS) << "Defect map denoising applied";
            return;
        }
    }

    // Check if we have valid dark data and then use it.
    QSharedPointer<FITSData> darkData;
    if (findDarkFrame(m_TargetChip, duration, darkData))
    {
        subtractDarkData(darkData, targetData, filter, offsetX, offsetY);
        qCDebug(KSTARS_EKOS) << "Dark frame subtraction applied";
        return;
    }

    emit newLog(i18n("No suitable dark frames or defect maps found. Please run the Dark Library wizard in Capture module."));
    emit darkFrameCompleted(false);

}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::processNewImage(SequenceJob *job, const QSharedPointer<FITSData> &data)
{
    Q_UNUSED(data);
    if (job->getStatus() == SequenceJob::JOB_IDLE)
        return;

    if (job->getCompleted() == job->getCount())
    {
        QJsonObject metadata
        {
            {"camera", m_CurrentCamera->getDeviceName()},
            {"chip", m_TargetChip->getType()},
            {"binx", job->getXBin()},
            {"biny", job->getYBin()},
            {"duration", job->getExposure()}
        };

        // Record temperature
        if (m_CurrentCamera->hasCooler())
            metadata["temperature"] = job->getCurrentTemperature();

        metadata["count"] = job->getCount();
        generateMasterFrame(m_CurrentDarkFrame, metadata);
        reloadDarksFromDatabase();
        populateMasterMetedata();
        histogramView->setImageData(m_CurrentDarkFrame);
        if (!Options::nonLinearHistogram() && !m_CurrentDarkFrame->isHistogramConstructed())
            m_CurrentDarkFrame->constructHistogram();
    }
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::processNewBLOB(IBLOB *bp)
{
    QByteArray buffer = QByteArray::fromRawData(reinterpret_cast<char *>(bp->blob), bp->size);
    if (!m_CurrentDarkFrame->loadFromBuffer(buffer, "fits"))
    {
        m_FileLabel->setText(i18n("Failed to process data."));
        return;
    }

    m_DarkView->loadData(m_CurrentDarkFrame);

    uint32_t totalElements = m_CurrentDarkFrame->channels() * m_CurrentDarkFrame->samplesPerChannel();
    if (totalElements != m_DarkMasterBuffer.size())
        m_DarkMasterBuffer.assign(totalElements, 0);

    aggregate(m_CurrentDarkFrame);
    darkProgress->setValue(darkProgress->value() + 1);
    m_StatusLabel->setText(i18n("Received %1/%2 images.", darkProgress->value(), darkProgress->maximum()));
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::Release()
{
    delete (_DarkLibrary);
    _DarkLibrary = nullptr;

    //    m_Cameras.clear();
    //    cameraS->clear();
    //    m_CurrentCamera = nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::closeEvent(QCloseEvent *ev)
{
    Q_UNUSED(ev);
    Options::setUseFITSViewer(m_RememberFITSViewer);
    Options::setUseFITSViewer(m_RememberSummaryView);
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::setCompleted()
{
    startB->setEnabled(true);
    stopB->setEnabled(false);

    Options::setUseFITSViewer(m_RememberFITSViewer);
    Options::setUseFITSViewer(m_RememberSummaryView);

    m_CurrentCamera->disconnect(this);
    m_CaptureModule->disconnect(this);
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::clearExpired()
{
    if (darkFramesModel->rowCount() == 0)
        return;

    // Anything before this must go
    QDateTime expiredDate = QDateTime::currentDateTime().addDays(kcfg_DarkLibraryDuration->value() * -1);

    QSqlDatabase userdb = QSqlDatabase::database("userdb");
    userdb.open();
    QSqlTableModel darkframe(nullptr, userdb);
    darkframe.setEditStrategy(QSqlTableModel::OnManualSubmit);
    darkframe.setTable("darkframe");
    // Select all those that already expired.
    darkframe.setFilter("ccd LIKE \'" + m_CurrentCamera->getDeviceName() + "\' AND timestamp < \'" + expiredDate.toString(
                            Qt::ISODate) + "\'");

    darkframe.select();

    // Now remove all the expired files from disk
    for (int i = 0; i < darkframe.rowCount(); ++i)
    {
        QString oneFile = darkframe.record(i).value("filename").toString();
        QFile::remove(oneFile);
        QString defectMap = darkframe.record(i).value("defectmap").toString();
        if (defectMap.isEmpty() == false)
            QFile::remove(defectMap);

    }

    // And remove them from the database
    darkframe.removeRows(0, darkframe.rowCount());
    darkframe.submitAll();
    userdb.close();

    Ekos::DarkLibrary::Instance()->refreshFromDB();

    reloadDarksFromDatabase();
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::clearBuffers()
{
    m_CurrentDarkFrame.clear();
    // Should clear existing view
    m_CurrentDarkFrame.reset(new FITSData(), &QObject::deleteLater);
    connect(m_CurrentDarkFrame.data(), &FITSData::histogramReady, [this]()
    {
        histogramView->setEnabled(true);
        histogramView->reset();
        histogramView->syncGUI();
    });
    m_DarkView->clearData();
    m_CurrentDefectMap.clear();

}
///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::clearAll()
{
    if (darkFramesModel->rowCount() == 0)
        return;

    if (KMessageBox::questionYesNo(KStars::Instance(),
                                   i18n("Are you sure you want to delete all dark frames images and data?")) ==
            KMessageBox::No)
        return;

    QSqlDatabase userdb = QSqlDatabase::database("userdb");
    userdb.open();
    QSqlTableModel darkframe(nullptr, userdb);
    darkFramesModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    darkframe.setTable("darkframe");
    darkframe.setFilter("ccd LIKE \'" + m_CurrentCamera->getDeviceName() + "\'");
    darkFramesModel->select();

    // Now remove all the expired files from disk
    for (int i = 0; i < darkframe.rowCount(); ++i)
    {
        QString oneFile = darkframe.record(i).value("filename").toString();
        QFile::remove(oneFile);
        QString defectMap = darkframe.record(i).value("defectmap").toString();
        if (defectMap.isEmpty() == false)
            QFile::remove(defectMap);

    }

    darkFramesModel->removeRows(0, darkFramesModel->rowCount());
    darkFramesModel->submitAll();
    userdb.close();

    Ekos::DarkLibrary::Instance()->refreshFromDB();

    // Refesh db entries for other cameras
    reloadDarksFromDatabase();
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::clearRow()
{
    QSqlDatabase userdb = QSqlDatabase::database("userdb");
    int row = darkTableView->currentIndex().row();

    QSqlRecord record = darkFramesModel->record(row);
    QString filename = record.value("filename").toString();
    QString defectMap = record.value("defectmap").toString();
    QFile::remove(filename);
    if (!defectMap.isEmpty())
        QFile::remove(defectMap);

    userdb.open();

    darkFramesModel->removeRow(row);
    darkFramesModel->submitAll();
    userdb.close();

    darkTableView->selectionModel()->select(darkFramesModel->index(row - 1, 0), QItemSelectionModel::ClearAndSelect);

    refreshFromDB();
    reloadDarksFromDatabase();
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::openDarksFolder()
{
    QString darkFilesPath = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("darks");

    QDesktopServices::openUrl(QUrl::fromLocalFile(darkFilesPath));
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::refreshDefectMastersList(const QString &camera)
{
    if (darkFramesModel->rowCount() == 0)
        return;

    masterDarksCombo->blockSignals(true);
    masterDarksCombo->clear();

    for (int i = 0; i < darkFramesModel->rowCount(); ++i)
    {
        QSqlRecord record = darkFramesModel->record(i);

        if (record.value("ccd") != camera)
            continue;

        int binX = record.value("binX").toInt();
        int binY = record.value("binY").toInt();
        double temperature = record.value("temperature").toDouble();
        double duration = record.value("duration").toDouble();
        QString ts = record.value("timestamp").toString();

        QString entry = QString("%1 secs %2x%3")
                        .arg(QString::number(duration, 'f', 1))
                        .arg(QString::number(binX))
                        .arg(QString::number(binY));

        if (temperature > INVALID_VALUE)
            entry.append(QString(" @ %1°").arg(QString::number(temperature, 'f', 1)));

        masterDarksCombo->addItem(entry);
    }

    masterDarksCombo->blockSignals(false);

    //loadDefectMap();

}
///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::reloadDarksFromDatabase()
{
    QSqlDatabase userdb = QSqlDatabase::database("userdb");
    userdb.open();

    const QString camera = m_CurrentCamera->getDeviceName();

    delete (darkFramesModel);
    delete (sortFilter);

    darkFramesModel = new QSqlTableModel(this, userdb);
    darkFramesModel->setTable("darkframe");
    darkFramesModel->setFilter(QString("ccd='%1'").arg(camera));
    darkFramesModel->select();

    sortFilter = new QSortFilterProxyModel(this);
    sortFilter->setSourceModel(darkFramesModel);
    sortFilter->sort (0);
    darkTableView->setModel (sortFilter);

    //darkTableView->setModel(darkFramesModel);
    // Hide ID
    darkTableView->hideColumn(0);
    // Hide Chip
    darkTableView->hideColumn(2);

    userdb.close();

    if (darkFramesModel->rowCount() == 0 && m_CurrentDarkFrame)
    {
        clearBuffers();
        return;
    }

    refreshDefectMastersList(camera);
    loadCurrentMasterDark(camera);
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::loadCurrentMasterDark(const QString &camera, int masterIndex)
{
    // Do not process empty models
    if (darkFramesModel->rowCount() == 0)
        return;

    if (masterIndex == -1)
        masterIndex = masterDarksCombo->currentIndex();

    if (masterIndex < 0 || masterIndex >= darkFramesModel->rowCount())
        return;

    QSqlRecord record = darkFramesModel->record(masterIndex);
    if (record.value("ccd") != camera)
        return;
    // Get the master dark frame file name
    m_MasterDarkFrameFilename = record.value("filename").toString();

    if (m_MasterDarkFrameFilename.isEmpty())
        return;

    // Get defect file name as well if available.
    m_DefectMapFilename = record.value("defectmap").toString();

    // If current dark frame is different from target filename, then load from file
    if (m_CurrentDarkFrame->filename() != m_MasterDarkFrameFilename)
        m_DarkFrameFutureWatcher.setFuture(m_CurrentDarkFrame->loadFromFile(m_MasterDarkFrameFilename));
    // If current dark frame is the same one loaded, then check if we need to reload defect map
    else
        loadCurrentMasterDefectMap();
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::loadCurrentMasterDefectMap()
{
    // Find if we have an existing map
    if (m_CachedDefectMaps.contains(m_MasterDarkFrameFilename))
    {
        if (m_CurrentDefectMap != m_CachedDefectMaps.value(m_MasterDarkFrameFilename))
        {
            m_CurrentDefectMap = m_CachedDefectMaps.value(m_MasterDarkFrameFilename);
            m_DarkView->setDefectMap(m_CurrentDefectMap);
            m_CurrentDefectMap->setDarkData(m_CurrentDarkFrame);
        }
    }
    // Create new defect map
    else
    {
        m_CurrentDefectMap.reset(new DefectMap());
        connect(m_CurrentDefectMap.data(), &DefectMap::pixelsUpdated, [this](uint32_t hot, uint32_t cold)
        {
            hotPixelsCount->setValue(hot);
            coldPixelsCount->setValue(cold);
            aggresivenessHotSlider->setValue(m_CurrentDefectMap->property("HotPixelAggressiveness").toInt());
            aggresivenessColdSlider->setValue(m_CurrentDefectMap->property("ColdPixelAggressiveness").toInt());
        });

        if (!m_DefectMapFilename.isEmpty())
            cacheDefectMapFromFile(m_MasterDarkFrameFilename, m_DefectMapFilename);

        m_DarkView->setDefectMap(m_CurrentDefectMap);
        m_CurrentDefectMap->setDarkData(m_CurrentDarkFrame);
    }
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::populateMasterMetedata()
{
    if (m_CurrentDarkFrame.isNull())
        return;

    QVariant value;
    // TS
    if (m_CurrentDarkFrame->getRecordValue("DATE-OBS", value))
        masterTime->setText(value.toString());
    // Temperature
    if (m_CurrentDarkFrame->getRecordValue("CCD-TEMP", value) && value.toDouble() < 100)
        masterTemperature->setText(QString::number(value.toDouble(), 'f', 1));
    // Exposure
    if (m_CurrentDarkFrame->getRecordValue("EXPTIME", value))
        masterExposure->setText(value.toString());
    // Median
    {
        double median = m_CurrentDarkFrame->getAverageMedian();
        if (median > 0)
            masterMedian->setText(QString::number(median, 'f', 1));
    }
    // Mean
    {
        double mean = m_CurrentDarkFrame->getAverageMean();
        masterMean->setText(QString::number(mean, 'f', 1));
    }
    // Standard Deviation
    {
        double stddev = m_CurrentDarkFrame->getAverageStdDev();
        masterDeviation->setText(QString::number(stddev, 'f', 1));
    }
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::loadDarkFITS(QModelIndex index)
{
    QSqlRecord record = darkFramesModel->record(index.row());

    QString filename = record.value("filename").toString();

    if (filename.isEmpty() == false)
        m_DarkView->loadFile(filename);
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::addCamera(ISD::GDInterface * newCCD)
{
    m_Cameras.append(static_cast<ISD::CCD*>(newCCD));
    cameraS->addItem(newCCD->getDeviceName());

    checkCamera();

    reloadDarksFromDatabase();
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::removeCamera(ISD::GDInterface * newCCD)
{
    m_Cameras.removeOne(static_cast<ISD::CCD*>(newCCD));
    cameraS->removeItem(cameraS->findText(newCCD->getDeviceName()));
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::checkCamera(int ccdNum)
{
    if (ccdNum == -1)
    {
        ccdNum = cameraS->currentIndex();

        if (ccdNum == -1)
            return;
    }

    if (ccdNum < m_Cameras.count())
    {
        // Check whether main camera or guide head only
        m_CurrentCamera = m_Cameras.at(ccdNum);

        const QString device = m_CurrentCamera->getDeviceName();
        if (m_DarkCameras.contains(device))
            preferDarksRadio->setChecked(true);
        else if (m_DefectCameras.contains(device))
            preferDefectsRadio->setChecked(true);

        m_TargetChip = nullptr;
        if (cameraS->itemText(ccdNum).right(6) == QString("Guider"))
        {
            m_UseGuideHead = true;
            m_TargetChip   = m_CurrentCamera->getChip(ISD::CCDChip::GUIDE_CCD);
        }

        if (m_TargetChip == nullptr)
        {
            m_UseGuideHead = false;
            m_TargetChip   = m_CurrentCamera->getChip(ISD::CCDChip::PRIMARY_CCD);
        }

        // Make sure we have a valid chip and valid base device.
        // Make sure we are not in capture process.
        if (!m_TargetChip || !m_TargetChip->getCCD() || !m_TargetChip->getCCD()->getBaseDevice() ||
                m_TargetChip->isCapturing())
            return;

        //        for (auto &ccd : m_Cameras)
        //        {
        //            disconnect(ccd, &ISD::CCD::numberUpdated, this, &Ekos::Capture::processCCDNumber);
        //            disconnect(ccd, &ISD::CCD::newTemperatureValue, this, &Ekos::Capture::updateCCDTemperature);
        //            disconnect(ccd, &ISD::CCD::coolerToggled, this, &Ekos::Capture::setCoolerToggled);
        //            disconnect(ccd, &ISD::CCD::newRemoteFile, this, &Ekos::Capture::setNewRemoteFile);
        //            disconnect(ccd, &ISD::CCD::videoStreamToggled, this, &Ekos::Capture::setVideoStreamEnabled);
        //            disconnect(ccd, &ISD::CCD::ready, this, &Ekos::Capture::ready);
        //        }

        if (m_CurrentCamera->hasCoolerControl())
        {
            temperatureLabel->setEnabled(true);
            temperatureStepLabel->setEnabled(true);
            temperatureToLabel->setEnabled(true);
            temperatureStepSpin->setEnabled(true);
            minTemperatureSpin->setEnabled(true);
            maxTemperatureSpin->setEnabled(true);

        }
        else
        {
            temperatureLabel->setEnabled(false);
            temperatureStepLabel->setEnabled(false);
            temperatureToLabel->setEnabled(false);
            temperatureStepSpin->setEnabled(false);
            minTemperatureSpin->setEnabled(false);
            maxTemperatureSpin->setEnabled(false);
        }

        countDarkTotalTime();
    }
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::countDarkTotalTime()
{

    //double exposureCount = (maxExposureSpin->value() - minExposureSpin->value()) / exposureStepSin->value();
    double temperatureCount = 1;
    if (m_CurrentCamera->hasCoolerControl() && std::fabs(maxTemperatureSpin->value() - minTemperatureSpin->value()) > 0)
        temperatureCount = (std::fabs((maxTemperatureSpin->value() - minTemperatureSpin->value())) / temperatureStepSpin->value()) +
                           1;
    int binnings = 0;
    if (bin1Check->isChecked())
        binnings++;
    if (bin2Check->isChecked())
        binnings++;
    if (bin4Check->isChecked())
        binnings++;

    double darkTime = 0;
    int imagesCount = 0;
    for (double startExposure = minExposureSpin->value(); startExposure <= maxExposureSpin->value();
            startExposure += exposureStepSin->value())
    {
        darkTime += startExposure * temperatureCount * binnings * countSpin->value();
        imagesCount += temperatureCount * binnings * countSpin->value();
    }

    totalTime->setText(QString::number(darkTime / 60.0, 'f', 1));
    totalImages->setText(QString::number(imagesCount));
    darkProgress->setMaximum(imagesCount);

}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::generateDarkJobs()
{
    // Always clear sequence queue before starting
    m_CaptureModule->clearSequenceQueue();

    QList<double> temperatures;
    if (m_CurrentCamera->hasCoolerControl() && std::fabs(maxTemperatureSpin->value() - minTemperatureSpin->value()) >= 0)
    {
        for (double oneTemperature = minTemperatureSpin->value(); oneTemperature <= maxTemperatureSpin->value();
                oneTemperature += temperatureStepSpin->value())
        {
            temperatures << oneTemperature;
        }

        // Enforce temperature set
        m_CaptureModule->setForceTemperature(true);
    }
    else
    {
        // Disable temperature set
        m_CaptureModule->setForceTemperature(false);
        temperatures << INVALID_VALUE;
    }

    QList<uint8_t> bins;
    if (bin1Check->isChecked())
        bins << 1;
    if (bin2Check->isChecked())
        bins << 2;
    if (bin4Check->isChecked())
        bins << 4;

    QList<double> exposures;
    for (double oneExposure = minExposureSpin->value(); oneExposure <= maxExposureSpin->value();
            oneExposure += exposureStepSin->value())
    {
        exposures << oneExposure;
    }

    QString prefix = QDir::tempPath() + QDir::separator() + QString::number(QDateTime::currentSecsSinceEpoch()) +
                     QDir::separator();


    int sequence = 0;
    for (const auto oneTemperature : qAsConst(temperatures))
    {
        for (const auto &oneExposure : qAsConst(exposures))
        {
            for (const auto &oneBin : qAsConst(bins))
            {
                sequence++;

                QJsonObject settings;

                settings["camera"] = cameraS->currentText();
                settings["exp"] = oneExposure;
                settings["bin"] = oneBin;
                settings["frameType"] = FRAME_DARK;
                settings["temperature"] = oneTemperature;
                settings["format"] = 0;

                QString directory = prefix + QString("sequence_%1").arg(sequence);
                QJsonObject fileSettings;

                fileSettings["directory"] = directory;
                m_CaptureModule->setPresetSettings(settings);
                m_CaptureModule->setFileSettings(fileSettings);
                m_CaptureModule->setCount(countSpin->value());
                m_CaptureModule->addJob();
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::executeDarkJobs()
{
    m_DarkImagesCounter = 0;
    darkProgress->setValue(0);
    darkProgress->setTextVisible(true);
    connect(m_CaptureModule, &Capture::newImage, this, &DarkLibrary::processNewImage, Qt::UniqueConnection);
    connect(m_CaptureModule, &Capture::newStatus, this, &DarkLibrary::setCaptureState, Qt::UniqueConnection);
    connect(m_CurrentCamera, &ISD::CCD::BLOBUpdated, this, &DarkLibrary::processNewBLOB, Qt::UniqueConnection);

    Options::setUseFITSViewer(false);
    Options::setUseSummaryPreview(false);
    startB->setEnabled(false);
    stopB->setEnabled(true);
    m_DarkView->reset();
    m_StatusLabel->setText(i18n("In progress..."));
    m_CaptureModule->start();

}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::stopDarkJobs()
{
    m_CaptureModule->abort();
    darkProgress->setValue(0);
    m_DarkView->reset();
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::initView()
{
    m_DarkView = new DarkView(darkWidget);
    m_DarkView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_DarkView->setBaseSize(darkWidget->size());
    m_DarkView->createFloatingToolBar();
    QVBoxLayout *vlayout = new QVBoxLayout();
    vlayout->addWidget(m_DarkView);
    darkWidget->setLayout(vlayout);
    //connect(m_DarkView, &FITSView::loaded, this, &DarkLibrary::loadCurrentMasterDefectMap);
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::aggregate(const QSharedPointer<FITSData> &data)
{
    switch (data->dataType())
    {
        case TBYTE:
            aggregateInternal<uint8_t>(data);
            break;

        case TSHORT:
            aggregateInternal<int16_t>(data);
            break;

        case TUSHORT:
            aggregateInternal<uint16_t>(data);
            break;

        case TLONG:
            aggregateInternal<int32_t>(data);
            break;

        case TULONG:
            aggregateInternal<uint32_t>(data);
            break;

        case TFLOAT:
            aggregateInternal<float>(data);
            break;

        case TLONGLONG:
            aggregateInternal<int64_t>(data);
            break;

        case TDOUBLE:
            aggregateInternal<double>(data);
            break;

        default:
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
template <typename T>
void DarkLibrary::aggregateInternal(const QSharedPointer<FITSData> &data)
{
    T const *darkBuffer  = reinterpret_cast<T const*>(data->getImageBuffer());
    for (uint32_t i = 0; i < m_DarkMasterBuffer.size(); i++)
        m_DarkMasterBuffer[i] += darkBuffer[i];
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::generateMasterFrame(const QSharedPointer<FITSData> &data, const QJsonObject &metadata)
{
    switch (data->dataType())
    {
        case TBYTE:
            generateMasterFrameInternal<uint8_t>(data, metadata);
            break;

        case TSHORT:
            generateMasterFrameInternal<int16_t>(data, metadata);
            break;

        case TUSHORT:
            generateMasterFrameInternal<uint16_t>(data, metadata);
            break;

        case TLONG:
            generateMasterFrameInternal<int32_t>(data, metadata);
            break;

        case TULONG:
            generateMasterFrameInternal<uint32_t>(data, metadata);
            break;

        case TFLOAT:
            generateMasterFrameInternal<float>(data, metadata);
            break;

        case TLONGLONG:
            generateMasterFrameInternal<int64_t>(data, metadata);
            break;

        case TDOUBLE:
            generateMasterFrameInternal<double>(data, metadata);
            break;

        default:
            break;
    }

    // Reset Master Buffer
    m_DarkMasterBuffer.assign(m_DarkMasterBuffer.size(), 0);
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
template <typename T>  void DarkLibrary::generateMasterFrameInternal(const QSharedPointer<FITSData> &data,
        const QJsonObject &metadata)
{
    T *writableBuffer = reinterpret_cast<T *>(data->getWritableImageBuffer());
    const uint32_t count = metadata["count"].toInt();
    // Average the values
    for (uint32_t i = 0; i < m_DarkMasterBuffer.size(); i++)
        writableBuffer[i] = m_DarkMasterBuffer[i] / count;


    QString ts = QDateTime::currentDateTime().toString("yyyy-MM-ddThh-mm-ss");
    QString path = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("darks/darkframe_" + ts + ".fits");

    data->calculateStats(true);
    if (!data->saveImage(path))
    {
        m_FileLabel->setText(i18n("Failed to save master frame: %1", data->getLastError()));
        return;
    }

    m_CachedDarkFrames[path] = data;

    QVariantMap map;
    map["ccd"]         = metadata["camera"].toString();
    map["chip"]        = metadata["chip"].toInt();
    map["binX"]        = metadata["binx"].toInt();
    map["binY"]        = metadata["biny"].toInt();
    map["temperature"] = metadata["temperature"].toDouble(INVALID_VALUE);
    map["duration"]    = metadata["duration"].toDouble();
    map["filename"]    = path;

    m_DarkFramesDatabaseList.append(map);
    m_FileLabel->setText(i18n("Master Dark saved to %1", path));
    KStarsData::Instance()->userdb()->AddDarkFrame(map);
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::setCaptureModule(Capture *instance)
{
    m_CaptureModule = instance;
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::setCaptureState(CaptureState state)
{
    switch (state)
    {
        case CAPTURE_ABORTED:
            setCompleted();
            m_StatusLabel->setText(i18n("Capture aborted."));
            break;
        case CAPTURE_COMPLETE:
            setCompleted();
            m_StatusLabel->setText(i18n("Capture completed."));
            break;
        default:
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::saveDefectMap()
{
    if (!m_CurrentDarkFrame)
        return;

    QString filename = m_CurrentDefectMap->filename();
    bool newFile = false;
    if (filename.isEmpty())
    {
        QString ts = QDateTime::currentDateTime().toString("yyyy-MM-ddThh-mm-ss");
        filename = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("defectmaps/defectmap_" + ts +
                   ".json");
        newFile = true;
    }

    if (m_CurrentDefectMap->save(filename, m_CurrentCamera->getDeviceName()))
    {
        m_FileLabel->setText(i18n("Defect map saved to %1", filename));

        if (newFile)
        {
            auto currentMap = std::find_if(m_DarkFramesDatabaseList.begin(),
                                           m_DarkFramesDatabaseList.end(), [&](const QVariantMap & oneMap)
            {
                return oneMap["filename"].toString() == m_CurrentDarkFrame->filename();
            });

            if (currentMap != m_DarkFramesDatabaseList.end())
            {
                (*currentMap)["defectmap"] = filename;
                KStarsData::Instance()->userdb()->UpdateDarkFrame(*currentMap);
            }
        }
    }
    else
    {
        m_FileLabel->setText(i18n("Failed to save defect map to %1", filename));
    }
}

}

