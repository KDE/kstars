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

/**
  * \class KStarsDocument
  * \brief Base class for KStars documents.
  * KStarsDocument is a base class for all KStars documents: finder charts, logging forms
  * etc.
  * \author Rafał Kułaga
  */
class KStarsDocument
{
public:
    /**
      * \brief Constructor.
      */
    KStarsDocument();

    /**
      * \brief Destructor.
      */
    ~KStarsDocument();

    /**
      * \brief Clears contents of the document.
      */
    void clearContent();

    /**
      * \brief Print contents of the document.
      * \param printer Printer on which document will be printed.
      */
    void print(QPrinter *printer);

    /**
      * \brief Write contents of the document to Open Document Text file.
      * \param fname File name.
      * \return Returns true if write is successful.
      */
    bool writeOdt(const QString &fname);

    /**
      * \brief Write contents of the document to the Postscript/PDF file.
      * \param fname File name.
      */
    void writePsPdf(const QString &fname);

protected:
    QTextDocument *m_Document;
};

#endif // KSTARSDOCUMENT_H
