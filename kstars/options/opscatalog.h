/***************************************************************************
                          opscatalog.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 29 2004
    copyright            : (C) 2004 by Jason Harris
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

#include "ui_opscatalog.h"

class KStars;
class QListWidgetItem;
class KConfigDialog;

/**
 * @class OpsCatalog
 * The Catalog page for the Options window.  This page allows the user
 * to modify display of the major object catalogs in KStars:
 * @li Hipparcos/Tycho Star Catalog
 *
 * DSO catalog control is deffered to `CatacalogsDBUI`.
 *
 * @short Catalog page of the Options window.
 * @author Jason Harris
 * @version 1.0
 */
class OpsCatalog : public QFrame, public Ui::OpsCatalog
{
    Q_OBJECT

  public:
    explicit OpsCatalog();
    virtual ~OpsCatalog() override = default;

  private slots:
    void slotStarWidgets(bool on);
    void slotDeepSkyWidgets(bool on);
    void slotApply();
    void slotCancel();

  private:
    KConfigDialog *m_ConfigDialog{ nullptr };
    float m_StarDensity{ 0 };
    bool isDirty{ false };
};
