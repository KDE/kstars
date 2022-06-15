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

dmsBox::dmsBox(QWidget *parent, Unit unit) : QLineEdit(parent), m_unit(unit)
{
    setMaxLength(14);
    setMaximumWidth(160);
    setUnits(unit);
}

void dmsBox::setPlaceholderText()
{
    if (m_unit == DEGREES)
        QLineEdit::setPlaceholderText("dd mm ss.s");
    else
        QLineEdit::setPlaceholderText("hh mm ss.s");
}

void dmsBox::setUnits(Unit unit)
{
    m_unit = unit;
    const bool t = (m_unit == Unit::DEGREES);

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
    setPlaceholderText();

    clear();
}

void dmsBox::show(const dms &d)
{
    if (m_unit == Unit::DEGREES)
    {
        double seconds = d.arcsec() + d.marcsec() / 1000.;
        setText(QString::asprintf("%02d %02d %05.2f", d.degree(), d.arcmin(), seconds));
    }
    else if (m_unit == Unit::HOURS)
    {
        double seconds = d.second() + d.msecond() / 1000.;
        setText(QString::asprintf("%02d %02d %05.2f", d.hour(), d.minute(), seconds));
    }
    else
    {
        Q_ASSERT(false); // Unhandled unit type
    }
}

dms dmsBox::createDms(bool *ok)
{
    dms dmsAngle;
    bool check;
    check = dmsAngle.setFromString(text(), (m_unit == Unit::DEGREES));
    if (ok)
        *ok = check; //ok might be a null pointer!

    return dmsAngle;
}
