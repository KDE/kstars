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
    Q_PROPERTY(bool degType READ degType WRITE setDegType)

  public:
    /**
     * Constructor for the dmsBox object.
     *
     * @param parent Pointer to the parent QWidget
     * @param deg If true use deg/arcmin/arcsec otherwise use hours/min/sec.
     */
    explicit dmsBox(QWidget *parent, bool deg = true);

    virtual ~dmsBox() override = default;

    /**
     * Display an angle using Hours/Min/Sec.
     *
     * @p t the dms object which is to be displayed
     */
    void showInHours(dms t);

    /**
     * Display an angle using Hours/Min/Sec. This behaves just
     * like the above function. It differs only in the data type of
     * the argument.
     *
     * @p t pointer to the dms object which is to be displayed
     */
    void showInHours(const dms *t);

    /**
     * Display an angle using Deg/Arcmin/Arcsec.
     *
     * @p t the dms object which is to be displayed
     */
    void showInDegrees(dms t);

    /**
     * Display an angle using Deg/Arcmin/Arcsec.  This behaves just
     * like the above function.  It differs only in the data type of
     * the argument.
     *
     * @p t pointer to the dms object which is to be displayed
     */
    void showInDegrees(const dms *t);

    /**
     * Display an angle.  Simply calls showInDegrees(t) or
     * showInHours(t) depending on the value of deg.
     *
     * @param t the dms object which is to be displayed.
     * @param deg if true, display Deg/Arcmin/Arcsec; otherwise
     * display Hours/Min/Sec.
     */
    void show(dms t, bool deg = true);

    /**
     * Display an angle.  Simply calls showInDegrees(t) or
     * showInHours(t) depending on the value of deg.
     * This behaves essentially like the above function.  It
     * differs only in the data type of its argument.
     *
     * @param t the dms object which is to be displayed.
     * @param deg if true, display Deg/Arcmin/Arcsec; otherwise
     * display Hours/Min/Sec.
     */
    void show(const dms *t, bool deg = true);

    /**
     * Simply display a string.
     *
     * @note JH: Why don't we just use QLineEdit::setText() instead?
     * @param s the string to display (it need not be a valid angle value).
     */
    void setDMS(const QString &s) { setText(s); }

    /**
     * Parse the text in the dmsBox as an angle.  The text may be an integer
     * or double value, or it may be a triplet of integer values (separated by spaces
     * or colons) representing deg/hrs, min, sec.  It is also possible to have two
     * fields.  In this case, if the second field is a double, it is converted
     * to decimal min and double sec.
     *
     * @param deg if true use deg/arcmin/arcsec; otherwise
     *           use hours/min/sec.
     * @param ok set to true if a dms object was successfully created.
     * @return a dms object constructed from the fields of the dmsbox
     */
    dms createDms(bool deg = true, bool *ok = nullptr);

    /** @return a boolean indicating if object contains degrees or hours */
    bool degType(void) const { return deg; }

    /**
     * @short set the dmsBox to Degrees or Hours
     *
     * @param t if true, the box expects angle values in degrees; otherwise
     * it expects values in hours
     */
    void setDegType(bool t);

    /** Clears the QLineEdit */
    void clearFields(void) { setDMS(QString()); }

    inline bool isEmpty() { return EmptyFlag; }

  protected:
    void focusInEvent(QFocusEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;

  private slots:
    void slotTextChanged(const QString &t);

  private:
    void setEmptyText();

    bool deg { false };
    bool EmptyFlag { false };
    dms degValue;
};
