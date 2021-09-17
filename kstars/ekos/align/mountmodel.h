/*  Ekos Mount Model
    SPDX-FileCopyrightText: 2018 Robert Lancaster

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once


#include "ui_mountmodel.h"
#include "ekos/ekos.h"
#include "starobject.h"

#include <QDialog>
#include <QUrl>

class QProgressIndicator;
class SkyObject;

namespace Ekos
{

class Align;

class MountModel : public QDialog, public Ui::mountModel
{
        Q_OBJECT

    public:
        explicit MountModel(Align *parent);
        ~MountModel();

        enum ModelObjectType
        {
            OBJECT_ANY_STAR,
            OBJECT_NAMED_STAR,
            OBJECT_ANY_OBJECT,
            OBJECT_FIXED_DEC,
            OBJECT_FIXED_GRID
        };

        void setTelescopeCoord(const SkyPoint &newCoord)
        {
            telescopeCoord = newCoord;
        }

        bool isRunning() const
        {
            return m_IsRunning;
        }

        void setAlignStatus(Ekos::AlignState state);

    protected:
        void slotWizardAlignmentPoints();
        void slotStarSelected(const QString selectedStar);
        void slotLoadAlignmentPoints();
        void slotSaveAsAlignmentPoints();
        void slotSaveAlignmentPoints();
        void slotClearAllAlignPoints();
        void slotRemoveAlignPoint();
        void slotAddAlignPoint();
        void slotFindAlignObject();
        void resetAlignmentProcedure();
        void startStopAlignmentProcedure();
        void startAlignmentPoint();
        void finishAlignmentPoint(bool solverSucceeded);
        void moveAlignPoint(int logicalIndex, int oldVisualIndex, int newVisualIndex);
        void alignTypeChanged(int alignType);
        void togglePreviewAlignPoints();
        void slotSortAlignmentPoints();


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
        int findNextAlignmentPointAfter(int currentSpot);
        int findClosestAlignmentPointToTelescope();
        void swapAlignPoints(int firstPt, int secondPt);

        /**
             * @brief Get formatted RA & DEC coordinates compatible with astrometry.net format.
             * @param ra Right ascension
             * @param dec Declination
             * @param ra_str will contain the formatted RA string
             * @param dec_str will contain the formatted DEC string
             */
        void getFormattedCoords(double ra, double dec, QString &ra_str, QString &dec_str);

    signals:
        void newLog(const QString &);

    private:

        Align *m_AlignInstance {nullptr};
        int currentAlignmentPoint { 0 };
        bool m_IsRunning { false };
        bool previewShowing { false };
        QVector<const StarObject *> alignStars;
        QUrl alignURL;
        QUrl alignURLPath;
        SkyPoint telescopeCoord;


};
}
