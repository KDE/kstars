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

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////

void Cloud::onConnected()
{
    auto node = qobject_cast<Node*>(sender());
    if (!node)
        return;

    qCInfo(KSTARS_EKOS) << "Connected to Cloud Websocket server at" << node->url().toDisplayString();

    emit connected();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Cloud::onDisconnected()
{
    qCInfo(KSTARS_EKOS) << "Disconnected from Cloud Websocket server.";
    m_sendBlobs = true;

    for (auto &oneFile : temporaryFiles)
        QFile::remove(oneFile);
    temporaryFiles.clear();

    emit disconnected();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
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

    const QJsonObject payload = serverMessage.object();
    const QString command = payload["type"].toString();
    if (command == commands[SET_BLOBS])
        m_sendBlobs = payload["payload"].toBool();
    else if (command == commands[OPTION_SET])
    {
        const QJsonArray options = payload["options"].toArray();
        for (const auto &oneOption : options)
        {
            auto name = oneOption[QStringLiteral("name")].toString().toLatin1();
            auto value = oneOption[QStringLiteral("value")].toVariant();

            Options::self()->setProperty(name, value);

            // Special case
            if (name == "ekosLiveCloud")
                updateOptions();
        }

        Options::self()->save();
    }
    else if (command == commands[LOGOUT] || command == commands[SESSION_EXPIRED])
    {
        auto node = qobject_cast<Node*>(sender());
        if (node)
        {
            qCInfo(KSTARS_EKOS) << "Cloud: Received" << command << "from node" << node->url().toDisplayString()
                                << ". Emitting globalLogoutTriggered signal with URL.";
            emit globalLogoutTriggered(node->url());
        }
        else
        {
            qCWarning(KSTARS_EKOS) << "Cloud: Received" << command << "but could not identify sender node.";
            // Fallback: if sender node can't be identified, emit without URL,
            // which might trigger a broader logout in the client if not handled carefully.
            // Or, decide to do nothing if URL is essential. For now, let's emit.
            emit globalLogoutTriggered(QUrl());
        }
        // Do not return here, let other processing for LOGOUT/SESSION_EXPIRED happen if any.
        // However, typically, after emitting logout, further processing for this command might not be needed.
        // For now, let's assume the signal emission is the primary action.
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Cloud::sendData(const QSharedPointer<FITSData> &data, const QString &uuid)
{
    if (Options::ekosLiveCloud() == false  || m_sendBlobs == false)
        return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QtConcurrent::run(&Cloud::dispatch, this, data, uuid);
#else
    QtConcurrent::run(this, &Cloud::dispatch, data, uuid);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Cloud::dispatch(const QSharedPointer<FITSData> &data, const QString &uuid)
{
    // Send complete metadata
    // Add file name and size
    QJsonObject metadata;
    // Skip empty or useless metadata
    for (const auto &oneRecord : data->getRecords())
    {
        if (oneRecord.key.isEmpty() || oneRecord.value.toString().isEmpty())
            continue;
        metadata.insert(oneRecord.key.toLower(), QJsonValue::fromVariant(oneRecord.value));
    }

    // Filename only without path
    QString filepath = data->filename();
    QString filenameOnly = QFileInfo(filepath).fileName();

    // Add filename and size as wells
    metadata.insert("uuid", uuid);
    metadata.insert("filename", filenameOnly);
    metadata.insert("filesize", static_cast<int>(data->size()));
    // Must set Content-Disposition so
    metadata.insert("Content-Disposition", QString("attachment;filename=%1.fz").arg(filenameOnly));

    QByteArray image;
    QByteArray meta = QJsonDocument(metadata).toJson(QJsonDocument::Compact);
    meta = meta.leftJustified(METADATA_PACKET, 0);
    image += meta;

    QString compressedFile = QDir::tempPath() + QString("/ekoslivecloud%1").arg(uuid);
    data->saveImage(compressedFile + QStringLiteral("[compress R]"));
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
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Cloud::uploadImage(const QByteArray &image)
{
    for (auto &nodeManager : m_NodeManagers)
    {
        if (nodeManager->cloud() == nullptr)
            continue;

        nodeManager->cloud()->sendBinaryMessage(image);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
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
