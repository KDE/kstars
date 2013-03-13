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

#ifndef WI_EQUIP_SETTINGS_H
#define WI_EQUIP_SETTINGS_H

#define INVALID_APERTURE -1

#include "ui_wiequipsettings.h"
#include "obsconditions.h"
#include "oal/scope.h"

class KStars;

/**
 * \class WIEquipSettings
 * \brief User interface for "Equipment Type and Parameters" page in WI settings dialog
 * \author Samikshan Bairagya
 */
class WIEquipSettings : public QFrame, public Ui::WIEquipSettings
{
    Q_OBJECT
public:
    /**
     * \enum ScopeRoles User-defined roles for scope item
     */
    enum ScopeItemRoles { Vendor = Qt::UserRole + 4, Model, Aperture, FocalLength, Type };

    /**
     * \brief Constructor
     */
    WIEquipSettings(KStars *ks);

    /**
     * \brief Inline method to return aperture
     */
    inline double getAperture() { return m_Aperture; }

    /**
     * \brief Set aperture to use
     */
    void setAperture();

    /**
     * \brief Inline method to return telescope type
     */
    inline ObsConditions::TelescopeType getTelType() { return m_TelType; }

    /**
     * \brief Populates scope list widget in UI with list of telescopes from KStars userdb.
     */
    void populateScopeListWidget();

private:
    KStars *m_Ks;
    QList<OAL::Scope *> m_ScopeList;
    double m_Aperture;                           ///Aperture of equipment to use
    ObsConditions::TelescopeType m_TelType;      ///Type of telescope being used

private slots:
    /**
     * \brief private slot - Equipment type selected - Telescope
     */
    void slotTelescopeCheck(bool on);

    /**
     * \brief private slot - Equipment type selected - Binoculars
     */
    void slotBinocularsCheck(bool on);

    /**
     * \brief private slot - No equipment type selected
     */
    void slotNoEquipCheck(bool on);

    /**
     * \brief private slot - Telescope selected from KStars userdb
     */
    void slotScopeSelected(QListWidgetItem *scopeItem);

    /**
     * \brief private slot - Add new telescope to KStars userdb
     */
    void slotSaveNewScope();
};

#endif
