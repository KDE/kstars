/***************************************************************************
                          opsguides.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun 6 Feb 2005
    copyright            : (C) 2005 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#pragma once

#include "ui_opsguides.h"

class KConfigDialog;

/**
 * @class OpsGuides
 * The guide page enables to user to select to turn on and off guide overlays such as
 * constellation lines, boundaries, flags..etc.
 */
class OpsGuides : public QFrame, public Ui::OpsGuides
{
        Q_OBJECT

    public:
        explicit OpsGuides();
        virtual ~OpsGuides() override = default;

    private slots:
        void slotApply();
        void slotToggleConstellOptions(bool state);
        void slotToggleConstellationArt(bool state);
        void slotToggleMilkyWayOptions(bool state);
        void slotToggleOpaqueGround(bool state);
        void slotToggleAutoSelectGrid(bool state);

    private:
        KConfigDialog *m_ConfigDialog { nullptr };
        bool isDirty { false };
};

