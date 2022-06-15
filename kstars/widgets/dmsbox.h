/*
    SPDX-FileCopyrightText: 2001-2002 Pablo de Vicente <vicente@oan.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "dms.h"

#include <QFocusEvent>
#include <QLineEdit>

/**
 * @class dmsBox
 *
 * A QLineEdit which is capable of displaying and parsing angle values
 * flexibly and robustly.  Angle values can be displayed and parsed as
 * Degrees or Hours.  When displaying a value, it uses a space-delimited
 * triplet of integers representing the degrees, arcminutes, and arcseconds
 * of the angle (or hours, minutes, seconds).  For example, "-34 45 57".
 * When parsing a value input by the user, it can also understand
 * a number of other formats:
 * @li colon-delimited fields ("-34:45:57")
 * @li one or two fields ("-35"; "-34 46")
 * @li fields with unit-labels ("-34d 45m 57s")
 * @li floating-point numbers ("-34.76583")
 *
 * @note Inherits QLineEdit.
 * @author Pablo de Vicente
 * @version 1.0
 */
class dmsBox : public QLineEdit
{
        Q_OBJECT
        Q_PROPERTY(Unit units READ getUnits WRITE setUnits)

    public:

        typedef enum
        {
            HOURS,
            DEGREES
        } Unit;
        /**
         * Constructor for the dmsBox object.
         *
         * @param parent Pointer to the parent QWidget
         * @param unit Units to use (Degree/Arcmin/Arcsec or Hour/Min/Sec)
         */
        explicit dmsBox(QWidget *parent, Unit unit);

        /**
         * Deprecated delegating constructor for backwards compatibility
         *
         * @param parent Pointer to the parent QWidget
         * @param isDegree If true, use Unit::DEGREES; if false, use Unit::HOURS
         */
        explicit dmsBox(QWidget *parent, bool isDegree = true)
            : dmsBox(parent, isDegree ? Unit::DEGREES : Unit::HOURS) {}

        virtual ~dmsBox() override = default;

        /**
         * Display an angle.
         *
         * @param d the dms object which is to be displayed.
         */
        void show(const dms &d);

        /**
         * Display an angle.This behaves essentially like the above
         * function.  It differs only in the data type of its argument.
         *
         * @param t the dms object which is to be displayed.
         */
        inline void show(const dms *t)
        {
            show(*t);
        }

        /**
         * Parse the text in the dmsBox as an angle. The text may be an integer
         * or double value, or it may be a triplet of integer values (separated by spaces
         * or colons) representing deg/hrs, min, sec.  It is also possible to have two
         * fields.  In this case, if the second field is a double, it is converted
         * to decimal min and double sec.
         *
         * @param ok set to true if a dms object was successfully created.
         * @return a dms object constructed from the fields of the dmsbox
         */
        dms createDms(bool *ok = nullptr);

        /**
         * @return the unit being used (DEGREES or HOURS)
         */
        inline Unit getUnits() const
        {
            return m_unit;
        }

        /**
         * @short set the dmsBox to Degrees or Hours
         *
         * @param unit If Unit::DEGREES, then the display and
         * interpretation of text is in degrees, if Unit::HOURS then in
         * hours.
         */
        void setUnits(Unit unit);

        /** Clears the QLineEdit */
        inline void clearFields(void)
        {
            setText(QString());
        }

        inline bool isEmpty() const
        {
            return text().isEmpty();
        }

    private:

        void setPlaceholderText();

        Unit m_unit;
};
