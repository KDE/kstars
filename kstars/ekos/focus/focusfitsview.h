/*
    SPDX-FileCopyrightText: 2025 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QWidget>
#include "fitsviewer/fitsview.h"
#include "ekos/capture/capturehistorynavigation.h"

class FocusFITSView : public FITSView
{
    Q_OBJECT

public:
    explicit FocusFITSView(QWidget *parent = nullptr);

    /**
     * @brief showNavigation Show the navigation bar
     */
    void showNavigation(bool show) { m_focusHistoryNavigation->setVisible(show); }
    bool isNavigationVisible() { return m_focusHistoryNavigation->isVisible(); }

    /**
     * @brief addRun Add a new frame sequence
     */
    void addRun() {m_focusHistoryNavigation->addRun();}

    /**
     * @brief removeRun Remove a given frame sequence
         * @param deleteFiles delete captured focus frames
         * @param useTrash use trash when deleting
     */
    void removeRun(int run, bool deleteFiles, bool useTrash) { m_focusHistoryNavigation->removeRun(run, deleteFiles, useTrash); }

    /**
     * @brief Add a newly captured frame to the history
     * @param data frame data
     */
    void addFrame(CaptureHistory::FrameData data) {m_focusHistoryNavigation->addFrame(data, false);}

    /**
     * @brief loadFocusFrame Load stored focus frame for the current history position
     */
    bool loadCurrentFocusFrame();

    /**
     * @brief Capture history of the current focuser
     */
    CaptureHistory &captureHistory(int run) { return m_focusHistoryNavigation->captureHistory(run); }

    /**
     * @brief Retrieve the currently selected frame
     */
    const CaptureHistory::FrameData currentFrame() {return m_focusHistoryNavigation->currentFrame();}

    /**
     * @brief Retrieve the last captured frame
     */
    const CaptureHistory::FrameData lastFrame() {return m_focusHistoryNavigation->lastFrame();}

    /**
     * @brief lastAFRun ID of the last autofocus run
     */
    int lastAFRun(){return m_focusHistoryNavigation->lastRun();};

    /**
     * @brief currentRun Currently selected autofocus run
     */
    int currentAFRun() const { return m_focusHistoryNavigation->currentRun(); }

    QSharedPointer<CaptureHistoryNavigation> m_focusHistoryNavigation;

public slots:
    void resizeEvent(QResizeEvent *event) override;

    // ******************* history navigation ********************* //
    bool showFirstFrame();
    bool showLastFrame();
    bool showPreviousFrame();
    bool showNextFrame();
    bool showPreviousAFRun();
    bool showNextAFRun();
};
