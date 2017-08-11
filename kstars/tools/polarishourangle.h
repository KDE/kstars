/*
  Copyright (C) 2015-2017, Pavel Mraz

  Copyright (C) 2017, Jasem Mutlaq

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

#include <memory>

#include "ui_polarishourangle.h"

class SkyObject;

class PolarisHourAngle : public QDialog, public Ui::PolarisHourAngle
{
  Q_OBJECT

public:
  explicit PolarisHourAngle(QWidget *parent);

protected:
  void paintEvent(QPaintEvent *);

private slots:
  void onTimeUpdated(QDateTime newDateTime);

private:
  double m_polarisHourAngle;

  SkyObject *m_polaris = { nullptr };
  std::unique_ptr<QPixmap> m_reticle;
};

