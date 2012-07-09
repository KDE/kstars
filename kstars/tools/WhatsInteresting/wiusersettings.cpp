/***************************************************************************
                          wiusersettings.cpp  -  K Desktop Planetarium
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


#include "wiusersettings.h"
#include "WIView.h"
#include "kdebug.h"

WIUserSettings::WIUserSettings(QWidget* parent, Qt::WindowFlags flags): QWizard(parent, flags)
{
    setupUi(this);
    connect(this, SIGNAL(finished(int)), this, SLOT(slotFinished(int)));
}

WIUserSettings::~WIUserSettings() {}

void WIUserSettings::slotFinished( int )
{
    wi = new WIView();
}


#include "wiusersettings.moc"