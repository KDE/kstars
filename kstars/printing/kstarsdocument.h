/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QRectF>

#include <memory>

class QPrinter;
class QString;
class QTextDocument;

/**
 * @class KStarsDocument
 * @brief Base class for KStars documents.
 * KStarsDocument is a base class for all KStars documents: finder charts, logging forms
 * etc.
 *
 * @author Rafał Kułaga
 */
class KStarsDocument
{
  public:
    /** Constructor */
    KStarsDocument();

    /** Clears contents of the document. */
    void clearContent();

    /**
     * @brief Print contents of the document.
     * @param printer Printer on which document will be printed.
     */
    void print(QPrinter *printer);

    /**
     * @brief Write contents of the document to Open Document Text file.
     * @param fname File name.
     * @return Returns true if write is successful.
     */
    bool writeOdt(const QString &fname);

    /**
     * @brief Write contents of the document to the Postscript/PDF file.
     * @param fname File name.
     */
    void writePsPdf(const QString &fname);

  protected:
    std::unique_ptr<QTextDocument> m_Document;
};
