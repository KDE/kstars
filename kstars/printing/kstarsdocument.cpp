/***************************************************************************
                          kstarsdocument.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Aug 10 2011
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

#include "kstarsdocument.h"
#include "kstars.h"
#include "QPainter"
#include "QTextDocument"
#include "QTextDocumentWriter"
#include "QPrinter"

KStarsDocument::KStarsDocument()
{
    m_Document = new QTextDocument();
}

KStarsDocument::~KStarsDocument()
{
    if(m_Document)
    {
        delete m_Document;
    }
}

void KStarsDocument::clearContent()
{
    m_Document->clear();
}

void KStarsDocument::drawContents(QPainter *p, const QRectF &rect)
{
    m_Document->drawContents(p, rect);
}

void KStarsDocument::print(QPrinter *printer)
{
    m_Document->print(printer);
}

bool KStarsDocument::writeOdt(const QString &fname)
{
    QTextDocumentWriter writer(fname);
    return writer.write(m_Document);
}

void KStarsDocument::writePsPdf(const QString &fname)
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFileName(fname);
    printer.setOutputFormat(fname.endsWith(".pdf") ? QPrinter::PdfFormat : QPrinter::PostScriptFormat);
    m_Document->print(&printer);
}
