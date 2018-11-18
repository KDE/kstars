/*  Astrometry.net Parser - Remote
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include "astrometryparser.h"
#include "indi/indiccd.h"

namespace Ekos
{
class Align;

/**
 * @class  RemoteAstrometryParser
 * RemoteAstrometryParser invokes the remote astrometry.net solver in the remote CCD driver to solve the captured image.
 * The offline astrometry.net plus index files must be installed and configured on the remote host running the INDI CCD driver.
 *
 * @author Jasem Mutlaq
 */
class RemoteAstrometryParser : public AstrometryParser
{
    Q_OBJECT

  public:
    RemoteAstrometryParser();
    virtual ~RemoteAstrometryParser() override = default;

    virtual void setAlign(Align *_align) override { align = _align; }
    virtual bool init() override;
    virtual void verifyIndexFiles(double fov_x, double fov_y) override;
    virtual bool startSovler(const QString &filename, const QStringList &args, bool generated = true) override;
    virtual bool stopSolver() override;

    void setAstrometryDevice(ISD::GDInterface *device);
    void setEnabled(bool enable);
    bool sendArgs(const QStringList &args);
    bool setCCD(const QString& ccd);

  public slots:
    void checkStatus(ISwitchVectorProperty *svp);
    void checkResults(INumberVectorProperty *nvp);

  private:
    ISD::GDInterface *remoteAstrometry { nullptr };
    bool solverRunning { false };
    bool captureRunning { false };
    Align *align { nullptr };
    QTime solverTimer;
    QString parity;
    QString targetCCD;
};
}
