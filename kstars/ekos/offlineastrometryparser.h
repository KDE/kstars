/*  Astrometry.net Parser
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#ifndef OFFLINEASTROMETRYPARSER_H
#define OFFLINEASTROMETRYPARSER_H

#include <QMap>
#include <QProcess>
#include <QTime>

#include "astrometryparser.h"

namespace Ekos
{

class Align;

class OfflineAstrometryParser: public AstrometryParser
{
        Q_OBJECT

public:
    OfflineAstrometryParser();
    virtual ~OfflineAstrometryParser();

    virtual void setAlign(Align *_align) { align = _align; }
    virtual bool init();
    virtual void verifyIndexFiles(double fov_x, double fov_y);
    virtual bool startSovler(const QString &filename, const QStringList &args, bool generated=true);
    virtual bool stopSolver();

public slots:
    void solverComplete(int exist_status);
    void wcsinfoComplete(int exist_status);
    void logSolver();

private:
    bool astrometryNetOK();
    bool getAstrometryDataDir(QString &dataDir);

    QMap<float, QString> astrometryIndex;
    QProcess solver;
    QProcess wcsinfo;
    QTime solverTimer;
    QString fitsFile;
    bool astrometryFilesOK;
    Align *align;
};

}

#endif // OFFLINEASTROMETRYPARSER_H
