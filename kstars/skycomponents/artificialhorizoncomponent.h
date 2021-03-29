/*  Artificial Horizon Component
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include "noprecessindex.h"

#include <memory>

class ArtificialHorizonEntity
{
    public:
        ArtificialHorizonEntity() = default;
        ~ArtificialHorizonEntity();

        QString region() const;
        void setRegion(const QString &Region);

        bool enabled() const;
        void setEnabled(bool Enabled);

        void clearList();
        void setList(const std::shared_ptr<LineList> &list);
        std::shared_ptr<LineList> list();

        // Returns the altitude constraint for the azimuth angle.
        double altitudeConstraint(double azimuthDegrees);

    private:
        QString m_Region;
        bool m_Enabled { false };
        std::shared_ptr<LineList> m_List;
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
        void addRegion(const QString &regionName, bool enabled, const std::shared_ptr<LineList> &list);
        void removeRegion(const QString &regionName, bool lineOnly = false);
        inline QList<ArtificialHorizonEntity *> *horizonList()
        {
            return &m_HorizonList;
        }

        bool load();
        void save();

        // Returns the maximum altitude constraint from all the enabled ArtificialHorizonEntities.
        double altitudeConstraint(double azimuthDegrees) const;

        // Returns true if one or more artificial horizons are enabled.
        bool altitudeConstraintsExist() const;


    protected:
        void preDraw(SkyPainter *skyp) override;

    private:
        QList<ArtificialHorizonEntity *> m_HorizonList;
        std::shared_ptr<LineList> livePreview;
        int selectedPreviewPoint { -1 };

        friend class TestArtificialHorizon;
};
