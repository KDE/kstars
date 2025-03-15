/*
    SPDX-FileCopyrightText: 2024 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_imagingplanneroptions.h"

#include <QWidget>
#include <QInputDialog>

class ImagingPlannerOptionsUI : public QFrame, public Ui::ImagingPlannerOptions
{
    Q_OBJECT

  public:
    explicit ImagingPlannerOptionsUI(QWidget *p = nullptr);
};

class ImagingPlannerOptions : public QDialog
{
    Q_OBJECT

  public:

    explicit ImagingPlannerOptions(QWidget *parent);
    virtual ~ImagingPlannerOptions() override = default;

  protected:
    void showEvent(QShowEvent *) override;

  private slots:
    void slotIndependentWindow(bool checked);
    void slotCenterOnSkyMap(bool checked);
    void slotStartSolvingImmediately(bool checked);

  private:
    ImagingPlannerOptionsUI *ui { nullptr };
};

