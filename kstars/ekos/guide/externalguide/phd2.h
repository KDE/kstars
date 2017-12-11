/*  Ekos PHD2 Handler
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include "../guideinterface.h"
#include "fitsviewer/fitsview.h"
#include <QPointer>

#include <QAbstractSocket>
#include <QJsonArray>

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
    typedef enum {
        Version,
        LockPositionSet,
        CalibrationComplete,
        StarSelected,
        StartGuiding,
        Paused,
        StartCalibration,
        AppState,
        CalibrationFailed,
        CalibrationDataFlipped,
        LoopingExposures,
        LoopingExposuresStopped,
        SettleBegin,
        Settling,
        SettleDone,
        StarLost,
        GuidingStopped,
        Resumed,
        GuideStep,
        GuidingDithered,
        LockPositionLost,
        Alert
    } PHD2Event;
    typedef enum {
        STOPPED,
        SELECTED,
        LOSTLOCK,
        PAUSED,
        LOOPING,
        CALIBRATING,
        CALIBRATION_FAILED,
        CALIBRATION_SUCCESSFUL,
        GUIDING,
        DITHERING,
        DITHER_FAILED,
        DITHER_SUCCESSFUL
    } PHD2State;
    typedef enum {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        EQUIPMENT_DISCONNECTING,
        EQUIPMENT_DISCONNECTED,
        EQUIPMENT_CONNECTING,
        EQUIPMENT_CONNECTED
    } PHD2Connection;
    typedef enum { PHD2_UNKNOWN, PHD2_RESULT, PHD2_EVENT, PHD2_ERROR,
                   PHD2_STAR_IMAGE } PHD2MessageType;

    PHD2();
    ~PHD2();

    bool Connect() override;
    bool Disconnect() override;
    bool isConnected() override { return (connection == CONNECTED || connection == EQUIPMENT_CONNECTED); }

    bool calibrate() override;
    bool guide() override;
    bool abort() override;
    bool suspend() override;
    bool resume() override;
    bool dither(double pixels) override;
    bool clearCalibration() override;

    void setGuideView(FITSView *guideView);

  private slots:

    void readPHD2();
    void displayError(QAbstractSocket::SocketError socketError);

  private:

    void setEquipmentConnected(bool enable);

    QPointer<FITSView> guideFrame;

    void sendJSONRPCRequest(const QString &method, const QJsonArray args = QJsonArray());
    void processJSON(const QJsonObject &jsonObj, QString rawString);

    void processPHD2Event(const QJsonObject &jsonEvent);
    void processStarImage(const QJsonObject &jsonStarFrame);
    void processPHD2State(const QString &phd2State);
    void processPHD2Error(const QJsonObject &jsonError);

    QTcpSocket *tcpSocket { nullptr };
    qint64 methodID { 1 };

    QHash<QString, PHD2Event> events;

    PHD2State state { STOPPED };
    PHD2Connection connection { DISCONNECTED };
    PHD2Event event { Alert };
    uint8_t setConnectedRetries { 0 };
};
}
