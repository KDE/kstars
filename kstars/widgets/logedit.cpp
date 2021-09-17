/*
    SPDX-FileCopyrightText: 2005 Jason Harris and Jasem Mutlaq <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "logedit.h"

#include <QFrame>

LogEdit::LogEdit(QWidget *parent) : QTextEdit(parent)
{
    setFrameStyle(QFrame::StyledPanel);
    setFrameShadow(QFrame::Plain);
    setLineWidth(4);
}

void LogEdit::focusOutEvent(QFocusEvent *e)
{
    emit focusOut();
    QWidget::focusOutEvent(e);
}
