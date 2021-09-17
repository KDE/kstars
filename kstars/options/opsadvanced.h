/*
    SPDX-FileCopyrightText: 2004 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsadvanced.h"

/**
 * @class OpsAdvanced
 * The Advanced Tab of the Options window.  In this Tab the user can configure
 * advanced behaviors of the program, including:
 * @li Whether some objects are hidden when the map is moving (and which objects)
 * @li Whether positions are corrected for atmospheric refraction
 * @li Whether a slewing animation is used to move the Focus position
 * @li Whether centered objects are automatically labeled
 * @li whether a "transient" label is attached when the mouse "hovers" at an object.
 * @li whether to enable verbose debug output to a file which could be useful in troubleshooting any issues in KStars.
 *
 * @author Jason Harris, Jasem Mutlaq
 * @version 1.1
 */

class OpsAdvanced : public QFrame, public Ui::OpsAdvanced
{
        Q_OBJECT

    public:
        OpsAdvanced();
        virtual ~OpsAdvanced() override = default;

    private slots:
        void slotChangeTimeScale(float newScale);
        void slotToggleHideOptions();
        void slotToggleVerbosityOptions();
        void slotToggleOutputOptions();
        void slotShowLogFiles();
        void slotApply();
        void slotPurge();
};
