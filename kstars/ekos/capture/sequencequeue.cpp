/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sequencequeue.h"

#include <knotification.h>
#include <ekos_capture_debug.h>

#include "camerastate.h"
#include "capturedeviceadaptor.h"
#include "ksnotification.h"
#include "sequencejob.h"

// Current Sequence File Format:
constexpr double SQ_FORMAT_VERSION = 2.6;
// We accept file formats with version back to:
constexpr double SQ_COMPAT_VERSION = 2.0;

namespace Ekos
{

bool SequenceQueue::load(const QString &fileURL, const QString &targetName,
                         const QSharedPointer<CaptureDeviceAdaptor> devices,
                         const QSharedPointer<CameraState> state)
{
    QFile sFile(fileURL);
    if (!sFile.open(QIODevice::ReadOnly))
    {
        QString message = i18n("Unable to open file %1", fileURL);
        KSNotification::sorry(message, i18n("Could Not Open File"));
        return false;
    }

    LilXML * xmlParser = newLilXML();

    char errmsg[MAXRBUF];
    XMLEle * root = nullptr;
    XMLEle * ep   = nullptr;
    char c;

    // We expect all data read from the XML to be in the C locale - QLocale::c().
    QLocale cLocale = QLocale::c();

    while (sFile.getChar(&c))
    {
        root = readXMLEle(xmlParser, c, errmsg);

        if (root)
        {
            double sqVersion = cLocale.toDouble(findXMLAttValu(root, "version"));
            if (sqVersion < SQ_COMPAT_VERSION)
            {
                emit newLog(i18n("Deprecated sequence file format version %1. Please construct a new sequence file.",
                                 sqVersion));
                return false;
            }

            for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            {
                if (!strcmp(tagXMLEle(ep), "Observer"))
                {
                    state->setObserverName(QString(pcdataXMLEle(ep)));
                }
                else if (!strcmp(tagXMLEle(ep), "GuideDeviation"))
                {
                    m_GuideDeviationSet = true;
                    m_EnforceGuideDeviation = !strcmp(findXMLAttValu(ep, "enabled"), "true");
                    m_GuideDeviation = cLocale.toDouble(pcdataXMLEle(ep));
                }
                else if (!strcmp(tagXMLEle(ep), "CCD"))
                {
                    // Old field in some files. Without this empty test, it would fall through to the else condition and create a job.
                }
                else if (!strcmp(tagXMLEle(ep), "FilterWheel"))
                {
                    // Old field in some files. Without this empty test, it would fall through to the else condition and create a job.
                }
                else if (!strcmp(tagXMLEle(ep), "GuideStartDeviation"))
                {
                    m_GuideStartDeviationSet = true;
                    m_EnforceStartGuiderDrift = !strcmp(findXMLAttValu(ep, "enabled"), "true");
                    m_StartGuideDeviation = cLocale.toDouble(pcdataXMLEle(ep));
                }
                else if (!strcmp(tagXMLEle(ep), "Autofocus"))
                {
                    // Old field in some files. Without this empty test, it would fall through to the else condition and create a job.
                }
                else if (!strcmp(tagXMLEle(ep), "HFRCheck"))
                {
                    m_AutofocusSet = true;
                    m_EnforceAutofocusHFR = !strcmp(findXMLAttValu(ep, "enabled"), "true");

                    XMLEle *epHFR;
                    //Set default values in case of malformed XML
                    m_HFRDeviation = 0.0;
                    m_HFRCheckAlgorithm = HFR_CHECK_LAST_AUTOFOCUS;
                    m_HFRCheckThresholdPercentage = HFR_CHECK_DEFAULT_THRESHOLD;
                    m_HFRCheckFrames = 1;

                    for (epHFR = nextXMLEle(ep, 1); epHFR != nullptr; epHFR = nextXMLEle(ep, 0))
                    {
                        if (!strcmp(tagXMLEle(epHFR), "HFRDeviation"))
                        {
                            double const HFRValue = cLocale.toDouble(pcdataXMLEle(epHFR));
                            // Set the HFR value from XML, or reset it to zero, don't let another unrelated older HFR be used
                            if (HFRValue >= 0.0)
                                m_HFRDeviation = HFRValue;
                        }

                        if (!strcmp(tagXMLEle(epHFR), "HFRCheckAlgorithm"))
                        {
                            int HFRCheckAlgo = cLocale.toInt(pcdataXMLEle(epHFR));
                            // Set the HFR Check Algo from XML, or reset it to Last Autofocus
                            if (HFRCheckAlgo >= 0 && HFRCheckAlgo < HFR_CHECK_MAX_ALGO)
                                m_HFRCheckAlgorithm = static_cast<HFRCheckAlgorithm>(HFRCheckAlgo);
                        }
                        else if (!strcmp(tagXMLEle(epHFR), "HFRCheckThreshold"))
                        {
                            double const hFRCheckThreshold = cLocale.toDouble(pcdataXMLEle(epHFR));
                            // Set the HFR Threshold Percentage from XML, or reset it to 10%, don't let another unrelated older value be used
                            if (hFRCheckThreshold >= 0.0)
                                m_HFRCheckThresholdPercentage = hFRCheckThreshold;
                        }
                        else if (!strcmp(tagXMLEle(epHFR), "HFRCheckFrames"))
                        {
                            int const hFRCheckFrames = cLocale.toInt(pcdataXMLEle(epHFR));
                            // Set the HFR Frames from XML, or reset it to 1, don't let another unrelated older value be used
                            if (hFRCheckFrames > 1)
                                m_HFRCheckFrames = hFRCheckFrames;
                        }
                    }
                }
                else if (!strcmp(tagXMLEle(ep), "RefocusOnTemperatureDelta"))
                {
                    m_RefocusOnTemperatureDeltaSet = true;
                    m_EnforceAutofocusOnTemperature = !strcmp(findXMLAttValu(ep, "enabled"), "true");
                    double const deltaValue = cLocale.toDouble(pcdataXMLEle(ep));
                    m_MaxFocusTemperatureDelta = deltaValue;
                }
                else if (!strcmp(tagXMLEle(ep), "RefocusEveryN"))
                {
                    m_RefocusEveryNSet = true;
                    m_EnforceRefocusEveryN = !strcmp(findXMLAttValu(ep, "enabled"), "true");
                    int const minutesValue = cLocale.toInt(pcdataXMLEle(ep));
                    // Set the refocus period from XML, or reset it to zero, don't let another unrelated older refocus period be used.
                    m_RefocusEveryN = minutesValue > 0 ? minutesValue : 0;
                }
                else if (!strcmp(tagXMLEle(ep), "RefocusOnMeridianFlip"))
                {
                    m_RefocusOnMeridianFlipSet = true;
                    m_RefocusAfterMeridianFlip = !strcmp(findXMLAttValu(ep, "enabled"), "true");
                }
                else if (!strcmp(tagXMLEle(ep), "MeridianFlip"))
                {
                    // meridian flip is managed by the mount only
                    // older files might nevertheless contain MF settings
                    if (! strcmp(findXMLAttValu(ep, "enabled"), "true"))
                        emit newLog(
                            i18n("Meridian flip configuration has been shifted to the mount module. Please configure the meridian flip there."));
                }
                else
                {
                    auto job = QSharedPointer<SequenceJob>(new SequenceJob(devices, state, SequenceJob::JOBTYPE_BATCH, ep, targetName));
                    m_allJobs.append(job);
                }
            }
            delXMLEle(root);
        }
        else if (errmsg[0])
        {
            emit newLog(QString(errmsg));
            delLilXML(xmlParser);
            return false;
        }
    }

    state->setSequenceURL(QUrl::fromLocalFile(fileURL));
    state->setDirty(false);
    delLilXML(xmlParser);
    return true;
}

void SequenceQueue::setOptions()
{
    if (m_GuideDeviationSet)
    {
        Options::setEnforceGuideDeviation(m_EnforceGuideDeviation);
        Options::setGuideDeviation(m_GuideDeviation);
    }
    if (m_GuideStartDeviationSet)
    {
        Options::setEnforceStartGuiderDrift(m_EnforceStartGuiderDrift);
        Options::setStartGuideDeviation(m_StartGuideDeviation);
    }
    if (m_AutofocusSet)
    {
        Options::setEnforceAutofocusHFR(m_EnforceAutofocusHFR);
        Options::setHFRCheckAlgorithm(m_HFRCheckAlgorithm);
        Options::setHFRThresholdPercentage(m_HFRCheckThresholdPercentage);
        Options::setInSequenceCheckFrames(m_HFRCheckFrames);
        Options::setHFRDeviation(m_HFRDeviation);
    }
    if (m_RefocusOnTemperatureDeltaSet)
    {
        Options::setEnforceAutofocusOnTemperature(m_EnforceAutofocusOnTemperature);
        Options::setMaxFocusTemperatureDelta(m_MaxFocusTemperatureDelta);
    }
    if (m_RefocusEveryNSet)
    {
        Options::setEnforceRefocusEveryN(m_EnforceRefocusEveryN);
        Options::setRefocusEveryN(m_RefocusEveryN);
    }
    if (m_RefocusOnMeridianFlipSet)
    {
        Options::setRefocusAfterMeridianFlip(m_RefocusAfterMeridianFlip);
    }
}

void SequenceQueue::loadOptions()
{
    m_GuideDeviationSet = true;

    m_EnforceGuideDeviation = Options::enforceGuideDeviation();
    m_GuideDeviation = Options::guideDeviation();

    m_GuideStartDeviationSet = true;
    m_EnforceStartGuiderDrift = Options::enforceStartGuiderDrift();
    m_StartGuideDeviation = Options::startGuideDeviation();

    m_AutofocusSet = true;
    m_EnforceAutofocusHFR = Options::enforceAutofocusHFR();
    m_HFRCheckAlgorithm = static_cast<HFRCheckAlgorithm>(Options::hFRCheckAlgorithm());
    m_HFRCheckThresholdPercentage = Options::hFRThresholdPercentage();
    m_HFRCheckFrames = Options::inSequenceCheckFrames();
    m_HFRDeviation = Options::hFRDeviation();

    m_RefocusOnTemperatureDeltaSet = true;
    m_EnforceAutofocusOnTemperature = Options::enforceAutofocusOnTemperature();
    m_MaxFocusTemperatureDelta = Options::maxFocusTemperatureDelta();

    m_RefocusEveryNSet = true;
    m_EnforceRefocusEveryN = Options::enforceRefocusEveryN();
    m_RefocusEveryN = Options::refocusEveryN();

    m_RefocusOnMeridianFlipSet = true;
    m_RefocusAfterMeridianFlip = Options::refocusAfterMeridianFlip();
}

bool SequenceQueue::save(const QString &path, const QString &observerName)
{
    QFile file;
    file.setFileName(path);

    if (!file.open(QIODevice::WriteOnly))
    {
        QString message = i18n("Unable to write to file %1", path);
        KSNotification::sorry(message, i18n("Could not open file"));
        return false;
    }

    QTextStream outstream(&file);

    // We serialize sequence data to XML using the C locale
    QLocale cLocale = QLocale::c();

    outstream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << Qt::endl;
    outstream << "<SequenceQueue version='" << SQ_FORMAT_VERSION << "'>" << Qt::endl;
    if (observerName.isEmpty() == false)
        outstream << "<Observer>" << observerName << "</Observer>" << Qt::endl;
    outstream << "<GuideDeviation enabled='" << (m_EnforceGuideDeviation ? "true" : "false") << "'>"
              << cLocale.toString(m_GuideDeviation) << "</GuideDeviation>" << Qt::endl;
    outstream << "<GuideStartDeviation enabled='" << (m_EnforceStartGuiderDrift ? "true" : "false") << "'>"
              << cLocale.toString(m_StartGuideDeviation) << "</GuideStartDeviation>" << Qt::endl;
    outstream << "<HFRCheck enabled='" << (m_EnforceAutofocusHFR ? "true" : "false") << "'>" << Qt::endl;
    outstream << "<HFRDeviation>" << cLocale.toString(m_HFRDeviation) << "</HFRDeviation>" << Qt::endl;
    outstream << "<HFRCheckAlgorithm>" << m_HFRCheckAlgorithm << "</HFRCheckAlgorithm>" << Qt::endl;
    outstream << "<HFRCheckThreshold>" << cLocale.toString(m_HFRCheckThresholdPercentage) << "</HFRCheckThreshold>" << Qt::endl;
    outstream << "<HFRCheckFrames>" << cLocale.toString(m_HFRCheckFrames) << "</HFRCheckFrames>" << Qt::endl;
    outstream << "</HFRCheck>" << Qt::endl;
    outstream << "<RefocusOnTemperatureDelta enabled='" << (m_EnforceAutofocusOnTemperature ? "true" : "false") <<
              "'>"
              << cLocale.toString(m_MaxFocusTemperatureDelta) << "</RefocusOnTemperatureDelta>" << Qt::endl;
    outstream << "<RefocusEveryN enabled='" << (m_EnforceRefocusEveryN ? "true" : "false") << "'>"
              << cLocale.toString(m_RefocusEveryN) << "</RefocusEveryN>" << Qt::endl;
    outstream << "<RefocusOnMeridianFlip enabled='" << (m_RefocusAfterMeridianFlip ? "true" : "false") << "'/>"
              << Qt::endl;

    for (auto &job : m_allJobs)
    {
        job->saveTo(outstream, cLocale);
    }

    outstream << "</SequenceQueue>" << Qt::endl;

    emit newLog(i18n("Sequence queue saved to %1", path));
    file.flush();
    file.close();

    return true;
}

}

