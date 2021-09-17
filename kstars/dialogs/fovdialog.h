/*
    SPDX-FileCopyrightText: 2003 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef FOVDIALOG_H_
#define FOVDIALOG_H_

#include <QPaintEvent>
#include <QDialog>
#include <QDoubleSpinBox>

#include "fov.h"
#include "ui_fovdialog.h"
#include "ui_newfov.h"

class FOVDialogUI : public QFrame, public Ui::FOVDialog
{
        Q_OBJECT
    public:
        explicit FOVDialogUI(QWidget *parent = nullptr);
};

class NewFOVUI : public QFrame, public Ui::NewFOV
{
        Q_OBJECT
    public:
        explicit NewFOVUI(QWidget *parent = nullptr);
};

/** @class FOVDialog
 *  FOVDialog is dialog to select a Field-of-View indicator (or create a new one)
    *@author Jason Harris
    *@version 1.0
    */
class FOVDialog : public QDialog
{
        Q_OBJECT
    public:
        explicit FOVDialog(QWidget *parent = nullptr);
        ~FOVDialog() override;
    private slots:
        void slotNewFOV();
        void slotEditFOV();
        void slotRemoveFOV();
        void slotSelect(int);

    private:
        /** Add new widget to list box */
        QListWidgetItem *addListWidget(FOV *f);

        unsigned int currentItem() const;
        FOVDialogUI *fov;
        static int fovID;
};

/** @class NewFOV
        * Dialog for defining a new FOV symbol
	*@author Jason Harris
	*@version 1.0
	*/
class NewFOV : public QDialog
{
        Q_OBJECT
    public:
        /** Create new dialog
             * @param parent parent widget
             * @param fov widget to copy data from. If it's empty will create empty one.
             */
        explicit NewFOV(QWidget *parent = nullptr, const FOV *fov = nullptr);
        ~NewFOV() override = default;
        /** Return reference to FOV. */
        const FOV &getFOV() const
        {
            return f;
        }

    public slots:
        void slotBinocularFOVDistanceChanged(int index);
        void slotUpdateFOV();
        void slotComputeFOV();
        void slotEyepieceAFOVChanged(int index);
        void slotComputeTelescopeFL();
        void slotDetectFromINDI();

    private:
        FOV f;
        NewFOVUI *ui;
        QPushButton *okB;
};

/**
 *@class TelescopeFL
 *Dialog for calculating telescope focal length from f-number and diameter
 *@author Akarsh Simha
 *@version 1.0
 */

class TelescopeFL : public QDialog
{
        Q_OBJECT
    public:
        /**
             * Create a telescope focal length dialog
             * @param parent parent widget
             */
        explicit TelescopeFL(QWidget *parent = nullptr);

        ~TelescopeFL() override = default;

        /**
             * Compute and return the focal length in mm
             * @return focal length in mm
             */
        double computeFL() const;

    private:
        QDoubleSpinBox *aperture, *fNumber;
        QComboBox *apertureUnit;
};

#endif
