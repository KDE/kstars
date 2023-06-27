#ifndef TESTHELPERS_H
#define TESTHELPERS_H

#include "config-kstars.h"

#include <QObject>
#include <QTest>
#include <QStandardPaths>

#include "kstars.h"
#include "kspaths.h"

/** @brief Helper to clean application user folders when in test mode.
 *
 * It verifies that App(Data|Config|Cache)Location folders can be removed, that KSPaths::writableLocation
 * will not recreate them by itself and that those folders can be recreated manually.
 *
 * @param recreate is a boolean that requires the helper to recreate the application user folders after removal.
 */
#define KTEST_CLEAN_TEST(recreate) do { \
    if (!QStandardPaths::isTestModeEnabled()) \
        qFatal("Helper KTEST_CLEAN_TEST only works in test mode."); \
    QList<QStandardPaths::StandardLocation> const locs = { \
        QStandardPaths::AppLocalDataLocation, \
        QStandardPaths::AppConfigLocation, \
        QStandardPaths::CacheLocation }; \
    for (auto loc: locs) { \
        QString const path = KSPaths::writableLocation(loc); \
        if (!QDir(path).removeRecursively()) \
            qFatal("Local application location '%s' must be removable.", qPrintable(path)); \
        if (QDir(KSPaths::writableLocation(loc)).exists()) \
            qFatal("Local application location '%s' must not exist after having been removed.", qPrintable(path)); \
        if (recreate) \
            if (!QDir(path).mkpath(".")) \
                qFatal("Local application location '%s' must be recreatable.", qPrintable(path)); \
        }} while(false)

#define KTEST_CLEAN_RCFILE() do { \
    if (!QStandardPaths::isTestModeEnabled()) \
        qFatal("Helper KTEST_CLEAN_RCFILE only works in test mode."); \
    QString const rcfilepath = KSPaths::locate(QStandardPaths::GenericConfigLocation, qAppName() + "rc"); \
    if (!rcfilepath.isEmpty() && QFileInfo(rcfilepath).exists()) { \
        if (!QFile(rcfilepath).remove()) \
            qFatal("Local application location RC file must be removable."); \
    }} while(false)

/** @brief Helper to begin a test.
 *
 * For now, this puts the application paths in test mode, and removes and recreates
 * the three application user folders.
 */
#define KTEST_BEGIN() do { \
    QStandardPaths::setTestModeEnabled(true); \
    KTEST_CLEAN_TEST(true); \
    KTEST_CLEAN_RCFILE(); } while(false)

/** @brief Helper to end a test.
 *
 * For now, this removes the three application user folders.
 */
#define KTEST_END() do { \
    KTEST_CLEAN_RCFILE(); \
    KTEST_CLEAN_TEST(false); } while(false)

#endif // TESTHELPERS_H
