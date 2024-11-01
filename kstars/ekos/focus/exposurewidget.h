/*
    SPDX-FileCopyrightText: 2024 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDoubleSpinBox>

// ExposureWidgetis a subclass of QDoubleSpinBox.
// It has been created for use with the Focus exposure field. This field has been extended to allow exposures
// as short as 0.00001s. To keep consistency with the exposure field in Capture only 3dps are displayed by
// default, e.g.:
// 2 is displayed as 2.000
// 2.1 is displayed as 2.100
// 2.001 is displayed as 2.001
// 2.0001 is displayed as 2.0001
// 2.00001 is displayed as 2.00001
//
// This header file is linked into QT Designer using the Promote widget approach so that ExposureWidget rather than
// QDoubleSpinBox is used.
//
class ExposureWidget : public QDoubleSpinBox
{
        Q_OBJECT

    public:

        /**
         * @brief Create an ExposureWidget
         * @param parent widget
         */
        ExposureWidget(QWidget *parent = 0);
        ~ExposureWidget();

        /**
         * @brief textFromValue displays textual information for the associated value
         * @param event
         */
        QString textFromValue(double value) const override;
};
