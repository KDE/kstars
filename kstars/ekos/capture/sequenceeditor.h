/*
    SPDX-FileCopyrightText: 2024 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_sequenceeditorui.h"

#include <QDialog>

namespace Ekos
{

class Capture;

class SequenceEditor : public QDialog, public Ui::SequenceEditorUI
{
        Q_OBJECT

    public:

    SequenceEditor(QWidget *parent = nullptr);

    void showEvent(QShowEvent* event) override;

    public slots:

    signals:

    private slots:

    private:
        QSharedPointer<Capture> m_capture;
};

}
