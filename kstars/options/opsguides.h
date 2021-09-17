/*
    SPDX-FileCopyrightText: 2005 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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

