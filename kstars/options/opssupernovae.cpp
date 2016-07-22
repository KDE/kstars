/***************************************************************************
                          opssupernovae.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Thu, 25 Aug 2011
    copyright            : (C) 2011 by Samikshan Bairagya
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

#include "opssupernovae.h"

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymapcomposite.h"
#include "skycomponents/supernovaecomponent.h"

OpsSupernovae::OpsSupernovae() : QFrame(KStars::Instance())
{
    setupUi(this);

    // Signals and slots connections
    connect(supUpdateButton, SIGNAL(clicked()),
            KStarsData::Instance()->skyComposite()->supernovaeComponent(), SLOT(slotTriggerDataFileUpdate()));
}
