/*  Astrometry.net Parser
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#ifndef ASTROMETRYPARSER_H
#define ASTROMETRYPARSER_H

#include <QObject>

namespace Ekos
{

class Align;

class AstrometryParser : public QObject
{
    Q_OBJECT

public:
    AstrometryParser();
    virtual ~AstrometryParser();

    virtual void setAlign(Align *align) = 0;
    virtual bool init() = 0;
    virtual void verifyIndexFiles(double fov_x, double fov_y) =0;
    virtual bool startSovler(const QString &filename, const QStringList &args, bool generated=true) =0;
    virtual bool stopSolver() = 0;

signals:
    void solverFinished(double orientation, double ra, double dec);
    void solverFailed();
};

}

#endif // ASTROMETRYPARSER_H
