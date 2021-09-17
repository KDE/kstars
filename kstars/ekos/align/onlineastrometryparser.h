/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "astrometryparser.h"

#include <QTime>

class QNetworkAccessManager;
class QNetworkReply;

namespace Ekos
{
class Align;

/**
 * @class  OnlineAstrometryParser
 * OnlineAstrometryParser invokes the online services provided by astrometry.net solver to
 * find solutions to captured images.
 *
 * @author Jasem Mutlaq
 */
class OnlineAstrometryParser : public AstrometryParser
{
        Q_OBJECT

    public:
        OnlineAstrometryParser();

        virtual void setAlign(Align *_align) override
        {
            align = _align;
        }
        virtual bool init() override;
        virtual void verifyIndexFiles(double fov_x, double fov_y) override;
        virtual bool startSolver(const QString &filename, const QStringList &args, bool generated = true) override;
        virtual bool stopSolver() override;

        typedef enum
        {
            NO_STAGE,
            AUTH_STAGE,
            UPLOAD_STAGE,
            JOB_ID_STAGE,
            JOB_STATUS_STAGE,
            JOB_CALIBRATION_STAGE
        } WorkflowStage;

    public slots:
        void onResult(QNetworkReply *reply);
        void uploadFile();
        void getJobID();
        void checkJobs();
        void checkJobCalibration();
        void resetSolver();
    signals:
        void authenticateFinished();
        void uploadFinished();
        void jobIDFinished();
        void jobFinished();

    private:
        void authenticate();

        WorkflowStage workflowStage { NO_STAGE };
        QNetworkAccessManager *networkManager { nullptr };
        QString sessionKey;
        int subID { 0 };
        int jobID { 0 };
        int job_retries { 0 };
        int solver_retries { 0 };
        QTime solverTimer;
        QString filename, units;
        double lowerScale { 0 };
        double upperScale { 0 };
        double center_ra { 0 };
        double center_dec { 0 };
        double radius { 0 };
        double pixscale { 0 };
        bool useWCSCenter { false };
        int parity { 0 };
        double ra { 0 };
        double dec { 0 };
        double orientation { 0 };
        int downsample_factor { 0 };
        bool isGenerated { true };
        Align *align { nullptr };
};
}
