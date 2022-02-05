/*
    SPDX-FileCopyrightText: 2022 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsdeveloper.h"

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"

OpsDeveloper::OpsDeveloper() : QFrame(KStars::Instance())
{
    setupUi(this);
}
