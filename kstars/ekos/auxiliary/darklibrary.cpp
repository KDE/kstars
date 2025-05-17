/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "darklibrary.h"
#include "Options.h"

#include "ekos/manager.h"
#include "ekos/capture/capture.h"
#include "ekos/capture/sequencejob.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/profilesettings.h"
#include "ekos/auxiliary/opticaltrainsettings.h"
#include "kstars.h"
#include "kspaths.h"
#include "kstarsdata.h"
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitsview.h"

#include <QDesktopServices>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QStatusBar>
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

    QDir writableDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    writableDir.mkpath("darks");
    writableDir.mkpath("defectmaps");

    // Setup Debounce timer to limit over-activation of settings changes
    m_DebounceTimer.setInterval(500);
    m_DebounceTimer.setSingleShot(true);
    connect(&m_DebounceTimer, &QTimer::timeout, this, &DarkLibrary::settleSettings);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Dark Generation Connections
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    m_CurrentDarkFrame.reset(new FITSData(), &QObject::deleteLater);

    connect(darkTableView,  &QAbstractItemView::doubleClicked, this, [this](QModelIndex index)
    {
        loadIndexInView(index.row());
    });
    connect(openDarksFolderB, &QPushButton::clicked, this, &DarkLibrary::openDarksFolder);
    connect(clearAllB, &QPushButton::clicked, this, &DarkLibrary::clearAll);
    connect(clearRowB, &QPushButton::clicked, this, [this]()
    {
        auto selectionModel = darkTableView->selectionModel();
        if (selectionModel->hasSelection())
        {
            auto index = selectionModel->currentIndex().row();
            clearRow(index);
        }
    });

    connect(clearExpiredB, &QPushButton::clicked, this, &DarkLibrary::clearExpired);
    connect(refreshB, &QPushButton::clicked, this, &DarkLibrary::reloadDarksFromDatabase);

    connect(&m_DarkFrameFutureWatcher, &QFutureWatcher<bool>::finished, this, [this]()
    {
        // If loading is successful, then set it in current dark view
        if (m_DarkFrameFutureWatcher.result())
        {
            m_DarkView->loadData(m_CurrentDarkFrame);
            loadCurrentMasterDefectMap();
            populateMasterMetedata();
        }
        else
            m_FileLabel->setText(i18n("Failed to load %1: %2",  m_MasterDarkFrameFilename, m_CurrentDarkFrame->getLastError()));

    });

    connect(masterDarksCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int index)
    {
        if (m_Camera)
            DarkLibrary::loadCurrentMasterDark(m_Camera->getDeviceName(), index);
    });

    connect(minExposureSpin, &QDoubleSpinBox::editingFinished, this, &DarkLibrary::countDarkTotalTime);
    connect(maxExposureSpin, &QDoubleSpinBox::editingFinished, this, &DarkLibrary::countDarkTotalTime);
    connect(exposureStepSin, &QDoubleSpinBox::editingFinished, this, &DarkLibrary::countDarkTotalTime);

    connect(minTemperatureSpin, &QDoubleSpinBox::editingFinished, this, [this]()
    {
        maxTemperatureSpin->setMinimum(minTemperatureSpin->value());
        countDarkTotalTime();
    });
    connect(maxTemperatureSpin, &QDoubleSpinBox::editingFinished, this, [this]()
    {
        minTemperatureSpin->setMaximum(maxTemperatureSpin->value());
        countDarkTotalTime();
    });
    connect(temperatureStepSpin, &QDoubleSpinBox::editingFinished, this, [this]()
    {
        maxTemperatureSpin->setMinimum(minTemperatureSpin->value());
        minTemperatureSpin->setMaximum(maxTemperatureSpin->value());
        countDarkTotalTime();
    });

    connect(countSpin, &QDoubleSpinBox::editingFinished, this, &DarkLibrary::countDarkTotalTime);
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(binningButtonGroup, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::buttonToggled),
            this, [this](int, bool)
#else
    connect(binningButtonGroup, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::idToggled),
            this, [this](int, bool)
#endif
    {
        countDarkTotalTime();
    });

    connect(startB, &QPushButton::clicked, this, &DarkLibrary::start);
    connect(stopB, &QPushButton::clicked, this, &DarkLibrary::stop);

    KStarsData::Instance()->userdb()->GetAllDarkFrames(m_DarkFramesDatabaseList);
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Defect Map Connections
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    connect(darkTabsWidget, &QTabWidget::currentChanged, this, [this](int index)
    {
        m_DarkView->setDefectMapEnabled(index == 1 && m_CurrentDefectMap);
    });
    connect(aggresivenessHotSlider, &QSlider::valueChanged, aggresivenessHotSpin, &QSpinBox::setValue);
    connect(aggresivenessColdSlider, &QSlider::valueChanged, aggresivenessColdSpin, &QSpinBox::setValue);
    connect(hotPixelsEnabled, &QCheckBox::toggled, this, [this](bool toggled)
    {
        if (m_CurrentDefectMap)
            m_CurrentDefectMap->setProperty("HotEnabled", toggled);
    });
    connect(coldPixelsEnabled, &QCheckBox::toggled, this, [this](bool toggled)
    {
        if (m_CurrentDefectMap)
            m_CurrentDefectMap->setProperty("ColdEnabled", toggled);
    });
    connect(generateMapB, &QPushButton::clicked, this, [this]()
    {
        if (m_CurrentDefectMap)
        {
            m_CurrentDefectMap->setProperty("HotPixelAggressiveness", aggresivenessHotSpin->value());
            m_CurrentDefectMap->setProperty("ColdPixelAggressiveness", aggresivenessColdSpin->value());
            m_CurrentDefectMap->filterPixels();
            emit newFrame(m_DarkView);
        }
    });
    connect(resetMapParametersB, &QPushButton::clicked, this, [this]()
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

    loadGlobalSettings();

    connectSettings();

    setupOpticalTrainManager();
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
bool DarkLibrary::findDarkFrame(ISD::CameraChip *m_TargetChip, double duration, QSharedPointer<FITSData> &darkData)
{
    QVariantMap bestCandidate;
    for (auto &map : m_DarkFramesDatabaseList)
    {
        // First check CCD name matches and check if we are on the correct chip
        if (map["ccd"].toString() == m_TargetChip->getCCD()->getDeviceName() &&
                map["chip"].toInt() == static_cast<int>(m_TargetChip->getType()))
        {
            // Match Gain
            int gain = getGain();
            if (gain >= 0 && map["gain"].toInt() != gain)
                continue;

            // Match ISO
            QString isoValue;
            if (m_TargetChip->getISOValue(isoValue) && map["iso"].toString() != isoValue)
                continue;

            // Match binning
            int binX = 1, binY = 1;
            m_TargetChip->getBinning(&binX, &binY);

            // Then check if binning is the same
            if (map["binX"].toInt() != binX || map["binY"].toInt() != binY)
                continue;

            // If camera has an active cooler, then we check temperature against the absolute threshold.
            if (m_TargetChip->getCCD()->hasCoolerControl())
            {
                double temperature = 0;
                m_TargetChip->getCCD()->getTemperature(&temperature);
                double darkTemperature = map["temperature"].toDouble();
                // If different is above threshold, it is completely rejected.
                if (darkTemperature != INVALID_VALUE && fabs(darkTemperature - temperature) > maxDarkTemperatureDiff->value())
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
                    thisMapScore += 5;
                else if (diffBest < diffMap)
                    bestCandidateScore += 5;
            }

            // More recent has a higher score than older.
            {
                const QDateTime now = QDateTime::currentDateTime();
                int64_t diffMap  = map["timestamp"].toDateTime().secsTo(now);
                int64_t diffBest = bestCandidate["timestamp"].toDateTime().secsTo(now);
                if (diffMap < diffBest)
                    thisMapScore++;
                else if (diffBest < diffMap)
                    bestCandidateScore++;
            }

            // Find candidate with closest time in case we have multiple defect maps
            if (thisMapScore > bestCandidateScore)
                bestCandidate = map;
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

    // Before adding to cache, clear the cache if memory drops too low.
    auto memoryMB = KSUtils::getAvailableRAM() / 1e6;
    if (memoryMB < CACHE_MEMORY_LIMIT)
        m_CachedDarkFrames.clear();

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
bool DarkLibrary::findDefectMap(ISD::CameraChip *m_TargetChip, double duration, QSharedPointer<DefectMap> &defectMap)
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
    QSharedPointer<FITSData> data;
    data.reset(new FITSData(FITS_CALIBRATE), &QObject::deleteLater);
    QFuture<bool> rc = data->loadFromFile(filename);

    rc.waitForFinished();
    if (rc.result())
    {
        m_CachedDarkFrames[filename] = data;
    }
    else
    {
        emit newLog(i18n("Failed to load dark frame file %1", filename));
    }

    return rc.result();
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::processNewImage(const QSharedPointer<SequenceJob> &job, const QSharedPointer<FITSData> &data)
{
    Q_UNUSED(data)
    if (job->getStatus() == JOB_IDLE)
        return;

    if (job->getCompleted() == job->getCoreProperty(SequenceJob::SJ_Count).toInt())
    {
        QJsonObject metadata
        {
            {"camera", m_Camera->getDeviceName()},
            {"chip", m_TargetChip->getType()},
            {"binx", job->getCoreProperty(SequenceJob::SJ_Binning).toPoint().x()},
            {"biny", job->getCoreProperty(SequenceJob::SJ_Binning).toPoint().y()},
            {"duration", job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble()}
        };

        // Record temperature
        double value = 0;
        bool success = m_Camera->getTemperature(&value);
        if (success)
            metadata["temperature"] = value;

        success = m_Camera->hasGain() && m_Camera->getGain(&value);
        if (success)
            metadata["gain"] = value;

        QString isoValue;
        success = m_TargetChip->getISOValue(isoValue);
        if (success)
            metadata["iso"] = isoValue;

        metadata["count"] = job->getCoreProperty(SequenceJob::SJ_Count).toInt();
        generateMasterFrame(m_CurrentDarkFrame, metadata);
        reloadDarksFromDatabase();
        populateMasterMetedata();
    }
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::updateProperty(INDI::Property prop)
{
    if (prop.getType() != INDI_BLOB)
        return;

    auto bp = prop.getBLOB()->at(0);
    m_CurrentDarkFrame->setExtension(QString(bp->getFormat()));
    QByteArray buffer = QByteArray::fromRawData(reinterpret_cast<char *>(bp->getBlob()), bp->getSize());
    if (!m_CurrentDarkFrame->loadFromBuffer(buffer))
    {
        m_FileLabel->setText(i18n("Failed to process dark data."));
        return;
    }

    if (!m_DarkView->loadData(m_CurrentDarkFrame))
    {
        m_FileLabel->setText(i18n("Failed to load dark data."));
        return;
    }

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
    Q_UNUSED(ev)
    Options::setUseFITSViewer(m_RememberFITSViewer);
    Options::setUseSummaryPreview(m_RememberSummaryView);
    if (m_JobsGenerated)
    {
        QSharedPointer<Camera> cam = m_CaptureModule->findCamera(opticalTrain());
        m_JobsGenerated = false;
        cam->clearSequenceQueue();
        cam->setAllSettings(m_CaptureModuleSettings);
    }
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::setCompleted()
{
    startB->setEnabled(true);
    stopB->setEnabled(false);

    Options::setUseFITSViewer(m_RememberFITSViewer);
    Options::setUseSummaryPreview(m_RememberSummaryView);
    if (m_JobsGenerated)
    {
        QSharedPointer<Camera> cam = m_CaptureModule->findCamera(opticalTrain());
        m_JobsGenerated = false;
        cam->clearSequenceQueue();
        cam->setAllSettings(m_CaptureModuleSettings);
    }

    m_Camera->disconnect(this);
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
    QDateTime expiredDate = QDateTime::currentDateTime().addDays(darkLibraryDuration->value() * -1);

    auto userdb = QSqlDatabase::database(KStarsData::Instance()->userdb()->connectionName());
    QSqlTableModel darkframe(nullptr, userdb);
    darkframe.setEditStrategy(QSqlTableModel::OnManualSubmit);
    darkframe.setTable("darkframe");
    // Select all those that already expired.
    darkframe.setFilter("ccd LIKE \'" + m_Camera->getDeviceName() + "\' AND timestamp < \'" + expiredDate.toString(
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

    if (KMessageBox::warningContinueCancel(KStars::Instance(),
                                           i18n("Are you sure you want to delete all dark frames images and data?")) ==
            KMessageBox::Cancel)
        return;

    // Now remove all the expired files from disk
    for (int i = 0; i < darkFramesModel->rowCount(); ++i)
    {
        QString oneFile = darkFramesModel->record(i).value("filename").toString();
        QFile::remove(oneFile);
        QString defectMap = darkFramesModel->record(i).value("defectmap").toString();
        if (defectMap.isEmpty() == false)
            QFile::remove(defectMap);
        KStarsData::Instance()->userdb()->DeleteDarkFrame(oneFile);

    }

    // Refesh db entries for other cameras
    refreshFromDB();
    reloadDarksFromDatabase();
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::clearRow(int index)
{
    if (index < 0)
        return;

    QSqlRecord record = darkFramesModel->record(index);
    QString filename = record.value("filename").toString();
    QString defectMap = record.value("defectmap").toString();
    QFile::remove(filename);
    if (!defectMap.isEmpty())
        QFile::remove(defectMap);

    KStarsData::Instance()->userdb()->DeleteDarkFrame(filename);
    refreshFromDB();
    reloadDarksFromDatabase();
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::openDarksFolder()
{
    QString darkFilesPath = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("darks");

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

        auto binX = record.value("binX").toInt();
        auto binY = record.value("binY").toInt();
        auto temperature = record.value("temperature").toDouble();
        auto duration = record.value("duration").toDouble();
        auto gain = record.value("gain").toInt();
        auto iso = record.value("iso").toString();
        QString ts = record.value("timestamp").toString();

        QString entry = QString("%1 secs %2x%3")
                        .arg(QString::number(duration, 'f', 1))
                        .arg(QString::number(binX))
                        .arg(QString::number(binY));

        if (temperature > INVALID_VALUE)
            entry.append(QString(" @ %1°").arg(QString::number(temperature, 'f', 1)));

        if (gain >= 0)
            entry.append(QString(" G %1").arg(gain));
        if (!iso.isEmpty())
            entry.append(QString(" ISO %1").arg(iso));

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
    if (!m_Camera) return;
    auto userdb = QSqlDatabase::database(KStarsData::Instance()->userdb()->connectionName());

    const QString camera = m_Camera->getDeviceName();

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

    if (m_MasterDarkFrameFilename.isEmpty() || !QFileInfo::exists(m_MasterDarkFrameFilename))
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
        connect(m_CurrentDefectMap.data(), &DefectMap::pixelsUpdated, this, [this](uint32_t hot, uint32_t cold)
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
///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::loadIndexInView(int row)
{
    QSqlRecord record = darkFramesModel->record(row);
    QString filename = record.value("filename").toString();
    // Avoid duplicate loads
    if (m_DarkView->imageData().isNull() || m_DarkView->imageData()->filename() != filename)
        m_DarkView->loadFile(filename);
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
bool DarkLibrary::setCamera(ISD::Camera * device)
{
    if (m_Camera == device)
        return false;

    if (m_Camera)
        m_Camera->disconnect(this);

    m_Camera = device;

    if (m_Camera)
    {
        darkTabsWidget->setEnabled(true);
        checkCamera();
        // JM 2024.03.09: Add a bandaid for a mysteroius crash that sometimes happen
        // when loading dark frame on Ekos startup. The crash occurs in cfitsio
        // Hopefully this delay might fix it
        QTimer::singleShot(1000, this, &DarkLibrary::reloadDarksFromDatabase);
        return true;
    }
    else
    {
        darkTabsWidget->setEnabled(false);
        return false;
    }
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::removeDevice(const QSharedPointer<ISD::GenericDevice> &device)
{
    if (m_Camera && m_Camera->getDeviceName() == device->getDeviceName())
    {
        m_Camera->disconnect(this);
        m_Camera = nullptr;
    }
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::checkCamera()
{
    if (!m_Camera)
        return;

    auto device = m_Camera->getDeviceName();

    m_TargetChip = nullptr;
    // FIXME TODO
    // Need to figure guide head
    if (device.contains("Guider"))
    {
        m_UseGuideHead = true;
        m_TargetChip   = m_Camera->getChip(ISD::CameraChip::GUIDE_CCD);
    }

    if (m_TargetChip == nullptr)
    {
        m_UseGuideHead = false;
        m_TargetChip   = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);
    }

    // Make sure we have a valid chip and valid base device.
    // Make sure we are not in capture process.
    if (!m_TargetChip || !m_TargetChip->getCCD() || m_TargetChip->isCapturing())
        return;

    if (m_Camera->hasCoolerControl())
    {
        temperatureLabel->setEnabled(true);
        temperatureStepLabel->setEnabled(true);
        temperatureToLabel->setEnabled(true);
        temperatureStepSpin->setEnabled(true);
        minTemperatureSpin->setEnabled(true);
        maxTemperatureSpin->setEnabled(true);

        // Get default temperature
        double temperature = 0;
        // Update if no setting was previously set
        if (m_Camera->getTemperature(&temperature))
        {
            minTemperatureSpin->setValue(temperature);
            maxTemperatureSpin->setValue(temperature);
        }

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

    QStringList isoList = m_TargetChip->getISOList();
    captureISOS->blockSignals(true);
    captureISOS->clear();

    // No ISO range available
    if (isoList.isEmpty())
    {
        captureISOS->setEnabled(false);
    }
    else
    {
        captureISOS->setEnabled(true);
        captureISOS->addItems(isoList);
        captureISOS->setCurrentIndex(m_TargetChip->getISOIndex());
    }
    captureISOS->blockSignals(false);

    // Gain Check
    if (m_Camera->hasGain())
    {
        double min, max, step, value, targetCustomGain;
        m_Camera->getGainMinMaxStep(&min, &max, &step);

        // Allow the possibility of no gain value at all.
        GainSpinSpecialValue = min - step;
        captureGainN->setRange(GainSpinSpecialValue, max);
        captureGainN->setSpecialValueText(i18n("--"));
        captureGainN->setEnabled(true);
        captureGainN->setSingleStep(step);
        m_Camera->getGain(&value);

        targetCustomGain = getGain();

        // Set the custom gain if we have one
        // otherwise it will not have an effect.
        if (targetCustomGain > 0)
            captureGainN->setValue(targetCustomGain);
        else
            captureGainN->setValue(GainSpinSpecialValue);

        captureGainN->setReadOnly(m_Camera->getGainPermission() == IP_RO);
    }
    else
        captureGainN->setEnabled(false);

    countDarkTotalTime();

}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::countDarkTotalTime()
{
    double temperatureCount = 1;
    if (m_Camera && m_Camera->hasCoolerControl() && std::abs(maxTemperatureSpin->value() - minTemperatureSpin->value()) > 0)
        temperatureCount = (std::abs((maxTemperatureSpin->value() - minTemperatureSpin->value())) / temperatureStepSpin->value()) +
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
    QSharedPointer<Camera> cam = m_CaptureModule->findCamera(opticalTrain());
    // Always clear sequence queue before starting
    cam->clearSequenceQueue();

    if (m_JobsGenerated == false)
    {
        m_JobsGenerated = true;
        m_CaptureModuleSettings = cam->getAllSettings();
    }

    QList<double> temperatures;
    if (m_Camera->hasCoolerControl() && std::fabs(maxTemperatureSpin->value() - minTemperatureSpin->value()) >= 0)
    {
        for (double oneTemperature = minTemperatureSpin->value(); oneTemperature <= maxTemperatureSpin->value();
                oneTemperature += temperatureStepSpin->value())
        {
            temperatures << oneTemperature;
        }

        // Enforce temperature set
        cam->setForceTemperature(true);
    }
    else
    {
        // Disable temperature set
        cam->setForceTemperature(false);
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
    for (auto &oneTemperature : temperatures)
    {
        for (auto &oneExposure : exposures)
        {
            for (auto &oneBin : bins)
            {
                sequence++;
                QVariantMap settings;

                settings["opticalTrainCombo"] = opticalTrainCombo->currentText();
                settings["captureExposureN"] = oneExposure;
                settings["captureBinHN"] = oneBin;
                settings["captureBinVN"] = oneBin;
                settings["captureTypeS"] = "Dark";
                settings["cameraTemperatureN"] = oneTemperature;
                if (captureGainN->isEnabled())
                    settings["captureGainN"] = captureGainN->value();
                if (captureISOS->isEnabled())
                    settings["captureISOS"] = captureISOS->currentText();

                settings["fileDirectoryT"] = QString(prefix + QString("sequence_%1").arg(sequence));
                settings["captureCountN"] = countSpin->value();

                cam->setAllSettings(settings);
                cam->createJob();
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::execute()
{
    m_DarkImagesCounter = 0;
    darkProgress->setValue(0);
    darkProgress->setTextVisible(true);
    connect(m_CaptureModule, &Capture::newImage, this, &DarkLibrary::processNewImage, Qt::UniqueConnection);
    connect(m_CaptureModule, &Capture::newStatus, this, &DarkLibrary::setCaptureState, Qt::UniqueConnection);
    connect(m_Camera, &ISD::Camera::propertyUpdated, this, &DarkLibrary::updateProperty, Qt::UniqueConnection);

    Options::setUseFITSViewer(false);
    Options::setUseSummaryPreview(false);
    startB->setEnabled(false);
    stopB->setEnabled(true);
    m_DarkView->reset();
    m_StatusLabel->setText(i18n("In progress..."));
    QSharedPointer<Camera> cam = m_CaptureModule->findCamera(opticalTrain());
    cam->start();

}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::stop()
{
    QSharedPointer<Camera> cam = m_CaptureModule->findCamera(opticalTrain());
    cam->abort();
    darkProgress->setValue(0);
    m_DarkView->reset();
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::initView()
{
    m_DarkView.reset(new DarkView(darkWidget));
    m_DarkView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_DarkView->setBaseSize(darkWidget->size());
    m_DarkView->createFloatingToolBar();
    QVBoxLayout *vlayout = new QVBoxLayout();
    vlayout->addWidget(m_DarkView.get());
    darkWidget->setLayout(vlayout);
    connect(m_DarkView.get(), &DarkView::loaded, this, [this]()
    {
        emit newImage(m_DarkView->imageData());
    });
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

    emit newImage(data);
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
    QString path = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("darks/darkframe_" + ts +
                   data->extension());

    data->calculateStats(true);
    if (!data->saveImage(path))
    {
        m_FileLabel->setText(i18n("Failed to save master frame: %1", data->getLastError()));
        return;
    }

    auto memoryMB = KSUtils::getAvailableRAM() / 1e6;
    if (memoryMB > CACHE_MEMORY_LIMIT)
        cacheDarkFrameFromFile(data->filename());

    QVariantMap map;
    map["ccd"]         = metadata["camera"].toString();
    map["chip"]        = metadata["chip"].toInt();
    map["binX"]        = metadata["binx"].toInt();
    map["binY"]        = metadata["biny"].toInt();
    map["temperature"] = metadata["temperature"].toDouble(INVALID_VALUE);
    map["gain"] = metadata["gain"].toInt(-1);
    map["iso"] = metadata["iso"].toString();
    map["duration"]    = metadata["duration"].toDouble();
    map["filename"]    = path;
    map["timestamp"]   = QDateTime::currentDateTime().toString(Qt::ISODate);

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
    if (!m_CurrentDarkFrame || !m_CurrentDefectMap)
        return;

    QString filename = m_CurrentDefectMap->filename();
    bool newFile = false;
    if (filename.isEmpty())
    {
        QString ts = QDateTime::currentDateTime().toString("yyyy-MM-ddThh-mm-ss");
        filename = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("defectmaps/defectmap_" + ts +
                   ".json");
        newFile = true;
    }

    if (m_CurrentDefectMap->save(filename, m_Camera->getDeviceName()))
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
                (*currentMap)["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
                KStarsData::Instance()->userdb()->UpdateDarkFrame(*currentMap);
            }
        }
    }
    else
    {
        m_FileLabel->setText(i18n("Failed to save defect map to %1", filename));
    }
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::start()
{
    generateDarkJobs();
    execute();
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::setCameraPresets(const QJsonObject &settings)
{
    const auto opticalTrain = settings["optical_train"].toString();
    const auto isDarkPrefer = settings["isDarkPrefer"].toBool(preferDarksRadio->isChecked());
    const auto isDefectPrefer = settings["isDefectPrefer"].toBool(preferDefectsRadio->isChecked());
    opticalTrainCombo->setCurrentText(opticalTrain);
    preferDarksRadio->setChecked(isDarkPrefer);
    preferDefectsRadio->setChecked(isDefectPrefer);
    checkCamera();
    reloadDarksFromDatabase();
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
QJsonObject DarkLibrary::getCameraPresets()
{
    QJsonObject cameraSettings =
    {
        {"optical_train", opticalTrainCombo->currentText()},
        {"preferDarksRadio", preferDarksRadio->isChecked()},
        {"preferDefectsRadio", preferDefectsRadio->isChecked()},
        {"fileName", m_FileLabel->text()}
    };
    return cameraSettings;
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
QJsonArray DarkLibrary::getViewMasters()
{
    QJsonArray array;

    for(int i = 0; i < darkFramesModel->rowCount(); i++)
    {
        QSqlRecord record = darkFramesModel->record(i);
        auto camera = record.value("ccd").toString();
        auto binX = record.value("binX").toInt();
        auto binY = record.value("binY").toInt();
        auto temperature = record.value("temperature").toDouble();
        auto duration = record.value("duration").toDouble();
        auto ts = record.value("timestamp").toString();
        auto gain = record.value("gain").toInt();
        auto iso = record.value("iso").toString();

        QJsonObject filterRows =
        {
            {"camera", camera},
            {"binX", binX},
            {"binY", binY},
            {"temperature", temperature},
            {"duaration", duration},
            {"ts", ts}
        };

        if (gain >= 0)
            filterRows["gain"] = gain;
        if (!iso.isEmpty())
            filterRows["iso"] = iso;

        array.append(filterRows);
    }
    return array;
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::setDefectPixels(const QJsonObject &payload)
{
    const auto hotSpin = payload["hotSpin"].toInt();
    const auto coldSpin = payload["coldSpin"].toInt();
    const auto hotEnabled = payload["hotEnabled"].toBool(hotPixelsEnabled->isChecked());
    const auto coldEnabled = payload["coldEnabled"].toBool(coldPixelsEnabled->isChecked());

    hotPixelsEnabled->setChecked(hotEnabled);
    coldPixelsEnabled->setChecked(coldEnabled);

    aggresivenessHotSpin->setValue(hotSpin);
    aggresivenessColdSpin->setValue(coldSpin);

    m_DarkView->ZoomDefault();

    setDefectMapEnabled(true);
    generateMapB->click();
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::setDefectMapEnabled(bool enabled)
{
    m_DarkView->setDefectMapEnabled(enabled);
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
double DarkLibrary::getGain()
{
    // Gain is manifested in two forms
    // Property CCD_GAIN and
    // Part of CCD_CONTROLS properties.
    // Therefore, we have to find what the currently camera supports first.
    auto gain = m_Camera->getProperty("CCD_GAIN");
    if (gain)
        return gain.getNumber()->at(0)->value;


    auto controls = m_Camera->getProperty("CCD_CONTROLS");
    if (controls)
    {
        auto oneGain = controls.getNumber()->findWidgetByName("Gain");
        if (oneGain)
            return oneGain->value;
    }

    return -1;
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::setupOpticalTrainManager()
{
    connect(OpticalTrainManager::Instance(), &OpticalTrainManager::updated, this, &DarkLibrary::refreshOpticalTrain);
    connect(trainB, &QPushButton::clicked, this, [this]()
    {
        OpticalTrainManager::Instance()->openEditor(opticalTrainCombo->currentText());
    });
    connect(opticalTrainCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index)
    {
        ProfileSettings::Instance()->setOneSetting(ProfileSettings::DarkLibraryOpticalTrain,
                OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(index)));
        refreshOpticalTrain();
        emit trainChanged();
    });
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::refreshOpticalTrain()
{
    opticalTrainCombo->blockSignals(true);
    opticalTrainCombo->clear();
    opticalTrainCombo->addItems(OpticalTrainManager::Instance()->getTrainNames());
    trainB->setEnabled(true);

    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::DarkLibraryOpticalTrain);

    if (trainID.isValid())
    {
        auto id = trainID.toUInt();

        // If train not found, select the first one available.
        if (OpticalTrainManager::Instance()->exists(id) == false)
        {
            emit newLog(i18n("Optical train doesn't exist for id %1", id));
            id = OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(0));
        }

        auto name = OpticalTrainManager::Instance()->name(id);

        opticalTrainCombo->setCurrentText(name);

        auto camera = OpticalTrainManager::Instance()->getCamera(name);
        if (camera)
        {
            auto scope = OpticalTrainManager::Instance()->getScope(name);
            opticalTrainCombo->setToolTip(QString("%1 @ %2").arg(camera->getDeviceName(), scope["name"].toString()));
        }
        setCamera(camera);

        // Load train settings
        OpticalTrainSettings::Instance()->setOpticalTrainID(id);
        auto settings = OpticalTrainSettings::Instance()->getOneSetting(OpticalTrainSettings::DarkLibrary);
        if (settings.isValid())
        {
            auto map = settings.toJsonObject().toVariantMap();
            if (map != m_Settings)
            {
                m_Settings.clear();
                setAllSettings(map);
            }
        }
        else
            m_Settings = m_GlobalSettings;
    }

    opticalTrainCombo->blockSignals(false);
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::loadGlobalSettings()
{
    QString key;
    QVariant value;

    QVariantMap settings;
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
    {
        if (oneWidget->objectName() == "opticalTrainCombo")
            continue;

        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid() && oneWidget->count() > 0)
        {
            oneWidget->setCurrentText(value.toString());
            settings[key] = value;
        }
    }

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setValue(value.toDouble());
            settings[key] = value;
        }
    }

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setValue(value.toInt());
            settings[key] = value;
        }
    }

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setChecked(value.toBool());
            settings[key] = value;
        }
    }

    m_GlobalSettings = m_Settings = settings;
}


///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::connectSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        connect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Ekos::DarkLibrary::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        connect(oneWidget, &QDoubleSpinBox::editingFinished, this, &Ekos::DarkLibrary::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        connect(oneWidget, &QSpinBox::editingFinished, this, &Ekos::DarkLibrary::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        connect(oneWidget, &QCheckBox::toggled, this, &Ekos::DarkLibrary::syncSettings);

    // All Radio buttons
    for (auto &oneWidget : findChildren<QRadioButton*>())
        connect(oneWidget, &QRadioButton::toggled, this, &Ekos::DarkLibrary::syncSettings);

    // Train combo box should NOT be synced.
    disconnect(opticalTrainCombo, QOverload<int>::of(&QComboBox::activated), this, &Ekos::DarkLibrary::syncSettings);
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::disconnectSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        disconnect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Ekos::DarkLibrary::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        disconnect(oneWidget, &QDoubleSpinBox::editingFinished, this, &Ekos::DarkLibrary::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        disconnect(oneWidget, &QSpinBox::editingFinished, this, &Ekos::DarkLibrary::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        disconnect(oneWidget, &QCheckBox::toggled, this, &Ekos::DarkLibrary::syncSettings);

    // All Radio buttons
    for (auto &oneWidget : findChildren<QRadioButton*>())
        disconnect(oneWidget, &QRadioButton::toggled, this, &Ekos::DarkLibrary::syncSettings);

}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
QVariantMap DarkLibrary::getAllSettings() const
{
    QVariantMap settings;

    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->currentText());

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->value());

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->value());

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->isChecked());

    return settings;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::setAllSettings(const QVariantMap &settings)
{
    // Disconnect settings that we don't end up calling syncSettings while
    // performing the changes.
    disconnectSettings();

    for (auto &name : settings.keys())
    {
        // Combo
        auto comboBox = findChild<QComboBox*>(name);
        if (comboBox)
        {
            syncControl(settings, name, comboBox);
            continue;
        }

        // Double spinbox
        auto doubleSpinBox = findChild<QDoubleSpinBox*>(name);
        if (doubleSpinBox)
        {
            syncControl(settings, name, doubleSpinBox);
            continue;
        }

        // spinbox
        auto spinBox = findChild<QSpinBox*>(name);
        if (spinBox)
        {
            syncControl(settings, name, spinBox);
            continue;
        }

        // checkbox
        auto checkbox = findChild<QCheckBox*>(name);
        if (checkbox)
        {
            syncControl(settings, name, checkbox);
            continue;
        }

        // Radio button
        auto radioButton = findChild<QRadioButton*>(name);
        if (radioButton)
        {
            syncControl(settings, name, radioButton);
            continue;
        }
    }

    // Sync to options
    for (auto &key : settings.keys())
    {
        auto value = settings[key];
        // Save immediately
        Options::self()->setProperty(key.toLatin1(), value);

        m_Settings[key] = value;
        m_GlobalSettings[key] = value;
    }

    emit settingsUpdated(getAllSettings());

    // Save to optical train specific settings as well
    OpticalTrainSettings::Instance()->setOpticalTrainID(OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText()));
    OpticalTrainSettings::Instance()->setOneSetting(OpticalTrainSettings::DarkLibrary, m_Settings);

    // Restablish connections
    connectSettings();
}

void DarkLibrary::setOpticalTrain(const QString &value, bool enabled)
{
    opticalTrainCombo->setCurrentText(value);
    opticalTrainCombo->setEnabled(enabled);
    trainB->setEnabled(enabled);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
bool DarkLibrary::syncControl(const QVariantMap &settings, const QString &key, QWidget * widget)
{
    QSpinBox *pSB = nullptr;
    QDoubleSpinBox *pDSB = nullptr;
    QCheckBox *pCB = nullptr;
    QComboBox *pComboBox = nullptr;
    QRadioButton *pRadioButton = nullptr;
    bool ok = false;

    if ((pSB = qobject_cast<QSpinBox *>(widget)))
    {
        const int value = settings[key].toInt(&ok);
        if (ok)
        {
            pSB->setValue(value);
            return true;
        }
    }
    else if ((pDSB = qobject_cast<QDoubleSpinBox *>(widget)))
    {
        const double value = settings[key].toDouble(&ok);
        if (ok)
        {
            pDSB->setValue(value);
            return true;
        }
    }
    else if ((pCB = qobject_cast<QCheckBox *>(widget)))
    {
        const bool value = settings[key].toBool();
        if (value != pCB->isChecked())
            pCB->setChecked(value);
        return true;
    }
    // ONLY FOR STRINGS, not INDEX
    else if ((pComboBox = qobject_cast<QComboBox *>(widget)))
    {
        const QString value = settings[key].toString();
        pComboBox->setCurrentText(value);
        return true;
    }
    else if ((pRadioButton = qobject_cast<QRadioButton *>(widget)))
    {
        const bool value = settings[key].toBool();
        if (value)
            pRadioButton->setChecked(true);
        return true;
    }

    return false;
};

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::syncSettings()
{
    QDoubleSpinBox *dsb = nullptr;
    QSpinBox *sb = nullptr;
    QCheckBox *cb = nullptr;
    QComboBox *cbox = nullptr;
    QRadioButton *cradio = nullptr;

    QString key;
    QVariant value;

    if ( (dsb = qobject_cast<QDoubleSpinBox*>(sender())))
    {
        key = dsb->objectName();
        value = dsb->value();

    }
    else if ( (sb = qobject_cast<QSpinBox*>(sender())))
    {
        key = sb->objectName();
        value = sb->value();
    }
    else if ( (cb = qobject_cast<QCheckBox*>(sender())))
    {
        key = cb->objectName();
        value = cb->isChecked();
    }
    else if ( (cbox = qobject_cast<QComboBox*>(sender())))
    {
        key = cbox->objectName();
        value = cbox->currentText();
    }
    else if ( (cradio = qobject_cast<QRadioButton*>(sender())))
    {
        key = cradio->objectName();
        // Discard false requests
        if (cradio->isChecked() == false)
        {
            m_Settings.remove(key);
            return;
        }
        value = true;
    }

    // Save immediately
    Options::self()->setProperty(key.toLatin1(), value);
    m_Settings[key] = value;
    m_GlobalSettings[key] = value;

    m_DebounceTimer.start();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void DarkLibrary::settleSettings()
{
    emit settingsUpdated(getAllSettings());
    Options::self()->save();
    // Save to optical train specific settings as well
    OpticalTrainSettings::Instance()->setOpticalTrainID(OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText()));
    OpticalTrainSettings::Instance()->setOneSetting(OpticalTrainSettings::DarkLibrary, m_Settings);
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
QJsonObject DarkLibrary::getDefectSettings()
{
    QStringList darkMasters;
    for (int i = 0; i < masterDarksCombo->count(); i++)
        darkMasters << masterDarksCombo->itemText(i);

    QJsonObject createDefectMaps =
    {
        {"masterTime", masterTime->text()},
        {"masterDarks", darkMasters.join('|')},
        {"masterExposure", masterExposure->text()},
        {"masterTempreture", masterTemperature->text()},
        {"masterMean", masterMean->text()},
        {"masterMedian", masterMedian->text()},
        {"masterDeviation", masterDeviation->text()},
        {"hotPixelsEnabled", hotPixelsEnabled->isChecked()},
        {"coldPixelsEnabled", coldPixelsEnabled->isChecked()},
    };
    return createDefectMaps;
}



}

