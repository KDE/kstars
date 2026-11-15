/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cdsmirrors.h"

#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkReply>
#include <QTimer>

namespace CDSMirrors
{
namespace
{
QStringList defaultBaseUrls(Service service)
{
    switch (service)
    {
        case Service::Sesame:
            return
            {
                QStringLiteral("https://vizier.cds.unistra.fr/viz-bin/nph-sesame"),
                QStringLiteral("https://vizier.cfa.harvard.edu/viz-bin/nph-sesame")
            };
        case Service::Simbad:
            return
            {
                QStringLiteral("https://simbad.cds.unistra.fr"),
                QStringLiteral("https://simbad.cfa.harvard.edu")
            };
        case Service::MocServer:
            return
            {
                QStringLiteral("https://alasky.cds.unistra.fr/MocServer/query"),
                QStringLiteral("https://alaskybis.cds.unistra.fr/MocServer/query")
            };
    }
    return {};
}

struct State
{
    QMutex mutex;
    QHash<int, QStringList> overrides;
    QHash<int, QString> preferred;
    int timeoutMs { DefaultRequestTimeoutMs };
};

State &state()
{
    static State s;
    return s;
}

QStringList configuredBaseUrls(Service service)
{
    // Caller holds the mutex
    const auto overrideIt = state().overrides.constFind(static_cast<int>(service));
    if (overrideIt != state().overrides.constEnd())
        return overrideIt.value();
    return defaultBaseUrls(service);
}
}

QStringList baseUrls(Service service)
{
    QMutexLocker locker(&state().mutex);
    QStringList urls = configuredBaseUrls(service);
    const QString preferred = state().preferred.value(static_cast<int>(service));
    const int index = urls.indexOf(preferred);
    if (index > 0)
        urls.move(index, 0);
    return urls;
}

QString preferredBaseUrl(Service service)
{
    const QStringList urls = baseUrls(service);
    return urls.isEmpty() ? QString() : urls.first();
}

void reportSuccess(Service service, const QString &baseUrl)
{
    QMutexLocker locker(&state().mutex);
    if (configuredBaseUrls(service).contains(baseUrl))
        state().preferred[static_cast<int>(service)] = baseUrl;
}

void setBaseUrlsForTesting(Service service, const QStringList &urls)
{
    QMutexLocker locker(&state().mutex);
    if (urls.isEmpty())
        state().overrides.remove(static_cast<int>(service));
    else
        state().overrides[static_cast<int>(service)] = urls;
    state().preferred.remove(static_cast<int>(service));
}

int requestTimeoutMs()
{
    QMutexLocker locker(&state().mutex);
    return state().timeoutMs;
}

void setRequestTimeoutForTesting(int milliseconds)
{
    QMutexLocker locker(&state().mutex);
    state().timeoutMs = milliseconds > 0 ? milliseconds : DefaultRequestTimeoutMs;
}

QUrl sesameUrl(const QString &baseUrl, const QString &name)
{
    return QUrl(QString("%1/-oxpFI/SNV?%2").arg(baseUrl, name));
}

QUrl simbadUrl(const QString &baseUrl, const QString &path)
{
    return QUrl(baseUrl + path);
}

void abortAfter(QNetworkReply *reply, int milliseconds)
{
    if (reply == nullptr || milliseconds <= 0)
        return;

    auto *timer = new QTimer(reply);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, reply, [reply]()
    {
        if (reply->isRunning())
            reply->abort();
    });
    timer->start(milliseconds);
}

QString upgradeLegacyUrl(const QString &url)
{
    // Host names of the CDS HiPS servers that no longer resolve, and their current names.
    static const QHash<QString, QString> legacyHosts =
    {
        { QStringLiteral("alasky.u-strasbg.fr"), QStringLiteral("alasky.cds.unistra.fr") },
        { QStringLiteral("alasky.unistra.fr"), QStringLiteral("alasky.cds.unistra.fr") },
        { QStringLiteral("alaskybis.u-strasbg.fr"), QStringLiteral("alaskybis.cds.unistra.fr") },
        { QStringLiteral("alaskybis.unistra.fr"), QStringLiteral("alaskybis.cds.unistra.fr") },
        { QStringLiteral("simbad.u-strasbg.fr"), QStringLiteral("simbad.cds.unistra.fr") },
        { QStringLiteral("aladin.u-strasbg.fr"), QStringLiteral("aladin.cds.unistra.fr") },
    };

    QUrl parsed(url);
    if (!parsed.isValid())
        return url;

    const QString host = parsed.host().toLower();
    bool changed = false;

    const auto legacyIt = legacyHosts.constFind(host);
    if (legacyIt != legacyHosts.constEnd())
    {
        parsed.setHost(legacyIt.value());
        changed = true;
    }

    if (parsed.host().endsWith(QLatin1String(".cds.unistra.fr")) && parsed.scheme() == QLatin1String("http"))
    {
        parsed.setScheme(QStringLiteral("https"));
        // Drop an explicit HTTP port
        if (parsed.port() == 80)
            parsed.setPort(-1);
        changed = true;
    }

    return changed ? parsed.toString() : url;
}

QStringList defaultHipsMirrors(const QString &id)
{
    if (id == QLatin1String("CDS/P/DSS2/color"))
        return
    {
        QStringLiteral("https://alaskybis.cds.unistra.fr/DSS/DSSColor"),
        QStringLiteral("https://irsa.ipac.caltech.edu/data/hips/CDS/DSS2/color"),
        QStringLiteral("https://skies.esac.esa.int/DSSColor")
    };
    if (id == QLatin1String("CDS/P/2MASS/color"))
        return
    {
        QStringLiteral("https://alaskybis.cds.unistra.fr/2MASS/Color"),
        QStringLiteral("https://irsa.ipac.caltech.edu/data/hips/CDS/2MASS/Color")
    };
    if (id == QLatin1String("CDS/P/Fermi/color"))
        return
    {
        QStringLiteral("https://alaskybis.cds.unistra.fr/Fermi/Color"),
        QStringLiteral("https://skies.esac.esa.int/FermiColor")
    };
    return {};
}

MirrorFailover::MirrorFailover(int failureThreshold) : m_FailureThreshold(qMax(1, failureThreshold))
{
}

void MirrorFailover::setUrls(const QStringList &urls)
{
    m_Urls.clear();
    for (const auto &oneUrl : urls)
    {
        const QString trimmed = oneUrl.trimmed();
        if (!trimmed.isEmpty() && !m_Urls.contains(trimmed))
            m_Urls.append(trimmed);
    }
    m_Index = 0;
    m_Failures = 0;
}

QString MirrorFailover::current() const
{
    return m_Urls.isEmpty() ? QString() : m_Urls.at(m_Index);
}

bool MirrorFailover::reportResult(const QString &url, bool success)
{
    if (m_Urls.isEmpty() || url != current())
        return false;

    if (success)
    {
        m_Failures = 0;
        return false;
    }

    if (++m_Failures < m_FailureThreshold || m_Urls.count() < 2)
        return false;

    m_Index = (m_Index + 1) % m_Urls.count();
    m_Failures = 0;
    return true;
}
}
