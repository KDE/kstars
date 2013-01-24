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

#include "ui_wiequipsettings.h"

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
     * \brief Constructor
     */
    WIEquipSettings(KStars *ks);

private:
    KStars *m_Ks;

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
};

#endif
