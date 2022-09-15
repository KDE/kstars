/*  Progress and status of the meridian flip
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_meridianflipstatuswidget.h"
#include "ekos/ekos.h"

#include <KLed>
#include <QWidget>

namespace Ekos
{

class MeridianFlipStatusWidget : public QWidget, Ui::MeridianFlipStatusWidget
{
    Q_OBJECT
public:
    MeridianFlipStatusWidget(QWidget *parent = nullptr);

    /**
     * @brief Change the status text and LED color
     */
    void setStatus(QString text);

    /**
     * @brief retrieve the currently displayed status
     */
    QString getStatus();

signals:

};

} // namespace
