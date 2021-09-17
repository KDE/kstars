/*  Artificial Horizon UI test
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TestArtificialHorizon_H
#define TestArtificialHorizon_H

#include "config-kstars.h"

#if defined(HAVE_INDI)

#include <QObject>
#include <QPushButton>
#include <QCheckBox>
#include <QtTest>

#include "skypoint.h"
#include "typedef.h"

class QStandardItemModel;
class QStandardItem;

/** @brief Helper to retrieve a gadget in the artificial horizon menu.
 * @param klass is the class of the gadget to look for.
 * @param name is the gadget name to look for in the UI configuration.
 * @warning Fails the test if the gadget "name" of class "klass" does not exist in the artificial horizon module
 */
#define KTRY_AH_GADGET(klass, name) klass * const name = KStars::Instance()->m_HorizonManager->findChild<klass*>(#name); \
    QVERIFY2(name != nullptr, QString(#klass " '%1' does not exist and cannot be used").arg(#name).toStdString().c_str())

/** @brief Helper to click a button in the artificial horizon menu.
 * @param button is the gadget name of the button to look for in the UI configuration.
 * @warning Fails the test if the button is not currently enabled.
 */
#define KTRY_AH_CLICK(button) do { \
    bool clicked = false; \
    QTimer::singleShot(100, KStars::Instance(), [&]() { \
        KTRY_AH_GADGET(QPushButton, button); \
        QVERIFY2(button->isEnabled(), QString("QPushButton '%1' is disabled and cannot be clicked").arg(#button).toStdString().c_str()); \
        QTest::mouseClick(button, Qt::LeftButton); \
        clicked = true; }); \
    QTRY_VERIFY_WITH_TIMEOUT(clicked, 150); } while(false)

class TestArtificialHorizon : public QObject
{
        Q_OBJECT

    public:
        explicit TestArtificialHorizon(QObject *parent = nullptr);

    private slots:
        void initTestCase();
        void cleanupTestCase();

        void init();
        void cleanup();

        void testArtificialHorizon_data();
        void testArtificialHorizon();
    private:
        QStandardItem *getRegion(int region);
        QList<SkyPoint> getRegionPoints(int region);
        bool compareLivePreview(int region, SkyList *previewPoints);
        bool checkForRepeatedAzAlt(int region);

        QStandardItemModel *m_Model {nullptr};
};

#endif // HAVE_INDI
#endif // TestArtificialHorizon_H
