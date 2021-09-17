/*  KStars UI tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>
    Fabrizio Pollastri <mxgbot@gmail.com> 2020-08-30

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TESTEKOSMOUNT_H
#define TESTEKOSMOUNT_H

#include "config-kstars.h"
#include "test_ekos.h"

#if defined(HAVE_INDI)
#include "indicom.h"

#include <QObject>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QtTest>
#include <QDialog>

/** @brief Helper to retrieve a gadget in the Mount tab specifically.
 * @param klass is the class of the gadget to look for.
 * @param name is the gadget name to look for in the UI configuration.
 * @warning Fails the test if the gadget "name" of class "klass" does not exist in the Mount module
 */
#define KTRY_MOUNT_GADGET(klass, name) klass * const name = Ekos::Manager::Instance()->mountModule()->findChild<klass*>(#name); \
    QVERIFY2(name != nullptr, QString(#klass " '%1' does not exist and cannot be used").arg(#name).toStdString().c_str())

/** @brief Helper to click a button in the Mount tab specifically.
 * @param button is the gadget name of the button to look for in the UI configuration.
 * @warning Fails the test if the button is not currently enabled.
 */
#define KTRY_MOUNT_CLICK(button) do { \
    QTimer::singleShot(100, Ekos::Manager::Instance(), []() { \
        KTRY_MOUNT_GADGET(QPushButton, button); \
        QVERIFY2(button->isEnabled(), QString("QPushButton '%1' is disabled and cannot be clicked").arg(#button).toStdString().c_str()); \
        QTest::mouseClick(button, Qt::LeftButton); }); \
    QTest::qWait(200); } while(false)

/** @brief Helper to sync the mount to an object.
 * @param name is the name of the object to sync to.
 * @param track whether to enable track or not.
 */
#define KTRY_MOUNT_SYNC_NAMED(name, track) do { \
    QVERIFY(KStars::Instance()); \
    QVERIFY(KStars::Instance()->data()); \
    SkyObject const * const so = KStars::Instance()->data()->objectNamed(name); \
    QVERIFY(so != nullptr); \
    QVERIFY(KStarsData::Instance()); \
    GeoLocation * const geo = KStarsData::Instance()->geo(); \
    KStarsDateTime const now(KStarsData::Instance()->lt()); \
    KSNumbers const numbers(now.djd()); \
    CachingDms const LST = geo->GSTtoLST(geo->LTtoUT(now).gst()); \
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule() != nullptr, 5000); \
    Ekos::Manager::Instance()->mountModule()->setMeridianFlipValues(true, 0); \
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->unpark(), 5000); \
    SkyObject o(*so); \
    o.updateCoordsNow(&numbers); \
    o.EquatorialToHorizontal(&LST, geo->lat()); \
    QVERIFY(Ekos::Manager::Instance()->mountModule()->sync(o.ra().Hours(), o.dec().Degrees())); \
    if (!track) \
        QTimer::singleShot(1000, [&]{ \
        QDialog * const dialog = qobject_cast <QDialog*> (QApplication::activeModalWidget()); \
        if(dialog != nullptr) emit dialog->accept(); }); \
    Ekos::Manager::Instance()->mountModule()->setTrackEnabled(track); \
    if (track) \
        QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->status() == ISD::Telescope::Status::MOUNT_TRACKING, 30000); \
    } while (false)

/** @brief Helper to sync the mount at the meridian for focus tests.
 * @warning This is needed because the CCD Simulator has much rotation jitter at the celestial pole.
 * @param alt is the altitude to sync to, use 60.0 as degrees for instance.
 * @param track whether to enable track or not.
 * @param ha_ofs is the offset to add to the LST before syncing (east is positive).
 */
#define KTRY_MOUNT_SYNC(alt, track, ha_ofs) do { \
    QVERIFY(KStarsData::Instance()); \
    GeoLocation * const geo = KStarsData::Instance()->geo(); \
    KStarsDateTime const now(KStarsData::Instance()->lt()); \
    KSNumbers const numbers(now.djd()); \
    CachingDms const LST = geo->GSTtoLST(geo->LTtoUT(now).gst()); \
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule() != nullptr, 5000); \
    Ekos::Manager::Instance()->mountModule()->setMeridianFlipValues(true, 0); \
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->unpark(), 5000); \
    QVERIFY(Ekos::Manager::Instance()->mountModule()->sync(range24(LST.Hours()+(ha_ofs+0.002)), (alt))); \
    QVERIFY(Ekos::Manager::Instance()->mountModule()->slew(range24(LST.Hours()+(ha_ofs)), (alt))); \
    if (!track) \
        QTimer::singleShot(1000, [&]{ \
        QDialog * const dialog = qobject_cast <QDialog*> (QApplication::activeModalWidget()); \
        if(dialog != nullptr) emit dialog->accept(); }); \
    Ekos::Manager::Instance()->mountModule()->setTrackEnabled(track); \
    if (track) \
        QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->status() == ISD::Telescope::Status::MOUNT_TRACKING, 30000); \
    } while (false)

class TestEkosMount : public QObject
{
    Q_OBJECT

public:
    explicit TestEkosMount(QObject *parent = nullptr);

private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    void testMountCtrlCoordLabels();
    void testMountCtrlCoordConversion();
    void testMountCtrlGoto();
    void testMountCtrlSync();

private:
    Ekos::Manager *ekos;
    QWindow *mountControl;
    QObject *raLabel, *deLabel, *raText, *deText,
        *coordRaDe, *coordAzAl, *coordHaDe,
        *raValue, *deValue, *azValue, *altValue, *haValue, *zaValue,
        *gotoButton, *syncButton;
    double degreePrecision = 2 * 1.0 / 3600.0;
    double hourPrecision = 2 * 15.0 / 3600.0;


};

#endif // HAVE_INDI
#endif // TESTEKOSMOUNT_H
