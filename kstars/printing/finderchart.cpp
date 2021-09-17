/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "finderchart.h"

#include "detailstable.h"
#include "geolocation.h"
#include "kstarsdatetime.h"
#include "loggingform.h"

#include <QLocale>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextDocumentWriter>
#include <QTextTable>

FinderChart::FinderChart() : KStarsDocument()
{
}

void FinderChart::insertTitleSubtitle(const QString &title, const QString &subtitle)
{
    QTextCursor cursor(m_Document.get());
    cursor.movePosition(QTextCursor::Start);

    QTextBlockFormat titleBlockFmt;
    titleBlockFmt.setAlignment(Qt::AlignCenter);

    if (!title.isEmpty())
    {
        QTextCharFormat titleCharFmt;
        QFont titleFont("Times", 20, QFont::Bold);
        titleCharFmt.setFont(titleFont);

        cursor.insertBlock(titleBlockFmt, titleCharFmt);
        cursor.insertText(title);
    }

    if (!subtitle.isEmpty())
    {
        QTextCharFormat subtitleCharFmt;
        QFont subtitleFont("Times", 14);
        subtitleCharFmt.setFont(subtitleFont);

        cursor.insertBlock(titleBlockFmt, subtitleCharFmt);
        cursor.insertText(subtitle);

        cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
    }
}

void FinderChart::insertDescription(const QString &description)
{
    QTextCursor cursor = m_Document->rootFrame()->lastCursorPosition();

    QTextBlockFormat descrBlockFmt;
    descrBlockFmt.setAlignment(Qt::AlignJustify);
    QTextCharFormat descrCharFmt;
    QFont descrFont("Times", 10);
    descrCharFmt.setFont(descrFont);

    cursor.insertBlock(descrBlockFmt, descrCharFmt);
    cursor.insertText(description);

    cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
}

void FinderChart::insertGeoTimeInfo(const KStarsDateTime &ut, GeoLocation *geo)
{
    QTextCursor cursor = m_Document->rootFrame()->lastCursorPosition();

    QTextBlockFormat geoBlockFmt;
    geoBlockFmt.setAlignment(Qt::AlignLeft);
    QTextCharFormat geoCharFmt;
    QFont geoFont("Times", 10, QFont::Bold);
    geoCharFmt.setFont(geoFont);

    cursor.insertBlock(geoBlockFmt);
    cursor.insertText(i18n("Date, time and location: "), geoCharFmt);

    QString geoStr = geo->translatedName();
    if (!geo->translatedProvince().isEmpty())
    {
        if (!geoStr.isEmpty())
        {
            geoStr.append(", ");
        }
        geoStr.append(geo->translatedProvince());
    }
    if (!geo->translatedCountry().isEmpty())
    {
        if (!geoStr.isEmpty())
        {
            geoStr.append(", ");
        }
        geoStr.append(geo->translatedCountry());
    }

    geoFont.setBold(false);
    geoCharFmt.setFont(geoFont);
    //cursor.insertText(QLocale().toString(ut.dateTime()) + ", " + geoStr, geoCharFmt);
    cursor.insertText(QLocale().toString(ut) + ", " + geoStr, geoCharFmt);

    cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
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

    if (descriptionBelow)
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

void FinderChart::insertSectionTitle(const QString &title)
{
    QTextCursor cursor = m_Document->rootFrame()->lastCursorPosition();

    QTextBlockFormat titleBlockFmt;
    titleBlockFmt.setAlignment(Qt::AlignLeft);
    QTextCharFormat titleCharFmt;
    QFont titleFont("Times", 16, QFont::Bold);
    titleFont.setCapitalization(QFont::AllUppercase);
    titleCharFmt.setFont(titleFont);

    cursor.insertBlock(titleBlockFmt, titleCharFmt);
    cursor.insertText(title);
}
