/*
    SPDX-FileCopyrightText: 2001-2002 Pablo de Vicente <vicente@oan.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dmsbox.h"

#include <QApplication>
#include <QFocusEvent>
#include <QRegExp>

#include <KLocalizedString>

#include <QDebug>

#include <cstdlib>

dmsBox::dmsBox(QWidget *parent, bool dg) : QLineEdit(parent), EmptyFlag(true)
{
    setMaxLength(14);
    setMaximumWidth(160);
    setDegType(dg);

    connect(this, SIGNAL(textChanged(QString)), this, SLOT(slotTextChanged(QString)));
}

void dmsBox::setEmptyText()
{
    //Set the text color to the average between
    //QColorGroup::Text and QColorGroup::Base
    QPalette p = QApplication::palette();
    QColor txc = p.color(QPalette::Active, QPalette::Text);
    QColor bgc = p.color(QPalette::Active, QPalette::Base);
    int r((txc.red() + bgc.red()) / 2);
    int g((txc.green() + bgc.green()) / 2);
    int b((txc.blue() + bgc.blue()) / 2);

    p.setColor(QPalette::Active, QPalette::Text, QColor(r, g, b));
    p.setColor(QPalette::Inactive, QPalette::Text, QColor(r, g, b));
    setPalette(p);

    if (degType())
        setText("dd mm ss.s");
    else
        setText("hh mm ss.s");

    EmptyFlag = true;
}

void dmsBox::focusInEvent(QFocusEvent *e)
{
    QLineEdit::focusInEvent(e);

    if (EmptyFlag)
    {
        clear();
        setPalette(QApplication::palette());
        EmptyFlag = false;
    }
}

void dmsBox::focusOutEvent(QFocusEvent *e)
{
    QLineEdit::focusOutEvent(e);

    if (text().isEmpty())
    {
        setEmptyText();
    }
}

void dmsBox::slotTextChanged(const QString &t)
{
    if (!hasFocus())
    {
        if (EmptyFlag && !t.isEmpty())
        {
            EmptyFlag = false;
        }

        if (!EmptyFlag && t.isEmpty())
        {
            setEmptyText();
        }
    }
}

void dmsBox::setDegType(bool t)
{
    deg = t;

    QString sTip = (t ? i18n("Angle value in degrees.") : i18n("Angle value in hours."));
    QString sWhatsThis;

    if (isReadOnly())
    {
        if (t)
        {
            sWhatsThis = i18n("This box displays an angle in degrees. "
                              "The three numbers displayed are the angle's "
                              "degrees, arcminutes, and arcseconds.");
        }
        else
        {
            sWhatsThis = i18n("This box displays an angle in hours. "
                              "The three numbers displayed are the angle's "
                              "hours, minutes, and seconds.");
        }
    }
    else
    {
        if (t)
        {
            sTip += i18n("  You may enter a simple integer, or a floating-point value, "
                         "or space- or colon-delimited values specifying "
                         "degrees, arcminutes and arcseconds");

            sWhatsThis = i18n("Enter an angle value in degrees.  The angle can be expressed "
                              "as a simple integer (\"12\"), a floating-point value "
                              "(\"12.33\"), or as space- or colon-delimited "
                              "values specifying degrees, arcminutes and arcseconds (\"12:20\", \"12:20:00\", "
                              "\"12 20\", \"12 20 00.0\", etc.).");
        }
        else
        {
            sTip += i18n("  You may enter a simple integer, or a floating-point value, "
                         "or space- or colon-delimited values specifying "
                         "hours, minutes and seconds");

            sWhatsThis = i18n("Enter an angle value in hours.  The angle can be expressed "
                              "as a simple integer (\"12\"), a floating-point value "
                              "(\"12.33\"), or as space- or colon-delimited "
                              "values specifying hours, minutes and seconds (\"12:20\", \"12:20:00\", "
                              "\"12 20\", \"12 20 00.0\", etc.).");
        }
    }

    setToolTip(sTip);
    setWhatsThis(sWhatsThis);

    clear();
    EmptyFlag = false;
    setEmptyText();
}

void dmsBox::showInDegrees(const dms *d)
{
    showInDegrees(dms(*d));
}
void dmsBox::showInDegrees(dms d)
{
    double seconds = d.arcsec() + d.marcsec() / 1000.;
    setDMS(QString::asprintf("%02d %02d %05.2f", d.degree(), d.arcmin(), seconds));
}

void dmsBox::showInHours(const dms *d)
{
    showInHours(dms(*d));
}
void dmsBox::showInHours(dms d)
{
    double seconds = d.second() + d.msecond() / 1000.;
    setDMS(QString::asprintf("%02d %02d %05.2f", d.hour(), d.minute(), seconds));
}

void dmsBox::show(const dms *d, bool deg)
{
    show(dms(*d), deg);
}
void dmsBox::show(dms d, bool deg)
{
    if (deg)
        showInDegrees(d);
    else
        showInHours(d);
}

dms dmsBox::createDms(bool deg, bool *ok)
{
    dms dmsAngle(0.0); // FIXME: Should we change this to NaN?
    bool check;
    check = dmsAngle.setFromString(text(), deg);
    if (ok)
        *ok = check; //ok might be a null pointer!

    return dmsAngle;
}
