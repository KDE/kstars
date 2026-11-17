/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QSharedPointer>
#include <QVector>

namespace EkosLive
{
class NodeManager;
class Media;
}
class FakeServer;

/**
 * @brief Tests which EkosLive server receives the native livestacker's stacked images ("+LS").
 * Two local websocket servers stand in for the online server (ekoslive.com) and the offline one
 * (ekoslive-offline). Only the online one may get the image, and only while a client is on it.
 */
class TestEkosLiveMedia : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void initTestCase();
        void init();
        void cleanup();

        void testNotSentWhenDisconnected();
        void testNotSentWithoutClient();
        void testSentToOnlineOnly_data();
        void testSentToOnlineOnly();
        void testNotSentToOfflineAlone();
        void testNotSentWhenBlobsDisabled();
        void testSmallImageNotUpscaled();
        void testWorstCaseMetadataFits();
        void testOversizedMetadataNotSent();

    private:
        bool connectNodes();
        // Sends a marker on the node and waits for the server to get it. Anything sent on that
        // node before arrives first, so after this the server's list is complete.
        bool flush(int index, FakeServer *server);

        QVector<QSharedPointer<EkosLive::NodeManager>> m_Managers;
        EkosLive::Media *m_Media { nullptr };
        FakeServer *m_Online { nullptr };
        FakeServer *m_Offline { nullptr };
};
