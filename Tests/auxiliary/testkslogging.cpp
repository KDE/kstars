/*
    SPDX-FileCopyrightText: 2026 KStars Team

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ksutils.h"

#include <QCoreApplication>
#include <QDirIterator>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QtTest>

// When this variable is set, the binary runs as a child process that starts the
// logging thread and returns from main() without calling Logging::Shutdown(),
// like KStars does on exit paths that never destroy the main window.
static const char *ChildModeVariable = "KSTARS_TEST_LOGGING_CHILD";
static const char *ChildMessage = "logging-thread-child-message";

class TestKSLogging : public QObject
{
        Q_OBJECT

    private slots:
        void testExitWithoutShutdown();
};

void TestKSLogging::testExitWithoutShutdown()
{
    // Keep the child's log file out of the user's data directory
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    QProcess child;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(ChildModeVariable, "1");
    env.insert("XDG_DATA_HOME", dataDir.path());
    child.setProcessEnvironment(env);
    child.start(QCoreApplication::applicationFilePath(), QStringList());
    QVERIFY(child.waitForStarted(10000));
    QVERIFY(child.waitForFinished(30000));

    // The logging thread is a static std::thread: if it is still joinable when
    // the static is destroyed, std::terminate() aborts the process (Sentry KSTARS-K4).
    QCOMPARE(child.exitStatus(), QProcess::NormalExit);
    QCOMPARE(child.exitCode(), 0);

    // Messages queued before exit must still be written to the log file.
    QByteArray logContents;
    QDirIterator logs(dataDir.path(), QStringList() << "log_*.txt", QDir::Files, QDirIterator::Subdirectories);
    while (logs.hasNext())
    {
        QFile log(logs.next());
        QVERIFY(log.open(QFile::ReadOnly));
        logContents += log.readAll();
    }
    QVERIFY2(logContents.contains(ChildMessage), "Queued log message was not written on exit");
}

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsSet(ChildModeVariable))
    {
        QCoreApplication app(argc, argv);
        KSUtils::Logging::UseFile();
        qInfo() << ChildMessage;
        return 0;
    }

    QCoreApplication app(argc, argv);
    TestKSLogging test;
    return QTest::qExec(&test, argc, argv);
}

#include "testkslogging.moc"
