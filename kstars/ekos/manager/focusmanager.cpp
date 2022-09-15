/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikartech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "focusmanager.h"
#include "kstarsdata.h"
#include "Options.h"

namespace Ekos
{
FocusManager::FocusManager(QWidget * parent) : QWidget(parent)
{
    setupUi(this);
}

void FocusManager::updateCurrentHFR(double newHFR)
{
    currentHFR->setText(QString("%1").arg(newHFR, 0, 'f', 2) + " px");
    profilePlot->drawProfilePlot(newHFR);
}

void FocusManager::updateFocusDetailView()
{
    const int pos = focusDetailView->currentIndex();
    if (pos == 1 && focusStarPixmap.get() != nullptr)
    {
        focusStarView->setPixmap(focusStarPixmap.get()->scaled(focusDetailView->width(), focusDetailView->height(),
                                                               Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void FocusManager::updateFocusStarPixmap(QPixmap &starPixmap)
{
    if (starPixmap.isNull())
        return;

    focusStarPixmap.reset(new QPixmap(starPixmap));
    updateFocusDetailView();
}

void FocusManager::updateFocusStatus(Ekos::FocusState status)
{
    focusStatus->setFocusState(status);
}

void FocusManager::init()
{

    // focus details buttons
    connect(focusDetailNextButton, &QPushButton::clicked, [this]() {
        const int pos = focusDetailView->currentIndex();
        if (pos == 0 || (pos == 1 && focusStarPixmap.get() != nullptr))
            focusDetailView->setCurrentIndex(pos+1);
        else if (pos > 0)
            focusDetailView->setCurrentIndex(0);
        updateFocusDetailView();
    });
    connect(focusDetailPrevButton, &QPushButton::clicked, [this]() {
        const int pos = focusDetailView->currentIndex();
        if (pos == 0 && focusStarPixmap.get() != nullptr)
            focusDetailView->setCurrentIndex(pos+2);
        else if (pos == 0)
            focusDetailView->setCurrentIndex(pos+1);
        else if (pos > 0)
            focusDetailView->setCurrentIndex(pos-1);
        updateFocusDetailView();
    });
}

void FocusManager::reset()
{
    focusStatus->setFocusState(FOCUS_IDLE);
}

} // namespace Ekos
