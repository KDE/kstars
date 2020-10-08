/*  Astrometry.net Options Editor
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    Copyright (C) 2017 Robert Lancaster <rlancaste@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include "ui_opsastrometry.h"

#include <QWidget>

class KConfigDialog;

namespace Ekos
{
class Align;

class OpsAstrometry : public QWidget, public Ui::OpsAstrometry
{
    Q_OBJECT

  public:
    enum
    {
        SCALE_DEGREES,
        SCALE_ARCMINUTES,
        SCALE_ARCSECPERPIX
    };

    explicit OpsAstrometry(Align *parent);
    virtual ~OpsAstrometry() override = default;

  protected:
    void showEvent(QShowEvent *) override;

  private slots:
    void slotUpdatePosition();
    void slotUpdateScale();
    void slotApply();

  private:
    KConfigDialog *m_ConfigDialog { nullptr };
    Align *alignModule { nullptr };
};
}
