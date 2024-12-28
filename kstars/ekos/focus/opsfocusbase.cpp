/*
    SPDX-FileCopyrightText: 2024 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "opsfocusbase.h"
#include "kstars.h"

namespace Ekos
{

OpsFocusBase::OpsFocusBase(const QString name): QFrame(KStars::Instance())
{
    m_dialogName = name;
}

} // namespace Ekos
