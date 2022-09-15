/*  Progress and status of the meridian flip
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "meridianflipstatuswidget.h"

namespace Ekos
{

MeridianFlipStatusWidget::MeridianFlipStatusWidget(QWidget *parent)
    : QWidget{parent}
{
    setupUi(this);
    statusText->setText("");
}

void MeridianFlipStatusWidget::setStatus(QString text)
{
    statusText->setText(text);
}

QString MeridianFlipStatusWidget::getStatus()
{
    return statusText->text();
}
} // namespace
