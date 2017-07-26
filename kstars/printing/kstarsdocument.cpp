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

#include <QPrinter>
#include <QTextDocument>
#include <QTextDocumentWriter>

KStarsDocument::KStarsDocument()
{
    m_Document.reset(new QTextDocument());
}

void KStarsDocument::clearContent()
{
    m_Document->clear();
}

void KStarsDocument::print(QPrinter *printer)
{
    m_Document->print(printer);
}

bool KStarsDocument::writeOdt(const QString &fname)
{
    QTextDocumentWriter writer(fname);

    return writer.write(m_Document.get());
}

void KStarsDocument::writePsPdf(const QString &fname)
{
    QPrinter printer(QPrinter::HighResolution);

    printer.setOutputFileName(fname);
    printer.setOutputFormat(fname.endsWith(QLatin1String(".pdf")) ? QPrinter::PdfFormat : QPrinter::NativeFormat);
    m_Document->print(&printer);
}
