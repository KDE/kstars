/*  CalibrationProcess class test.
    Copyright (C) 2021 Hy Murveit

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ekos/guide/internalguide/calibrationprocess.h"

#include <QTest>
#include <QObject>

#include "Options.h"
#include "ekos/guide/internalguide/calibration.h"
//#include "guidelog.h"
#include "indi/indicommon.h"
#include "ekos/ekos.h"

using Ekos::GuideState;
using Ekos::CalibrationProcess;

struct CalibrationIteration
{
    double x, y;
    GuideDirection pulseDir;
    int pulseMsec;
    GuideState state;
};

struct CalibrationTest
{
    // input parameters
    bool twoAxis, justRA;
    int iterations;
    bool backlash;
    int pulse;
    int maxMove;
    // input data
    QVector<CalibrationIteration> io;
    // calibration results
    double raAngle, decAngle;
    double raMsPerPixel, decMsPerPixel;
    bool decSwap;
};

class TestCalibrationProcess : public QObject
{
        Q_OBJECT

    public:
        TestCalibrationProcess();
        ~TestCalibrationProcess() override = default;

    private slots:
        void basicTest();

    private:
        void runTest(const CalibrationTest &test);
};

#include "testcalibrationprocess.moc"

TestCalibrationProcess::TestCalibrationProcess() : QObject()
{
}

bool compareDouble(double d1, double d2, double tolerance = .001)
{
    return (fabs(d1 - d2) < tolerance);
}

bool checkCalibrationProcessOutputs(CalibrationProcess *cp,
                                    GuideDirection pulseDir, int pulseMsec,
                                    GuideState calibrationState)
{
    GuideDirection dir;
    int msec;
    cp->getPulse(&dir, &msec);
    if (dir != pulseDir || msec != pulseMsec)
    {
        fprintf(stderr, "dir,pulse %d %d don't match %d,%d\n",
                static_cast<int>(dir), msec, pulseDir, pulseMsec);
        return false;
    }

    Ekos::GuideState status = cp->getStatus();
    if (status != calibrationState)
    {
        fprintf(stderr, "status %d doesn't match %d\n",
                static_cast<int>(status), static_cast<int>(calibrationState));
        return false;
    }
    return true;
}

// These test cases are taken from scraping data from running the simulator with the calibration
// scheme (before a refractor, had seemed to be working well for a number of years).

// Standard successful calibration. Hits the limit of 5 iterations per direction--doesn't reach the 15-pixel max-move.
// Sumulator had binning 2x2, so moved fewer pixels per pulse.
CalibrationTest calTest1 =
{
    /*twoAxis*/true, /*justRA*/false, /*iterations*/5, /*backlash*/true, /*pulse*/1100, /*maxMove*/15,
    {   { 104.989693, 85.002304, GuideDirection::RA_INC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 104.999672, 85.989754, GuideDirection::RA_INC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 105.013229, 87.017471, GuideDirection::RA_INC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 104.977692, 87.086075, GuideDirection::RA_INC_DIR, 2200, GuideState::GUIDE_CALIBRATING },
        { 104.989830, 89.007881, GuideDirection::RA_INC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 105.034843, 89.030762, GuideDirection::RA_DEC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 104.995323, 88.999382, GuideDirection::RA_DEC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 105.000000, 88.005966, GuideDirection::RA_DEC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 104.995422, 86.993683, GuideDirection::RA_DEC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 105.005203, 86.976059, GuideDirection::RA_DEC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 105.018433, 86.018028, GuideDirection::DEC_INC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 106.029907, 86.026863, GuideDirection::DEC_INC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 105.992226, 85.968170, GuideDirection::DEC_INC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 107.023537, 85.995636, GuideDirection::DEC_INC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 106.964882, 85.875992, GuideDirection::DEC_INC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 107.960190, 86.033669, GuideDirection::DEC_INC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 108.015701, 86.008209, GuideDirection::DEC_INC_DIR, 2200, GuideState::GUIDE_CALIBRATING },
        { 108.999283, 85.967667, GuideDirection::DEC_INC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 110.004478, 85.963715, GuideDirection::DEC_INC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 110.056694, 85.984901, GuideDirection::DEC_DEC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 110.009071, 85.982864, GuideDirection::DEC_DEC_DIR, 2200, GuideState::GUIDE_CALIBRATING },
        { 109.005264, 86.021690, GuideDirection::DEC_DEC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 107.971313, 86.018730, GuideDirection::DEC_DEC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 107.890785, 85.981506, GuideDirection::DEC_DEC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 107.052650, 85.997002, GuideDirection::DEC_DEC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 106.989433, 86.010635, GuideDirection::DEC_DEC_DIR, 1100, GuideState::GUIDE_CALIBRATING },
        { 105.983643, 85.992851, GuideDirection::NO_DIR, 0, GuideState::GUIDE_CALIBRATION_SUCCESS }
    },
    /*raAngle*/ 270.642141, /*decAngle*/357.982581, /*raMsPerPx*/ 1365.201045, /*decMsPerPx*/1777.789432, /*decSwap*/true
};

// Similar successful calibration. Because of the long (5000ms) pulses, hits the maxMove limit before the 10 iterations.
CalibrationTest calTest2 =
{
    /*twoAxis*/true, /*justRA*/false, /*iterations*/10, /*backlash*/true, /*pulse*/5000, /*maxMove*/15,
    {   { 155.981155, 62.958744, GuideDirection::RA_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 155.995193, 66.021431, GuideDirection::RA_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 155.981567, 67.996651, GuideDirection::RA_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 156.015305, 71.015106, GuideDirection::RA_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 156.101456, 74.015106, GuideDirection::RA_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 155.998688, 77.011536, GuideDirection::RA_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 156.046844, 80.006264, GuideDirection::RA_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 156.001404, 77.005074, GuideDirection::RA_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 155.995056, 73.996086, GuideDirection::RA_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 155.955582, 71.022438, GuideDirection::RA_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 156.002792, 67.984299, GuideDirection::RA_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 155.975494, 65.963425, GuideDirection::RA_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 155.978531, 63.003754, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 158.011566, 63.041481, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 161.019012, 62.996876, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 163.008865, 62.995979, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 165.022339, 63.001591, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 168.014999, 62.961784, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 170.014236, 62.975964, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 172.002640, 63.029968, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 174.016953, 63.025021, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 177.018311, 63.009003, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 174.006042, 63.034149, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 172.016876, 62.972244, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 170.013077, 62.983200, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 167.967270, 62.898540, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 164.990936, 63.028084, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 162.964462, 62.978321, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 161.016037, 63.011227, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 158.015213, 63.022118, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        { 156.009674, 62.980377, GuideDirection::NO_DIR, 0, GuideState::GUIDE_CALIBRATION_SUCCESS }
    },
    /*raAngle*/ 270.220776, /*decAngle*/359.956572, /*raMsPerPx*/ 1759.773729, /*decMsPerPx*/2187.595339, /*decSwap*/true
};

// Standard successful calibration. Limited by the 25 max-move (had binning 1x1).
CalibrationTest calTest3 =
{
    /*twoAxis*/true, /*justRA*/false, /*iterations*/10, /*backlash*/true, /*pulse*/5000, /*maxMove*/25,
    {
        {446.999634, 55.999676, GuideDirection::RA_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {447.034882, 61.971882, GuideDirection::RA_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {446.993652, 67.997871, GuideDirection::RA_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {446.981628, 73.984337, GuideDirection::RA_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {446.996338, 80.001945, GuideDirection::RA_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {446.988403, 85.002335, GuideDirection::RA_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {446.994995, 79.995117, GuideDirection::RA_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {446.998596, 73.953369, GuideDirection::RA_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {446.974060, 67.939110, GuideDirection::RA_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {447.005188, 61.985844, GuideDirection::RA_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {447.020874, 55.966709, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {450.979340, 55.993347, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {455.958221, 56.010612, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {460.005951, 55.988449, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {465.008972, 56.003986, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {469.441742, 55.977436, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {474.052795, 56.021942, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {478.997711, 55.995457, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {474.003296, 55.990456, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {469.449066, 55.949821, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {464.989258, 56.006073, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {460.006256, 55.995888, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {455.963013, 56.034840, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {451.027283, 56.041237, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {447.000793, 56.051575, GuideDirection::NO_DIR, 0, GuideState::GUIDE_CALIBRATION_SUCCESS }
    },
    /*raAngle*/ 269.977814, /*decAngle*/359.995686, /*raMsPerPx*/ 861.989870, /*decMsPerPx*/1070.726035, /*decSwap*/true
};

// Like above, but tricked the simulator to move in the opposite dec direction --> decSwap = false.
CalibrationTest calTest4 =
{
    /*twoAxis*/true, /*justRA*/false, /*iterations*/10, /*backlash*/true, /*pulse*/5000, /*maxMove*/25,
    {

        {157.016357, 251.045471, GuideDirection::RA_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {156.975937, 257.043976, GuideDirection::RA_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {157.006653, 263.000519, GuideDirection::RA_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {157.004440, 269.004608, GuideDirection::RA_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {156.989700, 275.023102, GuideDirection::RA_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {157.025482, 281.030853, GuideDirection::RA_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {157.013367, 275.001831, GuideDirection::RA_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {157.000717, 269.004669, GuideDirection::RA_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {157.008133, 263.028442, GuideDirection::RA_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {157.025223, 256.971863, GuideDirection::RA_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {157.004089, 251.025360, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {151.997040, 250.967361, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {147.966919, 251.009552, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {143.053177, 251.002197, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {139.009094, 250.998291, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {133.968353, 251.036285, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {129.004593, 251.000305, GuideDirection::DEC_INC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {125.026917, 251.038864, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {129.021027, 250.987061, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {133.992371, 250.999512, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {138.996811, 251.009186, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {143.031677, 250.988510, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {148.006699, 251.001663, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {151.995514, 251.071701, GuideDirection::DEC_DEC_DIR, 5000, GuideState::GUIDE_CALIBRATING },
        {157.012283, 251.060547, GuideDirection::NO_DIR, 0, GuideState::GUIDE_CALIBRATION_SUCCESS }
    },
    /*raAngle*/ 270.017435, /*decAngle*/180.151901, /*raMsPerPx*/ 833.739546, /*decMsPerPx*/1112.338058, /*decSwap*/false
};


void TestCalibrationProcess::runTest(const CalibrationTest &test)
{
    //Options::setTwoAxisEnabled(true);
    Options::setAutoModeIterations(test.iterations);
    Options::setGuideCalibrationBacklash(test.backlash);
    Options::setCalibrationPulseDuration(test.pulse);
    Options::setCalibrationMaxMove(test.maxMove);

    Calibration calibration;

    std::unique_ptr<CalibrationProcess> calibrationProcess;
    calibrationProcess.reset(new CalibrationProcess(test.io[0].x, test.io[0].y, test.justRA));
    QVERIFY(!calibrationProcess->inProgress());
    calibrationProcess->useCalibration(&calibration);

    for (const auto &d : test.io)
    {
        calibrationProcess->iterate(d.x, d.y);
        bool checkCalibrationProcessOutputs(CalibrationProcess * cp,
                                            GuideDirection pulseDir, int pulseMsec,
                                            GuideState calibrationState);
        QVERIFY(checkCalibrationProcessOutputs(calibrationProcess.get(), d.pulseDir, d.pulseMsec, d.state));
    }

    QVERIFY(compareDouble(calibration.getRAAngle(), test.raAngle));
    QVERIFY(compareDouble(calibration.getDECAngle(), test.decAngle));
    QVERIFY(compareDouble(calibration.raPulseMillisecondsPerArcsecond() * calibration.xArcsecondsPerPixel(),
                          test.raMsPerPixel));
    QVERIFY(compareDouble(calibration.decPulseMillisecondsPerArcsecond() * calibration.yArcsecondsPerPixel(),
                          test.decMsPerPixel));
    QVERIFY(calibration.declinationSwapEnabled() == test.decSwap);
}

void TestCalibrationProcess::basicTest()
{
    runTest(calTest1);
    runTest(calTest2);
    runTest(calTest3);
    runTest(calTest4);
}

QTEST_GUILESS_MAIN(TestCalibrationProcess)
