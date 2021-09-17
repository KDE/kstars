/*
    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
                double pixelScale = 0;                             // arcseconds/pixel
                int binning = 1;
                double focalLength = 0;                            // millimeters
                // Recent mount position.
                double ra = 0, dec = 0, azimuth = 0, altitude = 0; // degrees
                ISD::Telescope::PierSide pierSide = ISD::Telescope::PierSide::PIER_UNKNOWN;
                double xangle = 0.0, yangle = 0.0;                 // degrees, x,y axis vs ra,dec.
                double xrate = 1.0, yrate = 1.0;                   // pixels/second of pulsing.
        };

        class GuideData
        {
            public:
                enum GuideDataType { MOUNT, DROP };
                GuideDataType type = MOUNT;
                double dx = 0, dy = 0;                            // Should be in units of pixels.
                double raDistance = 0, decDistance = 0;           // Should be in units of arcseconds.
                double raGuideDistance = 0, decGuideDistance = 0; // Should be in units of arcseconds.
                int raDuration = 0, decDuration = 0;              // Should be in units of milliseconds.
                GuideDirection raDirection, decDirection = NO_DIR;
                double mass = 0;
                double snr = 0;
                // From https://openphdguiding.org/PHD2_User_Guide.pdf and logs
                enum ErrorCode
                {
                    NO_ERRORS = 0,
                    STAR_SATURATED = 1,
                    LOW_SNR = 2,
                    STAR_LOST_LOW_MASS = 3,
                    EDGE_OF_FRAME = 4,
                    STAR_MASS_CHANGED = 5,
                    STAR_LOST_MASS_CHANGED = 6,
                    NO_STAR_FOUND = 7
                };
                ErrorCode code = NO_ERRORS;
        };

        GuideLog();
        ~GuideLog();

        // Won't log unless enable() is called.
        void enable()
        {
            enabled = true;
        }
        void disable()
        {
            enabled = false;
        }

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
