/***************************************************************************
                          wiusersettings.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/09/07
    copyright            : (C) 2012 by Samikshan Bairagya
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

#ifndef WI_USER_SETTINGS_H
#define WI_USER_SETTINGS_H

class WIView;

#include "obsconditions.h"
#include <QWizard>
#include "ui_wiusersettings.h"

/**
 * \class WIUserSettings
 * \brief Wizard to set the location and equipment specifications for "What's Interesting..."
 * \author Samikshan Bairagya
 */
class WIUserSettings : public QWizard, public Ui::WIUserSettingsUI
{
    Q_OBJECT
public:

    /**
     * \brief Constructor
     */
    WIUserSettings(QWidget *parent = 0, Qt::WindowFlags flags = 0);

public slots:

    /**
     * \brief Finish wizard and display QML interface for "What's Interesting..."
     */
    void slotFinished(int status);

    /**
     * \brief Telescope available - check/uncheck.
     */
    void slotTelescopeCheck(bool on);

    /**
     * \brief Binoculars available - check/uncheck.
     */
    void slotBinocularsCheck(bool on);

private:

    /**
     * \brief Make connections between signals and corresponding slots.
     */
    void makeConnections();

    ObsConditions::Equipment m_Equip;
    ObsConditions::TelescopeType m_TelType;
    WIView *m_WI;
};

#endif
