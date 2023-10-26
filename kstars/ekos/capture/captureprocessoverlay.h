/*
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "ui_captureprocessoverlay.h"

#include "indi/indicommon.h"

#include <QWidget>

class FITSData;

class CaptureProcessOverlay : public QWidget,  public Ui::CaptureProcessOverlay
{
    Q_OBJECT

public:
    explicit CaptureProcessOverlay(QWidget *parent = nullptr);

    // data describing a single frame
    struct FrameData
    {
        QString target;
        CCDFrameType frameType;
        QString filterName;
        QString filename;
        double exptime;
        double targetdrift;
        QPoint binning;
        int width;
        int height;
        double gain;
        double offset;
        QString iso;
    };

    // structs for frame statistics
    // map (frame type, filter) --> (exp time * 100, counter)
    typedef QMap<QPair<CCDFrameType, QString>, QList<QPair<int, int>*>> FrameStatistics;
    // map target --> frame statistics
    typedef QMap<QString, FrameStatistics> TargetStatistics;

    /**
     * @brief Navigator through the capture history.
     */
    class CaptureHistory {
    public:
        /**
         * @brief Add a newly captured frame to the history
         * @param data frame data
         * @return true iff this is a new frame, i.e. its filename does not exist in the history
         */
        bool addFrame(FrameData data);

        /**
         * @brief Delete the current frame and (if possible) the corresponding file.
         * If the last one has been deleted, navigate to the frame before, if possible.
         * @return true iff deleting was successful
         */
        bool deleteFrame(int pos);

        /**
         * @brief the currently pointed capture frame
         */
        const FrameData currentFrame() {return m_history.at(m_position);}

        /**
         * @brief The current navigation position in the capture history
         */
        int position() {return m_position;}
        /**
         * @brief Obtain the frame from the given position in the history
         */
        const FrameData getFrame(int pos) {return m_history.at(pos);}
        /**
         * @brief Capture history size
         */
        int size() {return m_history.size();}
        /**
         * @brief Reset the history
         */
        void reset();
        /**
         * @brief Move one step forward in the history
         * @return true iff the move was possible within the limits
         */
        bool forward();
        /**
         * @brief Move one step backwards in the history
         * @return true iff the move was possible within the limits
         */
        bool backward();
        /**
         * @brief Iterate over the current target history and add all
         *        those where the corresponding file exists.
         */
        void updateTargetStatistics();

        // capture statistics
        TargetStatistics statistics;

    private:
        QList<FrameData> m_history;
        int m_position = -1;

        /**
         * @brief Add a new frame to the statistics
         * @param target current target being processed
         * @param frameType type of the currently captured frame
         * @param filter selected filter for the captured frame
         * @param exptime exposure time of the captured frame
         */
        void countNewFrame(QString target, CCDFrameType frameType, QString filter, double exptime);
    };

    bool addFrameData(FrameData data);
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
    int currentPosition() {return m_captureHistory.position();}

    /**
     * @brief Retrieve the currently selected frame
     */
    const FrameData currentFrame() {return m_captureHistory.currentFrame();}

    /**
     * @brief Obtain the frame from the given position in the history
     */
    const FrameData getFrame(int pos) {return m_captureHistory.getFrame(pos);}

    /**
     * @brief Obtain the position of the current frame from the history
     */

    /**
     * @brief Returns true iff there are frames in the capture history
     */
    bool hasFrames() {return m_captureHistory.size() > 0;}

    /**
     * @brief Update the statistics display for captured frames
     */
    void displayTargetStatistics();

    /**
     * @brief Loads a new frame into the view and displays meta data in the overlay
     * @param data pointer to FITSData object
     */
    bool addFrame(const QSharedPointer<FITSData> &data);

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

private:
    //capture history
    CaptureHistory m_captureHistory;

};
