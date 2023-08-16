/*
    SPDX-FileCopyrightText: 2023 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsimageoverlay.h"
#include <QFrame>
#include <kconfigdialog.h>

class KStars;

/**
 * @class OpsImageOverlay
 * The terrain page enables to user to manage the options for image overlays.
 */
class OpsImageOverlay : public QFrame, public Ui::OpsImageOverlay
{
        Q_OBJECT

    public:
        explicit OpsImageOverlay();
        virtual ~OpsImageOverlay() override = default;
        void syncOptions();
        QTableWidget *table();
        QGroupBox *tableTitleBox();
        QPlainTextEdit *statusDisplay();
        QPushButton *solvePushButton();
        QComboBox *solverProfile();

    private slots:
        void slotApply();

    private:
        KConfigDialog *m_ConfigDialog { nullptr };
};

