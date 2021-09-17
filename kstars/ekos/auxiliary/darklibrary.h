/*
    SPDX-FileCopyrightText: 2021 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indi/indiccd.h"
#include "indi/indicap.h"
#include "darkview.h"
#include "defectmap.h"
#include "ekos/ekos.h"

#include <QDialog>
#include <QPointer>
#include "ui_darklibrary.h"

class QSqlTableModel;
class QSortFilterProxyModel;
class FITSHistogramView;

namespace Ekos
{

class Capture;
class SequenceJob;

/**
 * @class DarkLibrary
 * @short Handles acquisition & loading of dark frames for cameras. If a suitable dark frame exists,
 * it is loaded from disk, otherwise it gets captured and saved for later use.
 *
 * @author Jasem Mutlaq
 * @version 1.0
 */
class DarkLibrary : public QDialog, public Ui::DarkLibrary
{
        Q_OBJECT

    public:
        static DarkLibrary *Instance();
        static void Release();

        bool findDarkFrame(ISD::CCDChip *targetChip, double duration, QSharedPointer<FITSData> &darkData);
        bool findDefectMap(ISD::CCDChip *targetChip, double duration, QSharedPointer<DefectMap> &defectMap);
        // Return false if canceled. True if dark capture proceeds
        void denoise(ISD::CCDChip *targetChip, const QSharedPointer<FITSData> &targetData, double duration,
                     FITSScale filter, uint16_t offsetX, uint16_t offsetY);
        void refreshFromDB();
        void addCamera(ISD::GDInterface * newCCD);
        void removeCamera(ISD::GDInterface * newCCD);
        void checkCamera(int ccdNum = -1);
        //void reset();
        void setCaptureModule(Capture *instance);

    protected:
        virtual void closeEvent(QCloseEvent *ev) override;

    signals:
        void darkFrameCompleted(bool);
        void newLog(const QString &message);

    public slots:
        void processNewImage(SequenceJob *job, const QSharedPointer<FITSData> &data);
        void processNewBLOB(IBLOB *bp);

    private slots:
        void clearAll();
        void clearRow();
        void clearExpired();
        void openDarksFolder();
        void saveDefectMap();
        void setCompleted();
        void loadDarkFITS(QModelIndex index);

    private:
        explicit DarkLibrary(QWidget *parent);
        ~DarkLibrary();

        static DarkLibrary *_DarkLibrary;

        ////////////////////////////////////////////////////////////////////////////////////////////////
        /// Dark Frames Functions
        ////////////////////////////////////////////////////////////////////////////////////////////////
        /**
         * @brief countDarkTotalTime Given current settings, count how many minutes
         * are required to complete all the darks.
         */
        void countDarkTotalTime();

        /**
         * @brief generateDarkJobs Check the user frame parameters in the Darks tab and generate the corresponding
         * capture jobs. Populate capture module with the dark jobs.
         */
        void generateDarkJobs();

        /**
         * @brief executeDarkJobs Start executing the dark jobs in capture module.
         */
        void executeDarkJobs();

        /**
         * @brief stopDarkJobs Abort all dark job captures.
         */
        void stopDarkJobs();

        /**
         * @brief generateMasterFrameHelper Calls templated generateMasterFrame with the correct data type.
         * @param data Passed dark frame data to generateMasterFrame
         * @param metadata passed metadata to generateMasterFrame
         */
        void generateMasterFrame(const QSharedPointer<FITSData> &data, const QJsonObject &metadata);

        /**
         * @brief generateMasterFrame After data aggregation is done, the selected stacking algorithm is applied and the master dark
         * frame is saved to disk and user database along with the metadata.
         * @param data last used data. This is not used for reading, but to simply apply the algorithm to the FITSData buffer
         * and then save it to disk.
         * @param metadata information on frame to help in the stacking process.
         */
        template <typename T>  void generateMasterFrameInternal(const QSharedPointer<FITSData> &data, const QJsonObject &metadata);

        /**
         * @brief aggregateHelper Calls tempelated aggregate function with the appropiate data type.
         * @param data Dark frame data to pass on to aggregate function.
         */
        void aggregate(const QSharedPointer<FITSData> &data);

        /**
         * @brief aggregate Aggregate the data as per the selected algorithm. Each time a new dark frame is received, this function
         * adds the frame data to the dark buffer.
         * @param data Dark frame data.
         */
        template <typename T> void aggregateInternal(const QSharedPointer<FITSData> &data);

        /**
         * @brief subtractHelper Calls tempelated subtract function
         * @param darkData passes dark frame data to templerated subtract function.
         * @param lightData passes list frame data to templerated subtract function.
         * @param filter passes filter to templerated subtract function.
         * @param offsetX passes offsetX to templerated subtract function.
         * @param offsetY passes offsetY to templerated subtract function.
         */
        void subtractDarkData(const QSharedPointer<FITSData> &darkData, const QSharedPointer<FITSData> &lightData, FITSScale filter,
                              uint16_t offsetX, uint16_t offsetY);

        /**
         * @brief subtract Subtracts dark pixels from light pixels given the supplied parameters
         * @param darkData Dark frame data.
         * @param lightData Light frame data. The light frame data is modified in this process.
         * @param filter Any filters to apply to light frame data post-subtraction.
         * @param offsetX Only apply subtraction beyond offsetX in X-axis.
         * @param offsetY Only apply subtraction beyond offsetY in Y-axis.
         */
        template <typename T>
        void subtractInternal(const QSharedPointer<FITSData> &darkData, const QSharedPointer<FITSData> &lightData, FITSScale filter,
                              uint16_t offsetX, uint16_t offsetY);

        /**
         * @brief cacheDarkFrameFromFile Load dark frame from disk and saves it in the local dark frames cache
         * @param filename path of dark frame to load
         * @return True if file is successfully loaded, false otherwise.
         */
        bool cacheDarkFrameFromFile(const QString &filename);


        ////////////////////////////////////////////////////////////////////////////////////////////////
        /// Misc Functions
        ////////////////////////////////////////////////////////////////////////////////////////////////
        void initView();
        void setCaptureState(CaptureState state);
        void reloadDarksFromDatabase();
        void loadCurrentMasterDefectMap();
        void clearBuffers();

        ////////////////////////////////////////////////////////////////////////////////////////////////
        /// Defect Map Functions
        ////////////////////////////////////////////////////////////////////////////////////////////////
        void refreshDefectMastersList(const QString &camera);
        void loadCurrentMasterDark(const QString &camera, int masterIndex = -1);
        void populateMasterMetedata();
        /**
         * @brief cacheDefectMapFromFile Load defect map from disk and saves it in the local defect maps cache
         * @param key dark file name that is used as the key in the defect map cache
         * @param filename path of dark frame to load
         * @return True if file is successfully loaded, false otherwise.
         */
        bool cacheDefectMapFromFile(const QString &key, const QString &filename);

        /**
         * @brief normalizeDefects Remove defects from LIGHT image by replacing bad pixels with a 3x3 median filter around
         * them.
         * @param defectMap Defect Map containing a list of hot and cold pixels.
         * @param lightData Target light data to remove noise from.
         * @param filter Filter used for light data
         * @param offsetX Only apply filtering beyond offsetX in X-axis.
         * @param offsetY Only apply filtering beyond offsetX in Y-axis.
         */
        void normalizeDefects(const QSharedPointer<DefectMap> &defectMap, const QSharedPointer<FITSData> &lightData,
                              FITSScale filter, uint16_t offsetX, uint16_t offsetY);

        template <typename T>
        void normalizeDefectsInternal(const QSharedPointer<DefectMap> &defectMap, const QSharedPointer<FITSData> &lightData,
                                      FITSScale filter, uint16_t offsetX, uint16_t offsetY);

        template <typename T>
        T median3x3Filter(uint16_t x, uint16_t y, uint32_t width, T *buffer);


        ////////////////////////////////////////////////////////////////////////////////////////////////
        /// Member Variables
        ////////////////////////////////////////////////////////////////////////////////////////////////

        QList<QVariantMap> m_DarkFramesDatabaseList;
        QMap<QString, QSharedPointer<FITSData>> m_CachedDarkFrames;
        QMap<QString, QSharedPointer<DefectMap>> m_CachedDefectMaps;

        ISD::CCD *m_CurrentCamera {nullptr};
        ISD::CCDChip *m_TargetChip {nullptr};
        QList<ISD::CCD *> m_Cameras;
        bool m_UseGuideHead {false};

        Capture *m_CaptureModule {nullptr};
        QSqlTableModel *darkFramesModel = nullptr;
        QSortFilterProxyModel *sortFilter = nullptr;

        std::vector<uint32_t> m_DarkMasterBuffer;
        uint32_t m_DarkImagesCounter {0};
        bool m_RememberFITSViewer {true};
        bool m_RememberSummaryView {true};
        QString m_DefectMapFilename, m_MasterDarkFrameFilename;
        QStringList m_DarkCameras, m_DefectCameras;
        QPointer<DarkView> m_DarkView;
        QPointer<QStatusBar> m_StatusBar;
        QPointer<QLabel> m_StatusLabel, m_FileLabel;
        QSharedPointer<DefectMap> m_CurrentDefectMap;
        QSharedPointer<FITSData> m_CurrentDarkFrame;
        QFutureWatcher<bool> m_DarkFrameFutureWatcher;
};
}
