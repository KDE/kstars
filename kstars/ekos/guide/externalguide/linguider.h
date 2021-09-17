/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "../guideinterface.h"

#include <QAbstractSocket>
#include <QTimer>

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
    typedef enum {
        GET_VER = 1,
        SET_GUIDER_SQUARE_POS,
        SAVE_FRAME,
        DITHER,
        DITHER_NO_WAIT_XY,
        GET_DISTANCE,
        SAVE_FRAME_DECORATED,
        GUIDER,
        GET_GUIDER_STATE,
        SET_GUIDER_OVLS_POS,
        SET_GUIDER_RETICLE_POS,
        FIND_STAR,
        SET_DITHERING_RANGE,
        GET_RA_DEC_DRIFT
    } LinGuiderCommand;
    typedef enum { IDLE, GUIDING } LinGuiderState;
    typedef enum { DISCONNECTED, CONNECTING, CONNECTED } LinGuiderConnection;

    LinGuider();
    virtual ~LinGuider() override = default;

    bool Connect() override;
    bool Disconnect() override;
    bool isConnected() override { return (connection == CONNECTED); }

    bool calibrate() override;
    bool guide() override;
    bool abort() override;
    bool suspend() override;
    bool resume() override;
    bool dither(double pixels) override;
    bool clearCalibration() override { return true;}

  private slots:

    void readLinGuider();
    void onConnected();
    void displayError(QAbstractSocket::SocketError socketError);

  private:
    void sendCommand(LinGuiderCommand command, const QString &args = QString());
    void processResponse(LinGuiderCommand command, const QString &reply);

    QTcpSocket *tcpSocket { nullptr };
    QByteArray rawBuffer;

    LinGuiderState state { IDLE };
    LinGuiderConnection connection { DISCONNECTED };

    QTimer deviationTimer;
    QString starCenter;
};
}
