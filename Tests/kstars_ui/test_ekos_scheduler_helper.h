/*
    Helper class of KStars UI scheduler tests

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "test_ekos_helper.h"

class TestEkosSchedulerHelper : public TestEkosHelper
{
public:
    struct StartupCondition
    {
        SchedulerJob::StartupCondition type;
        int culminationOffset;
        QDateTime atLocalDateTime;  // This is in local time, not universal time.
    };

    struct ScheduleSteps
    {
        bool track, focus, align, guide;
    };

    TestEkosSchedulerHelper();

    // This writes the the scheduler and capture files into the locations given.
    static bool writeSimpleSequenceFiles(const QString &eslContents, const QString &eslFile, const QString &esqContents,
                                         const QString &esqFile);

    static QString getSchedulerFile(const SkyObject *targetObject, StartupCondition startupCondition, ScheduleSteps steps,
                                    bool enforceTwilight, bool enforceArtificialHorizon, int minAltitude = 30, QString fitsFile = nullptr);

    // This is a capture sequence file needed to start up the scheduler. Most fields are ignored by the scheduler,
    // and by the Mock capture module as well.
    static QString getDefaultEsqContent()
    {
        return QString("<?xml version=\"1.0\" encoding=\"UTF-8\"?><SequenceQueue version='2.1'><CCD>CCD Simulator</CCD>"
                       "<FilterWheel>CCD Simulator</FilterWheel><GuideDeviation enabled='false'>2</GuideDeviation>"
                       "<GuideStartDeviation enabled='false'>2</GuideStartDeviation><Autofocus enabled='false'>0</Autofocus>"
                       "<RefocusOnTemperatureDelta enabled='false'>1</RefocusOnTemperatureDelta>"
                       "<RefocusEveryN enabled='false'>60</RefocusEveryN><Job><Exposure>200</Exposure><Binning><X>1</X><Y>1</Y>"
                       "</Binning><Frame><X>0</X><Y>0</Y><W>1280</W><H>1024</H></Frame><Temperature force='false'>0</Temperature>"
                       "<Filter>Red</Filter><Type>Light</Type><Prefix><RawPrefix></RawPrefix><FilterEnabled>0</FilterEnabled>"
                       "<ExpEnabled>0</ExpEnabled><TimeStampEnabled>0</TimeStampEnabled></Prefix>"
                       "<Count>300</Count><Delay>0</Delay><FITSDirectory>.</FITSDirectory><UploadMode>0</UploadMode>"
                       "<FormatIndex>0</FormatIndex><Properties></Properties><Calibration><FlatSource><Type>Manual</Type>"
                       "</FlatSource><FlatDuration><Type>ADU</Type><Value>15000</Value><Tolerance>1000</Tolerance></FlatDuration>"
                       "<PreMountPark>False</PreMountPark><PreDomePark>False</PreDomePark></Calibration></Job></SequenceQueue>");
    }
protected:
    // Simple write-string-to-file utility.
    static bool writeFile(const QString &filename, const QString &contents);

};
