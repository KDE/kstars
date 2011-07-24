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
#include "kstars.h"
#include "loggingform.h"

FinderChart::FinderChart()
    : m_Log(0)
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
    QTextCursor cursor(m_Document);

    cursor.insertFragment(QTextDocumentFragment(log->getDocument()));
}

void FinderChart::insertFovImage(const QImage &img, const QString &description)
{

}

void FinderChart::insertDetailsTable(SkyObject *obj, bool general, bool position)
{

}

void FinderChart::insertLegend(const QImage &img)
{

}

void FinderChart::insertImage(const QImage &img, const QString &description)
{

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
