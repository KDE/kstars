/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "diagnosismanager.h"

#include "Options.h"

#include <KLocalizedString>

#include <QUuid>

#include <ekos_debug.h>

namespace Ekos
{

using namespace Diagnosis;

DiagnosisManager::DiagnosisManager(QObject *parent) : QObject(parent)
{
    m_RunningTimer.setSingleShot(true);
    connect(&m_RunningTimer, &QTimer::timeout, this, [this]()
    {
        if (m_Running)
            finishItem(m_Running->diagnosisId, m_Running->itemId, false, i18n("Timed out."));
    });
}

QString DiagnosisManager::key(const QString &module, const QString &train)
{
    return module + QLatin1Char('|') + train;
}

void DiagnosisManager::registerModule(const QString &module, const ModuleHooks &hooks)
{
    m_Modules.insert(module, hooks);
}

void DiagnosisManager::unregisterModule(const QString &module)
{
    m_Modules.remove(module);

    m_Actions.erase(std::remove_if(m_Actions.begin(), m_Actions.end(), [&module](const Action & action)
    {
        return action.module == module;
    }), m_Actions.end());

    for (auto it = m_Reports.begin(); it != m_Reports.end();)
    {
        if (it.value().module == module)
        {
            m_Chains.remove(it.key());
            it = m_Reports.erase(it);
        }
        else
            ++it;
    }

    if (m_Running && m_Running->key.startsWith(module + QLatin1Char('|')))
    {
        m_RunningTimer.stop();
        m_Running.reset();
    }
}

void DiagnosisManager::registerAction(const QString &module, const QString &itemId, const ApplyFunction &apply,
                                      bool prefix)
{
    m_Actions.erase(std::remove_if(m_Actions.begin(), m_Actions.end(), [&](const Action & action)
    {
        return action.module == module && action.id == itemId;
    }), m_Actions.end());

    Action action;
    action.module = module;
    action.id = itemId;
    action.prefix = prefix;
    action.apply = apply;
    m_Actions.append(action);
}

const DiagnosisManager::Action *DiagnosisManager::findAction(const QString &module, const QString &itemId) const
{
    for (const auto &action : m_Actions)
    {
        if (action.module == module && !action.prefix && action.id == itemId)
            return &action;
    }
    for (const auto &action : m_Actions)
    {
        if (action.module == module && action.prefix && itemId.startsWith(action.id))
            return &action;
    }
    return nullptr;
}

QStringList DiagnosisManager::tried(const QString &module, const QString &train) const
{
    return m_Chains.value(key(module, train)).tried;
}

QStringList DiagnosisManager::dismissed(const QString &module) const
{
    const QString prefix = module + QLatin1Char(':');
    QStringList ids;
    for (const auto &entry : m_SessionDismissed + Options::diagnosisDismissedItems())
    {
        if (entry.startsWith(prefix) && !ids.contains(entry.mid(prefix.size())))
            ids << entry.mid(prefix.size());
    }
    return ids;
}

void DiagnosisManager::log(const QString &module, const QString &text) const
{
    const auto hooks = m_Modules.value(module);
    if (hooks.log)
        hooks.log(text);
}

void DiagnosisManager::submit(Report report)
{
    const QString reportKey = key(report.module, report.train);
    auto &chain = m_Chains[reportKey];

    // A manual item was marked done, but the module failed again
    if (!chain.pendingVerifyItem.isEmpty())
    {
        update(chain.pendingVerifyReport, chain.pendingVerifyItem, QStringLiteral("verify_failed"));
        chain.pendingVerifyItem.clear();
        chain.pendingVerifyReport.clear();
    }

    chain.attempt = chain.tried.isEmpty() ? 1 : chain.attempt + 1;
    chain.lastAppliedItem.clear();
    chain.lastAppliedReport.clear();

    if (report.id.isEmpty())
        report.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!report.created.isValid())
        report.created = QDateTime::currentDateTimeUtc();
    report.attempt = chain.attempt;
    report.tried = chain.tried;

    m_Reports.insert(reportKey, report);

    const auto hooks = m_Modules.value(report.module);
    for (const auto &line : toLogLines(report, hooks.name.isEmpty() ? report.module : hooks.name))
        log(report.module, line);

    qCInfo(KSTARS_EKOS) << "Diagnosis" << report.module << report.train << report.reasonKey << "attempt" << report.attempt
                        << "items" << [&report]()
    {
        QStringList ids;
        for (const auto &item : report.items)
            ids << item.id;
        return ids;
    }
    ();

    Q_EMIT newDiagnosis(report.module, toJson(report));
}

QJsonArray DiagnosisManager::checkup(const QString &module, const QString &train)
{
    const auto hooks = m_Modules.value(module);
    if (!hooks.checkup)
        return reportsJson(module, train);

    auto report = hooks.checkup(train);
    if (!report)
        return reportsJson(module, train);

    const QString reportKey = key(report->module, report->train);
    const auto cached = m_Reports.constFind(reportKey);
    if (cached != m_Reports.constEnd())
    {
        // A failure report explains more, and an item that is running belongs to the cached report
        const bool busyHere = m_Running && m_Running->key == reportKey;
        if (!isCheckup(*cached) || busyHere)
            return QJsonArray{toJson(*cached)};

        // Same findings: keep the cached report, so the items a client shows stay valid
        Report same = *report;
        same.id = cached->id;
        same.created = cached->created;
        same.attempt = cached->attempt;
        same.tried = cached->tried;
        if (toJson(same) == toJson(*cached))
            return QJsonArray{toJson(*cached)};
    }

    report->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    report->created = QDateTime::currentDateTimeUtc();
    report->attempt = 1;
    report->tried.clear();
    m_Reports.insert(reportKey, *report);

    // Only findings go to the log, not every look at a setup that is fine
    if (!report->items.isEmpty())
    {
        for (const auto &line : toLogLines(*report, hooks.name.isEmpty() ? module : hooks.name))
            log(module, line);
    }
    qCInfo(KSTARS_EKOS) << "Diagnosis check-up" << module << report->train << "items" << report->items.size();

    return QJsonArray{toJson(*report)};
}

void DiagnosisManager::moduleSucceeded(const QString &module, const QString &train)
{
    const QString reportKey = key(module, train);
    if (!m_Chains.contains(reportKey) && !m_Reports.contains(reportKey))
        return;

    const Chain chain = m_Chains.take(reportKey);
    m_Reports.remove(reportKey);

    if (!chain.pendingVerifyItem.isEmpty())
    {
        update(chain.pendingVerifyReport, chain.pendingVerifyItem, QStringLiteral("verified"));
        log(module, i18n("Diagnosis: the fix worked."));
    }
    else if (!chain.lastAppliedItem.isEmpty())
    {
        update(chain.lastAppliedReport, chain.lastAppliedItem, QStringLiteral("fixed"));
        log(module, i18n("Diagnosis: the fix worked."));
    }
}

QList<Report> DiagnosisManager::reports(const QString &module, const QString &train) const
{
    QList<Report> list;
    for (const auto &report : m_Reports)
    {
        if (!module.isEmpty() && report.module != module)
            continue;
        if (!train.isEmpty() && report.train != train)
            continue;
        list << report;
    }
    std::sort(list.begin(), list.end(), [](const Report & a, const Report & b)
    {
        return a.created < b.created;
    });
    return list;
}

QJsonArray DiagnosisManager::reportsJson(const QString &module, const QString &train) const
{
    QJsonArray array;
    for (const auto &report : reports(module, train))
        array.append(toJson(report));
    return array;
}

std::optional<Report> DiagnosisManager::report(const QString &diagnosisId) const
{
    for (const auto &report : m_Reports)
    {
        if (report.id == diagnosisId)
            return report;
    }
    return std::nullopt;
}

QString DiagnosisManager::keyOfReport(const QString &diagnosisId) const
{
    for (auto it = m_Reports.constBegin(); it != m_Reports.constEnd(); ++it)
    {
        if (it.value().id == diagnosisId)
            return it.key();
    }
    return QString();
}

QJsonObject DiagnosisManager::update(const QString &diagnosisId, const QString &itemId, const QString &state,
                                     const QString &message, const QString &reason, const QJsonObject &extra)
{
    QJsonObject object = extra;
    object.insert("diagnosisId", diagnosisId);
    object.insert("itemId", itemId);
    object.insert("state", state);
    if (!message.isEmpty())
        object.insert("message", message);
    if (!reason.isEmpty())
        object.insert("reason", reason);

    Q_EMIT diagnosisUpdated(object);
    return object;
}

QJsonObject DiagnosisManager::refuse(const QString &diagnosisId, const QString &itemId, const QString &reason)
{
    qCInfo(KSTARS_EKOS) << "Diagnosis item refused:" << diagnosisId << itemId << reason;
    return update(diagnosisId, itemId, QStringLiteral("refused"), QString(), reason);
}

QJsonObject DiagnosisManager::apply(const QString &diagnosisId, const QString &itemId, bool confirmed)
{
    const QString reportKey = keyOfReport(diagnosisId);
    if (reportKey.isEmpty())
        return refuse(diagnosisId, itemId, QStringLiteral("unknown_item"));

    const Report report = m_Reports.value(reportKey);
    const Item *item = findItem(report, itemId);
    if (item == nullptr)
        return refuse(diagnosisId, itemId, QStringLiteral("unknown_item"));

    if (item->kind != Kind::Auto)
        return refuse(diagnosisId, itemId, QStringLiteral("not_applicable"));

    if (m_Running)
        return refuse(diagnosisId, itemId, QStringLiteral("busy"));

    if (item->cost.testFlag(MovesHardware) && !confirmed)
        return refuse(diagnosisId, itemId, QStringLiteral("needs_confirm"));

    const Action *action = findAction(report.module, itemId);
    if (action == nullptr || !action->apply)
        return refuse(diagnosisId, itemId, QStringLiteral("unknown_item"));

    log(report.module, i18n("Diagnosis: applying \"%1\"...", item->title));
    qCInfo(KSTARS_EKOS) << "Diagnosis applying" << report.module << itemId;

    // Copy what the action needs: it may submit a new report synchronously
    const Item itemCopy = *item;
    Running running;
    running.diagnosisId = diagnosisId;
    running.itemId = itemId;
    running.key = reportKey;
    m_Running = running;

    const ApplyResult result = action->apply(itemCopy, report);

    switch (result.outcome)
    {
        case Outcome::Running:
        {
            // The action already called finishItem(), which published the result
            if (!m_Running || m_Running->diagnosisId != diagnosisId || m_Running->itemId != itemId)
                return QJsonObject{{"diagnosisId", diagnosisId}, {"itemId", itemId}, {"state", "finished"}};
            if (result.revert)
                m_Reverts.insert(diagnosisId + QLatin1Char('|') + itemId, result.revert);
            m_RunningTimer.start(result.timeoutMs);
            return update(diagnosisId, itemId, QStringLiteral("running"), result.message);
        }

        case Outcome::Done:
        {
            m_Running.reset();
            // What changed, so a client can show it and offer to undo it
            QJsonObject change;
            if (result.oldValue.isValid())
                change.insert("oldValue", QJsonValue::fromVariant(result.oldValue));
            if (result.newValue.isValid())
                change.insert("newValue", QJsonValue::fromVariant(result.newValue));
            if (result.revert)
                change.insert("canRevert", true);
            QJsonObject done = update(diagnosisId, itemId, QStringLiteral("done"), result.message, QString(), change);
            itemApplied(reportKey, diagnosisId, itemId, result);
            return done;
        }

        case Outcome::Failed:
            break;
    }

    m_Running.reset();
    log(report.module, i18n("Diagnosis: \"%1\" failed. %2", itemCopy.title, result.message));
    return update(diagnosisId, itemId, QStringLiteral("failed"), result.message);
}

void DiagnosisManager::finishItem(const QString &diagnosisId, const QString &itemId, bool ok, const QString &message)
{
    if (!m_Running || m_Running->diagnosisId != diagnosisId || m_Running->itemId != itemId)
        return;

    const QString reportKey = m_Running->key;
    m_RunningTimer.stop();
    m_Running.reset();

    const QString module = reportKey.section(QLatin1Char('|'), 0, 0);
    if (!ok)
    {
        m_Reverts.remove(diagnosisId + QLatin1Char('|') + itemId);
        log(module, i18n("Diagnosis: the fix failed. %1", message));
        update(diagnosisId, itemId, QStringLiteral("failed"), message);
        return;
    }

    QJsonObject change;
    if (m_Reverts.contains(diagnosisId + QLatin1Char('|') + itemId))
        change.insert("canRevert", true);
    update(diagnosisId, itemId, QStringLiteral("done"), message, QString(), change);
    ApplyResult result;
    result.outcome = Outcome::Done;
    itemApplied(reportKey, diagnosisId, itemId, result);
}

void DiagnosisManager::itemApplied(const QString &reportKey, const QString &diagnosisId, const QString &itemId,
                                   const ApplyResult &result)
{
    if (result.revert)
        m_Reverts.insert(diagnosisId + QLatin1Char('|') + itemId, result.revert);

    auto &chain = m_Chains[reportKey];
    if (!chain.tried.contains(itemId))
        chain.tried << itemId;
    chain.lastAppliedItem = itemId;
    chain.lastAppliedReport = diagnosisId;

    const auto cached = m_Reports.value(reportKey);
    const Item *item = findItem(cached, itemId);
    if (item == nullptr || !item->autoRetry || item->cost.testFlag(MovesHardware))
        return;

    const QString module = reportKey.section(QLatin1Char('|'), 0, 0);
    const QString train = reportKey.section(QLatin1Char('|'), 1);
    const auto hooks = m_Modules.value(module);
    if (!hooks.retry)
        return;

    log(module, i18n("Diagnosis: retrying..."));
    update(diagnosisId, itemId, QStringLiteral("waiting"));
    if (!hooks.retry(train))
        update(diagnosisId, itemId, QStringLiteral("failed"), i18n("The retry could not start."));
}

QJsonObject DiagnosisManager::mark(const QString &diagnosisId, const QString &itemId, const QString &action)
{
    if (action == QLatin1String("revert"))
    {
        const QString revertKey = diagnosisId + QLatin1Char('|') + itemId;
        if (!m_Reverts.contains(revertKey))
            return refuse(diagnosisId, itemId, QStringLiteral("unknown_item"));
        auto revert = m_Reverts.take(revertKey);
        revert();
        const auto cached = report(diagnosisId);
        if (cached)
            log(cached->module, i18n("Diagnosis: the change was reverted."));
        return update(diagnosisId, itemId, QStringLiteral("reverted"));
    }

    const QString reportKey = keyOfReport(diagnosisId);
    if (reportKey.isEmpty())
        return refuse(diagnosisId, itemId, QStringLiteral("unknown_item"));

    auto &cached = m_Reports[reportKey];
    const Item *item = findItem(cached, itemId);
    if (item == nullptr)
        return refuse(diagnosisId, itemId, QStringLiteral("unknown_item"));

    if (action == QLatin1String("done"))
    {
        if (item->kind != Kind::Manual && item->kind != Kind::Open)
            return refuse(diagnosisId, itemId, QStringLiteral("not_applicable"));
        if (m_Running)
            return refuse(diagnosisId, itemId, QStringLiteral("busy"));

        const bool retry = item->autoRetry;
        auto &chain = m_Chains[reportKey];
        if (!chain.tried.contains(itemId))
            chain.tried << itemId;
        chain.pendingVerifyItem = itemId;
        chain.pendingVerifyReport = diagnosisId;

        QJsonObject done = update(diagnosisId, itemId, QStringLiteral("done"));

        const QString module = cached.module;
        const QString train = cached.train;
        const auto hooks = m_Modules.value(module);
        // Manual Done is a tap by the user, so the retry runs even for items that move hardware
        if (retry && hooks.retry)
        {
            log(module, i18n("Diagnosis: retrying..."));
            update(diagnosisId, itemId, QStringLiteral("waiting"));
            if (!hooks.retry(train))
                update(diagnosisId, itemId, QStringLiteral("verify_failed"), i18n("The retry could not start."));
        }
        return done;
    }

    if (action == QLatin1String("dismiss") || action == QLatin1String("dismiss_forever"))
    {
        const QString entry = cached.module + QLatin1Char(':') + itemId;
        if (!m_SessionDismissed.contains(entry))
            m_SessionDismissed << entry;

        if (action == QLatin1String("dismiss_forever"))
        {
            auto stored = Options::diagnosisDismissedItems();
            if (!stored.contains(entry))
            {
                stored << entry;
                Options::setDiagnosisDismissedItems(stored);
            }
        }

        cached.items.erase(std::remove_if(cached.items.begin(), cached.items.end(), [&itemId](const Item & oneItem)
        {
            return oneItem.id == itemId;
        }), cached.items.end());

        return update(diagnosisId, itemId, QStringLiteral("dismissed"));
    }

    return refuse(diagnosisId, itemId, QStringLiteral("unknown_action"));
}

void DiagnosisManager::setClientUtc(const QDateTime &utc)
{
    if (!utc.isValid())
        return;
    m_ClientOffsetMs = QDateTime::currentDateTimeUtc().msecsTo(utc.toUTC());
}

std::optional<QDateTime> DiagnosisManager::clientUtc() const
{
    if (!m_ClientOffsetMs)
        return std::nullopt;
    return QDateTime::currentDateTimeUtc().addMSecs(*m_ClientOffsetMs);
}

void DiagnosisManager::refresh(const QString &module, bool publish)
{
    for (auto it = m_Reports.begin(); it != m_Reports.end(); ++it)
    {
        if (!module.isEmpty() && it.value().module != module)
            continue;

        const auto hooks = m_Modules.value(it.value().module);
        if (!hooks.refresh)
            continue;

        auto rebuilt = hooks.refresh(it.value());
        if (!rebuilt)
            continue;

        // Same failure, same id: items of the old report stay valid for apply()
        rebuilt->id = it.value().id;
        rebuilt->created = it.value().created;
        rebuilt->attempt = it.value().attempt;
        rebuilt->tried = it.value().tried;

        if (toJson(*rebuilt) == toJson(it.value()))
            continue;

        // A check-up with new findings is a new check-up, as in checkup(): clients tell them apart by id. One whose
        // item is running stays.
        if (isCheckup(it.value()))
        {
            if (m_Running && m_Running->key == it.key())
                continue;
            rebuilt->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            rebuilt->created = QDateTime::currentDateTimeUtc();
            rebuilt->attempt = 1;
            rebuilt->tried.clear();
        }

        it.value() = *rebuilt;
        if (publish)
            Q_EMIT newDiagnosis(rebuilt->module, toJson(*rebuilt));
    }
}

void DiagnosisManager::clear()
{
    m_Reports.clear();
    m_Chains.clear();
    m_Reverts.clear();
    m_RunningTimer.stop();
    m_Running.reset();
}

}
