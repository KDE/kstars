/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "linguider.h"

#include "Options.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QNetworkReply>

namespace Ekos
{
LinGuider::LinGuider()
{
    tcpSocket = new QTcpSocket(this);

    rawBuffer.clear();

    connect(tcpSocket, SIGNAL(readyRead()), this, SLOT(readLinGuider()));
    connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this,
            SLOT(displayError(QAbstractSocket::SocketError)));

    connect(tcpSocket, SIGNAL(connected()), this, SLOT(onConnected()));

    deviationTimer.setInterval(1000);
    connect(&deviationTimer, &QTimer::timeout, this, [&]() { sendCommand(GET_RA_DEC_DRIFT); });
}

bool LinGuider::Connect()
{
    if (connection == DISCONNECTED)
    {
        rawBuffer.clear();
        connection = CONNECTING;
        tcpSocket->connectToHost(Options::linGuiderHost(), Options::linGuiderPort());
    }
    // Already connected, let's connect equipment
    else
        emit newStatus(GUIDE_CONNECTED);

    return true;
}

bool LinGuider::Disconnect()
{
    rawBuffer.clear();
    connection = DISCONNECTED;
    tcpSocket->disconnectFromHost();

    emit newStatus(GUIDE_DISCONNECTED);

    return true;
}

void LinGuider::displayError(QAbstractSocket::SocketError socketError)
{
    switch (socketError)
    {
        case QAbstractSocket::RemoteHostClosedError:
            break;
        case QAbstractSocket::HostNotFoundError:
            emit newLog(i18n("The host was not found. Please check the host name and port settings in Guide options."));
            emit newStatus(GUIDE_DISCONNECTED);
            break;
        case QAbstractSocket::ConnectionRefusedError:
            emit newLog(i18n("The connection was refused by the peer. Make sure the LinGuider is running, and check "
                             "that the host name and port settings are correct."));
            emit newStatus(GUIDE_DISCONNECTED);
            break;
        default:
            emit newLog(i18n("The following error occurred: %1.", tcpSocket->errorString()));
    }

    connection = DISCONNECTED;
}

void LinGuider::readLinGuider()
{
    while (tcpSocket->atEnd() == false)
    {
        rawBuffer += tcpSocket->readAll();

        while (1)
        {
            if (rawBuffer.count() < 8)
                break;

            if (Options::guideLogging())
                qDebug() << "Guide:" << rawBuffer;

            qint16 magicNumber = *(reinterpret_cast<qint16 *>(rawBuffer.data()));
            if (magicNumber != 0x02)
            {
                emit newLog(i18n("Invalid response."));
                rawBuffer = rawBuffer.mid(1);
                continue;
            }

            qint16 command = *(reinterpret_cast<qint16 *>(rawBuffer.data() + 2));
            if (command < GET_VER || command > GET_RA_DEC_DRIFT)
            {
                emit newLog(i18n("Invalid response."));
                rawBuffer = rawBuffer.mid(1);
                continue;
            }

            qint16 datalen = *(reinterpret_cast<qint16 *>(rawBuffer.data() + 4));
            if (rawBuffer.count() < datalen + 8)
                break;

            QString reply = rawBuffer.mid(8, datalen);
            processResponse(static_cast<LinGuiderCommand>(command), reply);
            rawBuffer = rawBuffer.mid(8 + datalen);
        }
    }
}

void LinGuider::processResponse(LinGuiderCommand command, const QString &reply)
{
    if (reply == "Error: Guiding not started.")
    {
        state = IDLE;
        emit newStatus(GUIDE_ABORTED);
        deviationTimer.stop();
        return;
    }

    switch (command)
    {
        case GET_VER:
            emit newLog(i18n("Connected to LinGuider %1", reply));
            if (reply < "v.4.1.0")
            {
                emit newLog(
                    i18n("Only LinGuider v4.1.0 or higher is supported. Please upgrade LinGuider and try again."));
                Disconnect();
            }

            sendCommand(GET_GUIDER_STATE);
            break;

        case GET_GUIDER_STATE:
            if (reply == "GUIDING")
            {
                state = GUIDING;
                emit newStatus(GUIDE_GUIDING);
                deviationTimer.start();
            }
            else
            {
                state = IDLE;
                deviationTimer.stop();
            }
            break;

        case FIND_STAR:
        {
            emit newLog(i18n("Auto star selected %1", reply));
            QStringList pos = reply.split(' ');
            if (pos.count() == 2)
            {
                starCenter = reply;
                sendCommand(SET_GUIDER_RETICLE_POS, reply);
            }
            else
            {
                emit newLog(i18n("Failed to process star position."));
                emit newStatus(GUIDE_CALIBRATION_ERROR);
            }
        }
        break;

        case SET_GUIDER_RETICLE_POS:
            if (reply == "OK")
            {
                sendCommand(SET_GUIDER_SQUARE_POS, starCenter);
            }
            else
            {
                emit newLog(i18n("Failed to set guider reticle position."));
                emit newStatus(GUIDE_CALIBRATION_ERROR);
            }
            break;

        case SET_GUIDER_SQUARE_POS:
            if (reply == "OK")
            {
                emit newStatus(GUIDE_CALIBRATION_SUCESS);
            }
            else
            {
                emit newLog(i18n("Failed to set guider square position."));
                emit newStatus(GUIDE_CALIBRATION_ERROR);
            }
            break;

        case GUIDER:
            if (reply == "OK")
            {
                if (state == IDLE)
                {
                    emit newStatus(GUIDE_GUIDING);
                    state = GUIDING;

                    deviationTimer.start();
                }
                else
                {
                    emit newStatus(GUIDE_IDLE);
                    state = IDLE;

                    deviationTimer.stop();
                }
            }
            else
            {
                if (state == IDLE)
                    emit newLog(i18n("Failed to start guider."));
                else
                    emit newLog(i18n("Failed to stop guider."));
            }
            break;

        case GET_RA_DEC_DRIFT:
        {
            if (state != GUIDING)
            {
                state = GUIDING;
                emit newStatus(GUIDE_GUIDING);
            }

            QStringList pos = reply.split(' ');
            if (pos.count() == 2)
            {
                bool raOK = false, deOK = false;

                double raDev = pos[0].toDouble(&raOK);
                double deDev = pos[1].toDouble(&deOK);

                if (raOK && deOK)
                    emit newAxisDelta(raDev, deDev);
            }
            else
            {
                emit newLog(i18n("Failed to get RA/DEC Drift."));
            }
        }
        break;

        case SET_DITHERING_RANGE:
            if (reply == "OK")
            {
                sendCommand(DITHER);

                deviationTimer.stop();
            }
            else
            {
                emit newLog(i18n("Failed to set dither range."));
            }
            break;

        case DITHER:
            if (reply == "Long time cmd finished")
                emit newStatus(GUIDE_DITHERING_SUCCESS);
            else
                emit newStatus(GUIDE_DITHERING_ERROR);

            state = GUIDING;
            deviationTimer.start();
            break;

        default:
            break;
    }
}

void LinGuider::onConnected()
{
    connection = CONNECTED;

    emit newStatus(GUIDE_CONNECTED);
    // Get version

    sendCommand(GET_VER);
}

void LinGuider::sendCommand(LinGuiderCommand command, const QString &args)
{
    // Command format: Magic Number (0x00 0x02), cmd (2 bytes), len_of_param (4 bytes), param (ascii)

    int size = 8 + args.size();

    QByteArray cmd(size, 0);

    // Magic number
    cmd[0] = 0x02;
    cmd[1] = 0x00;

    // Command
    cmd[2] = command;
    cmd[3] = 0x00;

    // Len
    qint32 len = args.size();
    memcpy(cmd.data() + 4, &len, 4);

    // Params
    if (args.isEmpty() == false)
        memcpy(cmd.data() + 8, args.toLatin1().data(), args.size());

    tcpSocket->write(cmd);
}

bool LinGuider::calibrate()
{
    // Let's start calibration. It is already calibrated but in this step we auto-select and star and set the square
    emit newStatus(Ekos::GUIDE_CALIBRATING);

    sendCommand(FIND_STAR);

    return true;
}

bool LinGuider::guide()
{
    sendCommand(GUIDER, "start");
    return true;
}

bool LinGuider::abort()
{
    sendCommand(GUIDER, "stop");
    return true;
}

bool LinGuider::suspend()
{
    return abort();
}

bool LinGuider::resume()
{
    return guide();
}

bool LinGuider::dither(double pixels)
{
    QString pixelsString = QString::number(pixels, 'f', 2);
    QString args         = QString("%1 %2").arg(pixelsString, pixelsString);

    sendCommand(SET_DITHERING_RANGE, args);

    return true;
}
}
