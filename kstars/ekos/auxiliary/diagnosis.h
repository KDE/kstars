/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDateTime>
#include <QFlags>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <optional>

namespace Ekos
{
/**
 * @namespace Ekos::Diagnosis
 * @short Value types for a failure diagnosis: what went wrong, the evidence, and the suggested fixes.
 *
 * A module that fails builds a Report from facts it already holds, with a pure rules function. The
 * DiagnosisManager caches the report, writes it to the module log and sends it to clients. Clients only
 * send back the report id and an item id; the manager maps them to its own action table.
 *
 * These types carry no QObject and no widgets so the rules and the serialisation can be unit tested.
 */
namespace Diagnosis
{

/** What kind of fix an item offers. */
enum class Kind
{
    Auto,   ///< KStars can apply it, then retries.
    Manual, ///< The user does something, then taps Done.
    Open,   ///< Opens a settings screen (openTarget).
    Link,   ///< Points at another module's diagnosis (linkModule).
    Info    ///< Explains something; nothing to apply.
};

/** Order of the items: hardware problems first, tuning last. */
enum class Tier
{
    Hardware = 0,
    Configuration = 1,
    Data = 2,
    Tuning = 3
};

/** What applying an item costs the user. */
enum Cost
{
    NoCost = 0,
    MovesHardware = 1,   ///< Moves equipment. Always needs a tap, never chained.
    NeedsInternet = 2,   ///< Downloads data (see Item::downloadBytes).
    ChangesSettings = 4, ///< Changes a saved setting. The old value is kept for Revert.
    StopsActivity = 8    ///< Stops another activity, e.g. another module's exposure.
};
Q_DECLARE_FLAGS(Costs, Cost)

/** One fact shown to the user, e.g. the exposure time. */
struct Evidence
{
    enum Level
    {
        Bad,
        Ok,
        Info
    };

    QString key;            ///< Stable key, e.g. "exposure".
    QString label;          ///< Translated label.
    QVariant value;         ///< Number or string, never translated.
    QString unit;           ///< "s", "arcmin", "px"...
    Level level {Info};
};

/** How KStars checks that an item worked. */
struct Verify
{
    QString action;         ///< "retry", "device_state", "index_scan", "test_frame"...
    QString metric;         ///< "park_status", "series_installed", "stars"...
    QString op;             ///< ">=", "<=", "=="
    double threshold {0};
};

/** One suggested fix. */
struct Item
{
    QString id;             ///< Stable id, e.g. "align_bin_max" or "index_install_07".
    Kind kind {Kind::Info};
    Tier tier {Tier::Tuning};
    bool blocker {true};    ///< False for warnings.
    bool autoRetry {true};  ///< Retry the module after an auto item is applied.
    Costs cost;
    qint64 downloadBytes {0};
    QString title;          ///< Translated.
    QString why;            ///< Translated, with numbers.
    QStringList steps;      ///< Translated steps of a manual item.
    std::optional<Verify> verify;
    QString openTarget;     ///< "optical_train", "indi_panel:<device>:<group>", "align_settings:<page>"...
    QString linkModule;
    QString linkDiagnosisId;
};

/** The diagnosis of one failure. */
struct Report
{
    QString id;             ///< Unique id, without braces.
    QString module;         ///< "align", ...
    QString train;          ///< Optical train name.
    int state {0};          ///< Module state enum value.
    QString status;         ///< Untranslated module state name, e.g. "Failed".
    int reason {0};         ///< Module reason enum value.
    QString reasonKey;      ///< Stable machine key, e.g. "solver_failed".
    QString title;          ///< Translated.
    QString summary;        ///< Translated.
    QList<Evidence> evidence;
    QStringList ruledOut;   ///< Translated lines.
    QStringList notChecked; ///< Translated lines.
    QList<Item> items;      ///< Ordered with order().
    QDateTime created;
    int attempt {1};
    QStringList tried;      ///< Item ids applied in this failure chain.
};

/**
 * @brief order Sort items by tier, then blockers before warnings. Items that compare equal keep their order.
 */
void order(QList<Item> &items);

/** Find an item by id. Returns nullptr if the report has no such item. */
const Item *findItem(const Report &report, const QString &itemId);

QString kindKey(Kind kind);
QString levelKey(Evidence::Level level);

QJsonObject toJson(const Evidence &evidence);
QJsonObject toJson(const Verify &verify);
QJsonObject toJson(const Item &item);
QJsonObject toJson(const Report &report);

/**
 * @brief toLogLines The report as lines for a module log, translated. Same content as the JSON.
 * @param report the report
 * @param moduleName translated module name, e.g. "Align"
 */
QStringList toLogLines(const Report &report, const QString &moduleName);

}
}

Q_DECLARE_OPERATORS_FOR_FLAGS(Ekos::Diagnosis::Costs)
