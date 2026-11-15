/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

class QNetworkReply;

/**
 * @namespace CDSMirrors
 *
 * @short Host names and mirrors of the CDS (Strasbourg) services used by KStars.
 *
 * KStars uses the CDS Sesame name resolver, SIMBAD, the MocServer and the CDS HiPS surveys. CDS has renamed
 * its hosts over the years (u-strasbg.fr, unistra.fr, cds.unistra.fr) and its services are sometimes down,
 * so every service has an ordered list of base URLs. The first entry is the CDS server, the others are
 * mirrors (e.g. at the CfA). Callers try the base URLs in order and report the one that worked, so that
 * later requests start with it.
 */
namespace CDSMirrors
{
enum class Service
{
    Sesame,     ///< Sesame name resolver, base URL up to and including "nph-sesame"
    Simbad,     ///< SIMBAD, base URL is the host only, e.g. https://simbad.cds.unistra.fr
    MocServer   ///< MocServer, base URL up to and including "MocServer/query"
};

/** @brief Default time in milliseconds to wait for a reply before trying the next mirror. */
constexpr int DefaultRequestTimeoutMs = 15000;

/**
 * @brief baseUrls Base URLs of a service in the order they should be tried.
 * The last base URL reported with reportSuccess() comes first, the other ones keep their default order.
 */
QStringList baseUrls(Service service);

/** @brief preferredBaseUrl The base URL to use when only one can be used, e.g. to open a web page. */
QString preferredBaseUrl(Service service);

/**
 * @brief reportSuccess Remember that a base URL of a service answered, so that it is tried first next time.
 * Unknown base URLs are ignored.
 */
void reportSuccess(Service service, const QString &baseUrl);

/**
 * @brief setBaseUrlsForTesting Replace the base URLs of a service. An empty list restores the defaults.
 * This also forgets the base URL reported with reportSuccess().
 */
void setBaseUrlsForTesting(Service service, const QStringList &urls);

/** @brief requestTimeoutMs Time in milliseconds to wait for a reply before trying the next mirror. */
int requestTimeoutMs();

/** @brief setRequestTimeoutForTesting Change requestTimeoutMs(). Zero or less restores the default. */
void setRequestTimeoutForTesting(int milliseconds);

/**
 * @brief sesameUrl Build a Sesame query that returns XML with the SIMBAD, NED and VizieR resolvers.
 * @param baseUrl Sesame base URL from baseUrls(Service::Sesame).
 * @param name Object name to resolve.
 */
QUrl sesameUrl(const QString &baseUrl, const QString &name);

/**
 * @brief simbadUrl Build a SIMBAD URL.
 * @param baseUrl SIMBAD base URL from baseUrls(Service::Simbad).
 * @param path Path of the SIMBAD script, e.g. "/simbad/sim-id".
 */
QUrl simbadUrl(const QString &baseUrl, const QString &path);

/**
 * @brief abortAfter Abort a network reply if it has not finished after the given time.
 * The reply then finishes with QNetworkReply::OperationCanceledError, so the caller can try the next mirror.
 * The timer is owned by the reply and dies with it.
 */
void abortAfter(QNetworkReply *reply, int milliseconds);

/**
 * @brief upgradeLegacyUrl Replace a CDS host name that no longer exists with the current one.
 * alasky.u-strasbg.fr and alasky.unistra.fr become alasky.cds.unistra.fr, alaskybis.u-strasbg.fr becomes
 * alaskybis.cds.unistra.fr, and so on. CDS URLs are switched to HTTPS. Other URLs are returned unchanged.
 */
QString upgradeLegacyUrl(const QString &url);

/**
 * @brief defaultHipsMirrors Mirrors of the HiPS surveys that KStars installs by default.
 * @param id HiPS survey ID, e.g. "CDS/P/DSS2/color".
 * @return Mirror base URLs, without the main hips_service_url. Empty for other surveys.
 */
QStringList defaultHipsMirrors(const QString &id);

/** @brief Separator of the URLs in the hips_service_mirrors column of the user database. */
constexpr char HipsMirrorSeparator = ' ';

/**
 * @class MirrorFailover
 * @short Switch to the next mirror after several requests to the current one failed in a row.
 *
 * Results of requests that were sent to another mirror than the current one are ignored, so replies
 * that arrive after a switch cannot cause another switch.
 */
class MirrorFailover
{
    public:
        explicit MirrorFailover(int failureThreshold = 8);

        /** @brief setUrls Set the mirrors, the first one becomes current. Empty and duplicate URLs are dropped. */
        void setUrls(const QStringList &urls);

        const QStringList &urls() const
        {
            return m_Urls;
        }

        /** @brief current The mirror to send requests to, or an empty string when there is none. */
        QString current() const;

        int currentIndex() const
        {
            return m_Index;
        }

        /**
         * @brief reportResult Report the result of a request.
         * @param url The mirror the request was sent to.
         * @param success True if the request succeeded.
         * @return True if the current mirror changed.
         */
        bool reportResult(const QString &url, bool success);

    private:
        QStringList m_Urls;
        int m_Index { 0 };
        int m_Failures { 0 };
        int m_FailureThreshold { 8 };
};
}
