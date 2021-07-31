/***************************************************************************
            kshelplabel.cpp - Help label used to document astronomical terms
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

#include "kshelplabel.h"
#include "Options.h"
#include <KHelpClient>
#include <QMessageBox>

KSHelpLabel::KSHelpLabel(const QString &text, const QString &anchor, QWidget *parent) : QLabel(parent), m_anchor(anchor)
{
    setText(text);
    updateText();
    connect(this, SIGNAL(linkActivated(QString)), SLOT(slotShowDefinition(QString)));
}

KSHelpLabel::KSHelpLabel(QWidget *parent) : QLabel(parent)
{
    connect(this, SIGNAL(linkActivated(QString)), SLOT(slotShowDefinition(QString)));
}

void KSHelpLabel::setAnchor(const QString &anchor)
{
    m_anchor = anchor;
    updateText();
}

void KSHelpLabel::updateText()
{
    //TODO This function probably needs to be removed since the theme setting stakes care already of link colors
    /*
    QString linkcolor =
        (Options::darkAppColors() ?
             "red" :
             "blue"); // In night colors mode, use red links because blue links are black through a red filter.
    QLabel::setText("<a href=\"ai-" + m_anchor + "\" style=\"color: " + linkcolor + "\" >" + text() + "</a>");
    */
    QLabel::setText("<a href=\"ai-" + m_anchor + "\">" + text() + "</a>");
}

void KSHelpLabel::slotShowDefinition(const QString &term)
{
    KHelpClient::invokeHelp(term);
}

void KSHelpLabel::setText(const QString &txt)
{
    m_cleanText = txt;
    updateText();
}
