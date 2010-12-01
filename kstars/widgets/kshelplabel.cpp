#include "kshelplabel.h"
#include <ktoolinvocation.h>
#include <QtGui/QMessageBox>

KSHelpLabel::KSHelpLabel(const QString& text, const QString& anchor,
			 QWidget *parent) : QLabel(parent), m_anchor(anchor)
{
    setText(text);
    updateText();
    connect(this, SIGNAL(linkActivated(QString)), SLOT(slotShowDefinition(QString)));
}

KSHelpLabel::KSHelpLabel(QWidget *parent) : QLabel(parent)
{
    connect(this, SIGNAL(linkActivated(QString)), SLOT(slotShowDefinition(QString)));
}

void KSHelpLabel::setAnchor(const QString& anchor) {
    m_anchor = anchor;
    updateText();
}

void KSHelpLabel::updateText() {
    QLabel::setText("<a href=\"ai-" + m_anchor + "\">" + text() + "</a>");
}

void KSHelpLabel::slotShowDefinition(const QString & term) {
    KToolInvocation::invokeHelp(term);
}

void KSHelpLabel::setText(const QString& txt) {
    m_cleanText = txt;
    QLabel::setText("<a href=\"ai-" + m_anchor + "\">" + m_cleanText + "</a>");
}