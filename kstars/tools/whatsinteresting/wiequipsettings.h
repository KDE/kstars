/***************************************************************************
                          wiequipsettings.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2013/09/01
    copyright            : (C) 2013 by Samikshan Bairagya
    email                : samikshan@gmail.com
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

#include "ui_wiequipsettings.h"
#include "obsconditions.h"
#include "oal/scope.h"

#define INVALID_APERTURE -1

/**
 * @class WIEquipSettings
 * @brief User interface for "Equipment Type and Parameters" page in WI settings dialog
 *
 * @author Samikshan Bairagya
 * @author Jasem Mutlaq
 * @version 1.1
 */
class WIEquipSettings : public QFrame, public Ui::WIEquipSettings
{
    Q_OBJECT

  public:
    /** @enum ScopeRoles User-defined roles for scope item */
    enum ScopeItemRoles
    {
        Vendor = Qt::UserRole + 4,
        Model,
        Aperture,
        FocalLength,
        Type
    };

    WIEquipSettings();

    /** Inline method to return aperture */
    inline double getAperture() { return m_Aperture; }

    /** Set aperture to use */
    void setAperture();

    /** Inline method to return telescope type */
    inline ObsConditions::TelescopeType getTelType() { return m_TelType; }

    /** Populates scope list widget in UI with list of telescopes from KStars userdb. */
    void populateScopeListWidget();

  private:
    QList<OAL::Scope *> m_ScopeList;
    /// Aperture of equipment to use
    double m_Aperture { 0 };
    /// Telescope type
    ObsConditions::TelescopeType m_TelType { ObsConditions::Invalid };

  private slots:
    /** Private slot - Equipment type selected - Telescope */
    void slotTelescopeCheck(bool on);

    /** Private slot - Equipment type selected - Binoculars */
    void slotBinocularsCheck(bool on);

    /** Private slot - Telescope selected from KStars userdb */
    void slotScopeSelected(int row);

    /** Private slot - Add new telescope to KStars userdb */
    void slotAddNewScope();
};
