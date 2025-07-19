/*
    SPDX-FileCopyrightText: 2021/2025 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "ekos/ekos.h"
#include "indi/indicommon.h"
#include "sequencejob.h"


class CaptureHistory
{
public:

    // structs for frame statistics
    // map (frame type, filter) --> (exp time * 100, counter)
    typedef QMap<QPair<CCDFrameType, QString>, QList<QPair<int, int>*>> FrameStatistics;
    // map target --> frame statistics
    typedef QMap<QString, FrameStatistics> TargetStatistics;

    // data describing a single frame
    struct FrameData
    {
        QString target;
        CCDFrameType frameType;
        Ekos::SequenceJob::SequenceJobType jobType;
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
        int count, completed;
        QList<QSharedPointer<Edge>> starCenters;
        double hfr { Ekos::INVALID_STAR_MEASURE };
        double fwhm { Ekos::INVALID_STAR_MEASURE };
        double numStars { Ekos::INVALID_STAR_MEASURE };
        double measure { Ekos::INVALID_STAR_MEASURE };
        double fourierPower { Ekos::INVALID_STAR_MEASURE };
        double blurriness { Ekos::INVALID_STAR_MEASURE };
        double weight { 0 };
    };

    /**
     * @brief Add a newly captured frame to the history
     * @param data frame data
     * @param noduplicates flag if the file name should be used to avoid doublicate entries
     * @return true iff this is a new frame, i.e. its filename does not exist in the history
     */
    bool addFrame(FrameData data, bool noduplicates = true);

    /**
     * @brief Delete the current frame and (if possible) the corresponding file.
     * If the last one has been deleted, navigate to the frame before, if possible.
     * @param existingFilesOnly if true the history is cleared for non existing files
     * @return true iff deleting was successful
     */
    bool deleteFrame(int pos, bool existingFilesOnly = true);

    /**
     * @brief the currently pointed capture frame
     */
    const FrameData currentFrame() {return m_history.at(m_position);}

    /**
     * @brief firstFrame first frame of the history (without altering current)
     */
    const FrameData firstFrame();
    /**
     * @brief firstFrame last frame of the history (without altering current)
     */
    const FrameData lastFrame();

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
     * @brief first Move to the first history entry
     * @return true iff the move was possible within the limits
     */
    bool first();
    /**
     * @brief first Move to the last history entry
     * @return true iff the move was possible within the limits
     */
    bool last();

    /**
     * @brief Iterate over the current target history and add all
     *        those where the corresponding file exists.
     * @param existingFilesOnly if true the history is cleared for non existing files
     */
    void updateTargetStatistics(bool existingFilesOnly = true);

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
