#ifndef KSTARSDOCUMENT_H
#define KSTARSDOCUMENT_H

#include "QRectF"

class QTextDocument;
class QString;
class QPainter;
class QPrinter;

class KStarsDocument
{
public:
    KStarsDocument();
    ~KStarsDocument();

    void clearContent();
    void drawContents(QPainter *p, const QRectF &rect = QRectF());
    void print(QPrinter *printer);

    bool writeOdt(const QString &fname);
    void writePsPdf(const QString &fname);
    void writeSvg(const QString &fname);

protected:
    QTextDocument *m_Document;
};

#endif // KSTARSDOCUMENT_H
