/***************************************************************************
            kshelplabel.h - Help label used to document astronomical terms
                             -------------------
    begin                : Wed 1 Dec 2010
    copyright            : (C) 2010 by Valery Kharitonov
    email                : kharvd@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KSHELPLABEL_H
#define KSHELPLABEL_H

#include <QLabel>

/** Label for displaying links to AstroInfo project
 * @author Valery Kharitonov
 */
class KSHelpLabel : public QLabel
{
    Q_OBJECT
    Q_PROPERTY(QString anchor READ anchor WRITE setAnchor)
    Q_PROPERTY(QString text READ text WRITE setText)

  public:
    /**
         * Constructor. Creates clear label
         */
    explicit KSHelpLabel(QWidget *parent = nullptr);

    /**
         * Constructor. Creates label with a text and help anchor.
         * @param text Text of the label
         * @param anchor Name of the section in the AstroInfo project (without 'ai-')
         * @param parent Parent widget
         */
    KSHelpLabel(const QString &text, const QString &anchor, QWidget *parent = nullptr);

    QString text() const { return m_cleanText; }
    void setText(const QString &text);

    void setAnchor(const QString &anchor);
    QString anchor() const { return m_anchor; }

  private slots:
    /** Open AstroInfo definition of the terms
         * @param term jargon term */
    void slotShowDefinition(const QString &term);

  private:
    /**
         * Updates text with the new anchor
         */
    void updateText();

    /**
         * Anchor in AstroInfo project
         */
    QString m_anchor;

    /**
         * String without markup
         */
    QString m_cleanText;
};

#endif // KSHELPLABEL_H
