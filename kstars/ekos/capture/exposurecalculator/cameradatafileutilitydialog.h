/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>

namespace Ui {
class CameraDataFileUtilityDialog;
}

class CameraDataFileUtilityDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CameraDataFileUtilityDialog(QWidget *parent = nullptr);
    ~CameraDataFileUtilityDialog();

private:
    Ui::CameraDataFileUtilityDialog *ui;
};

