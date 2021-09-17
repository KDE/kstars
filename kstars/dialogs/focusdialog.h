/*
    SPDX-FileCopyrightText: 2002 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_focusdialog.h"

#include <QDialog>

class QPushButton;

class SkyPoint;

class FocusDialogUI : public QFrame, public Ui::FocusDialog
{
        Q_OBJECT

    public:
        explicit FocusDialogUI(QWidget *parent = nullptr);
};

/**
 * @class FocusDialog
 * @short A small dialog for setting the focus coordinates manually.
 *
 * @author Jason Harris
 * @version 1.0
 */
class FocusDialog : public QDialog
{
        Q_OBJECT

    public:
        /** Constructor. */
        FocusDialog();

        /** @return pointer to the SkyPoint described by the entered RA, Dec */
        inline SkyPoint *point()
        {
            return Point;
        }

        /** @return suggested size of focus window. */
        QSize sizeHint() const override;

        /** @return whether user set the AltAz coords */
        inline bool usedAltAz() const
        {
            return UsedAltAz;
        }

        /** Show the Az/Alt page instead of the RA/Dec page. */
        void activateAzAltPage() const;

    public slots:
        /** If text has been entered in both KLineEdits, enable the Ok button. */
        void checkLineEdits();

        /**
         * Attempt to interpret the text in the KLineEdits as Ra and Dec values.
         * If the point is validated, close the window.
         */
        void validatePoint();

    private:
        SkyPoint *Point { nullptr };
        FocusDialogUI *fd { nullptr };
        bool UsedAltAz { false };
        QPushButton *okB { nullptr };
        bool UseJNow { true };
};
