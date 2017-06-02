/***************************************************************************
                          equator.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-08-09
    copyright            : (C) 2007 by James B. Bowlin
    email                : bowlin@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef EQUATOR_H
#define EQUATOR_H

#include "noprecessindex.h"
#include "linelistlabel.h"


/**
 * @class Equator
 * Represents the equator on the sky map.
 *
 * @author James B. Bowlin
 * @version 0.1
 */
class Equator : public NoPrecessIndex
{
    public:

        /** @short Constructor
         * @p parent pointer to the parent SkyComposite object
         * name is the name of the subclass
         */
        explicit Equator( SkyComposite * parent );

        bool selected() Q_DECL_OVERRIDE;
        void draw( SkyPainter * skyp ) Q_DECL_OVERRIDE;
        virtual void drawCompassLabels();
        LineListLabel * label() Q_DECL_OVERRIDE
        {
            return &m_label;
        }

    protected:
        void preDraw( SkyPainter * skyp ) Q_DECL_OVERRIDE;

    private:
        LineListLabel m_label;
};

#endif
