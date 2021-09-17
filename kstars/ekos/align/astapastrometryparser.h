/*
    SPDX-FileCopyrightText: 2019 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
