/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "diagnosis.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <functional>
#include <optional>

namespace Ekos
{

/**
 * @class DiagnosisManager
 * @short Keeps the latest failure diagnosis of each module and runs the items it produced.
 *
 * Modules submit a Diagnosis::Report when they fail and register the actions behind their auto items.
 * Clients (EkosLive, D-Bus) only send a report id and an item id: the manager looks the item up in the
 * cached report and in its own action table, so no option names or values cross the wire.
 *
 * Rules:
 * - apply() only runs auto items of a cached report. Unknown items and stale report ids are refused.
 * - One item runs at a time. Items that move hardware need confirmed == true, and are never run by the
 *   manager on its own.
 * - After an auto item is applied its id is added to the chain's tried list and, if the item allows it,
 *   the module is retried. A new failure of the same module and train builds a new report that skips
 *   the tried items. A success closes the chain and reports the item that fixed it.
 * - Setting changes keep a revert function, run by mark(..., "revert").
 *
 * Everything runs on the GUI thread.
 */
class DiagnosisManager : public QObject
{
        Q_OBJECT

    public:
        enum class Outcome
        {
            Done,       ///< Applied synchronously.
            Running,    ///< The action calls finishItem() when it is done.
            Failed      ///< Could not be applied.
        };

        /** What an action returns when it is applied. */
        struct ApplyResult
        {
            Outcome outcome {Outcome::Failed};
            QString message;
            /// Restores a changed setting. Empty for items that change nothing.
            std::function<void()> revert;
            QVariant oldValue;
            QVariant newValue;
            /// For Running items: give up after this long.
            int timeoutMs {120000};
        };

        using ApplyFunction = std::function<ApplyResult(const Diagnosis::Item &item, const Diagnosis::Report &report)>;

        /** What the manager needs from a module. */
        struct ModuleHooks
        {
            /// Translated module name, for the log.
            QString name;
            /// Run the module again after a fix. Returns false if it could not start.
            std::function<bool(const QString &train)> retry;
            /// Write a line to the module log.
            std::function<void(const QString &text)> log;
            /// Rebuild a cached report, e.g. after a client sent its time. May return nothing.
            std::function<std::optional<Diagnosis::Report>(const Diagnosis::Report &report)> refresh;
        };

        explicit DiagnosisManager(QObject *parent = nullptr);

        /** Register a module. Replaces earlier hooks of the same module. */
        void registerModule(const QString &module, const ModuleHooks &hooks);

        /** Forget a module: its hooks, actions and cached reports. Call when the module is destroyed. */
        void unregisterModule(const QString &module);

        /**
         * @brief registerAction Register the action behind an auto item.
         * @param module module key, e.g. "align"
         * @param itemId item id, or an id prefix if prefix is true (e.g. "index_install_")
         */
        void registerAction(const QString &module, const QString &itemId, const ApplyFunction &apply, bool prefix = false);

        /** Item ids applied in the current failure chain of a module and train. */
        QStringList tried(const QString &module, const QString &train) const;

        /** Item ids of a module the user dismissed, for this session or for good. */
        QStringList dismissed(const QString &module) const;

        /**
         * @brief submit Cache a new report, write it to the module log and publish it.
         * The manager fills the id, the creation time, the attempt number and the tried list.
         */
        void submit(Diagnosis::Report report);

        /** The module succeeded: close its failure chain and drop its cached report. */
        void moduleSucceeded(const QString &module, const QString &train);

        /** Cached reports, optionally of one module and train. */
        QList<Diagnosis::Report> reports(const QString &module = QString(), const QString &train = QString()) const;
        QJsonArray reportsJson(const QString &module = QString(), const QString &train = QString()) const;
        std::optional<Diagnosis::Report> report(const QString &diagnosisId) const;

        /**
         * @brief apply Apply an auto item of a cached report.
         * @param confirmed true when a person asked for it (a tap, a D-Bus call). Hardware items need it.
         * @return the first update, as also emitted by diagnosisUpdated: state "running", "done", "failed" or "refused"
         */
        QJsonObject apply(const QString &diagnosisId, const QString &itemId, bool confirmed = true);

        /**
         * @brief mark Handle a user action on an item: "done", "dismiss", "dismiss_forever" or "revert".
         * @return the update, as also emitted by diagnosisUpdated
         */
        QJsonObject mark(const QString &diagnosisId, const QString &itemId, const QString &action);

        /** Called by a Running action when it ends. */
        void finishItem(const QString &diagnosisId, const QString &itemId, bool ok, const QString &message = QString());

        /** Id of the item running now, if any. */
        bool isBusy() const
        {
            return m_Running.has_value();
        }

        /** Store the time of a client. Only compared by the rules, never applied. */
        void setClientUtc(const QDateTime &utc);
        /** The client time, advanced by the local clock since it was received. */
        std::optional<QDateTime> clientUtc() const;

        /**
         * @brief refresh Rebuild the cached reports of a module (all modules if empty).
         * @param publish emit newDiagnosis for the reports that changed
         */
        void refresh(const QString &module = QString(), bool publish = true);

        /** Drop all cached reports and chains. Dismissals are kept. */
        void clear();

    Q_SIGNALS:
        /** A report was created or replaced. */
        void newDiagnosis(const QString &module, const QJsonObject &report);
        /** Progress of an item: {diagnosisId, itemId, state, message?, value?, reason?}. */
        void diagnosisUpdated(const QJsonObject &update);

    private:
        struct Action
        {
            QString module;
            QString id;
            bool prefix {false};
            ApplyFunction apply;
        };

        struct Chain
        {
            QStringList tried;
            int attempt {0};
            /// Item applied last, reported as the fix on success
            QString lastAppliedItem;
            QString lastAppliedReport;
            /// Manual item marked done, waiting for the retry result
            QString pendingVerifyItem;
            QString pendingVerifyReport;
        };

        struct Running
        {
            QString diagnosisId;
            QString itemId;
            QString key;
        };

        static QString key(const QString &module, const QString &train);
        const Action *findAction(const QString &module, const QString &itemId) const;
        QString keyOfReport(const QString &diagnosisId) const;
        QJsonObject update(const QString &diagnosisId, const QString &itemId, const QString &state,
                           const QString &message = QString(), const QString &reason = QString());
        QJsonObject refuse(const QString &diagnosisId, const QString &itemId, const QString &reason);
        void itemApplied(const QString &key, const QString &diagnosisId, const QString &itemId, const ApplyResult &result);
        void log(const QString &module, const QString &text) const;

        QHash<QString, ModuleHooks> m_Modules;
        QList<Action> m_Actions;
        QHash<QString, Diagnosis::Report> m_Reports;
        QHash<QString, Chain> m_Chains;
        /// Revert functions by "diagnosisId|itemId"
        QHash<QString, std::function<void()>> m_Reverts;
        QStringList m_SessionDismissed;
        std::optional<Running> m_Running;
        QTimer m_RunningTimer;
        std::optional<qint64> m_ClientOffsetMs;
};

}
