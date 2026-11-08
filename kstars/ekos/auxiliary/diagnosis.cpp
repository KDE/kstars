/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "diagnosis.h"

#include <KLocalizedString>

#include <QJsonArray>

#include <algorithm>

namespace Ekos
{
namespace Diagnosis
{

namespace
{
// What applying an item costs: settings first, then downloads, then moving equipment
int costRank(const Item &item)
{
    if (item.cost.testFlag(MovesHardware))
        return 2;
    if (item.cost.testFlag(NeedsInternet))
        return 1;
    return 0;
}
}

void order(QList<Item> &items)
{
    std::stable_sort(items.begin(), items.end(), [](const Item & a, const Item & b)
    {
        if (a.likelihood != b.likelihood)
            return static_cast<int>(a.likelihood) < static_cast<int>(b.likelihood);
        // Blockers before warnings
        if (a.blocker != b.blocker)
            return a.blocker;
        if (a.tier != b.tier)
            return static_cast<int>(a.tier) < static_cast<int>(b.tier);
        return costRank(a) < costRank(b);
    });
}

QString checkupReasonKey()
{
    return QStringLiteral("checkup");
}

bool isCheckup(const Report &report)
{
    return report.reasonKey == checkupReasonKey();
}

const Item *findItem(const Report &report, const QString &itemId)
{
    for (const auto &item : report.items)
    {
        if (item.id == itemId)
            return &item;
    }
    return nullptr;
}

QString kindKey(Kind kind)
{
    switch (kind)
    {
        case Kind::Auto:
            return QStringLiteral("auto");
        case Kind::Manual:
            return QStringLiteral("manual");
        case Kind::Open:
            return QStringLiteral("open");
        case Kind::Link:
            return QStringLiteral("link");
        case Kind::Info:
            break;
    }
    return QStringLiteral("info");
}

QString likelihoodKey(Likelihood likelihood)
{
    switch (likelihood)
    {
        case Likelihood::Likely:
            return QStringLiteral("likely");
        case Likelihood::Possible:
            break;
        case Likelihood::Check:
            return QStringLiteral("check");
    }
    return QStringLiteral("possible");
}

QString levelKey(Evidence::Level level)
{
    switch (level)
    {
        case Evidence::Bad:
            return QStringLiteral("bad");
        case Evidence::Ok:
            return QStringLiteral("ok");
        case Evidence::Info:
            break;
    }
    return QStringLiteral("info");
}

QJsonObject toJson(const Evidence &evidence)
{
    QJsonObject object =
    {
        {"key", evidence.key},
        {"label", evidence.label},
        {"value", QJsonValue::fromVariant(evidence.value)},
        {"level", levelKey(evidence.level)}
    };
    if (!evidence.unit.isEmpty())
        object.insert("unit", evidence.unit);
    return object;
}

QJsonObject toJson(const Verify &verify)
{
    return
    {
        {"action", verify.action},
        {"metric", verify.metric},
        {"op", verify.op},
        {"threshold", verify.threshold}
    };
}

QJsonObject toJson(const Item &item)
{
    QJsonObject cost =
    {
        {"movesHardware", item.cost.testFlag(MovesHardware)},
        {"needsInternet", item.cost.testFlag(NeedsInternet)},
        {"changesSettings", item.cost.testFlag(ChangesSettings)},
        {"stopsActivity", item.cost.testFlag(StopsActivity)}
    };
    if (item.downloadBytes > 0)
        cost.insert("downloadBytes", item.downloadBytes);

    QJsonObject object =
    {
        {"id", item.id},
        {"kind", kindKey(item.kind)},
        {"likelihood", likelihoodKey(item.likelihood)},
        {"tier", static_cast<int>(item.tier)},
        {"blocker", item.blocker},
        {"autoRetry", item.autoRetry},
        {"cost", cost},
        {"title", item.title},
        {"why", item.why}
    };

    if (!item.steps.isEmpty())
        object.insert("steps", QJsonArray::fromStringList(item.steps));
    if (item.verify)
        object.insert("verify", toJson(*item.verify));
    if (!item.openTarget.isEmpty())
        object.insert("openTarget", item.openTarget);
    if (!item.linkModule.isEmpty())
        object.insert("linkModule", item.linkModule);
    if (!item.linkDiagnosisId.isEmpty())
        object.insert("linkDiagnosisId", item.linkDiagnosisId);

    return object;
}

QJsonObject toJson(const Report &report)
{
    QJsonArray evidence;
    for (const auto &oneEvidence : report.evidence)
        evidence.append(toJson(oneEvidence));

    QJsonArray items;
    for (const auto &item : report.items)
        items.append(toJson(item));

    return
    {
        {"id", report.id},
        {"module", report.module},
        {"train", report.train},
        {"state", report.state},
        {"status", report.status},
        {"reason", report.reason},
        {"reasonKey", report.reasonKey},
        {"title", report.title},
        {"summary", report.summary},
        {"evidence", evidence},
        {"ruledOut", QJsonArray::fromStringList(report.ruledOut)},
        {"notChecked", QJsonArray::fromStringList(report.notChecked)},
        {"created", report.created.toUTC().toString(Qt::ISODate)},
        {"attempt", report.attempt},
        {"tried", QJsonArray::fromStringList(report.tried)},
        {"items", items}
    };
}

QStringList toLogLines(const Report &report, const QString &moduleName)
{
    QStringList lines;

    if (isCheckup(report))
    {
        if (report.summary.isEmpty())
            lines << i18nc("%1 module name, %2 result", "%1 check-up: %2", moduleName, report.title);
        else
            lines << i18nc("%1 module name, %2 result, %3 explanation", "%1 check-up: %2. %3", moduleName, report.title,
                           report.summary);
    }
    else if (report.summary.isEmpty())
        lines << i18nc("%1 module name, %2 cause", "%1 failed: %2", moduleName, report.title);
    else
        lines << i18nc("%1 module name, %2 cause, %3 explanation", "%1 failed: %2. %3", moduleName, report.title,
                       report.summary);

    if (!report.ruledOut.isEmpty())
        lines << i18nc("%1 list of things that were checked", "Checked: %1", report.ruledOut.join(QLatin1Char(' ')));
    if (!report.notChecked.isEmpty())
        lines << i18nc("%1 list of things that could not be checked", "Not checked: %1",
                       report.notChecked.join(QLatin1Char(' ')));

    if (!report.items.isEmpty())
    {
        lines << i18n("Suggested:");
        int number = 1;
        for (const auto &item : report.items)
        {
            QStringList notes;
            if (item.likelihood == Likelihood::Likely)
                notes << i18n("likely cause");
            if (item.cost.testFlag(MovesHardware))
                notes << i18n("moves equipment");
            if (item.cost.testFlag(NeedsInternet))
            {
                if (item.downloadBytes > 0)
                    notes << i18n("download about %1 MB", qRound64(item.downloadBytes / 1.0e6));
                else
                    notes << i18n("needs internet");
            }
            if (item.cost.testFlag(StopsActivity))
                notes << i18n("stops another activity");

            QString line = QString(" %1. %2").arg(number++).arg(item.title);
            if (!notes.isEmpty())
                line += QString(" (%1)").arg(notes.join(QLatin1String(", ")));
            lines << line;
        }
    }

    return lines;
}

}
}
