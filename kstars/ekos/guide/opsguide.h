/*  Ekos Options
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef OpsGuide_H_
#define OpsGuide_H_

#include "ui_opsguide.h"
#include "guide.h"

class KConfigDialog;

namespace Ekos
{

/**
 * @class OpsGuide
 *
 * Enables the user to set guide options
 *
 * @author Jasem Mutlaq
 */
class OpsGuide : public QFrame, public Ui::OpsGuide
{
    Q_OBJECT

public:
    explicit OpsGuide();
    ~OpsGuide();

protected:
    void showEvent(QShowEvent *);

private slots:

    void slotApply();
    void slotLoadSettings(int guiderType);

signals:
    void guiderTypeChanged(int guiderType);

private:
    KConfigDialog *m_ConfigDialog;    
};

}

#endif

