/*
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsterrain.h"
#include <QFrame>
#include <kconfigdialog.h>

class KStars;

/**
 * @class OpsTerrain
 * The terrain page enables to user to manage the options for the terrain overlay.
 */
class OpsTerrain : public QFrame, public Ui::OpsTerrain
{
        Q_OBJECT

    public:
        explicit OpsTerrain();
        virtual ~OpsTerrain() override = default;
        void syncOptions();

    private slots:
        void slotApply();
        void saveTerrainFilename();

    private:
        KConfigDialog *m_ConfigDialog { nullptr };
};

