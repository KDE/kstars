/*  Artificial Horizon Component
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#ifndef ARTFICIALHORIZONCOMPONENT_H
#define ARTFICIALHORIZONCOMPONENT_H

#include "noprecessindex.h"

class ArtificialHorizonEntity
{
public:
    ArtificialHorizonEntity();
    ~ArtificialHorizonEntity();

    QString region() const;
    void setRegion(const QString &Region);

    bool enabled() const;
    void setEnabled(bool Enabled);

    void clearList();
    void setList(LineList *List);
    LineList *list();

private:

    QString m_Region;
    bool m_Enabled;
    LineList *m_List;
};

/**
 * @class ArtificialHorizon
 * Represents custom area from the horizon upwards which represent blocked views from the vantage point of the user.
 * Such blocked views could stem for example from tall trees or buildings. The user can define one or more polygons to
 * represent the blocked areas.
 *
 * @author Jasem Mutlaq
 * @version 0.1
 */
class ArtificialHorizonComponent : public NoPrecessIndex
{
public:    

    /** @short Constructor
     * @p parent pointer to the parent SkyComposite object
     * name is the name of the subclass
     */
    explicit ArtificialHorizonComponent( SkyComposite *parent );

    virtual bool selected();
    virtual void draw( SkyPainter *skyp );

    void setLivePreview(LineList *preview) { livePreview = preview; }
    void addRegion(const QString &regionName, bool enabled, LineList *list);
    void removeRegion(const QString &regionName, bool lineOnly=false);
    inline QList<ArtificialHorizonEntity *> * horizonList() { return &m_HorizonList; }

    bool load();
    void save();

protected:
    virtual void preDraw( SkyPainter *skyp );

private:
    QList<ArtificialHorizonEntity *> m_HorizonList;
    LineList *livePreview;
};

#endif
