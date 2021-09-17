/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "noprecessindex.h"

#include <memory>

class TestArtificialHorizon;

// An ArtificialHorizonEntity is a set of Azimuth & Altitude values defining
// a series of connected line segments. Assuming ceiling is false (the default)
// these lines define a horizon--coordinates indicating where the view is blocked
// (below the line segments, lower in altitude) and where it is not blocked
// (above the line segments, higher altitude values). If ceiling is true, then
// this definition is flipped--the sky higher in altitude than the line segments
// is considered blocked.
class ArtificialHorizonEntity
{
    public:
        ArtificialHorizonEntity() = default;
        ~ArtificialHorizonEntity();

        QString region() const;
        void setRegion(const QString &Region);

        bool enabled() const;
        void setEnabled(bool Enabled);

        bool ceiling() const;
        void setCeiling(bool value);

        void clearList();
        void setList(const std::shared_ptr<LineList> &list);
        std::shared_ptr<LineList> list() const;

        // Returns the altitude constraint for the azimuth angle (degrees).
        // constraintExists will be set to false if there is no constraint for the azimuth.
        double altitudeConstraint(double azimuthDegrees, bool *constraintExists) const;

    private:
        QString m_Region;
        bool m_Enabled { false };
        bool m_Ceiling { false };
        std::shared_ptr<LineList> m_List;
};

// ArtificialHorizon can contain several ArtificialHorizonEntities. That is,
// it can have several sets of connected line segments. Assuming all the entities
// are not ceilings, then the view is considered blocked below the highest line
// segment that intersects a given azimuth. If none of the line segments cross
// a given azimuth, then the view is not blocked at any altitude for that azimuth.
// Similarly, if there are only "ceiling" horizon entities, then the view is blocked
// at altitudes above the lowest ceiling. If there are a mix of ceilings and standard
// entities, then for the given azimuth, at an altitude A, the view is blocked if
// either the closest line below is a ceiling, or if the closest line above is a non-ceiling.
class ArtificialHorizon
{
    public:
        ArtificialHorizon() {}
        ~ArtificialHorizon();

        ArtificialHorizonEntity *findRegion(const QString &regionName);
        void addRegion(const QString &regionName, bool enabled, const std::shared_ptr<LineList> &list, bool ceiling);
        void removeRegion(const QString &regionName, bool lineOnly = false);
        bool enabled(int i) const;
        void load(const QList<ArtificialHorizonEntity *> &list);

        const QList<ArtificialHorizonEntity *> *horizonList() const
        {
            return &m_HorizonList;
        }

        // Returns true if one or more artificial horizons are enabled.
        bool altitudeConstraintsExist() const;

        // Returns true if the azimuth/altitude point is not blocked by the artificial horzon entities.
        bool isVisible(double azimuthDegrees, double altitudeDegrees) const;

        // returns the (highest) altitude constraint at the given azimuth.
        // If there are no constraints, then it returns -90.
        double altitudeConstraint(double azimuthDegrees) const;

        // Finds the nearest enabled constraint at the azimuth and above or below (not not exactly at)
        // the altitude given.
        const ArtificialHorizonEntity *getConstraintAbove(double azimuthDegrees, double altitudeDegrees,
                const ArtificialHorizonEntity *ignore = nullptr) const;
        const ArtificialHorizonEntity *getConstraintBelow(double azimuthDegrees, double altitudeDegrees,
                const ArtificialHorizonEntity *ignore = nullptr) const;

        // Draw the blocked areas on the skymap using the SkyPainter.
        // If painter is a nullptr, nothing is drawn.
        // If regious is not a nullpointer, all the polygon coordinates are placed
        // in the QList (for testing).
        void drawPolygons(SkyPainter *painter, QList<LineList> *regions = nullptr);

    private:
        // Removes a call to KStars::Instance() which is not necessary in testing.
        void setTesting()
        {
            testing = true;
        }
        void drawPolygons(int entity, SkyPainter *painter, QList<LineList> *regions = nullptr);
        void drawSampledPolygons(int entity, double az1, double alt1, double az2, double alt2,
                                 double sampling, SkyPainter *painter, QList<LineList> *regions);
        bool computePolygon(int entity, double az1, double alt1, double az2, double alt2,
                            LineList *region);

        QList<ArtificialHorizonEntity *> m_HorizonList;
        bool testing { false };

        friend TestArtificialHorizon;
};

/**
 * @class ArtificialHorizon
 * Represents custom area from the horizon upwards which represent blocked views from the vantage point of the user.
 * Such blocked views could stem for example from tall trees or buildings. The user can define a series of line segments to
 * represent the blocked areas.
 *
 * @author Jasem Mutlaq
 * @version 0.1
 */
class ArtificialHorizonComponent : public NoPrecessIndex
{
    public:
        /**
         * @short Constructor
         *
         * @p parent pointer to the parent SkyComposite object
         * name is the name of the subclass
         */
        explicit ArtificialHorizonComponent(SkyComposite *parent);

        virtual ~ArtificialHorizonComponent() override;

        bool selected() override;
        void draw(SkyPainter *skyp) override;

        void setLivePreview(const std::shared_ptr<LineList> &preview)
        {
            livePreview = preview;
        }
        void setSelectedPreviewPoint(int index)
        {
            selectedPreviewPoint = index;
        }
        void addRegion(const QString &regionName, bool enabled, const std::shared_ptr<LineList> &list, bool ceiling);
        void removeRegion(const QString &regionName, bool lineOnly = false);

        const ArtificialHorizon &getHorizon()
        {
            return horizon;
        }

        bool load();
        void save();

    protected:
        void preDraw(SkyPainter *skyp) override;

    private:
        ArtificialHorizon horizon;
        std::shared_ptr<LineList> livePreview;
        int selectedPreviewPoint { -1 };

        friend class TestArtificialHorizon;
};
