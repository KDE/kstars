/*
    SPDX-FileCopyrightText: 2013 Samikshan Bairagya <samikshan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "wilpsettings.h"

#include "kstars.h"

WILPSettings::WILPSettings(KStars *ks) : QFrame(ks), m_Ks(ks)
{
    setupUi(this);
}
