/*
    SPDX-FileCopyrightText: 2004 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opssolarsystem.h"

class KConfigDialog;

/** @class OpsSolarSystem
	*The Solar System page for the Options window.  This page allows the user
	*to modify display of solar system bodies in KStars, including the
	*major planets, the Sun and Moon, and the comets and asteroids.
	*@short The Solar System page of the Options window.
	*@author Jason Harris
	*@version 1.0
	*/
class OpsSolarSystem : public QFrame, public Ui::OpsSolarSystem
{
    Q_OBJECT

  public:
    explicit OpsSolarSystem();
    virtual ~OpsSolarSystem() override = default;

  private slots:
    void slotChangeMagDownload(double mag);
    void slotAllWidgets(bool on);
    void slotAsteroidWidgets(bool on);
    void slotCometWidgets(bool on);
    void slotSelectPlanets();
    void slotApply();

  private:
    bool isDirty { false };
    KConfigDialog *m_ConfigDialog { nullptr };
};
