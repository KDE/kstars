/*
    SPDX-FileCopyrightText: 2025 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "focusfitsview.h"
#include "QGraphicsOpacityEffect"
#include "ksmessagebox.h"

FocusFITSView::FocusFITSView(QWidget *parent): FITSView(parent, FITS_FOCUS)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    createFloatingToolBar();

    m_focusHistoryNavigation.reset(new CaptureHistoryNavigation(this));
    auto * eff = new QGraphicsOpacityEffect(this);
    eff->setOpacity(0.7);
    m_focusHistoryNavigation->setGraphicsEffect(eff);
    m_focusHistoryNavigation->resize(width(), m_focusHistoryNavigation->height());
    m_focusHistoryNavigation->move(0, height() - m_focusHistoryNavigation->height());

    m_focusHistoryNavigation->filenameValue->setVisible(false);

    const QString style = "QPushButton:hover{background: rgba(200, 200, 200, 255);border:solid; color: white}";
    // needs to be set explicitly, since Qt does not inherit automatically
    m_focusHistoryNavigation->historyPreviousRunB->setStyleSheet(style);
    m_focusHistoryNavigation->historyFirstB->setStyleSheet(style);
    m_focusHistoryNavigation->historyBackwardB->setStyleSheet(style);
    m_focusHistoryNavigation->historyForwardB->setStyleSheet(style);
    m_focusHistoryNavigation->historyLastB->setStyleSheet(style);
    m_focusHistoryNavigation->historyNextRunB->setStyleSheet(style);

    showNavigation(false);
    m_focusHistoryNavigation->raise();
}





bool FocusFITSView::loadCurrentFocusFrame()
{
    if (currentFrame().filename == "")
        return false;

    loadFile(currentFrame().filename);
    // add stars of the frame to the FITS view (take a clone since
    // it gets deleted in FITSData
    for (QSharedPointer<Edge> star : currentFrame().starCenters)
        imageData()->appendStar(star->clone());

    return true;
}

void FocusFITSView::resizeEvent(QResizeEvent *event)
{
    FITSView::resizeEvent(event);
    m_focusHistoryNavigation->resize(width(), m_focusHistoryNavigation->height());
    m_focusHistoryNavigation->move(0, height() - m_focusHistoryNavigation->height());
}

bool FocusFITSView::showFirstFrame()
{
    if (m_focusHistoryNavigation->showFirstFrame())
    {
        loadCurrentFocusFrame();
        return true;
    }

    return false;
}

bool FocusFITSView::showLastFrame()
{
    if (m_focusHistoryNavigation->showLastFrame())
    {
        loadCurrentFocusFrame();
        return true;
    }

    return false;
}

bool FocusFITSView::showPreviousFrame()
{
    if (m_focusHistoryNavigation->showPreviousFrame())
    {
        loadCurrentFocusFrame();
        return true;
    }

    return false;
}

bool FocusFITSView::showNextFrame()
{
    if (m_focusHistoryNavigation->showNextFrame())
    {
        loadCurrentFocusFrame();
        return true;
    }

    return false;
}

bool FocusFITSView::showPreviousAFRun()
{
    if (m_focusHistoryNavigation->showPreviousRun())
    {
        loadCurrentFocusFrame();
        return true;
    }

    return false;
}

bool FocusFITSView::showNextAFRun()
{
    if (m_focusHistoryNavigation->showNextRun())
    {
        loadCurrentFocusFrame();
        return true;
    }

    return false;
}
