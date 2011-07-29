/***************************************************************************
                          loggingform.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Jul 20 2011
    copyright            : (C) 2011 by Rafał Kułaga
    email                : rl.kulaga@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "loggingform.h"

#include "QTextDocument"
#include "QTextTable"
#include "kstars.h"

LoggingForm::LoggingForm()
{
    m_Document = new QTextDocument(KStars::Instance());
}

LoggingForm::~LoggingForm()
{
    if(m_Document)
    {
        delete m_Document;
    }
}

void LoggingForm::createFinderChartLogger()
{
    QTextCursor cursor(m_Document);
    cursor.movePosition(QTextCursor::Start);

    QTextTableFormat tableFormat;
    tableFormat.setAlignment(Qt::AlignHCenter);
    tableFormat.setBorder(2);
    tableFormat.setCellPadding(2);
    tableFormat.setCellSpacing(4);

    QVector<QTextLength> constraints;
    constraints << QTextLength(QTextLength::PercentageLength, 25)
                << QTextLength(QTextLength::PercentageLength, 25)
                << QTextLength(QTextLength::PercentageLength, 25)
                << QTextLength(QTextLength::PercentageLength, 25);
    tableFormat.setColumnWidthConstraints(constraints);

    QTextTable *table = cursor.insertTable(5, 4, tableFormat);
    table->mergeCells(0, 0, 1, 4);
    table->cellAt(0, 0).firstCursorPosition().insertText(i18n("Observer:"));

    table->mergeCells(1, 0, 1, 2);
    table->cellAt(1, 0).firstCursorPosition().insertText(i18n("Date:"));
    table->mergeCells(1, 2, 1, 2);
    table->cellAt(1, 2).firstCursorPosition().insertText(i18n("Time:"));

    table->mergeCells(2, 0, 1, 2);
    table->cellAt(2, 0).firstCursorPosition().insertText(i18n("Site:"));
    table->cellAt(2, 2).firstCursorPosition().insertText(i18n("Seeing:"));
    table->cellAt(2, 3).firstCursorPosition().insertText(i18n("Trans:"));

    table->mergeCells(3, 0, 1, 4);
    table->cellAt(3, 0).firstCursorPosition().insertText(i18n("Telescope:"));

    table->mergeCells(4, 0, 1, 3);
    table->cellAt(4, 0).firstCursorPosition().insertText(i18n("Eyepiece:"));
    table->cellAt(4, 3).firstCursorPosition().insertText(i18n("Power:"));
}

QTextDocument* LoggingForm::getDocument()
{
    return m_Document;
}


