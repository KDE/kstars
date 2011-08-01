/***************************************************************************
                          finderchart.cpp  -  K Desktop Planetarium
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

#include "finderchart.h"

#include "QTextDocument"
#include "QTextDocumentFragment"
#include "QTextTable"
#include "kstars.h"
#include "loggingform.h"
#include "detailstable.h"

FinderChart::FinderChart()
{
    m_Document = new QTextDocument(KStars::Instance());
}

FinderChart::~FinderChart()
{
    if(m_Document)
    {
        delete m_Document;
    }
}

void FinderChart::insertTitle(const QString &title)
{
    QTextCursor cursor(m_Document);
    cursor.movePosition(QTextCursor::Start);

    QTextBlockFormat titleBlockFmt;
    titleBlockFmt.setAlignment(Qt::AlignCenter);
    QTextCharFormat titleCharFmt;
    QFont titleFont("Times", 20, QFont::Bold);
    titleCharFmt.setFont(titleFont);

    cursor.insertBlock(titleBlockFmt, titleCharFmt);
    cursor.insertText(title);

    cursor.insertBlock();
}

void FinderChart::insertLoggingForm(LoggingForm *log)
{
    QTextCursor cursor = m_Document->rootFrame()->lastCursorPosition();
    cursor.insertFragment(QTextDocumentFragment(log->getDocument()));

    cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
    cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
}

void FinderChart::insertImage(const QImage &img, const QString &description, bool descriptionBelow)
{
    QTextCursor cursor = m_Document->rootFrame()->lastCursorPosition();
    QTextCharFormat textFmt;
    QTextBlockFormat blockFmt;
    blockFmt.setAlignment(Qt::AlignHCenter);

    if(descriptionBelow)
    {
        cursor.insertBlock(blockFmt, textFmt);
        cursor.insertImage(img);
        cursor.insertBlock(blockFmt, textFmt);
        cursor.insertText(description);
    }

    else
    {
        cursor.insertBlock(blockFmt, textFmt);
        cursor.insertText(description);
        cursor.insertBlock(blockFmt, textFmt);
        cursor.insertImage(img);
    }

    cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
    cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
}

void FinderChart::insertDetailsTable(DetailsTable *table)
{
    QTextCursor cursor = m_Document->rootFrame()->lastCursorPosition();
    cursor.insertFragment(QTextDocumentFragment(table->getDocument()));

    cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
    cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
}

void FinderChart::clearContent()
{
    m_Document->clear();
}

void FinderChart::drawContents(QPainter *p, const QRectF &rect)
{
    m_Document->drawContents(p, rect);
}

void FinderChart::print(QPrinter *printer)
{
    m_Document->print(printer);
}
