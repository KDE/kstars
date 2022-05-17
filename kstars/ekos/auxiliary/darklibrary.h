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
 * @short Handles acquisition & loading of dark frames and defect map for cameras. If a suitable dark frame exists,
 * it is loaded from disk, otherwise it gets captured and saved for later use.
 *
 * Dark Frames:
 *
 * The user can generate dark frames from an average combination of the camera dark frames. By default, 5 dark frames
 * are captured to merged into a single master frame. Frame duration, binning, and temperature are all configurable.
 * If the user select "Dark" in any of the Ekos module, Dark Library can be queried if a suitable dark frame exists given
 * the current camera settings (binning, temperature..etc). If a suitable frame exists, it is loaded up and send to /class DarkProcessor
 * class along with the light frame to perform subtraction or defect map corrections.
 *
 * Defect Maps:
 *
 * Some CMOS cameras exhibit hot pixels that are better treated with a defect map. A defect map is a collection of "bad" pixels that
 * are above or below certain threshold controlled by the user. This should isolate the cold and hotpixels in frames so that they are
 * removed from the light frames once the defect map is applied against it. This is done using 3x3 median filter over the bad pixels.
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

        /**
         * @brief findDarkFrame Search for a dark frame that matches the passed paramters.
         * @param targetChip Camera chip pointer to lookup for relevant information (binning, ROI..etc).
         * @param duration Duration is second to match it against the database.
         * @param darkData If a frame is found, load it from disk and store it in a shared FITSData pointer.
         * @return True if a suitable frame was found the loaded successfully, false otherwise.
         */
        bool findDarkFrame(ISD::CCDChip *targetChip, double duration, QSharedPointer<FITSData> &darkData);

        /**
         * @brief findDefectMap Search for a defect map that matches the passed paramters.
         * @param targetChip Camera chip pointer to lookup for relevant information (binning, ROI..etc).
         * @param duration Duration is second to match it against the database.
         * @param defectMap If a frame is found, load it from disk and store it in a shared DefectMap pointer.
         * @return True if a suitable frame was found the loaded successfully, false otherwise.
         */
        bool findDefectMap(ISD::CCDChip *targetChip, double duration, QSharedPointer<DefectMap> &defectMap);

        /**
         * @brief cameraHasDefectMaps Check if camera has any defect maps available.
         * @param name Camera name
         * @return True if at least one defect maps exists for this camera, false otherwise.
         */
        bool cameraHasDefectMaps(const QString &name) const
        {
            return m_DefectCameras.contains(name);
        }

        void refreshFromDB();
        void addCamera(ISD::GDInterface * newCCD);
        void removeCamera(ISD::GDInterface * newCCD);
        void checkCamera(int ccdNum = -1);
        //void reset();
        void setCaptureModule(Capture *instance);

        void start();
        void setDarkSettings(const QJsonObject &settings);
        void setCameraPresets(const QJsonObject &settings);
        QJsonObject getCameraPresets();
        QJsonObject getDarkSettings();
        QJsonObject getDefectSettings();
        void setDefectPixels(const QJsonObject &payload);
        QJsonArray getViewMasters();
        void getloadDarkViewMasterFITS(int index);
        void setDefectSettings(const QJsonObject row);
        void setDefectMapEnabled(bool enabled);

        /**
         * @brief stop Abort all dark job captures.
         */
        void stop();
    protected:
        virtual void closeEvent(QCloseEvent *ev) override;

    signals:
        void newLog(const QString &message);
        void newImage(const QSharedPointer<FITSData> &data);
        void newFrame(FITSView *view);

    public slots:
        void processNewImage(SequenceJob *job, const QSharedPointer<FITSData> &data);
        void processNewBLOB(IBLOB *bp);
        void loadIndexInView(int row);
        void clearRow(int row = -1);


    private slots:
        void clearAll();
        void clearExpired();
        void openDarksFolder();
        void saveDefectMap();
        void setCompleted();


    private:
        explicit DarkLibrary(QWidget *parent);
        ~DarkLibrary() override;

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
         * @brief execute Start executing the dark jobs in capture module.
         */
        void execute();

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
        /// Camera Functions
        ////////////////////////////////////////////////////////////////////////////////////////////////
        double getGain();

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
        double GainSpinSpecialValue { INVALID_VALUE };

        Capture *m_CaptureModule {nullptr};
        QSqlTableModel *darkFramesModel = nullptr;
        QSortFilterProxyModel *sortFilter = nullptr;

        std::vector<uint32_t> m_DarkMasterBuffer;
        uint32_t m_DarkImagesCounter {0};
        bool m_RememberFITSViewer {true};
        bool m_RememberSummaryView {true};
        bool m_JobsGenerated {false};
        QJsonObject m_PresetSettings, m_FileSettings;
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
