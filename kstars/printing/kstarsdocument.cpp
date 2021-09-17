/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
