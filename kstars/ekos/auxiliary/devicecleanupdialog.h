/*
    SPDX-FileCopyrightText: 2026 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>

class QListWidget;
class QLabel;

namespace Ekos
{

/**
 * @class DeviceCleanupDialog
 * @brief Lists stored device references (optical train fields and scope catalog
 * entries) that are neither currently connected nor used by any saved profile,
 * and lets the user clear them one at a time.
 */
class DeviceCleanupDialog : public QDialog
{
        Q_OBJECT

    public:
        explicit DeviceCleanupDialog(QWidget *parent = nullptr);

    private:
        void refreshList();

        QListWidget *m_List { nullptr };
        QLabel *m_ExplainerLabel { nullptr };
        QLabel *m_EmptyLabel { nullptr };
};

}
