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

#include "guide.h"

class QTcpSocket;

namespace Ekos
{

/**
 * @class  PHD2
 * Uses external PHD2 for guiding.
 *
 * @authro Jasem Mutlaq
 */
class PHD2 : public QObject
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

    void connectPHD2();
    void disconnectPHD2();

    bool isConnected();
    bool isCalibrating() { return state == CALIBRATING; }
    bool isCalibrationComplete() { return state > CALIBRATING; }
    bool isCalibrationSuccessful() { return state >= CALIBRATION_SUCCESSFUL; }
    bool isGuiding()     { return state == GUIDING; }
    bool isDithering()   { return state == DITHERING; }

    void setCCDMountParams(double ccd_pix_w, double ccd_pix_h, double mount_focal);

    void setEquipmentConnected(bool enable);

    bool startGuiding();
    bool stopGuiding();
    bool pauseGuiding();
    bool resumeGuiding();
    bool dither(double pixels);

private slots:

    void readPHD2();
    void displayError(QAbstractSocket::SocketError socketError);

signals:

    void newLog(const QString &);
    void connected();
    void disconnected();
    void calibrationCompleted(bool);
    void ditherComplete();
    void ditherFailed();
    void newAxisDelta(double delta_ra, double delta_dec);
    void autoGuidingToggled(bool);
    void guideReady();
    void newStatus(Ekos::GuideState);

private:

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

    double ccd_pixel_width, ccd_pixel_height, focal;
};

}

#endif // PHD2_H
