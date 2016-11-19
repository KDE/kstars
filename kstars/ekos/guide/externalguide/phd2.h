/*  Ekos PHD2 Handler
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#ifndef PHD2_H
#define PHD2_H

#include <QAbstractSocket>
#include <QJsonArray>

#include "../guideinterface.h"

class QTcpSocket;

namespace Ekos
{

/**
 * @class  PHD2
 * Uses external PHD2 for guiding.
 *
 * @author Jasem Mutlaq
 * @version 1.1
 */
class PHD2 : public GuideInterface
{
        Q_OBJECT

public:

    typedef enum { Version, LockPositionSet, CalibrationComplete, StarSelected, StartGuiding, Paused, StartCalibration, AppState, CalibrationFailed, CalibrationDataFlipped, LoopingExposures,
                   LoopingExposuresStopped, Settling, SettleDone, StarLost, GuidingStopped, Resumed, GuideStep, GuidingDithered, LockPositionLost, Alert } PHD2Event;
    typedef enum { STOPPED, SELECTED, LOSTLOCK, PAUSED, LOOPING, CALIBRATING, CALIBRATION_FAILED, CALIBRATION_SUCCESSFUL, GUIDING, DITHERING, DITHER_FAILED, DITHER_SUCCESSFUL } PHD2State;
    typedef enum { DISCONNECTED, CONNECTING, CONNECTED, EQUIPMENT_DISCONNECTING, EQUIPMENT_DISCONNECTED, EQUIPMENT_CONNECTING, EQUIPMENT_CONNECTED  } PHD2Connection;
    typedef enum { PHD2_UNKNOWN, PHD2_RESULT, PHD2_EVENT, PHD2_ERROR } PHD2MessageType;

    PHD2();
    ~PHD2();

    bool Connect() override;
    bool Disconnect() override;
    bool isConnected() override { return (connection == CONNECTED); }

    bool calibrate() override;
    bool guide() override;
    bool abort() override;
    bool suspend() override;
    bool resume() override;
    bool dither(double pixels) override;

private slots:

    void readPHD2();
    void displayError(QAbstractSocket::SocketError socketError);

private:

    void setEquipmentConnected(bool enable);

    void sendJSONRPCRequest(const QString & method, const QJsonArray args = QJsonArray());
    void processJSON(const QJsonObject &jsonObj);

    void processPHD2Event(const QJsonObject &jsonEvent);
    void processPHD2State(const QString &phd2State);
    void processPHD2Error(const QJsonObject &jsonError);

    QTcpSocket *tcpSocket;
    qint64 methodID;

    QHash<QString, PHD2Event> events;

    PHD2State state;
    PHD2Connection connection;
    PHD2Event event;    
};

}

#endif // PHD2_H
