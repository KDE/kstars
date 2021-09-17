/*
    SPDX-FileCopyrightText: 2015-2017 Pavel Mraz

    SPDX-FileCopyrightText: 2017 Jasem Mutlaq

    SPDX-License-Identifier: GPL-2.0-or-later
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
  void paintEvent(QPaintEvent *) override;

private slots:
  void onTimeUpdated(QDateTime newDateTime);

private:
  double m_polarisHourAngle;

  SkyObject *m_polaris = { nullptr };
  std::unique_ptr<QPixmap> m_reticle12;
  std::unique_ptr<QPixmap> m_reticle24;
};

