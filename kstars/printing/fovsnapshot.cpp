/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fovsnapshot.h"

FovSnapshot::FovSnapshot(const QPixmap &pixmap, QString description, FOV *fov, const SkyPoint &center)
    : m_Pixmap(pixmap), m_Description(description), m_Fov(fov), m_CentralPoint(center)
{
}
