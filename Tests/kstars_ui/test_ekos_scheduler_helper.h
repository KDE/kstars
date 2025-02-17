/*
    Helper class of KStars UI scheduler tests

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "test_ekos_helper.h"
#include "ekos/scheduler/schedulerjob.h"

class TestEkosSchedulerHelper : public TestEkosHelper
{
    public:
        struct StartupCondition
        {
            Ekos::StartupCondition type;
            int culminationOffset;
            QDateTime atLocalDateTime;  // This is in local time, not universal time.
        };
        struct CompletionCondition
        {
            Ekos::CompletionCondition type;
            int repeat;
            QDateTime atLocalDateTime;  // This is in local time, not universal time.
        };

        struct ScheduleSteps
        {
            bool track, focus, align, guide;
        };

        struct ShutdownProcedure
        {
            bool warm_ccd, close_cap, park_mount, park_dome;
        };

        struct CaptureJob
        {
            int exposureTime;
            int count;
            QString filterName;
            QString fitsDirectory;
        };

        TestEkosSchedulerHelper();

        // This writes the scheduler and capture files into the locations given.
        static bool writeSimpleSequenceFiles(const QString &eslContents, const QString &eslFile, const QString &esqContents,
                                             const QString &esqFile);

        static QString getSchedulerFile(const SkyObject *targetObject, const StartupCondition &startupCondition,
                                        const CompletionCondition &completionCondition, ScheduleSteps steps,
                                        bool enforceTwilight, bool enforceArtificialHorizon, int minAltitude = 30, double maxMoonAltitude = 90, QString fitsFile = nullptr,
                                        ShutdownProcedure shutdownProcedure = {false, false, true, false}, int errorDelay = 0);

        // This is a capture sequence file needed to start up the scheduler. Most fields are ignored by the scheduler,
        // and by the Mock capture module as well.
        static QString getDefaultEsqContent()
        {
            return getEsqContent(QVector<CaptureJob>(1, {200, 1, "Red", "."}));
        }

        /**
         * @brief Create a capture sequence file with the given capture jobs
         * @param jobs
         * @return ESQ string to be handled by Capture
         */
        static QString getEsqContent(QVector<CaptureJob> jobs);

        // Simple write-string-to-file utility.
        static bool writeFile(const QString &filename, const QString &contents);

    protected:

};
