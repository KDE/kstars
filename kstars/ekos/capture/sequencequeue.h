/*
    SPDX-FileCopyrightText: 2024 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QList>
#include <QUrl>
#include <QString>
#include "sequencejob.h"

/**
 * @class SequenceJob
 * @short SequenceQueue represents a sequence of capture jobs to be executed
 * by the capture module.
 *
 * @author Hy Murveit
 * @version 1.0
 */
namespace Ekos
{

class SequenceJob;
class CaptureDeviceAdaptor;
class CameraState;

class SequenceQueue : public QObject
{
        Q_OBJECT

    public:
        SequenceQueue() {}

        bool load(const QString &fileURL, const QString &targetName,
                  const QSharedPointer<CaptureDeviceAdaptor> devices,
                  const QSharedPointer<CameraState> sharedState);

        bool save(const QString &path, const QString &observerName);

        void setOptions();
        void loadOptions();

        QList<QSharedPointer<SequenceJob>> &allJobs()
        {
            return m_allJobs;
        }
        const QUrl &sequenceURL() const
        {
            return m_SequenceURL;
        }
        void setSequenceURL(const QUrl &newSequenceURL)
        {
            m_SequenceURL = newSequenceURL;
        }
        bool getEnforceGuideDeviation()
        {
            return m_EnforceGuideDeviation;
        }
        void setEnforceGuideDeviation(bool value)
        {
            m_EnforceGuideDeviation = value;
        }
        double getGuideDeviation()
        {
            return m_GuideDeviation;
        }
        void setGuideDeviation(double value)
        {
            m_GuideDeviation = value;
        }
        bool getEnforceStartGuiderDrift()
        {
            return m_EnforceStartGuiderDrift;
        }
        void setEnforceStartGuiderDrift(bool value)
        {
            m_EnforceStartGuiderDrift = value;
        }
        double getStartGuideDeviation()
        {
            return m_StartGuideDeviation;
        }
        void setStartGuideDeviation(double value)
        {
            m_StartGuideDeviation = value;
        }
        bool getEnforceAutofocusHFR()
        {
            return m_EnforceAutofocusHFR;
        }
        void setEnforceAutofocusHFR(bool value)
        {
            m_EnforceAutofocusHFR = value;
        }
        double getHFRDeviation()
        {
            return m_HFRDeviation;
        }
        void setHFRDeviation(double value)
        {
            m_HFRDeviation = value;
        }
        bool getEnforceAutofocusOnTemperature()
        {
            return m_EnforceAutofocusOnTemperature;
        }
        void setEnforceAutofocusOnTemperature(bool value)
        {
            m_EnforceAutofocusOnTemperature = value;
        }
        double getMaxFocusTemperatureDelta()
        {
            return m_MaxFocusTemperatureDelta;
        }
        void setMaxFocusTemperatureDelta(double value)
        {
            m_MaxFocusTemperatureDelta = value;
        }
        bool getEnforceRefocusEveryN()
        {
            return m_EnforceRefocusEveryN;
        }
        void setEnforceRefocusEveryN(bool value)
        {
            m_EnforceRefocusEveryN = value;
        }
        double getRefocusEveryN()
        {
            return m_RefocusEveryN;
        }
        void setRefocusEveryN(double value)
        {
            m_RefocusEveryN = value;
        }
        bool getRefocusAfterMeridianFlip()
        {
            return m_RefocusAfterMeridianFlip;
        }
        void setRefocusAfterMeridianFlip(bool value)
        {
            m_RefocusAfterMeridianFlip = value;
        }

    signals:
        void newLog(const QString &message);

    private:

        // list of all sequence jobs
        QList<QSharedPointer<SequenceJob>> m_allJobs;
        // URL where the sequence queue file is stored.
        QUrl m_SequenceURL;

        bool m_GuideDeviationSet { false };
        bool m_EnforceGuideDeviation {false};
        double m_GuideDeviation = 0;

        bool m_GuideStartDeviationSet { false };
        bool m_EnforceStartGuiderDrift {false};
        double m_StartGuideDeviation = 0;

        bool m_AutofocusSet { false };
        bool m_EnforceAutofocusHFR { false };
        HFRCheckAlgorithm m_HFRCheckAlgorithm { HFR_CHECK_LAST_AUTOFOCUS };
        double m_HFRCheckThresholdPercentage = HFR_CHECK_DEFAULT_THRESHOLD;
        int m_HFRCheckFrames = 1;
        double m_HFRDeviation = 0;

        bool m_RefocusOnTemperatureDeltaSet { false };
        bool m_EnforceAutofocusOnTemperature { false };
        double m_MaxFocusTemperatureDelta = 0;

        bool m_RefocusEveryNSet { false };
        bool m_EnforceRefocusEveryN { false };
        double m_RefocusEveryN = 0;

        bool m_RefocusOnMeridianFlipSet { false };
        bool m_RefocusAfterMeridianFlip { false };
};

}
