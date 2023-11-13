/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QTableWidget>

// AbInsTableWidget is a subclass of table widget.
// It has been created for use in AberrationInspector in order to be able to access leaveEvent which is not
// possible in the QTableWidget implementation.
//
// This header file is linked into QT Designer using the Promote widget approach so that AbInsTableWidget rather than
// QTableWidget is used.
//
class AbInsTableWidget : public QTableWidget
{
        Q_OBJECT

    public:

        /**
         * @brief Create an AbInsTableWidget
         * @param parent widget
         */
        AbInsTableWidget(QWidget *parent = 0);
        ~AbInsTableWidget();

        /**
         * @brief event when mouse leaves the boundary of the widget
         * @param event
         */
        void leaveEvent(QEvent * event) override;

    signals:
        /**
         * @brief signal mouse left the widget boundary
         */
        void leaveTableEvent();
};
