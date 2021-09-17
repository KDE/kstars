/*
    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "guidelog.h"

#include <math.h>
#include <cstdint>

#include <QDateTime>
#include <QStandardPaths>
#include <QTextStream>

#include "auxiliary/kspaths.h"
#include <version.h>

// This class writes a guide log that is compatible with the phdlogview program.
// See https://openphdguiding.org/phd2-log-viewer/ for details on that program.

namespace
{

// These conversion aren't correct. I believe the KStars way of doing it, with RA_INC etc
// is better, however, it is consistent and will work with phdlogview.
QString directionString(GuideDirection direction)
{
    switch(direction)
    {
        case DEC_INC_DIR:
            return "N";
        case DEC_DEC_DIR:
            return "S";
        case RA_DEC_DIR:
            return "E";
        case RA_INC_DIR:
            return "W";
        case NO_DIR:
            return "";
    }
    return "";
}

QString directionStringLong(GuideDirection direction)
{
    switch(direction)
    {
        case DEC_INC_DIR:
            return "North";
        case DEC_DEC_DIR:
            return "South";
        case RA_DEC_DIR:
            return "East";
        case RA_INC_DIR:
            return "West";
        case NO_DIR:
            return "";
    }
    return "";
}

QString pierSideString(ISD::Telescope::PierSide side)
{
    switch(side)
    {
        case ISD::Telescope::PierSide::PIER_WEST:
            return QString("West");
        case ISD::Telescope::PierSide::PIER_EAST:
            return QString("East");
        case ISD::Telescope::PierSide::PIER_UNKNOWN:
            return QString("Unknown");
    }
    return QString("");
}

double degreesToHours(double degrees)
{
    return 24.0 * degrees / 360.0;
}

} // namespace

GuideLog::GuideLog()
{
}

GuideLog::~GuideLog()
{
    endLog();
}

void GuideLog::appendToLog(const QString &lines)
{
    if (!enabled)
        return;
    QTextStream out(&logFile);
    out << lines;
    out.flush();
}

// Creates the filename and opens the file.
// Prints a line like the one below.
//   KStars version 3.4.0. PHD2 log version 2.5. Log enabled at 2019-11-21 00:00:48
void GuideLog::startLog()
{
    QDir dir = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("guidelogs");
    dir.mkpath(".");

    logFileName = dir.filePath("guide_log-" + QDateTime::currentDateTime().toString("yyyy-MM-ddThh-mm-ss") + ".txt");
    logFile.setFileName(logFileName);
    logFile.open(QIODevice::WriteOnly | QIODevice::Text);

    appendToLog(QString("KStars version %1. PHD2 log version 2.5. Log enabled at %2\n\n")
                .arg(KSTARS_VERSION)
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));

    initialized = true;
}

// Prints a line like the one below and closes the file.
//   Log closed at 2019-11-21 08:46:38
void GuideLog::endLog()
{
    if (!enabled || !initialized)
        return;

    if (isGuiding && initialized)
        endGuiding();

    appendToLog(QString("Log closed at %1\n")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));
    logFile.close();
}

// Output at the start of Guiding.
// Note that in the PHD2 generated versions of this log, there is a lot of guiding information here.
// We just output two lines which phdlogview needs, for pixel scale and RA/DEC.
void GuideLog::startGuiding(const GuideInfo &info)
{
    if (!enabled)
        return;
    if (!initialized)
        startLog();

    // Currently phdlogview just reads the Pixel scale value on the 2nd line, and
    // just reads the Dec value on the 3rd line.
    // Note the log wants hrs for RA, the input to this method is in degrees.
    appendToLog(QString("Guiding Begins at %1\n"
                        "Pixel scale = %2 arc-sec/px, Binning = %3, Focal length = %4 mm\n"
                        "RA = %5 hr, Dec = %6 deg, Hour angle = N/A hr, Pier side = %7, "
                        "Rotator pos = N/A, Alt = %8 deg, Az = %9 deg\n"
                        "Mount = mount, xAngle = %10, xRate = %11, yAngle = %12, yRate = %13\n"
                        "Frame,Time,mount,dx,dy,RARawDistance,DECRawDistance,RAGuideDistance,DECGuideDistance,"
                        "RADuration,RADirection,DECDuration,DECDirection,XStep,YStep,StarMass,SNR,ErrorCode\n")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
                .arg(QString::number(info.pixelScale, 'f', 2))
                .arg(info.binning)
                .arg(info.focalLength)
                .arg(QString::number(degreesToHours(info.ra), 'f', 2))
                .arg(QString::number(info.dec, 'f', 1))
                .arg(pierSideString(info.pierSide))
                .arg(QString::number(info.altitude, 'f', 1))
                .arg(QString::number(info.azimuth, 'f', 1))
                .arg(QString::number(info.xangle, 'f', 1))
                .arg(QString::number(info.xrate, 'f', 3))
                .arg(QString::number(info.yangle, 'f', 1))
                .arg(QString::number(info.yrate, 'f', 3)));


    guideIndex = 1;
    isGuiding = true;
    timer.start();
}

// Prints a line that looks something like this:
//   55,467.914,"Mount",-1.347,-2.160,2.319,-1.451,1.404,-0.987,303,W,218,N,,,2173,26.91,0
// See page 56-57 in https://openphdguiding.org/PHD2_User_Guide.pdf for definitions of the fields.
void GuideLog::addGuideData(const GuideData &data)
{
    QString mountString = data.type == GuideData::MOUNT ? "\"Mount\"" : "\"DROP\"";
    QString xStepString = "";
    QString yStepString = "";
    appendToLog(QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18\n")
                .arg(guideIndex)
                .arg(QString::number(timer.elapsed() / 1000.0, 'f', 3))
                .arg(mountString)
                .arg(QString::number(data.dx, 'f', 3))
                .arg(QString::number(data.dy, 'f', 3))
                .arg(QString::number(data.raDistance, 'f', 3))
                .arg(QString::number(data.decDistance, 'f', 3))
                .arg(QString::number(data.raGuideDistance, 'f', 3))
                .arg(QString::number(data.decGuideDistance, 'f', 3))
                .arg(data.raDuration)
                .arg(directionString(data.raDirection))
                .arg(data.decDuration)
                .arg(directionString(data.decDirection))
                .arg(xStepString)
                .arg(yStepString)
                .arg(QString::number(data.mass, 'f', 0))
                .arg(QString::number(data.snr, 'f', 2))
                .arg(static_cast<int>(data.code)));
    ++guideIndex;
}

// Prints a line that looks like:
//   Guiding Ends at 2019-11-21 01:57:45
void GuideLog::endGuiding()
{
    appendToLog(QString("Guiding Ends at %1\n\n")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));
    isGuiding = false;
}

// Note that in the PHD2 generated versions of this log, there is a lot of calibration information here.
// We just output two lines which phdlogview needs, for pixel scale and RA/DEC.
void GuideLog::startCalibration(const GuideInfo &info)
{
    if (!enabled)
        return;
    if (!initialized)
        startLog();
    // Currently phdlogview just reads the Pixel scale value on the 2nd line, and
    // just reads the Dec value on the 3rd line.
    appendToLog(QString("Calibration Begins at %1\n"
                        "Pixel scale = %2 arc-sec/px, Binning = %3, Focal length = %4 mm\n"
                        "RA = %5 hr, Dec = %6 deg, Hour angle = N/A hr, Pier side = %7, "
                        "Rotator pos = N/A, Alt = %8 deg, Az = %9 deg\n"
                        "Direction,Step,dx,dy,x,y,Dist\n")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
                .arg(QString::number(info.pixelScale, 'f', 2))
                .arg(info.binning)
                .arg(info.focalLength)
                .arg(QString::number(degreesToHours(info.ra), 'f', 2))
                .arg(QString::number(info.dec, 'f', 1))
                .arg(pierSideString(info.pierSide))
                .arg(QString::number(info.altitude, 'f', 1))
                .arg(QString::number(info.azimuth, 'f', 1)));

    calibrationIndex = 1;
    timer.start();
    lastCalibrationDirection = NO_DIR;
}

// Prints a line that looks like:
//   West,2,-15.207,-1.037,54.800,58.947,15.242
void GuideLog::addCalibrationData(GuideDirection direction, double x, double y, double xOrigin, double yOrigin)
{
    if (direction != lastCalibrationDirection)
        calibrationIndex = 1;
    lastCalibrationDirection = direction;

    appendToLog(QString("%1,%2,%3,%4,%5,%6,%7\n")
                .arg(directionStringLong(direction))
                .arg(calibrationIndex)
                .arg(QString::number(x - xOrigin, 'f', 3))
                .arg(QString::number(y - yOrigin, 'f', 3))
                .arg(QString::number(x, 'f', 3))
                .arg(QString::number(y, 'f', 3))
                .arg(QString::number(hypot(x - xOrigin, y - yOrigin), 'f', 3)));

    // This is a little different than PHD2--they seem to count down in the reverse directions.
    calibrationIndex++;
}

// Prints a line that looks like:
//   West calibration complete. Angle = 106.8 deg
// Currently phdlogview ignores this line.
void GuideLog::endCalibrationSection(GuideDirection direction, double degrees)
{
    appendToLog(QString("%1 calibration complete. Angle = %2 deg\n")
                .arg(directionStringLong(direction))
                .arg(QString::number(degrees, 'f', 1)));
}

// Prints two lines that look like:
//  Calibration guide speeds: RA = 191.5 a-s/s, Dec = 408.0 a-s/s
//  Calibration complete
// The failed version is not in the PHD2 log, will be ignored by the viewer.
void GuideLog::endCalibration(double raSpeed, double decSpeed)
{
    if (raSpeed == 0 && decSpeed == 0)
        appendToLog(QString("Calibration complete (Failed)\n\n"));
    else
        appendToLog(QString("Calibration guide speeds: RA = %1 a-s/s, Dec = %2 a-s/s\n"
                            "Calibration complete\n\n")
                    .arg(QString::number(raSpeed, 'f', 1))
                    .arg(QString::number(decSpeed, 'f', 1)));
}

void GuideLog::ditherInfo(double dx, double dy, double x, double y)
{
    appendToLog(QString("INFO: DITHER by %1, %2, new lock pos = %3, %4\n")
                .arg(QString::number(dx, 'f', 3))
                .arg(QString::number(dy, 'f', 3))
                .arg(QString::number(x, 'f', 3))
                .arg(QString::number(y, 'f', 3)));
    // Below moved to ditherInfo from settleStartedInfo() to match phdlogview.
    appendToLog("INFO: SETTLING STATE CHANGE, Settling started\n");
}

void GuideLog::pauseInfo()
{
    appendToLog("INFO: Server received PAUSE\n");
}

void GuideLog::resumeInfo()
{
    appendToLog("INFO: Server received RESUME\n");
}

void GuideLog::settleStartedInfo()
{
    // This was moved to ditherInfo() to match phdlogview
    // appendToLog("INFO: SETTLING STATE CHANGE, Settling started\n");
}

void GuideLog::settleCompletedInfo()
{
    appendToLog("INFO: SETTLING STATE CHANGE, Settling complete\n");
}
