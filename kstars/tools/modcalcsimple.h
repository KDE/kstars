#pragma once

#include "dms.h"
#include "ui_modcalcsimple.h"

class QDateTime;

class GeoLocation;

class modCalcSimple : public QFrame, public Ui::modCalcSimple
{
    Q_OBJECT

  public:
    explicit modCalcSimple(QWidget *p);
    virtual ~modCalcSimple() override = default;

  public slots:
    void slotCompute1();
    void slotCompute2();
};
