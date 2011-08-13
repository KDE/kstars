/***************************************************************************
                          kstarsdocument.h  -  K Desktop Planetarium
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
