/*
    SPDX-FileCopyrightText: 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Cloud Channel

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cloud.h"
#include "commands.h"
#include "fitsviewer/fitsdata.h"

#include "ekos_debug.h"
#include "version.h"
#include "../fitsviewer/fpack.h"
#include "Options.h"

#include <QtConcurrent>
#include <QFutureWatcher>
#include <KFormat>

namespace EkosLive
{

Cloud::Cloud(Ekos::Manager * manager, QVector<QSharedPointer<NodeManager>> &nodeManagers):
    m_Manager(manager), m_NodeManagers(nodeManagers)
{
    for (auto &nodeManager : m_NodeManagers)
    {
        if (nodeManager->cloud() == nullptr)
            continue;

        connect(nodeManager->cloud(), &Node::connected, this, &Cloud::onConnected);
        connect(nodeManager->cloud(), &Node::disconnected, this, &Cloud::onDisconnected);
        connect(nodeManager->cloud(), &Node::onTextReceived, this, &Cloud::onTextReceived);
    }

    connect(&watcher, &QFutureWatcher<bool>::finished, this, &Cloud::sendImage, Qt::UniqueConnection);
    connect(this, &Cloud::newImage, this, &Cloud::uploadImage);
    connect(Options::self(), &Options::EkosLiveCloudChanged, this, &Cloud::updateOptions);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
bool Cloud::isConnected() const
{
    return std::any_of(m_NodeManagers.begin(), m_NodeManagers.end(), [](auto & nodeManager)
    {
        return nodeManager->cloud() && nodeManager->cloud()->isConnected();
    });
}

void Cloud::onConnected()
{
    auto node = qobject_cast<Node*>(sender());
    if (!node)
        return;

    qCInfo(KSTARS_EKOS) << "Connected to Cloud Websocket server at" << node->url().toDisplayString();

    emit connected();
}

void Cloud::onDisconnected()
{
    qCInfo(KSTARS_EKOS) << "Disconnected from Cloud Websocket server.";
    m_sendBlobs = true;

    for (auto &oneFile : temporaryFiles)
        QFile::remove(oneFile);
    temporaryFiles.clear();

    emit disconnected();
}

void Cloud::onTextReceived(const QString &message)
{
    qCInfo(KSTARS_EKOS) << "Cloud Text Websocket Message" << message;
    QJsonParseError error;
    auto serverMessage = QJsonDocument::fromJson(message.toLatin1(), &error);
    if (error.error != QJsonParseError::NoError)
    {
        qCWarning(KSTARS_EKOS) << "Ekos Live Parsing Error" << error.errorString();
        return;
    }

    const QJsonObject msgObj = serverMessage.object();
    const QString command = msgObj["type"].toString();
    if (command == commands[SET_BLOBS])
        m_sendBlobs = msgObj["payload"].toBool();
    else if (command == commands[LOGOUT])
    {
        for (auto &nodeManager : m_NodeManagers)
        {
            if (nodeManager->cloud() == nullptr)
                continue;

            nodeManager->cloud()->disconnectServer();
        }
    }
}

void Cloud::upload(const QSharedPointer<FITSData> &data, const QString &uuid)
{
    if (Options::ekosLiveCloud() == false  || m_sendBlobs == false)
        return;

    m_UUID = uuid;
    m_ImageData = data;
    sendImage();
}

void Cloud::upload(const QString &filename, const QString &uuid)
{
    if (Options::ekosLiveCloud() == false  || m_sendBlobs == false)
        return;

    watcher.waitForFinished();
    m_UUID = uuid;
    m_ImageData.reset(new FITSData(), &QObject::deleteLater);
    QFuture<bool> result = m_ImageData->loadFromFile(filename);
    watcher.setFuture(result);
}

void Cloud::sendImage()
{
    QtConcurrent::run(this, &Cloud::asyncUpload);
}

void Cloud::asyncUpload()
{
    // Send complete metadata
    // Add file name and size
    QJsonObject metadata;
    // Skip empty or useless metadata
    for (const auto &oneRecord : m_ImageData->getRecords())
    {
        if (oneRecord.key.isEmpty() || oneRecord.value.toString().isEmpty())
            continue;
        metadata.insert(oneRecord.key.toLower(), QJsonValue::fromVariant(oneRecord.value));
    }

    // Filename only without path
    QString filepath = m_ImageData->filename();
    QString filenameOnly = QFileInfo(filepath).fileName();

    // Add filename and size as wells
    metadata.insert("uuid", m_UUID);
    metadata.insert("filename", filenameOnly);
    metadata.insert("filesize", static_cast<int>(m_ImageData->size()));
    // Must set Content-Disposition so
    metadata.insert("Content-Disposition", QString("attachment;filename=%1.fz").arg(filenameOnly));

    QByteArray image;
    QByteArray meta = QJsonDocument(metadata).toJson(QJsonDocument::Compact);
    meta = meta.leftJustified(METADATA_PACKET, 0);
    image += meta;

    QString compressedFile = QDir::tempPath() + QString("/ekoslivecloud%1").arg(m_UUID);
    m_ImageData->saveImage(compressedFile + QStringLiteral("[compress R]"));
    // Upload the compressed image
    QFile compressedImage(compressedFile);
    if (compressedImage.open(QIODevice::ReadOnly))
    {
        image += compressedImage.readAll();
        emit newImage(image);
        qCInfo(KSTARS_EKOS) << "Uploaded" << compressedFile << " to the cloud";
    }

    // Remove from disk if temporary
    if (compressedFile != filepath && compressedFile.startsWith(QDir::tempPath()))
        QFile::remove(compressedFile);

    m_ImageData.reset();
}

void Cloud::uploadImage(const QByteArray &image)
{
    for (auto &nodeManager : m_NodeManagers)
    {
        if (nodeManager->cloud() == nullptr)
            continue;

        nodeManager->cloud()->sendBinaryMessage(image);
    }
}

void Cloud::updateOptions()
{
    // In case cloud storage is toggled, inform cloud
    // websocket channel of this change.
    QJsonObject payload = {{"name", "ekosLiveCloud"}, {"value", Options::ekosLiveCloud()}};
    QJsonObject message =
    {
        {"type",  commands[OPTION_SET]},
        {"payload", payload}
    };

    for (auto &nodeManager : m_NodeManagers)
    {
        if (nodeManager->cloud() == nullptr)
            continue;

        nodeManager->cloud()->sendTextMessage(QJsonDocument(message).toJson(QJsonDocument::Compact));
    }
}

}
