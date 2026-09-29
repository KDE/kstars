/*
    SPDX-FileCopyrightText: 2026 Ekos contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "testksnotification.h"

#include "ksnotification.h"

#include <QMessageBox>
#include <QPointer>
#include <QTest>

// The user closes the popup while the caller still holds the handle (e.g. while the Find dialog is still
// resolving the object online). The popup deletes itself on close, so the handle must become null instead
// of pointing to a deleted widget; closing it again through the handle must be safe.
void TestKSNotification::testHandleIsNullAfterUserClosedPopup()
{
    auto popup = KSNotification::closeableMessage(QStringLiteral("Searching the internet"), QString());
    QVERIFY(popup);

    QPointer<QMessageBox> guard(popup.data());
    QVERIFY(guard->isVisible());

    // The user closes the popup; WA_DeleteOnClose schedules its deletion.
    guard->close();
    QTRY_VERIFY(guard.isNull());

    QVERIFY2(popup.isNull(), "handle still points to the deleted popup");
    if (popup)
        popup->close();
}

// Normal Find dialog flow: the caller closes the popup and drops the handle. The popup must be deleted
// exactly once, without a leak.
void TestKSNotification::testPopupIsDeletedAfterCloseAndRelease()
{
    QPointer<QMessageBox> guard;
    {
        auto popup = KSNotification::closeableMessage(QStringLiteral("Searching the internet"), QString());
        QVERIFY(popup);
        guard = popup.data();
        if (popup)
            popup->close();
    }
    QTRY_VERIFY(guard.isNull());
}

QTEST_MAIN(TestKSNotification)
