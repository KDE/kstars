/*
    SPDX-FileCopyrightText: 2010 Valery Kharitonov <kharvd@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
