/*
  Copyright (C) 2021, Hy Murveit

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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

    private slots:
        void slotApply();
        void saveTerrainFilename();

    private:
        KConfigDialog *m_ConfigDialog { nullptr };
        bool isDirty { false };
};

