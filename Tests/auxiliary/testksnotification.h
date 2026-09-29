/*
    SPDX-FileCopyrightText: 2026 Ekos contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

/**
 * @class TestKSNotification
 * @short Tests for KSNotification::closeableMessage().
 *
 * The Find dialog shows a closeable "Searching the internet" popup while it resolves an object online and
 * closes it afterwards. The user can close the popup first; the handle returned by closeableMessage() must
 * then not dangle (this crashed KStars in FindDialog::finishProcessing()).
 */
class TestKSNotification : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void testHandleIsNullAfterUserClosedPopup();
        void testPopupIsDeletedAfterCloseAndRelease();
};
