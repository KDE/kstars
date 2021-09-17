/*
    SPDX-FileCopyrightText: 2017 Csaba Kertesz <csaba.kertesz@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kstars_ui_tests.h"

#include "kswizard.h"
#include "config-kstars.h"
#include "auxiliary/kspaths.h"
#include "test_kstars_startup.h"

#if defined(HAVE_INDI)
#include "test_ekos_wizard.h"
#include "test_ekos.h"
#include "test_ekos_simulator.h"
#include "test_ekos_focus.h"
#include "test_ekos_guide.h"
#include "ekos/manager.h"
#include "ekos/profileeditor.h"
#include "ekos/profilewizard.h"
#endif

#include <KActionCollection>
#include <KTipDialog>
#include <KCrash/KCrash>
#include <Options.h>

#include <QFuture>
#include <QtConcurrentRun>
#include <QtTest>
#include <QTest>
#include <QDateTime>
#include <QStandardPaths>
#include <QFileInfo>

#include <ctime>
#include <unistd.h>

QSystemTrayIcon * KStarsUiTests::m_Notifier { nullptr };
void KStarsUiTests::notifierBegin()
{
    if (!m_Notifier)
        m_Notifier = new QSystemTrayIcon(QIcon::fromTheme("kstars_stars"), Ekos::Manager::Instance());
    if (m_Notifier)
        KStarsUiTests::m_Notifier->show();
}

void KStarsUiTests::notifierHide()
{
    if (m_Notifier)
        m_Notifier->hide();
}

void KStarsUiTests::notifierMessage(QString title, QString message)
{
    qDebug() << message.replace('\n',' ');
    if (m_Notifier)
        m_Notifier->showMessage(title, message, QIcon());
}

void KStarsUiTests::notifierEnd()
{
    if (m_Notifier)
    {
        delete m_Notifier;
        m_Notifier = nullptr;
    }
}

// We want to launch the application before running our tests
// Thus we want to explicitly call QApplication::exec(), and run our tests in parallel of the event loop
// We then reimplement QTEST_MAIN(KStarsUiTests);
// The same will have to be done when interacting with a modal dialog: exec() in main thread, tests in timer-based thread

QT_BEGIN_NAMESPACE
QTEST_ADD_GPU_BLACKLIST_SUPPORT_DEFS
QT_END_NAMESPACE

void prepare_tests()
{
    // Configure our test UI
    QApplication::instance()->setAttribute(Qt::AA_Use96Dpi, true);
    QTEST_ADD_GPU_BLACKLIST_SUPPORT
    QTEST_SET_MAIN_SOURCE_PATH
    QApplication::processEvents();

    // Prepare our KStars configuration
    srand((unsigned int)time(nullptr));
    KSPaths::writableLocation(QStandardPaths::AppDataLocation);
    KSPaths::writableLocation(QStandardPaths::AppConfigLocation);
    KSPaths::writableLocation(QStandardPaths::CacheLocation);
    KCrash::initialize();

    // Explicitly provide the RC file from the main app resources, not the user-customized one
    KStars::setResourceFile(":/kxmlgui5/kstars/kstarsui.rc");
}

int run_wizards(int argc, char *argv[])
{
    int failure = 0;

    // This cleans the test user settings, creates our instance and manages the startup wizard
    if (!failure)
    {
        TestKStarsStartup * ti = new TestKStarsStartup();
        failure |= QTest::qExec(ti);//, argc, argv);
        delete ti;
    }

#if defined(HAVE_INDI)
    // If we test with INDI, this takes care of the Ekos startup wizard
    if (!failure)
    {
        TestEkosWizard * ew = new TestEkosWizard();
        failure |= QTest::qExec(ew);//, argc, argv);
        delete ew;
    }
#endif

    Q_UNUSED(argc);
    Q_UNUSED(argv);

    return failure;
}

void execute_tests()
{
    QCoreApplication *app = QApplication::instance();

    // Limit execution duration
    QTimer::singleShot(60 * 60 * 1000, app, &QCoreApplication::quit);

    app->exec();

    // Clean our instance up if it is still alive
    if( KStars::Instance() != nullptr)
    {
        KStars::Instance()->close();
        delete KStars::Instance();
    }
}

#if !defined(HAVE_INDI)
QTEST_KSTARS_MAIN(KStarsUiTests)
#endif
