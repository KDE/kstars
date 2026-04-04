#include "test_catalog_download.h"

#include "kstars_ui_tests.h"
#include "test_kstars_startup.h"

#include "Options.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QSet>
#include <QSignalSpy>
#include <QQuickItem>

#if __has_include(<KNSCore/enginebase.h>)
#include <KNSCore/enginebase.h>
using KNSEngineBase = KNSCore::EngineBase;
#else
#include <KNSCore/Engine>
using KNSEngineBase = KNSCore::Engine;
#endif
#if __has_include(<KNSCore/resultsstream.h>)
#define KSTARS_HAS_KNS_RESULTSSTREAM 1
#else
#define KSTARS_HAS_KNS_RESULTSSTREAM 0
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <KNSWidgets/dialog.h>
#include <KNSWidgets/Button>
using DownloadDialog = KNSWidgets::Dialog;
#else
#include <KNS3/DownloadDialog>
#include <KNS3/DownloadWidget>
#include <KNS3/Button>
using DownloadDialog = KNS3::DownloadDialog;
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using KnsErrorCode = KNSCore::ErrorCode::ErrorCode;
#else
using KnsErrorCode = KNSCore::ErrorCode;
#endif
#include <KMessageBox>

namespace
{
constexpr int DOWNLOAD_DIALOG_OPEN_TIMEOUT_MS = 15000;
constexpr int DOWNLOAD_DIALOG_SETTLE_WAIT_MS = 5000;
constexpr int MUTATION_SETTLE_TIMEOUT_MS = 3000;
constexpr int POST_ITERATION_WAIT_MS = 3000;

enum class ToggleCatalogResult
{
    Success,
    NetworkUnavailable,
    Skipped,
    Failure,
};

bool knsNetworkUnavailableForTests()
{
    return qEnvironmentVariableIsSet("KSTARS_TEST_NO_NETWORK") && qgetenv("KSTARS_TEST_NO_NETWORK") != QByteArray("0");
}

bool knsErrorIsNetworkRelated(const QSignalSpy &errorSpy)
{
    for (const QList<QVariant> &signalArguments : errorSpy)
    {
        if (signalArguments.isEmpty())
            continue;

        const auto errorCode = static_cast<KnsErrorCode>(signalArguments.at(0).toInt());
        if (errorCode == KNSCore::ErrorCode::NetworkError || errorCode == KNSCore::ErrorCode::TryAgainLaterError)
            return true;
    }

    return false;
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
QObject *entryActionOwner(QObject *object);
#else
QList<QToolButton *> downloadButtons(DownloadDialog *dialog);
#endif

bool isItemVisibleToDialog(const QObject *object)
{
    const auto *item = qobject_cast<const QQuickItem *>(object);
    if (item == nullptr)
        return false;

    if (!item->isVisible())
        return false;

    for (const QObject *parent = item->parent(); parent != nullptr; parent = parent->parent())
    {
        auto *parentItem = qobject_cast<const QQuickItem *>(parent);
        if (parentItem != nullptr && !parentItem->isVisible())
            return false;
    }

    return true;
}

void closeWidget(QWidget *widget)
{
    if (auto *dialog = qobject_cast<QDialog *>(widget))
        dialog->reject();
    else if (widget != nullptr)
        widget->close();
}

DownloadDialog *findOpenDownloadDialog()
{
    if (KStars::Instance() != nullptr)
    {
        const QList<DownloadDialog *> dialogs = KStars::Instance()->findChildren<DownloadDialog *>();
        DownloadDialog *candidate = nullptr;
        for (DownloadDialog *dialog : dialogs)
        {
            if (dialog != nullptr && dialog->isVisible())
                return dialog;

            if (candidate == nullptr)
                candidate = dialog;
        }

        if (candidate != nullptr)
            return candidate;
    }

    for (QWidget *widget : QApplication::topLevelWidgets())
    {
        auto *dialog = qobject_cast<DownloadDialog *>(widget);
        if (dialog != nullptr && dialog->isVisible())
            return dialog;
    }

    auto *modalDialog = qobject_cast<DownloadDialog *>(QApplication::activeModalWidget());
    return modalDialog != nullptr && modalDialog->isVisible() ? modalDialog : nullptr;
}

void closeDownloadDialog(DownloadDialog *dialog = nullptr)
{
    DownloadDialog *openDialog = dialog != nullptr ? dialog : findOpenDownloadDialog();
    if (openDialog != nullptr)
        closeWidget(openDialog);

    QWidget *modal = QApplication::activeModalWidget();
    if (modal != nullptr && modal != openDialog)
        closeWidget(modal);
}

DownloadDialog *waitForDownloadDialog(int timeoutMs)
{
    QTest::qWait(timeoutMs);
    return findOpenDownloadDialog();
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
QList<QToolButton *> downloadButtons(DownloadDialog *dialog)
{
    auto *widget = dialog != nullptr ? dialog->findChild<KNS3::DownloadWidget *>() : nullptr;
    return widget != nullptr ? widget->findChildren<QToolButton *>() : QList<QToolButton *> {};
}
#else
QObject *entryActionOwner(QObject *object)
{
    for (QObject *parent = object != nullptr ? object->parent() : nullptr; parent != nullptr; parent = parent->parent())
    {
        if (parent->property("entry").isValid())
            return parent;
    }

    return nullptr;
}

bool propertyIsTrue(QObject *object, const char *name, bool defaultValue = true)
{
    const QVariant value = object != nullptr ? object->property(name) : QVariant();
    return value.isValid() ? value.toBool() : defaultValue;
}

bool actionTextHasEllipsis(const QString &text)
{
    return text.endsWith(QStringLiteral("...")) || text.endsWith(QChar(0x2026));
}

QString catalogEntryKey(const KNSCore::Entry &entry)
{
    const QString uniqueId = entry.uniqueId().trimmed();
    return !uniqueId.isEmpty() ? uniqueId : entry.name().trimmed();
}

bool dialogChangedEntriesContain(const DownloadDialog *dialog, const QString &entryKey,
                                 KNSCore::Entry::Status previousStatus)
{
    if (dialog == nullptr || entryKey.isEmpty())
        return false;

    const QList<KNSCore::Entry> changedEntries = dialog->changedEntries();
    for (const KNSCore::Entry &changedEntry : changedEntries)
    {
        if (catalogEntryKey(changedEntry) == entryKey && changedEntry.status() != previousStatus)
            return true;
    }

    return false;
}

enum class DialogActionResult
{
    Mutated,
    NetworkUnavailable,
    NoChange,
};

DialogActionResult triggerDialogEntryAction(DownloadDialog *dialog, KNSEngineBase *engine, QObject *owner, QObject *action)
{
    if (dialog == nullptr || engine == nullptr || owner == nullptr || action == nullptr)
        return DialogActionResult::NoChange;

    const QVariant entryVariant = owner->property("entry");
    if (!entryVariant.isValid())
        return DialogActionResult::NoChange;

    const KNSCore::Entry entry = entryVariant.value<KNSCore::Entry>();
    if (!entry.isValid())
        return DialogActionResult::NoChange;

    const QString entryKey = catalogEntryKey(entry);
    const KNSCore::Entry::Status previousStatus = entry.status();
    QSignalSpy errorSpy(engine, &KNSEngineBase::signalErrorCode);

    if (!QMetaObject::invokeMethod(action, "trigger", Q_ARG(QObject *, static_cast<QObject *>(nullptr))))
        return DialogActionResult::NoChange;

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 5000)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

        QWidget *modal = QApplication::activeModalWidget();
        if (modal != nullptr && modal != dialog)
            closeWidget(modal);

        if (dialogChangedEntriesContain(dialog, entryKey, previousStatus))
            return DialogActionResult::Mutated;

        if (!errorSpy.isEmpty())
            return knsErrorIsNetworkRelated(errorSpy) ? DialogActionResult::NetworkUnavailable : DialogActionResult::NoChange;
    }

    return DialogActionResult::NoChange;
}

ToggleCatalogResult toggleCatalogEntries(DownloadDialog *dialog, QString *failure)
{
#if !KSTARS_HAS_KNS_RESULTSSTREAM
    if (failure != nullptr)
        *failure = QStringLiteral("catalog download dialog mutation checks are unavailable on this Qt6/KF6 build.");
    Q_UNUSED(dialog)
    return ToggleCatalogResult::Skipped;
#else
    constexpr int requiredMutations = 4;
    auto *engine = dialog != nullptr ? dialog->engine() : nullptr;
    if (engine == nullptr)
    {
        if (failure != nullptr)
            *failure = QStringLiteral("catalog download dialog has no engine.");
        return ToggleCatalogResult::Failure;
    }

    const QString useLabel = engine->useLabel();
    QSet<QObject *> visibleEntries;
    QSet<QObject *> toggledEntries;
    QSet<QObject *> attemptedActions;
    for (QObject *object : dialog->findChildren<QObject *>())
    {
        QObject *owner = entryActionOwner(object);
        if (owner == nullptr || !isItemVisibleToDialog(owner))
            continue;

        visibleEntries.insert(owner);
        if (toggledEntries.contains(owner) || attemptedActions.contains(object))
            continue;

        const QString text = object->property("text").toString().trimmed();
        if (text.isEmpty() || text == useLabel)
            continue;
        if (actionTextHasEllipsis(text))
            continue;
        if (!propertyIsTrue(object, "enabled") || !propertyIsTrue(object, "visible"))
            continue;

        attemptedActions.insert(object);
        const auto actionResult = triggerDialogEntryAction(dialog, engine, owner, object);
        if (actionResult == DialogActionResult::Mutated)
            toggledEntries.insert(owner);

        if (actionResult == DialogActionResult::NetworkUnavailable)
        {
            if (failure != nullptr)
                *failure = QStringLiteral("catalog download dialog hit a network-related engine error while mutating visible entries.");
            return ToggleCatalogResult::NetworkUnavailable;
        }

        if (actionResult == DialogActionResult::NoChange)
            continue;

        if (toggledEntries.size() == requiredMutations)
            break;
    }

    if (toggledEntries.size() == requiredMutations)
    {
        QTest::qWait(MUTATION_SETTLE_TIMEOUT_MS);
        return ToggleCatalogResult::Success;
    }

    if (failure != nullptr)
    {
        if (!visibleEntries.isEmpty() && toggledEntries.isEmpty())
            *failure =
                QStringLiteral("catalog download dialog exposed catalog entries, but none of their visible actions changed install state.");
        else
            *failure = QStringLiteral("catalog download dialog mutated only %1 of %2 visible catalogs.")
                       .arg(toggledEntries.size()).arg(requiredMutations);
    }
    return ToggleCatalogResult::Failure;
#endif
}
#endif
}

TestCatalogDownload::TestCatalogDownload(QObject *parent): QObject(parent)
{

}

void TestCatalogDownload::initTestCase()
{
    KTELL_BEGIN();
}

void TestCatalogDownload::cleanupTestCase()
{
    KTELL_END();
}

void TestCatalogDownload::init()
{
    if (!KStars::Instance()->isStartedWithClockRunning())
    {
        QVERIFY(KStarsData::Instance()->clock());
        KStarsData::Instance()->clock()->start();
    }
}

void TestCatalogDownload::cleanup()
{
    if (!KStars::Instance()->isStartedWithClockRunning())
        KStarsData::Instance()->clock()->stop();
}

void TestCatalogDownload::testCatalogDownloadWhileUpdating()
{
    if (knsNetworkUnavailableForTests())
        QSKIP("Catalog download UI test requires KNS networking.");

    const auto triggerDownloadDialog = []()
    {
        QTimer::singleShot(0, []()
        {
            KStars::Instance()->action("get_data")->activate(QAction::Trigger);
        });
    };

    KTELL("Zoom in enough so that updates are frequent");
    double const previous_zoom = Options::zoomFactor();
    KStars::Instance()->zoom(previous_zoom * 50);

    // This timer looks for message boxes to close until stopped
    QTimer close_message_boxes;
    close_message_boxes.setInterval(500);
    QObject::connect(&close_message_boxes, &QTimer::timeout, &close_message_boxes, [&]()
    {
        QDialog * const dialog = qobject_cast <QDialog*> (QApplication::activeModalWidget());
        if (dialog)
        {
            QList<QPushButton*> pb = dialog->findChildren<QPushButton*>();
            QTest::mouseClick(pb[0], Qt::MouseButton::LeftButton);
        }
    });

    int const count = 2;
    for (int i = 0; i < count; i++)
    {
        QString step = QString("[%1/%2] ").arg(i).arg(count);
        KTELL(step + "Open the Download Dialog, wait for plugins to load");
        QString iterationSkipReason;
        triggerDownloadDialog();
        DownloadDialog *d = waitForDownloadDialog(DOWNLOAD_DIALOG_OPEN_TIMEOUT_MS);
        QString iterationFailure;

        if (d == nullptr)
            iterationFailure = step + QStringLiteral("catalog download dialog was not created within %1 ms.")
                               .arg(DOWNLOAD_DIALOG_OPEN_TIMEOUT_MS);

        if (iterationFailure.isEmpty())
        {
            QTest::qWait(DOWNLOAD_DIALOG_SETTLE_WAIT_MS);
            KTELL(step + "Change the first four catalogs installation state");
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            QList<QToolButton*> wl = downloadButtons(d);
            if (wl.count() >= 8)
            {
                wl[1]->setFocus();
                QTest::keyClick(wl[1], Qt::Key_Space);
                wl[3]->setFocus();
                QTest::keyClick(wl[3], Qt::Key_Space);
                wl[5]->setFocus();
                QTest::keyClick(wl[5], Qt::Key_Space);
                wl[7]->setFocus();
                QTest::keyClick(wl[7], Qt::Key_Space);
                QTest::qWait(5000);
            }
            else
            {
                iterationFailure = step + QStringLiteral("catalog download dialog loaded only %1 provider buttons.").arg(wl.count());
            }
#else
            switch (toggleCatalogEntries(d, &iterationFailure))
            {
                case ToggleCatalogResult::Success:
                    break;

                case ToggleCatalogResult::NetworkUnavailable:
                    if (knsNetworkUnavailableForTests())
                    {
                        KTELL(step + QStringLiteral("Skip catalog mutation: test networking is disabled and KNS entries are unavailable."));
                        iterationFailure.clear();
                    }
                    else
                    {
                        iterationFailure = step + iterationFailure;
                    }
                    break;

                case ToggleCatalogResult::Skipped:
                    iterationSkipReason =
                        step + QStringLiteral("catalog download mutation checks are unavailable on this Qt6/KF6 build.");
                    break;

                case ToggleCatalogResult::Failure:
                    iterationFailure = step + iterationFailure;
                    break;
            }
#endif
        }

        KTELL(step + "Close the Download Dialog, accept all potential reinstalls");
        close_message_boxes.start();
        closeDownloadDialog(d);
        QTest::qWait(1000);
        close_message_boxes.stop();
        if (!iterationSkipReason.isEmpty())
        {
            Options::setZoomFactor(previous_zoom);
            QSKIP(qPrintable(iterationSkipReason));
        }
        QVERIFY2(iterationFailure.isEmpty(), qPrintable(iterationFailure));

        KTELL(step + "Wait a bit for pop-ups to appear");
        QTest::qWait(POST_ITERATION_WAIT_MS);
    }

    Options::setZoomFactor(previous_zoom);
}

QTEST_KSTARS_MAIN(TestCatalogDownload)
