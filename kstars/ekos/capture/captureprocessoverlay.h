/*
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "ui_captureprocessoverlay.h"
#include "sequencejob.h"

#include "indi/indicommon.h"
#include "capturehistory.h"

#include <QWidget>

class FITSData;

class CaptureProcessOverlay : public QWidget,  public Ui::CaptureProcessOverlay
{
    Q_OBJECT

public:
    explicit CaptureProcessOverlay(QWidget *parent = nullptr);

    bool addFrameData(CaptureHistory::FrameData data, const QString &devicename);
    /**
     * @brief Update the overlay with the meta data of the current frame and add it to the history
     */
    void updateFrameData();

    /**
     * @brief Update the current target distance.
     * @param targetDiff distance to the target in arcseconds.
     */
    void updateTargetDistance(double targetDiff);

    /**
     * @brief Obtain the position of the current frame from the history
     */
    int currentPosition() {return captureHistory().position();}

    /**
     * @brief Retrieve the currently selected frame
     */
    const CaptureHistory::FrameData currentFrame() {return captureHistory().currentFrame();}

    /**
     * @brief Obtain the frame from the given position in the history
     */
    const CaptureHistory::FrameData getFrame(int pos) {return captureHistory().getFrame(pos);}

    /**
     * @brief Obtain the position of the current frame from the history
     */

    /**
     * @brief Returns true iff there are frames in the capture history
     */
    bool hasFrames();

    /**
     * @brief Update the statistics display for captured frames
     */
    void displayTargetStatistics();

    /**
     * @brief Show the next frame from the capture history
     */
    bool showNextFrame();

    /**
     * @brief Show the previous frame from the capture history
     */
    bool showPreviousFrame();

    /**
     * @brief Delete the currently displayed frame
     */
    bool deleteFrame(int pos);

    void setCurrentTrainName(const QString &devicename);

    /**
     * @brief Capture history of the current camera device
     */
    CaptureHistory &captureHistory()
    {
        return m_captureHistory[m_currentTrainName];
    }

    /**
     * @brief refresh frame data and statistics
     */
    void refresh();

private:
    //capture history
    QMap<QString, CaptureHistory> m_captureHistory;
    // current camera device name
    QString m_currentTrainName = "";

};
