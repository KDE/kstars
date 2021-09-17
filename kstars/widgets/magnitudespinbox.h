/*
    SPDX-FileCopyrightText: 2001 Heiko Evermann <heiko@evermann.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/** @class MagnitudeSpinBox
	*A special spinbox for entering magnitude values.
	*This class now inherits QDoubleSpinBox instead of QSpinBox
	*@short a custom spinbox for magnitude (float) values.
	*@author Heiko Evermann
	*@version 1.0
	*/
#pragma once

#include <QDoubleSpinBox>

class QWidget;

class MagnitudeSpinBox : public QDoubleSpinBox
{
  Q_OBJECT
  public:
    /** Default Constructor */
    explicit MagnitudeSpinBox(QWidget *parent = nullptr);

    /** Constructor.  Set minimum and maximum values for the spinbox. */
    MagnitudeSpinBox(double minValue, double maxValue, QWidget *parent = nullptr);
};
