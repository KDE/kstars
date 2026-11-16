/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "avtplotwidget.h"

#include <kplotobject.h>

#include <QPixmap>
#include <QStringList>
#include <QTest>

#include <cmath>

namespace
{
QStringList s_imageWarnings;
QtMessageHandler s_previousHandler = nullptr;

void recordImageWarnings(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (message.startsWith(QLatin1String("QImage::")))
        s_imageWarnings << message;
    else if (s_previousHandler)
        s_previousHandler(type, context, message);
}
}

class TestAVTPlotWidget : public QObject
{
        Q_OBJECT

    private slots:
        void initTestCase();
        void cleanupTestCase();
        void testPaintWithoutPlotArea_data();
        void testPaintWithoutPlotArea();
};

void TestAVTPlotWidget::initTestCase()
{
    s_previousHandler = qInstallMessageHandler(recordImageWarnings);
}

void TestAVTPlotWidget::cleanupTestCase()
{
    qInstallMessageHandler(s_previousHandler);
}

void TestAVTPlotWidget::testPaintWithoutPlotArea_data()
{
    QTest::addColumn<int>("height");

    // The padding below is 25 pixels high, as in the scheduler altitude graph.
    QTest::newRow("squeezed below the padding") << 12;
    QTest::newRow("one pixel high") << 1;
    QTest::newRow("as high as the padding") << 25;
    QTest::newRow("normal height") << 200;
}

// A plot widget squeezed below its padding (e.g. by dragging a splitter) has no plot area.
// KPlotWidget then has a plot rectangle with a negative height and a null label mask, and
// drawing the curves used to log one QImage warning per pixel along every curve.
void TestAVTPlotWidget::testPaintWithoutPlotArea()
{
    QFETCH(int, height);

    AVTPlotWidget plot;
    plot.setTopPadding(0);
    plot.setBottomPadding(25);
    plot.setLeftPadding(25);
    plot.setRightPadding(10);
    plot.setLimits(0, 24, 0, 90);
    // A positive altitude axis skips the ground, which needs KStarsData.
    plot.setAltitudeAxis(0, 90);

    auto *curve = new KPlotObject(Qt::white, KPlotObject::Lines, 2);
    for (int i = 0; i <= 48; ++i)
    {
        const double hour = i * 0.5;
        curve->addPoint(hour, 45 + 40 * std::cos(hour / 24.0 * 2 * M_PI));
    }
    plot.addPlotObject(curve);

    plot.resize(400 + 2 * plot.frameWidth(), height + 2 * plot.frameWidth());

    s_imageWarnings.clear();
    QPixmap pixmap(plot.size());
    plot.render(&pixmap);

    QVERIFY2(s_imageWarnings.isEmpty(),
             qPrintable(QString("%1 QImage warnings, first: %2").arg(s_imageWarnings.size())
                        .arg(s_imageWarnings.value(0))));
}

QTEST_MAIN(TestAVTPlotWidget)

#include "testavtplotwidget.moc"
