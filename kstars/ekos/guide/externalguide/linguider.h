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
#include <QTimer>

#include "../guideinterface.h"

class QTcpSocket;

namespace Ekos
{

/**
 * @class  LinGuider
 * Uses external LinGuider for guiding.
 *
 * @author Jasem Mutlaq
 * @version 1.0
 */
class LinGuider : public GuideInterface
{
        Q_OBJECT

public:

    typedef enum { GET_VER = 1, SET_GUIDER_SQUARE_POS, SAVE_FRAME, DITHER, DITHER_NO_WAIT_XY, GET_DISTANCE, SAVE_FRAME_DECORATED,
                   GUIDER, GET_GUIDER_STATE, SET_GUIDER_OVLS_POS, SET_GUIDER_RETICLE_POS, FIND_STAR, SET_DITHERING_RANGE, GET_RA_DEC_DRIFT} LinGuiderCommand;
    typedef enum { IDLE, GUIDING} LinGuiderState;
    typedef enum { DISCONNECTED, CONNECTING, CONNECTED} LinGuiderConnection;

    LinGuider();
    ~LinGuider();

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

    void readLinGuider();
    void onConnected();
    void displayError(QAbstractSocket::SocketError socketError);

private:
    void sendCommand(LinGuiderCommand command, const QString &args = QString());
    void processResponse(LinGuiderCommand command, const QString &reply);

    QTcpSocket *tcpSocket;


    LinGuiderState state;
    LinGuiderConnection connection;

    QTimer deviationTimer;
    QString starCenter;
};

}

#endif // LinGuider_H
