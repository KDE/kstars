/*
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "summaryfitsview.h"
#include "QGraphicsOpacityEffect"


SummaryFITSView::SummaryFITSView(QWidget *parent): FITSView(parent, FITS_NORMAL, FITS_NONE)
{
    processInfoWidget = new QWidget(this);
    processInfoWidget->setVisible(m_showProcessInfo);
    processInfoWidget->setGraphicsEffect(new QGraphicsOpacityEffect(this));

    processInfoWidget->raise();
}

void SummaryFITSView::createFloatingToolBar()
{
    FITSView::createFloatingToolBar();

    floatingToolBar->addSeparator();
    toggleProcessInfoAction = floatingToolBar->addAction(QIcon::fromTheme("document-properties"),
                                                         i18n("Show Capture Process Information"),
                                                         this, SLOT(toggleShowProcessInfo()));
    toggleProcessInfoAction->setCheckable(true);
}

void SummaryFITSView::showProcessInfo(bool show)
{
    m_showProcessInfo = show;
    processInfoWidget->setVisible(show);
    if(toggleProcessInfoAction != nullptr)
        toggleProcessInfoAction->setChecked(show);
    updateFrame();
}

void SummaryFITSView::resizeEvent(QResizeEvent *event)
{
    FITSView::resizeEvent(event);
    // forward the viewport geometry to the overlay
    processInfoWidget->setGeometry(this->viewport()->geometry());
}


