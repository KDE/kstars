#include "test_catalog_download.h"

#include "kstars_ui_tests.h"
#include "test_kstars_startup.h"

#include "Options.h"
#include <KNS3/DownloadWidget>
#include <KNS3/Button>
#include <KMessageBox>

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

    int const count = 6;
    for (int i = 0; i < count; i++)
    {
        QString step = QString("[%1/%2] ").arg(i).arg(count);
        KTELL(step + "Open the Download Dialog, wait for plugins to load");
        volatile bool done = false;
        QTimer::singleShot(5000, [&]()
        {
            KTELL(step + "Change the first four catalogs installation state");
            KNS3::DownloadWidget * d = KStars::Instance()->findChild<KNS3::DownloadWidget*>("DownloadWidget");
            QList<QToolButton*> wl = d->findChildren<QToolButton*>();
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
                KTELL(step + "Failed to load XML providers!");
            }
            KTELL(step + "Close the Download Dialog, accept all potentiel reinstalls");
            close_message_boxes.start();
            d->parentWidget()->close();
            done = true;
        });
        KStars::Instance()->action("get_data")->activate(QAction::Trigger);
        QTRY_VERIFY_WITH_TIMEOUT(done, 10000);
        close_message_boxes.stop();

        KTELL(step + "Wait a bit for pop-ups to appear");
        QTest::qWait(5000);
    }

    Options::setZoomFactor(previous_zoom);
}

QTEST_KSTARS_MAIN(TestCatalogDownload)
