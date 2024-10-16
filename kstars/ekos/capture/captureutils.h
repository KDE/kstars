/*
    SPDX-FileCopyrightText: 2024 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <cstdint>
#include <QList>
#include <QSharedPointer>

#include "capturetypes.h"

namespace Ekos {

class SequenceJob;
class CameraState;
class SequenceQueue;
class CaptureDeviceAdaptor;

class CaptureUtils
{

public:

    /**
     * @brief Calculate the map signature -> expected number of captures from the given list of capture sequence jobs,
     *        i.e. the expected number of captures from a single scheduler job run.
    */
    static CapturedFramesMap calculateExpectedCapturesMap(QSharedPointer<SequenceQueue> sequenceQueue);

    /**
     * @brief updateCompletedJobsCount If signature is given, the update the counts for this dedicated signature. If it is
     *        empty, then examine sequence job storage and count captures for all sequence jobs. It updates
     *        the captured frames map (if force is true) and updates the total number of captured frames per job.
     *        These are distributed amongst the jobs with the same signature like it would happen if the entire sequence
     *        iterates through.
     * @param force force the refresh, otherwise load it only if there is no value known for the given signature.
     * @param signature signature for which the job counts should be refreshed
     */
    static void updateCompletedFramesCount(QSharedPointer<SequenceQueue> sequenceQueue, bool force, const QString &signature = "");

    /**
     * @brief estimateJobTime Estimates the time the job takes to complete based on already captured frames.
     * @param state camera state
     * @return estimation how long the imaging would take to complete
     */
    static int estimateRemainingImagingTime(QSharedPointer<SequenceQueue> sequenceQueue);
};

} // namespace
