/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "loggingform.h"

#include <KLocalizedString>

#include <QTextDocument>
#include <QTextTable>

void LoggingForm::createFinderChartLogger()
{
    QTextCursor cursor(m_Document.get());
    cursor.movePosition(QTextCursor::Start);

    QTextTableFormat tableFormat;
    tableFormat.setAlignment(Qt::AlignHCenter);
    tableFormat.setBorder(2);
    tableFormat.setCellPadding(2);
    tableFormat.setCellSpacing(4);

    QTextCharFormat fieldCharFmt;
    QFont font("Times", 10);
    fieldCharFmt.setFont(font);

    QVector<QTextLength> constraints;
    constraints << QTextLength(QTextLength::PercentageLength, 25) << QTextLength(QTextLength::PercentageLength, 25)
                << QTextLength(QTextLength::PercentageLength, 25) << QTextLength(QTextLength::PercentageLength, 25);
    tableFormat.setColumnWidthConstraints(constraints);

    QTextTable *table = cursor.insertTable(5, 4, tableFormat);
    table->mergeCells(0, 0, 1, 4);
    table->cellAt(0, 0).firstCursorPosition().insertText(i18n("Observer:"), fieldCharFmt);

    table->mergeCells(1, 0, 1, 2);
    table->cellAt(1, 0).firstCursorPosition().insertText(i18n("Date:"), fieldCharFmt);
    table->mergeCells(1, 2, 1, 2);
    table->cellAt(1, 2).firstCursorPosition().insertText(i18n("Time:"), fieldCharFmt);

    table->mergeCells(2, 0, 1, 2);
    table->cellAt(2, 0).firstCursorPosition().insertText(i18n("Site:"), fieldCharFmt);
    table->cellAt(2, 2).firstCursorPosition().insertText(i18n("Seeing:"), fieldCharFmt);
    table->cellAt(2, 3).firstCursorPosition().insertText(i18n("Trans:"), fieldCharFmt);

    table->mergeCells(3, 0, 1, 4);
    table->cellAt(3, 0).firstCursorPosition().insertText(i18n("Telescope:"), fieldCharFmt);

    table->mergeCells(4, 0, 1, 3);
    table->cellAt(4, 0).firstCursorPosition().insertText(i18n("Eyepiece:"), fieldCharFmt);
    table->cellAt(4, 3).firstCursorPosition().insertText(i18n("Power:"), fieldCharFmt);
}

QTextDocument *LoggingForm::getDocument()
{
    return m_Document.get();
}
