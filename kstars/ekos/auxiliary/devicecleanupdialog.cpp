/*
    SPDX-FileCopyrightText: 2026 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "devicecleanupdialog.h"
#include "opticaltrainmanager.h"

#include <KLocalizedString>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QToolButton>
#include <QDialogButtonBox>
#include <QIcon>

namespace Ekos
{

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
DeviceCleanupDialog::DeviceCleanupDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(i18nc("@title:window", "Clean Up Unused Devices"));

    auto mainLayout = new QVBoxLayout(this);

    m_ExplainerLabel = new QLabel(i18n("These stored device names are not currently connected. "
                                       "Some may only be temporarily unavailable, so check before removing."), this);
    m_ExplainerLabel->setWordWrap(true);
    mainLayout->addWidget(m_ExplainerLabel);

    m_List = new QListWidget(this);
    m_List->setSelectionMode(QAbstractItemView::NoSelection);
    mainLayout->addWidget(m_List);

    m_EmptyLabel = new QLabel(i18n("All stored devices are currently connected."), this);
    m_EmptyLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_EmptyLabel);

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    resize(380, 320);

    refreshList();
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void DeviceCleanupDialog::refreshList()
{
    m_List->clear();

    const auto devices = OpticalTrainManager::Instance()->getUnusedDevices();
    m_ExplainerLabel->setVisible(!devices.isEmpty());
    m_EmptyLabel->setVisible(devices.isEmpty());
    m_List->setVisible(!devices.isEmpty());

    for (const auto &device : devices)
    {
        auto row = new QWidget(m_List);
        auto rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(4, 4, 4, 4);

        auto textLayout = new QVBoxLayout();
        auto nameLabel = new QLabel(device.displayName, row);
        auto subtitleLabel = new QLabel(device.subtitle, row);
        // Disabled state renders with the palette's muted text color, used here purely for styling.
        subtitleLabel->setEnabled(false);
        textLayout->addWidget(nameLabel);
        textLayout->addWidget(subtitleLabel);
        rowLayout->addLayout(textLayout, 1);

        auto trashButton = new QToolButton(row);
        trashButton->setIcon(QIcon::fromTheme("edit-delete"));
        trashButton->setToolTip(i18n("Remove this stored device reference"));
        rowLayout->addWidget(trashButton);

        auto item = new QListWidgetItem(m_List);
        item->setSizeHint(row->sizeHint());
        m_List->setItemWidget(item, row);

        connect(trashButton, &QToolButton::clicked, this, [this, device]()
        {
            OpticalTrainManager::Instance()->removeUnusedDevice(device);
            refreshList();
        });
    }
}

}
