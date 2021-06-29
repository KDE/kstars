/*  Astrometry.net Parser
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include "astrometryparser.h"

#include <QMap>
#include <QProcess>
#include <QPointer>
#include <QTime>

namespace Ekos
{
class Align;

/**
 * @class  OfflineAstrometryParser
 * OfflineAstrometryParser invokes the offline astrometry.net solver to find solutions to captured images.
 *
 * @author Jasem Mutlaq
 */

class OfflineAstrometryParser : public AstrometryParser
{
        Q_OBJECT

    public:
        OfflineAstrometryParser();
        virtual ~OfflineAstrometryParser() override = default;

        virtual void setAlign(Align *_align) override
        {
            align = _align;
        }
        virtual bool init() override;
        virtual void verifyIndexFiles(double fov_x, double fov_y) override;
        virtual bool startSolver(const QString &filename, const QStringList &args, bool generated = true) override;
        virtual bool stopSolver() override;

    public slots:
        void solverComplete(int exist_status);
        void wcsinfoComplete(int exist_status);
        void logSolver();

    private:
        bool astrometryNetOK();

        QMap<float, QString> astrometryIndex;
        QString parity;
        QPointer<QProcess> solver;
        QPointer<QProcess> sextractorProcess;
        QProcess wcsinfo;
        QTime solverTimer;
        QString fitsFile;
        bool astrometryFilesOK { false };
        Align *align { nullptr };
};
}
