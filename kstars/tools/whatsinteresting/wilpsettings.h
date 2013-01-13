/***************************************************************************
                          wilpsettings.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2013/07/01
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

#ifndef WI_LP_SETTINGS_H
#define WI_LP_SETTINGS_H

#include "ui_wilpsettings.h"

class KStars;

class WILPSettings : public QFrame, public Ui::WILPSettings
{
    Q_OBJECT
public:
    WILPSettings(KStars *ks);

private:
    KStars *m_Ks;

private slots:
    void bortleValueChanged(int value);
};

#endif