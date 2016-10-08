/*  Ekos Lin Guider Handler
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#ifndef LinGuider_H
#define LinGuider_H

#include <QAbstractSocket>
#include <QJsonArray>

#include "../guideinterface.h"

class QTcpSocket;

namespace Ekos
{

/**
 * @class  LinGuider
 * Uses external LinGuider for guiding.
 *
 * @author Jasem Mutlaq
 * @version 1.1
 */
class LinGuider : public GuideInterface
{
        Q_OBJECT

public:

    typedef enum { Version, LockPositionSet, CalibrationComplete, StarSelected, StartGuiding, Paused, StartCalibration, AppState, CalibrationFailed, CalibrationDataFlipped, LoopingExposures,
                   LoopingExposuresStopped, Settling, SettleDone, StarLost, GuidingStopped, Resumed, GuideStep, GuidingDithered, LockPositionLost, Alert } LinGuiderEvent;
    typedef enum { STOPPED, SELECTED, LOSTLOCK, PAUSED, LOOPING, CALIBRATING, CALIBRATION_FAILED, CALIBRATION_SUCCESSFUL, GUIDING, DITHERING, DITHER_FAILED, DITHER_SUCCESSFUL } LinGuiderState;
    typedef enum { DISCONNECTED, CONNECTING, CONNECTED, EQUIPMENT_DISCONNECTING, EQUIPMENT_DISCONNECTED, EQUIPMENT_CONNECTING, EQUIPMENT_CONNECTED  } LinGuiderConnection;
    typedef enum { LINGUIDER_UNKNOWN, LINGUIDER_RESULT, LINGUIDER_EVENT, LINGUIDER_ERROR } LinGuiderMessageType;

    LinGuider();
    ~LinGuider();

    bool Connect() override;
    bool Disconnect() override;

    bool calibrate() override;
    bool guide() override;
    bool abort() override;
    bool suspend() override;
    bool resume() override;
    bool dither(double pixels) override;

private slots:

    void readLinGuider();
    void displayError(QAbstractSocket::SocketError socketError);

private:

    void setEquipmentConnected(bool enable);

    void sendJSONRPCRequest(const QString & method, const QJsonArray args = QJsonArray());
    void processJSON(const QJsonObject &jsonObj);

    void processLinGuiderEvent(const QJsonObject &jsonEvent);
    void processLinGuiderState(const QString &LinGuiderState);
    void processLinGuiderError(const QJsonObject &jsonError);

    QTcpSocket *tcpSocket;
    qint64 methodID;

    QHash<QString, LinGuiderEvent> events;

    LinGuiderState state;
    LinGuiderConnection connection;
    LinGuiderEvent event;
};

}

#endif // LinGuider_H
