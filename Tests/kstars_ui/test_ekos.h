/*  KStars UI tests
    SPDX-FileCopyrightText: 2018, 2020 Csaba Kertesz <csaba.kertesz@gmail.com>
    SPDX-FileCopyrightText: Jasem Mutlaq <knro@ikarustech.com>
    SPDX-FileCopyrightText: Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_EKOS_H
#define TEST_EKOS_H

#include "config-kstars.h"

#if defined(HAVE_INDI)

#include <KActionCollection>
#include <QtTest>
#include "kstars.h"
#include "ekos/manager.h"
#include "test_kstars_startup.h"

#define KVERIFY_EKOS_IS_HIDDEN() do { \
    if (Ekos::Manager::Instance() != nullptr) { \
        QVERIFY(!Ekos::Manager::Instance()->isVisible()); \
        QVERIFY(!Ekos::Manager::Instance()->isActiveWindow()); }} while(false)

#define KVERIFY_EKOS_IS_OPENED() do { \
    QVERIFY(Ekos::Manager::Instance() != nullptr); \
    QVERIFY(Ekos::Manager::Instance()->isVisible()); \
    QVERIFY(Ekos::Manager::Instance()->isActiveWindow()); } while(false)

#define KTRY_OPEN_EKOS() do { \
    if (Ekos::Manager::Instance() == nullptr || !Ekos::Manager::Instance()->isVisible()) { \
        KTRY_ACTION("show_ekos"); \
        QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance() != nullptr, 200); \
        QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->isVisible(), 200); \
        QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->isActiveWindow(), 1000); }} while(false)

#define KTRY_CLOSE_EKOS() do { \
    if (Ekos::Manager::Instance() != nullptr && Ekos::Manager::Instance()->isVisible()) { \
        KTRY_ACTION("show_ekos"); \
        QTRY_VERIFY_WITH_TIMEOUT(!Ekos::Manager::Instance()->isActiveWindow(), 200); \
        QTRY_VERIFY_WITH_TIMEOUT(!Ekos::Manager::Instance()->isVisible(), 200); }} while(false)

#define KHACK_RESET_EKOS_TIME() do { \
    QWARN("HACK HACK HACK: Reset clock to initial conditions when starting Ekos"); \
    if (KStars::Instance() != nullptr) \
        if (KStars::Instance()->data() != nullptr) \
            KStars::Instance()->data()->clock()->setUTC(KStarsDateTime(TestKStarsStartup::m_InitialConditions.dateTime)); } while(false)

#define KTRY_PROFILEEDITOR_GADGET(klass, name) klass * name = nullptr; \
    do { \
        ProfileEditor* profileEditor = Ekos::Manager::Instance()->findChild<ProfileEditor*>("profileEditorDialog"); \
        QVERIFY2(profileEditor != nullptr && profileEditor->isVisible(), "Profile Editor is not visible."); \
        name = Ekos::Manager::Instance()->findChild<klass*>(#name); \
        QVERIFY2(name != nullptr, QString(#klass "'%1' does not exist and cannot be used").arg(#name).toStdString().c_str()); \
    } while(false)

#define KTRY_PROFILEEDITOR_TREE_COMBOBOX(name, strvalue) \
    KTRY_PROFILEEDITOR_GADGET(QComboBox, name); do { \
    QString lookup(strvalue); \
    QModelIndexList const list = name->model()->match(name->model()->index(0, 0), Qt::DisplayRole, QVariant::fromValue(lookup), 1, Qt::MatchRecursive); \
    QVERIFY(0 < list.count()); \
    QModelIndex const &item = list.first(); \
    QCOMPARE(list.value(0).data().toString(), lookup); \
    QVERIFY(!item.parent().parent().isValid()); \
    name->setRootModelIndex(item.parent()); \
    name->setCurrentText(lookup); \
    QCOMPARE(name->currentText(), lookup); } while(false)

class TestEkos: public QObject
{
    Q_OBJECT
public:
    explicit TestEkos(QObject *parent = nullptr);

private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    void testOpenClose();
    void testSimulatorProfile();
    void testManipulateProfiles();
};

#endif // HAVE_INDI
#endif // TEST_EKOS_H
