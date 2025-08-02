/*
    SPDX-FileCopyrightText: 2024 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_sequenceeditorui.h"

#include <QDialog>

namespace Ekos
{

class Camera;

class SequenceEditor : public QDialog, public Ui::SequenceEditorUI
{
        Q_OBJECT

    public:

    SequenceEditor(QWidget *parent = nullptr);

    void showEvent(QShowEvent* event) override;

    bool loadSequenceQueue(const QString &fileURL, QString targetName = "");

    public slots:

    signals:

    private slots:

    private:
        QSharedPointer<Camera> m_camera;
        QVariantMap m_Settings;

        // Disable all the widgets that aren't used in stand-alone mode.
        void initStandAlone();

        void onStandAloneShow();
};

}
