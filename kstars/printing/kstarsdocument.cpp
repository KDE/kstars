#include "kstarsdocument.h"
#include "kstars.h"
#include "QPainter"
#include "QTextDocument"
#include "QTextDocumentWriter"
#include "QPrinter"

KStarsDocument::KStarsDocument()
{
    m_Document = new QTextDocument(KStars::Instance());
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
