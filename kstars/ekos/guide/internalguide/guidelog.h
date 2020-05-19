/*  GuideLog class.
    Copyright (C) 2020 Hy Murveit

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <QElapsedTimer>
#include <QFile>

#include "indi/indicommon.h"
#include "indi/inditelescope.h"

// This class will help write guide log files, using the PHD2 guide log format.

class GuideLog
{
public:
    class GuideInfo
    {
    public:
        double pixelScale = 0;
        int binning = 1;
        double focalLength = 0;
        // Recent mount position.
        double ra = 0, dec = 0, azimuth = 0, altitude = 0;
        ISD::Telescope::PierSide pierSide;
    };

    class GuideData
    {
    public:
        enum GuideDataType { MOUNT, DROP };
        GuideDataType type;
        double dx, dy;
        double raDistance, decDistance;
        double raGuideDistance, decGuideDistance;
        int raDuration, decDuration;
        GuideDirection raDirection, decDirection;
        double mass;
        double snr;
        // From https://openphdguiding.org/PHD2_User_Guide.pdf and logs
        enum ErrorCode {
            NO_ERROR = 0,
            STAR_SATURATED = 1,
            LOW_SNR = 2,
            STAR_LOST_LOW_MASS = 3,
            EDGE_OF_FRAME = 4,
            STAR_MASS_CHANGED = 5,
            STAR_LOST_MASS_CHANGED = 6,
            NO_STAR_FOUND = 7
        };
        ErrorCode code;
    };

    GuideLog();
    ~GuideLog();

    // Won't log unless enable() is called.
    void enable() { enabled = true; }
    void disable() { enabled = false; }

    // These are called for each guiding session.
    void startGuiding(const GuideInfo &info);
    void addGuideData(const GuideData &data);
    void endGuiding();

    // These are called for each calibration session.
    void startCalibration(const GuideInfo &info);
    void addCalibrationData(GuideDirection direction, double x, double y, double xOrigin, double yOrigin);
    void endCalibrationSection(GuideDirection direction, double degrees);
    void endCalibration(double raSpeed, double decSpeed);

    // INFO messages
    void ditherInfo(double dx, double dy, double x, double y);
    void pauseInfo();
    void resumeInfo();
    void settleStartedInfo();
    void settleCompletedInfo();

    // Deal with suspend, resume, dither, ...
private:
    // Write the file header and footer.
    void startLog();
    void endLog();
    void appendToLog(const QString &lines);

    // Log file info.
    QFile logFile;
    QString logFileName;

    // Message indeces and timers.
    int guideIndex = 1;
    int calibrationIndex = 1;
    QElapsedTimer timer;

    // Used to write and end-of-guiding message on exit, if this was not called.
    bool isGuiding = false;

    // Variable used to detect calibration change of direction.
    GuideDirection lastCalibrationDirection = NO_DIR;

    // If false, no logging will occur.
    bool enabled = false;

    // True means the filename was created and the log's header has been written.
    bool initialized = false;
};
