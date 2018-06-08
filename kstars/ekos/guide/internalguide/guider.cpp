/*  Ekos guide tool
    Copyright (C) 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "guider.h"

#include "gmath.h"
#include "kstars.h"
#include "kspaths.h"
#include "Options.h"
#include "scroll_graph.h"
#include "../phd2.h"
#include "../ekosmanager.h"
#include "fitsviewer/fitsview.h"

#include <KMessageBox>
#include <KLocalizedString>
#include <KNotifications/KNotification>

#include <cmath>
#include <cstdlib>
#include <cassert>

internalGuider::internalGuider(cgmath *mathObject, Ekos::Guide *parent) : QWidget(parent)
{
    ui.setupUi(this);

    guideModule = parent;

    phd2       = nullptr;
    targetChip = nullptr;

    m_useRapidGuide = false;
    first_frame     = false;
    first_subframe  = false;
    m_isSubFramed   = false;

    m_lostStarTries = 0;

    ui.comboBox_ThresholdAlg->clear();
    for (int i = 0; guide_square_alg[i].idx != -1; ++i)
        ui.comboBox_ThresholdAlg->addItem(QString(guide_square_alg[i].name));

    // connect ui
    connect(ui.rapidGuideCheck, SIGNAL(toggled(bool)), this, SLOT(onRapidGuideChanged(bool)));
    connect(ui.connectPHD2B, SIGNAL(clicked()), this, SLOT(connectPHD2()));
    connect(ui.captureB, SIGNAL(clicked()), this, SLOT(capture()));
    connect(ui.pushButton_StartStop, SIGNAL(clicked()), this, SLOT(onStartStopButtonClick()));
    connect(ui.ditherCheck, SIGNAL(toggled(bool)), this, SIGNAL(ditherToggled(bool)));

    pmath = mathObject;

    // init drift widget
    pDriftOut = new custom_drawer(ui.frame_Graph);
    pDriftOut->move(ui.frame_Graph->frameWidth(), ui.frame_Graph->frameWidth());
    pDriftOut->setAttribute(Qt::WA_NoSystemBackground, true);
    ui.frame_Graph->setAttribute(Qt::WA_NoSystemBackground, true);

    pDriftOut->set_source(drift_graph->get_buffer(), nullptr);

    drift_graph = new ScrollGraph(this, DRIFT_GRAPH_WIDTH, DRIFT_GRAPH_HEIGHT);
    drift_graph->set_visible_ranges(DRIFT_GRAPH_WIDTH, 60);
    drift_graph->on_paint();
    ui.frame_Graph->resize(DRIFT_GRAPH_WIDTH + 2 * ui.frame_Graph->frameWidth(),
                           DRIFT_GRAPH_HEIGHT + 2 * ui.frame_Graph->frameWidth());

    // not UI vars
    m_isStarted       = false;
    m_isReady         = false;
    half_refresh_rate = false;
    m_isDithering     = false;

    ui.ditherCheck->setChecked(Options::useDither());
    ui.ditherPixels->setValue(Options::ditherPixels());
    ui.spinBox_AOLimit->setValue(Options::aOLimit());

    QString logFileName = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/guide_log.txt";
    logFile.setFileName(logFileName);
}

internalGuider::~internalGuider()
{
    delete pDriftOut;
    delete drift_graph;
}

void internalGuider::setHalfRefreshRate(bool is_half)
{
    half_refresh_rate = is_half;
}

bool internalGuider::isGuiding(void) const
{
    return m_isStarted;
}

void internalGuider::setMathObject(cgmath *math)
{
    assert(math);
    pmath = math;
}

void internalGuider::setAO(bool enable)
{
    ui.spinBox_AOLimit->setEnabled(enable);
}

double internalGuider::getAOLimit()
{
    return ui.spinBox_AOLimit->value();
}

void internalGuider::setInterface(void)
{
    const cproc_out_params *out_params;
    info_params_t info_params;
    QString str;
    int rx, ry;

    assert(pmath);

    info_params = pmath->getInfoParameters();
    out_params  = pmath->getOutputParameters();

    drift_graph->get_visible_ranges(&rx, &ry);
    ui.spinBox_XScale->setValue(rx / drift_graph->get_grid_N());
    ui.spinBox_YScale->setValue(ry / drift_graph->get_grid_N());

    ui.comboBox_ThresholdAlg->setCurrentIndex(pmath->getSquareAlgorithmIndex());

    ui.l_RecommendedGain->setText(
        i18n("P: %1", QString().setNum(cgmath::preCalculateProportionalGain(Options::guidingRate()), 'f', 2)));
    ui.spinBox_GuideRate->setValue(Options::guidingRate());

    // info params...
    ui.l_Focal->setText(str.setNum((int)info_params.focal));
    ui.l_Aperture->setText(str.setNum((int)info_params.aperture));
    ui.l_FbyD->setText(QString().setNum(info_params.focal_ratio, 'f', 1));
    str = QString().setNum(info_params.fov_wd, 'f', 1) + 'x' + QString().setNum(info_params.fov_ht, 'f', 1);
    ui.l_FOV->setText(str);

    ui.checkBox_DirRA->setChecked(Options::enableRAGuide());
    ui.checkBox_DirDEC->setChecked(Options::enableDECGuide());

    ui.spinBox_PropGainRA->setValue(Options::rAProportionalGain());
    ui.spinBox_PropGainDEC->setValue(Options::dECProportionalGain());

    ui.spinBox_IntGainRA->setValue(Options::rAIntegralGain());
    ui.spinBox_IntGainDEC->setValue(Options::dECIntegralGain());

    ui.spinBox_DerGainRA->setValue(Options::rADerivativeGain());
    ui.spinBox_DerGainDEC->setValue(Options::dECDerivativeGain());

    ui.spinBox_MaxPulseRA->setValue(Options::rAMaximumPulse());
    ui.spinBox_MaxPulseDEC->setValue(Options::dECMaximumPulse());

    ui.spinBox_MinPulseRA->setValue(Options::rAMinimumPulse());
    ui.spinBox_MinPulseDEC->setValue(Options::dECMinimumPulse());

    ui.l_DeltaRA->setText(QString().setNum(out_params->delta[GUIDE_RA], 'f', 2));
    ui.l_DeltaDEC->setText(QString().setNum(out_params->delta[GUIDE_DEC], 'f', 2));

    ui.l_PulseRA->setText(QString().setNum(out_params->pulse_length[GUIDE_RA]));
    ui.l_PulseDEC->setText(QString().setNum(out_params->pulse_length[GUIDE_DEC]));

    ui.l_ErrRA->setText(QString().setNum(out_params->sigma[GUIDE_RA], 'g', 3));
    ui.l_ErrDEC->setText(QString().setNum(out_params->sigma[GUIDE_DEC], 'g', 3));
}

void internalGuider::setTargetChip(ISD::CCDChip *chip)
{
    targetChip = chip;
    targetChip->getFrame(&fx, &fy, &fw, &fh);
    if (phd2 == nullptr)
        ui.subFrameCheck->setEnabled(targetChip->canSubframe());
}

bool internalGuider::start()
{
    Options::setUseDither(ui.ditherCheck->isChecked());
    Options::setDitherPixels(ui.ditherPixels->value());
    Options::setAOLimit(ui.spinBox_AOLimit->value());
    Options::setGuidingRate(ui.spinBox_GuideRate->value());
    Options::setEnableRAGuide(ui.checkBox_DirRA->isChecked());
    Options::setEnableDECGuide(ui.checkBox_DirDEC->isChecked());
    Options::setRAProportionalGain(ui.spinBox_PropGainRA->value());
    Options::setDECProportionalGain(ui.spinBox_PropGainDEC->value());
    Options::setRAIntegralGain(ui.spinBox_IntGainRA->value());
    Options::setDECIntegralGain(ui.spinBox_IntGainDEC->value());
    Options::setRADerivativeGain(ui.spinBox_DerGainRA->value());
    Options::setDECDerivativeGain(ui.spinBox_DerGainDEC->value());
    Options::setRAMaximumPulse(ui.spinBox_MaxPulseRA->value());
    Options::setDECMaximumPulse(ui.spinBox_MaxPulseDEC->value());
    Options::setRAMinimumPulse(ui.spinBox_MinPulseRA->value());
    Options::setDECMinimumPulse(ui.spinBox_MinPulseDEC->value());

    if (guideFrame)
        disconnect(guideFrame, SIGNAL(trackingStarSelected(int,int)), 0, 0);

    // Let everyone know about dither option status
    emit ditherToggled(ui.ditherCheck->isChecked());

    if (phd2)
    {
        phd2->startGuiding();

        m_isStarted     = true;
        m_useRapidGuide = ui.rapidGuideCheck->isChecked();

        //pmain_wnd->setSuspended(false);

        ui.pushButton_StartStop->setText(i18n("Stop"));
        guideModule->appendLogText(i18n("Autoguiding started."));

        return true;
    }

    logFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&logFile);
    out << "Guiding rate,x15 arcsec/sec: " << ui.spinBox_GuideRate->value() << endl;
    out << "Focal,mm: " << ui.l_Focal->text() << endl;
    out << "Aperture,mm: " << ui.l_Aperture->text() << endl;
    out << "F/D: " << ui.l_FbyD->text() << endl;
    out << "FOV: " << ui.l_FOV->text() << endl;
    out << "Frame #, Time Elapsed (ms), RA Error (arcsec), RA Correction (ms), RA Correction Direction, DEC Error "
           "(arcsec), DEC Correction (ms), DEC Correction Direction"
        << endl;

    drift_graph->reset_data();
    ui.pushButton_StartStop->setText(i18n("Stop"));
    guideModule->appendLogText(i18n("Autoguiding started."));
    pmath->start();
    m_lostStarTries = 0;
    m_isStarted     = true;
    m_useRapidGuide = ui.rapidGuideCheck->isChecked();
    if (m_useRapidGuide)
        guideModule->startRapidGuide();

    emit newStatus(Ekos::GUIDE_GUIDING);

    guideModule->setSuspended(false);

    first_frame = true;

    if (ui.subFrameCheck->isEnabled() && ui.subFrameCheck->isChecked() && m_isSubFramed == false)
        first_subframe = true;

    capture();

    pmath->setLogFile(&logFile);

    return true;
}

bool internalGuider::stop()
{
    if (phd2)
    {
        ui.pushButton_StartStop->setText(i18n("Start Autoguide"));
        //emit autoGuidingToggled(false);

        m_isDithering = false;
        m_isStarted   = false;

        return phd2->stopGuiding();
    }

    if (guideFrame)
        connect(guideFrame, SIGNAL(trackingStarSelected(int,int)), this, SLOT(trackingStarSelected(int,int)),
                Qt::UniqueConnection);
    ui.pushButton_StartStop->setText(i18n("Start Autoguide"));
    guideModule->appendLogText(i18n("Autoguiding stopped."));
    pmath->stop();

    first_frame = false;
    logFile.close();

    targetChip->abortExposure();

    if (m_useRapidGuide)
        guideModule->stopRapidGuide();

    emit newStatus(Ekos::GUIDE_IDLE);

    m_isDithering = false;
    m_isStarted   = false;

    return true;
}

void internalGuider::toggleExternalGuideStateGUI(Ekos::GuideState state)
{
    if (phd2 == nullptr)
        return;

    // If not started already
    if (m_isStarted == false && state == Ekos::GUIDE_GUIDING)
    {
        m_isStarted     = true;
        m_useRapidGuide = ui.rapidGuideCheck->isChecked();

        ui.pushButton_StartStop->setText(i18n("Stop"));
        guideModule->appendLogText(i18n("Autoguiding started."));
    }
    // if already started
    else if (m_isStarted && state == Ekos::GUIDE_IDLE)
    {
        ui.pushButton_StartStop->setText(i18n("Start Autoguide"));
    }
}

// processing stuff
void internalGuider::onStartStopButtonClick()
{
    assert(pmath);
    assert(targetChip);

    // start
    if (!m_isStarted)
        start();
    // stop
    else
        stop();
}

void internalGuider::capture()
{
    if (ui.subFrameCheck->isChecked() && m_isSubFramed == false)
    {
        int x, y, w, h, binX = 1, binY = 1;
        targetChip->getBinning(&binX, &binY);
        int square_size = guideFrame->getTrackingBox().width();

        pmath->getReticleParameters(&ret_x, &ret_y, &ret_angle);
        x = (ret_x - square_size) * binX;
        y = (ret_y - square_size) * binY;
        w = square_size * 2 * binX;
        h = square_size * 2 * binY;

        int minX, maxX, minY, maxY, minW, maxW, minH, maxH;
        targetChip->getFrameMinMax(&minX, &maxX, &minY, &maxY, &minW, &maxW, &minH, &maxH);

        m_isSubFramed = true;

        if (x < minX)
            x = minX;
        if (y < minY)
            y = minY;
        if ((w + x) > maxW)
            w = maxW - x;
        if ((h + y) > maxH)
            h = maxH - y;

        pmath->setVideoParameters(w / binX, h / binY);

        targetChip->setFrame(x, y, w, h);

        //trackingStarSelected(w/(binX*2), h/(binY*2));

        pmath->setReticleParameters(w / (binX * 2), h / (binY * 2), ret_angle);

        //emit newStarPosition(QVector3D(w/(binX*2), h/(binY*2), 0), false);
        emit newStarPosition(QVector3D(), false);
    }
    else if (m_isSubFramed && ui.subFrameCheck->isChecked() == false)
    {
        m_isSubFramed = false;
        targetChip->resetFrame();

        emit newStarPosition(QVector3D(), false);
    }

    guideModule->capture();
}

void internalGuider::onSetDECSwap(bool enable)
{
    guideModule->setDECSwap(enable);
}

void internalGuider::setDECSwap(bool enable)
{
    ui.swapCheck->disconnect(this);
    ui.swapCheck->setChecked(enable);
    connect(ui.swapCheck, SIGNAL(toggled(bool)), this, SLOT(setDECSwap(bool)));
}

void internalGuider::guide(void)
{
    static int maxPulseCounter = 0;
    const cproc_out_params *out;
    QString str;
    uint32_t tick  = 0;
    double drift_x = 0, drift_y = 0;

    Q_ASSERT(pmath);

    if (first_subframe)
    {
        first_subframe = false;
        return;
    }
    else if (first_frame)
    {
        if (m_isDithering == false)
        {
            Vector star_pos = pmath->findLocalStarPosition();
            pmath->setReticleParameters(star_pos.x, star_pos.y, -1);

            //pmath->moveSquare( round(star_pos.x) - (double)square_size/(2*binx), round(star_pos.y) - (double)square_size/(2*biny) );
        }
        first_frame = false;
    }

    // calc math. it tracks square
    pmath->performProcessing();

    if (!m_isStarted)
        return;

    if (pmath->isStarLost() && ++m_lostStarTries > 2)
    {
        guideModule->appendLogText(
            i18n("Lost track of the guide star. Try increasing the square size and check the mount."));
        onStartStopButtonClick();
        return;
    }
    else
        m_lostStarTries = 0;

    // do pulse
    out = pmath->getOutputParameters();

    if (out->pulse_length[GUIDE_RA] == ui.spinBox_MaxPulseRA->value() ||
        out->pulse_length[GUIDE_DEC] == ui.spinBox_MaxPulseDEC->value())
        maxPulseCounter++;
    else
        maxPulseCounter = 0;

    if (maxPulseCounter > 3)
    {
        guideModule->appendLogText(i18n("Lost track of the guide star. Aborting guiding..."));
        abort();
        maxPulseCounter = 0;
    }

    guideModule->sendPulse(out->pulse_dir[GUIDE_RA], out->pulse_length[GUIDE_RA], out->pulse_dir[GUIDE_DEC],
                           out->pulse_length[GUIDE_DEC]);

    if (m_isDithering)
        return;

    pmath->getStarDrift(&drift_x, &drift_y);

    drift_graph->add_point(drift_x, drift_y);

    tick = pmath->getTicks();

    if (tick & 1)
    {
        // draw some params in window
        ui.l_DeltaRA->setText(str.setNum(out->delta[GUIDE_RA], 'f', 2));
        ui.l_DeltaDEC->setText(str.setNum(out->delta[GUIDE_DEC], 'f', 2));

        ui.l_PulseRA->setText(str.setNum(out->pulse_length[GUIDE_RA]));
        ui.l_PulseDEC->setText(str.setNum(out->pulse_length[GUIDE_DEC]));

        ui.l_ErrRA->setText(str.setNum(out->sigma[GUIDE_RA], 'g', 3));
        ui.l_ErrDEC->setText(str.setNum(out->sigma[GUIDE_DEC], 'g', 3));
    }

    // skip half frames
    if (half_refresh_rate && (tick & 1))
        return;

    drift_graph->on_paint();
    pDriftOut->update();

    profilePixmap = pDriftOut->grab(QRect(QPoint(0, 100), QSize(pDriftOut->width(), 101)));
    emit newProfilePixmap(profilePixmap);
}

void internalGuider::setImageView(FITSView *image)
{
    guideFrame = image;

    if (m_isReady && guideFrame && m_isStarted == false)
        connect(guideFrame, SIGNAL(trackingStarSelected(int,int)), this, SLOT(trackingStarSelected(int,int)),
                Qt::UniqueConnection);
}

void internalGuider::trackingStarSelected(int x, int y)
{
    pmath->setReticleParameters(x, y, guideModule->getReticleAngle());

    //pmath->moveSquare(x-square_size/(2*binx), y-square_size/(2*biny));
    QVector3D starCenter = guideModule->getStarPosition();

    starCenter.setX(x);
    starCenter.setY(y);

    emit newStarPosition(starCenter, true);
}

bool internalGuider::abort(bool silence)
{
    if (m_isStarted == true)
    {
        bool rc = stop();

        if (silence)
            return rc;

        KNotification::event(QLatin1String("GuideFailed"), i18n("Autoguiding failed with errors"));
    }

    return true;
}

bool internalGuider::dither()
{
    static Vector target_pos;
    static unsigned int retries = 0;

    if (ui.ditherCheck->isChecked() == false)
        return false;

    double cur_x, cur_y, ret_angle;
    pmath->getReticleParameters(&cur_x, &cur_y, &ret_angle);
    pmath->getStarScreenPosition(&cur_x, &cur_y);
    Matrix ROT_Z = pmath->getROTZ();

    //qDebug() << "Star Pos X " << cur_x << " Y " << cur_y;

    if (m_isDithering == false)
    {
        retries = 0;

        // JM 2016-05-8: CCD would abort if required.
        //targetChip->abortExposure();

        double ditherPixels = ui.ditherPixels->value();
        int polarity        = (rand() % 2 == 0) ? 1 : -1;
        double angle        = ((double)rand() / RAND_MAX) * M_PI / 2.0;
        double diff_x       = ditherPixels * cos(angle);
        double diff_y       = ditherPixels * sin(angle);

        m_isDithering = true;

        if (pmath->declinationSwapEnabled())
            diff_y *= -1;

        if (polarity > 0)
            target_pos = Vector(cur_x, cur_y, 0) + Vector(diff_x, diff_y, 0);
        else
            target_pos = Vector(cur_x, cur_y, 0) - Vector(diff_x, diff_y, 0);

        if (Options::guideLogging())
            qDebug() << "Guide: Dithering process started.. Reticle Target Pos X " << target_pos.x << " Y "
                     << target_pos.y;

        pmath->setReticleParameters(target_pos.x, target_pos.y, ret_angle);

        guide();

        // Take a new exposure if we're not already capturing
        if (targetChip->isCapturing() == false)
            guideModule->capture();

        return true;
    }

    Vector star_pos = Vector(cur_x, cur_y, 0) - Vector(target_pos.x, target_pos.y, 0);
    star_pos.y      = -star_pos.y;
    star_pos        = star_pos * ROT_Z;

    if (Options::guideLogging())
        qDebug() << "Guide: Dithering in progress. Diff star X:" << star_pos.x << "Y:" << star_pos.y;

    if (fabs(star_pos.x) < 1 && fabs(star_pos.y) < 1)
    {
        pmath->setReticleParameters(cur_x, cur_y, ret_angle);

        m_isDithering = false;

        if (Options::guideLogging())
            qDebug() << "Guide: Dither complete.";

        //emit ditherComplete();
        emit newStatus(Ekos::GUIDE_DITHERING_SUCCESS);
    }
    else
    {
        if (++retries > MAX_DITHER_RETIRES)
        {
            m_isDithering = false;
            return false;
        }

        guide();
    }

    guideModule->capture();

    return true;
}

QString internalGuider::getAlgorithm()
{
    return ui.comboBox_ThresholdAlg->currentText();
}

bool internalGuider::useSubFrame()
{
    return ui.subFrameCheck->isChecked();
}

bool internalGuider::useRapidGuide()
{
    return ui.rapidGuideCheck->isChecked();
}

void internalGuider::setGuideOptions(const QString &algorithm, bool useSubFrame, bool useRapidGuide)
{
    for (int i = 0; i < ui.comboBox_ThresholdAlg->count(); i++)
    {
        if (ui.comboBox_ThresholdAlg->itemText(i) == algorithm)
        {
            ui.comboBox_ThresholdAlg->setCurrentIndex(i);
            break;
        }
    }
    if (phd2 == nullptr)
        ui.subFrameCheck->setChecked(useSubFrame);
    ui.rapidGuideCheck->setChecked(useRapidGuide);
}

void internalGuider::setDither(bool enable, double value)
{
    ui.ditherCheck->setChecked(enable);

    if (enable && value > 0)
        ui.ditherPixels->setValue(value);
}

void internalGuider::setPHD2(Ekos::PHD2 *phd)
{
    // If we already have PHD2 set but we are asked to unset it then we shall disconnect all signals first
    if (phd2 && phd == nullptr)
        phd2->disconnect();

    phd2 = phd;

    bool enable = (phd2 == nullptr) ? true : false;

    if (phd2)
    {
        if (phd2->isConnected())
            setPHD2Connected();
        else
            setPHD2Disconnected();

        connect(phd2, SIGNAL(connected()), this, SLOT(setPHD2Connected()), Qt::UniqueConnection);
        connect(phd2, SIGNAL(disconnected()), this, SLOT(setPHD2Disconnected()), Qt::UniqueConnection);
    }

    ui.connectPHD2B->setHidden(enable);

    ui.pushButton_StartStop->setEnabled(enable);
    ui.controlGroup->setEnabled(enable);
    ui.infoGroup->setEnabled(enable);
    ui.captureB->setEnabled(enable);
    ui.subFrameCheck->setEnabled(enable);
    ui.rapidGuideCheck->setEnabled(enable);
    ui.comboBox_ThresholdAlg->setEnabled(enable);
    ui.ditherCheck->setEnabled(enable);
    ui.ditherPixels->setEnabled(enable);
    ui.driftGraphicsGroup->setEnabled(enable);
}

void internalGuider::connectPHD2()
{
    if (phd2)
    {
        if (phd2->isConnected())
            phd2->disconnectPHD2();
        else
            phd2->connectPHD2();
    }
}

void internalGuider::setPHD2Connected()
{
    ui.connectPHD2B->setText(i18n("Disconnect PHD2"));

    ui.pushButton_StartStop->setEnabled(true);
    ui.ditherCheck->setEnabled(true);
    ui.ditherPixels->setEnabled(true);
}

void internalGuider::setPHD2Disconnected()
{
    ui.connectPHD2B->setText(i18n("Connect PHD2"));

    ui.pushButton_StartStop->setEnabled(false);
    ui.ditherCheck->setEnabled(false);
    ui.ditherPixels->setEnabled(false);
}
