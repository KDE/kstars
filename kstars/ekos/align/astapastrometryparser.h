/*  ASTAP Parser
    Copyright (C) 2019 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include "astrometryparser.h"
#include "indi/indiccd.h"

#include <QPointer>
#include <QProcess>

namespace Ekos
{
class Align;

/**
 * @class  ASTAPAstrometryParser
 * ASTAPAstrometryParser invokes the local ASTAP solver.
 *
 * @author Jasem Mutlaq
 */
class ASTAPAstrometryParser : public AstrometryParser
{
        Q_OBJECT

    public:
        ASTAPAstrometryParser();
        virtual ~ASTAPAstrometryParser() override = default;

        virtual void setAlign(Align *_align) override
        {
            align = _align;
        }
        virtual bool init() override;
        virtual void verifyIndexFiles(double fov_x, double fov_y) override;
        virtual bool startSolver(const QString &filename, const QStringList &args, bool generated = true) override;
        virtual bool stopSolver() override;

    public slots:
        void solverComplete(int exitCode, QProcess::ExitStatus exitStatus);

    private:
        Align *align { nullptr };
        QTime solverTimer;
        QPointer<QProcess> solver;
};
}
