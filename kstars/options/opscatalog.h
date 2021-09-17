/*
    SPDX-FileCopyrightText: 2004 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
