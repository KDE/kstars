/*
    SPDX-FileCopyrightText: 2025 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "ui_capturehistorynavigation.h"

#include "ekos/capture/capturehistory.h"

#include <QWidget>

class CaptureHistoryNavigation : public QWidget, public Ui::CaptureHistoryNavigation
{
    Q_OBJECT

public:
    explicit CaptureHistoryNavigation(QWidget *parent = nullptr);

    /**
     * @brief addRun Add a new frame sequence
     */
    void addRun();

    /**
     * @brief removeRun Remove a given frame sequence and delete the corresponding
     *        frames as well, if requested.
     */
    void removeRun(int run, bool deleteFiles = false, bool useTrash = false);

    /**
     * @brief Add a newly captured frame to the history
     * @param data frame data
     * @param noduplicates flag if the file name should be used to avoid doublicate entries
     * @return true iff this is a new frame, i.e. its filename does not exist in the history
     */
    bool addFrame(CaptureHistory::FrameData data, bool noduplicates = true)
    {
        m_currentRun = m_lastRun;
        refreshNavigation();
        return captureHistory(m_lastRun).addFrame(data, noduplicates);
    }

    /**
     * @brief refreshHistory Refresh the history and remove all file names where the corresponding
     *        file does no longer exist.
     */
    void refreshHistory();

    /**
     * @brief Retrieve the currently selected frame
     */
    const CaptureHistory::FrameData currentFrame() {return captureHistory(m_currentRun).currentFrame();}

    /**
     * @brief Retrieve the last captured frame
     */
    const CaptureHistory::FrameData lastFrame() {return captureHistory(m_lastRun).lastFrame();}

    /**
     * @brief Capture history
     */
    CaptureHistory &captureHistory(int run);

    /**
     * @brief updateNavigationButtons Enable/disable navigation buttons, depending on the
     *        current position.
     */
    void refreshNavigation();

    // ******************* history navigation ********************* //
    bool showFirstFrame();
    bool showLastFrame();
    bool showPreviousFrame();
    bool showNextFrame();
    bool showPreviousRun();
    bool showNextRun();

    int currentRun() const { return m_currentRun; }
    int lastRun() const { return m_lastRun; }

private:

    // list of capture runs (e.g. a single autofocus run)
    QList<CaptureHistory> m_captureHistory;

    // id of the last capture run
    int m_lastRun = 0;
    // id of the currently selected run
    int m_currentRun = 0;
};
