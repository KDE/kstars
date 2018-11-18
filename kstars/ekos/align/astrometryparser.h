/*  Astrometry.net Parser
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include <QObject>

namespace Ekos
{
class Align;

/**
 * @class AstrometryParser
 * AstrometryParser is an interface for online and offline astrometry parsers.
 *
 * @author Jasem Mutlaq
 */

class AstrometryParser : public QObject
{
    Q_OBJECT

  public:
    AstrometryParser();
    virtual ~AstrometryParser() = default;

    virtual void setAlign(Align *align)                                                               = 0;
    virtual bool init()                                                                               = 0;
    virtual void verifyIndexFiles(double fov_x, double fov_y)                                         = 0;
    virtual bool startSovler(const QString &filename, const QStringList &args, bool generated = true) = 0;
    virtual bool stopSolver()                                                                         = 0;

  signals:
    void solverFinished(double orientation, double ra, double dec, double pixscale);
    void solverFailed();
};
}
