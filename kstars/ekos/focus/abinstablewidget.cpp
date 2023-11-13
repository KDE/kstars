/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "abinstablewidget.h"

AbInsTableWidget::AbInsTableWidget(QWidget *parent) : QTableWidget(parent)
{
    this->setMouseTracking(true);
}

AbInsTableWidget::~AbInsTableWidget()
{
}

void AbInsTableWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    // signal when the mouse cursor leaves the custom table widget
    emit leaveTableEvent();
}
