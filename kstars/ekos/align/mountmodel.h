/*  Ekos Mount Model
    SPDX-FileCopyrightText: 2018 Robert Lancaster

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once


#include "ui_mountmodel.h"
#include "ekos/ekos.h"
#include "indi/indistd.h"
#include "skypoint.h"

#include <QDialog>
#include <QUrl>
#include <functional>

class QProgressIndicator;
class SkyObject;
class StarObject;
class ArtificialHorizon;

namespace Ekos
{

class Align;

class MountModel : public QDialog, public Ui::mountModel
{
        Q_OBJECT

    public:
        struct AlignmentPoint
        {
            QString ra;
            QString dec;
            QString name;
        };

        explicit MountModel(Align *parent);
        ~MountModel();

        static QVector<AlignmentPoint> generateHaltonPoints(
            int points,
            double minAlt,
            double maxAlt,
            double maxAbsDec,
            const dms &lst,
            const dms &lat,
            const ArtificialHorizon *horizon,
            bool snap,
            const std::function<const SkyObject*(double, double)> &lookupObject = nullptr,
            const std::function<void(SkyObject*)> &updateCoords = nullptr
        );

        enum ModelObjectType
        {
            OBJECT_ANY_STAR,
            OBJECT_NAMED_STAR,
            OBJECT_ANY_OBJECT,
            OBJECT_HALTON_SEQUENCE,
            OBJECT_FIXED_DEC,
            OBJECT_FIXED_GRID
        };

        bool isRunning() const
        {
            return m_IsRunning;
        }

        void setAlignStatus(Ekos::AlignState state);

        /**
         * @brief Get all mount model settings as a variant map.
         * @return QVariantMap containing alignment points data.
         */
        Q_INVOKABLE QVariantMap getAllSettings();

        /**
         * @brief Set all mount model settings from a variant map.
         * @param settings QVariantMap containing alignment points data.
         */
        Q_INVOKABLE void setAllSettings(const QVariantMap &settings);

        /**
         * @brief Start or stop the alignment procedure.
         */
        Q_INVOKABLE void startStopAlignmentProcedure();

        /**
         * @brief Reset the alignment procedure.
         */
        Q_INVOKABLE void resetAlignmentProcedure();

        /**
         * @brief Clear all alignment points.
         */
        Q_INVOKABLE void slotClearAllAlignPoints();

        /**
         * @brief Generate wizard alignment points.
         */
        Q_INVOKABLE void slotWizardAlignmentPoints();

        /**
         * @brief Add an alignment point.
         */
        Q_INVOKABLE void slotAddAlignPoint();

        /**
         * @brief Remove the selected alignment point.
         */
        Q_INVOKABLE void slotRemoveAlignPoint();

        /**
         * @brief Sort alignment points.
         */
        Q_INVOKABLE void slotSortAlignmentPoints();

    protected:
        void slotStarSelected(const QString selectedStar);
        void slotLoadAlignmentPoints();
        void slotSaveAlignmentPoints();
        void slotFindAlignObject();
        void startAlignmentPoint();
        void finishAlignmentPoint(bool solverSucceeded);
        void moveAlignPoint(int logicalIndex, int oldVisualIndex, int newVisualIndex);
        void alignTypeChanged(int alignType);
        void togglePreviewAlignPoints();
        void onMountParkStatusChanged(ISD::ParkStatus status);


    private:

        void generateAlignStarList();
        bool alignmentPointsAreBad();
        bool loadAlignmentPoints(const QString &fileURL);
        bool saveAlignmentPoints(const QString &path);

        bool isVisible(const SkyObject *so);
        double getAltitude(const SkyObject *so);
        const SkyObject *getWizardAlignObject(double ra, double de);
        void calculateAngleForRALine(double &raIncrement, double &initRA, double initDEC, double lat, double raPoints,
                                     double minAlt);
        void calculateAZPointsForDEC(dms dec, dms alt, dms &AZEast, dms &AZWest);
        void updatePreviewAlignPoints();
        void sortTableRows(int fromRow, const SkyPoint &start);
        void swapAlignPoints(int firstPt, int secondPt);

        void saveAndOverrideSolverSettings();
        void restoreSolverSettings();

        /**
             * @brief Get formatted RA & DEC coordinates compatible with astrometry.net format.
             * @param ra Right ascension
             * @param dec Declination
             * @param ra_str will contain the formatted RA string
             * @param dec_str will contain the formatted DEC string
             */
        static void getFormattedCoords(double ra, double dec, QString &ra_str, QString &dec_str);
        QVariantList getAlignmentPointsList();

    Q_SIGNALS:
        void newLog(const QString &);
        void aborted();
        void progressUpdated(const QVariantMap &settings);

    private:

        Align *m_AlignInstance {nullptr};
        int currentAlignmentPoint { 0 };
        bool m_IsRunning { false };
        bool previewShowing { false };
        QVector<const StarObject *> alignStars;
        QUrl alignURL;
        // Saved solver settings restored after model run
        bool m_solverSettingsSaved { false };
        bool m_savedUsePosition { false };
        bool m_savedUseScale { false };
        int m_savedGotoMode { 2 };  // Align::GOTO_NOTHING -- safe no-op default

        bool m_WaitingForUnpark { false };

};
}
