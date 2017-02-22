/*  Ekos Alignment Module
    Copyright (C) 2013 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <QProcess>

#include "kstars.h"
#include "kstarsdata.h"
#include "align.h"
#include "dms.h"
#include "fov.h"
#include "ekos/auxiliary/darklibrary.h"

#include "Options.h"

#include <QFileDialog>
#include <KMessageBox>
#include <KConfigDialog>
#include <KNotifications/KNotification>

#include "ekos/auxiliary/QProgressIndicator.h"
#include "indi/driverinfo.h"
#include "indi/indicommon.h"
#include "indi/clientmanager.h"
#include "alignadaptor.h"

#include "fitsviewer/fitsviewer.h"
#include "fitsviewer/fitstab.h"
#include "fitsviewer/fitsview.h"

#include "ekos/ekosmanager.h"

#include "onlineastrometryparser.h"
#include "offlineastrometryparser.h"
#include "remoteastrometryparser.h"
#include "opsastrometry.h"
#include "opsalign.h"

#include <basedevice.h>

#define PAH_CUTOFF_FOV              30                   // Minimum FOV width in arcminutes for PAH to work
#define MAXIMUM_SOLVER_ITERATIONS   10

namespace Ekos
{

// 30 arcmiutes RA movement
const double Align::RAMotion = 0.5;
// Sidereal rate, degrees/s
const float Align::SIDRATE  = 0.004178;

Align::Align()
{
    setupUi(this);
    new AlignAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Align",  this);

    dirPath = QDir::homePath();

    state = ALIGN_IDLE;
    focusState = FOCUS_IDLE;
    pahStage = PAH_IDLE;

    currentCCD     = NULL;
    currentTelescope = NULL;
    currentFilter = NULL;
    useGuideHead = false;
    canSync = false;
    //loadSlewMode = false;
    loadSlewState=IPS_IDLE;
    //m_isSolverComplete = false;
    //m_isSolverSuccessful = false;
    //m_slewToTargetSelected=false;
    m_wcsSynced=false;
    //isFocusBusy=false;
    ccd_hor_pixel =  ccd_ver_pixel =  focal_length =  aperture = sOrientation = sRA = sDEC = -1;
    decDeviation = azDeviation = altDeviation = 0;

    rememberUploadMode = ISD::CCD::UPLOAD_CLIENT;
    currentFilter = NULL;
    filterPositionPending = false;
    lockedFilterIndex = currentFilterIndex = -1;
    retries=0;
    targetDiff=1e6;
    solverIterations=0;
    fov_x=fov_y=fov_pixscale=0;

    parser = NULL;
    solverFOV = new FOV();
    solverFOV->setColor(KStars::Instance()->data()->colorScheme()->colorNamed( "SolverFOVColor" ).name());
    onlineParser = NULL;
    offlineParser = NULL;
    remoteParser = NULL;

    showFITSViewerB->setIcon(QIcon::fromTheme("kstars_fitsviewer", QIcon(":/icons/breeze/default/kstars_fitsviewer.svg")));
    showFITSViewerB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(showFITSViewerB, SIGNAL(clicked()), this, SLOT(showFITSViewer()));

    toggleFullScreenB->setIcon(QIcon::fromTheme("view-fullscreen", QIcon(":/icons/breeze/default/view-fullscreen.svg")));
    toggleFullScreenB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(toggleFullScreenB, SIGNAL(clicked()), this, SLOT(toggleAlignWidgetFullScreen()));

    alignView = new AlignView(alignWidget, FITS_ALIGN);
    alignView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    alignView->setBaseSize(alignWidget->size());
    alignView->createFloatingToolBar();
    QVBoxLayout *vlayout = new QVBoxLayout();
    vlayout->addWidget(alignView);
    alignWidget->setLayout(vlayout);

    connect(solveB, SIGNAL(clicked()), this, SLOT(captureAndSolve()));
    connect(stopB, SIGNAL(clicked()), this, SLOT(abort()));
    connect(measureAltB, SIGNAL(clicked()), this, SLOT(measureAltError()));
    connect(measureAzB, SIGNAL(clicked()), this, SLOT(measureAzError()));
#if 0
    connect(raBox, SIGNAL(textChanged( const QString & ) ), this, SLOT( checkLineEdits() ) );
    connect(decBox, SIGNAL(textChanged( const QString & ) ), this, SLOT( checkLineEdits() ) );
    connect(syncBoxesB, SIGNAL(clicked()), this, SLOT(copyCoordsToBoxes()));
    connect(clearBoxesB, SIGNAL(clicked()), this, SLOT(clearCoordBoxes()));
#endif
    connect(editOptionsB,SIGNAL(clicked()), this, SLOT(slotEditOptions()));

    connect(CCDCaptureCombo, SIGNAL(activated(QString)), this, SLOT(setDefaultCCD(QString)));
    connect(CCDCaptureCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(checkCCD(int)));

    connect(correctAltB, SIGNAL(clicked()), this, SLOT(correctAltError()));
    connect(correctAzB, SIGNAL(clicked()), this, SLOT(correctAzError()));
    connect(loadSlewB, SIGNAL(clicked()), this, SLOT(loadAndSlew()));

    gotoModeButtonGroup->setId(syncR, GOTO_SYNC);
    gotoModeButtonGroup->setId(slewR, GOTO_SLEW);
    gotoModeButtonGroup->setId(nothingR, GOTO_NOTHING);

    connect(gotoModeButtonGroup, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, [=](int id){ this->currentGotoMode = static_cast<GotoMode>(id); });

    alignTimer.setInterval(Options::astrometryTimeout()*1000);
    connect(&alignTimer, SIGNAL(timeout()), this, SLOT(checkAlignmentTimeout()));

    currentGotoMode = static_cast<GotoMode>(Options::solverGotoOption());
    gotoModeButtonGroup->button(currentGotoMode)->setChecked(true);

    editOptionsB->setIcon(QIcon::fromTheme("document-edit", QIcon(":/icons/breeze/default/document-edit.svg")));
    editOptionsB->setAttribute(Qt::WA_LayoutUsesWidgetRect);    
    KConfigDialog* dialog = new KConfigDialog(this, "alignsettings", Options::self());

    opsAlign = new OpsAlign(this);
    dialog->addPage(opsAlign, i18n("Astrometry.net"));

    opsAstrometry = new OpsAstrometry(this);
    dialog->addPage(opsAstrometry, i18n("Solver Options"));
    connect(editOptionsB, SIGNAL(clicked()), dialog, SLOT(show()));


#if 0
    syncBoxesB->setIcon(QIcon::fromTheme("edit-copy", QIcon(":/icons/breeze/default/edit-copy.svg")));
    syncBoxesB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    clearBoxesB->setIcon(QIcon::fromTheme("edit-clear", QIcon(":/icons/breeze/default/edit-clear.svg")));
    clearBoxesB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    raBox->setDegType(false); //RA box should be HMS-style
#endif

    appendLogText(i18n("Idle."));

    pi = new QProgressIndicator(this);

    stopLayout->addWidget(pi);

    exposureIN->setValue(Options::alignExposure());

    altStage = ALT_INIT;
    azStage  = AZ_INIT;

    // Online/Offline/Remote solver check
    solverTypeGroup->setId(onlineSolverR, SOLVER_ONLINE);
    solverTypeGroup->setId(offlineSolverR, SOLVER_OFFLINE);
    solverTypeGroup->setId(remoteSolverR, SOLVER_REMOTE);
    solverTypeGroup->button(Options::solverType())->setChecked(true);
    connect(solverTypeGroup, SIGNAL(buttonClicked(int)), SLOT(setSolverType(int)));

    switch(solverTypeGroup->checkedId())
    {
    case SOLVER_ONLINE:
        onlineParser = new Ekos::OnlineAstrometryParser();
        parser = onlineParser;
        break;

    case SOLVER_OFFLINE:
        offlineParser = new OfflineAstrometryParser();
        parser = offlineParser;
        break;

    case SOLVER_REMOTE:
        remoteParser = new RemoteAstrometryParser();
        parser = remoteParser;
        break;
    }

    parser->setAlign(this);
    if (parser->init() == false)
        setEnabled(false);
    else
    {
        connect(parser, SIGNAL(solverFinished(double,double,double, double)), this, SLOT(solverFinished(double,double,double, double)), Qt::UniqueConnection);
        connect(parser, SIGNAL(solverFailed()), this, SLOT(solverFailed()), Qt::UniqueConnection);
    }

    //solverOptions->setText(Options::solverOptions());

    // Which telescope info to use for FOV calculations
    //kcfg_solverOTA->setChecked(Options::solverOTA());
    guideScopeCCDs = Options::guideScopeCCDs();
    connect(FOVScopeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateGuideScopeCCDs(int)));

    accuracySpin->setValue(Options::solverAccuracyThreshold());
    alignDarkFrameCheck->setChecked(Options::alignDarkFrame());

    delaySpin->setValue(Options::settlingTime());
    connect(delaySpin, SIGNAL(editingFinished()), this, SLOT(saveSettleTime()));

    connect(binningCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setBinningIndex(int)));

    // PAH Connections
    connect(PAHRestartB, SIGNAL(clicked()), this, SLOT(restartPAHProcess()));
    connect(PAHStartB, SIGNAL(clicked()), this, SLOT(startPAHProcess()));
    connect(PAHFirstCaptureB, &QPushButton::clicked, this, [this]() { PAHFirstCaptureB->setEnabled(false); captureAndSolve();});
    connect(PAHSecondCaptureB, &QPushButton::clicked, this, [this]() { PAHSecondCaptureB->setEnabled(false); captureAndSolve();});
    connect(PAHThirdCaptureB, &QPushButton::clicked, this, [this]() { PAHThirdCaptureB->setEnabled(false); captureAndSolve();});
    connect(PAHFirstRotateB, &QPushButton::clicked, this, [this]() { PAHFirstRotateB->setEnabled(false); rotatePAH();});
    connect(PAHSecondRotateB, &QPushButton::clicked, this, [this]() { PAHSecondRotateB->setEnabled(false); rotatePAH();});
    connect(PAHCorrectionsNextB, SIGNAL(clicked()), this, SLOT(setPAHCorrectionSelectionComplete()));
    connect(PAHRefreshB, SIGNAL(clicked()), this, SLOT(startPAHRefreshProcess()));
    connect(PAHDoneB, SIGNAL(clicked()), this, SLOT(setPAHRefreshComplete()));

    if (solverOptions->text().contains("no-fits2fits"))
        appendLogText(i18n("Warning: If using astrometry.net v0.68 or above, remove the --no-fits2fits from the astrometry options."));

    hemisphere = KStarsData::Instance()->geo()->lat()->Degrees() > 0 ? NORTH_HEMISPHERE : SOUTH_HEMISPHERE;
}

Align::~Align()
{
    delete(pi);
    delete(solverFOV);
    delete(parser);

    if (alignWidget->parent() == NULL)
        toggleAlignWidgetFullScreen();

    // Remove temporary FITS files left before by the solver
    QDir dir(QDir::tempPath());
    dir.setNameFilters(QStringList() << "fits*" << "tmp.*");
    dir.setFilter(QDir::Files);
    foreach(QString dirFile, dir.entryList())
        dir.remove(dirFile);
}

bool Align::isParserOK()
{
    bool rc = parser->init();

    if (rc)
    {
        connect(parser, SIGNAL(solverFinished(double,double,double,double)), this, SLOT(solverFinished(double,double,double,double)), Qt::UniqueConnection);
        connect(parser, SIGNAL(solverFailed()), this, SLOT(solverFailed()), Qt::UniqueConnection);
    }

    return rc;
}

void Align::checkAlignmentTimeout()
{
    if (loadSlewState != IPS_IDLE || ++solverIterations == MAXIMUM_SOLVER_ITERATIONS)
        abort();
    else if (loadSlewState == IPS_IDLE)
    {
        appendLogText(i18n("Solver timed out"));
        captureAndSolve();
    }
    // TODO must also account for loadAndSlew. Retain file name
}

void Align::setSolverType(int type)
{
    switch(type)
    {
    case SOLVER_ONLINE:
        if (onlineParser != NULL)
        {
            parser = onlineParser;
            return;
        }

        onlineParser = new Ekos::OnlineAstrometryParser();
        parser = onlineParser;
        break;

    case SOLVER_OFFLINE:
        if (offlineParser != NULL)
        {
            parser = offlineParser;

            return;
        }

        offlineParser = new Ekos::OfflineAstrometryParser();
        parser = offlineParser;
        break;

    case SOLVER_REMOTE:
        if (remoteParser != NULL)
        {
            parser = remoteParser;
            (dynamic_cast<RemoteAstrometryParser*>(parser))->setCCD(currentCCD);
            return;
        }

        remoteParser = new Ekos::RemoteAstrometryParser();
        parser = remoteParser;
        (dynamic_cast<RemoteAstrometryParser*>(parser))->setCCD(currentCCD);
        break;
    }

    parser->setAlign(this);
    if (parser->init())
    {
        connect(parser, SIGNAL(solverFinished(double,double,double, double)), this, SLOT(solverFinished(double,double,double, double)), Qt::UniqueConnection);
        connect(parser, SIGNAL(solverFailed()), this, SLOT(solverFailed()), Qt::UniqueConnection);
    }
    else
        parser->disconnect();

}

bool Align::setCCD(QString device)
{
    for (int i=0; i < CCDCaptureCombo->count(); i++)
        if (device == CCDCaptureCombo->itemText(i))
        {
            CCDCaptureCombo->setCurrentIndex(i);
            return true;
        }

    return false;
}

void Align::setDefaultCCD(QString ccd)
{
    Options::setDefaultAlignCCD(ccd);
}

void Align::checkCCD(int ccdNum)
{
    if (ccdNum == -1)
    {
        ccdNum = CCDCaptureCombo->currentIndex();

        if (ccdNum == -1)
            return;
    }

    if (ccdNum <= CCDs.count())
    {
        currentCCD = CCDs.at(ccdNum);

        if (solverTypeGroup->checkedId() == SOLVER_REMOTE)
            (dynamic_cast<RemoteAstrometryParser*>(parser))->setCCD(currentCCD);
    }

    FOVScopeCombo->setCurrentIndex(guideScopeCCDs.contains(currentCCD->getDeviceName()) ? ISD::CCD::TELESCOPE_GUIDE : ISD::CCD::TELESCOPE_PRIMARY);

    syncCCDInfo();

}

void Align::addCCD(ISD::GDInterface *newCCD)
{
    if (CCDs.contains(static_cast<ISD::CCD *>(newCCD)))
    {
       syncCCDInfo();
       return;
    }

    CCDs.append(static_cast<ISD::CCD *>(newCCD));

    CCDCaptureCombo->addItem(newCCD->getDeviceName());
}

void Align::setTelescope(ISD::GDInterface *newTelescope)
{
    currentTelescope = static_cast<ISD::Telescope*> (newTelescope);

    connect(currentTelescope, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processTelescopeNumber(INumberVectorProperty*)));

    syncTelescopeInfo();
}

void Align::syncTelescopeInfo()
{
    if (currentTelescope == NULL)
        return;

    canSync = currentTelescope->canSync();

    if (canSync == false && syncR->isEnabled())
    {
        slewR->setChecked(true);
        appendLogText(i18n("Mount does not support syncing."));
    }

    syncR->setEnabled(canSync);

    INumberVectorProperty * nvp = currentTelescope->getBaseDevice()->getNumber("TELESCOPE_INFO");

    double primaryFL=-1, primaryAperture=-1, guideFL=-1, guideAperture=-1;

    if (nvp)
    {
        INumber *np = NULL;

        np = IUFindNumber(nvp, "TELESCOPE_APERTURE");
        if (np && np->value > 0)
            primaryAperture = np->value;

        np = IUFindNumber(nvp, "GUIDER_APERTURE");
        if (np && np->value > 0)
            guideAperture = np->value;

        aperture = primaryAperture;

        if (currentCCD && Options::guideScopeCCDs().contains(currentCCD->getDeviceName()))
            aperture = guideAperture;

        np = IUFindNumber(nvp, "TELESCOPE_FOCAL_LENGTH");
        if (np && np->value > 0)
            primaryFL = np->value;

        np = IUFindNumber(nvp, "GUIDER_FOCAL_LENGTH");
        if (np && np->value > 0)
            guideFL = np->value;

        focal_length = primaryFL;

        if (currentCCD && Options::guideScopeCCDs().contains(currentCCD->getDeviceName()))
            focal_length = guideFL;
    }

    if (focal_length == -1 || aperture == -1)
        return;

    if (ccd_hor_pixel != -1 && ccd_ver_pixel != -1 && focal_length != -1 && aperture != -1)
    {
        FOVScopeCombo->setItemData(ISD::CCD::TELESCOPE_PRIMARY, i18nc("F-Number, Focal Length, Aperture", "<nobr>F<b>%1</b> Focal Length: <b>%2</b> mm Aperture: <b>%3</b> mm<sup>2</sup></nobr>",
                                                                     QString::number(primaryFL/primaryAperture, 'f', 1),
                                                                     QString::number(primaryFL, 'f', 2),
                                                                     QString::number(primaryAperture, 'f', 2)), Qt::ToolTipRole);
        FOVScopeCombo->setItemData(ISD::CCD::TELESCOPE_GUIDE, i18nc("F-Number, Focal Length, Aperture", "<nobr>F<b>%1</b> Focal Length: <b>%2</b> mm Aperture: <b>%3</b> mm<sup>2</sup></nobr>",
                                                                     QString::number(guideFL/guideAperture, 'f', 1),
                                                                     QString::number(guideFL, 'f', 2),
                                                                     QString::number(guideAperture, 'f', 2)), Qt::ToolTipRole);
        calculateFOV();

        generateArgs();
    }
}


void Align::syncCCDInfo()
{
    INumberVectorProperty * nvp = NULL;

    if (currentCCD == NULL)
        return;

    if (useGuideHead)
        nvp = currentCCD->getBaseDevice()->getNumber("GUIDER_INFO");
    else
        nvp = currentCCD->getBaseDevice()->getNumber("CCD_INFO");

    if (nvp)
    {
        INumber *np = IUFindNumber(nvp, "CCD_PIXEL_SIZE_X");
        if (np && np->value >0)
            ccd_hor_pixel = ccd_ver_pixel = np->value;

        np = IUFindNumber(nvp, "CCD_PIXEL_SIZE_Y");
        if (np && np->value >0)
            ccd_ver_pixel = np->value;

        np = IUFindNumber(nvp, "CCD_PIXEL_SIZE_Y");
        if (np && np->value >0)
            ccd_ver_pixel = np->value;
    }

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    ISwitchVectorProperty *svp = currentCCD->getBaseDevice()->getSwitch("WCS_CONTROL");
    if (svp)
        setWCSEnabled(Options::astrometrySolverWCS());

    targetChip->setImageView(alignView, FITS_ALIGN);

    targetChip->getFrameMinMax(NULL, NULL, NULL, NULL, NULL, &ccd_width, NULL, &ccd_height);
    //targetChip->getFrame(&x,&y,&ccd_width,&ccd_height);
    binningCombo->setEnabled(targetChip->canBin());
    if (targetChip->canBin())
    {
        binningCombo->blockSignals(true);

        int binx=1,biny=1;
        targetChip->getMaxBin(&binx, &biny);
        binningCombo->clear();

        for (int i=0; i < binx; i++)
            binningCombo->addItem(QString("%1x%2").arg(i+1).arg(i+1));

        binningCombo->setCurrentIndex(Options::solverBinningIndex());

        binningCombo->blockSignals(false);
    }

    if (ccd_hor_pixel == -1 || ccd_ver_pixel == -1)
        return;

    if (ccd_hor_pixel != -1 && ccd_ver_pixel != -1 && focal_length != -1 && aperture != -1)
    {
        calculateFOV();
        generateArgs();
    }
}

void Align::getFOVScale(double &fov_w, double & fov_h, double &fov_scale)
{
    fov_w = fov_x;
    fov_h = fov_y;
    fov_scale = fov_pixscale;
}

void Align::calculateFOV()
{
    // Calculate FOV

    // FOV in arcsecs
    fov_x = 206264.8062470963552 * ccd_width * ccd_hor_pixel / 1000.0 / focal_length;
    fov_y = 206264.8062470963552 * ccd_height * ccd_ver_pixel / 1000.0 / focal_length;

    // Pix Scale
    fov_pixscale = (fov_x * (Options::solverBinningIndex()+1)) / ccd_width;

    // FOV in arcmins
    fov_x /= 60.0;
    fov_y /= 60.0;

    solverFOV->setSize(fov_x, fov_y);

    FOVOut->setText(QString("%1' x %2'").arg(fov_x, 0, 'g', 3).arg(fov_y, 0, 'g', 3));

    if ( ((fov_x + fov_y) / 2.0) > PAH_CUTOFF_FOV)
    {
        PAHWidgets->setEnabled(true);
        PAHWidgets->setToolTip(QString());
        FOVDisabledLabel->hide();
    }
    else
    {
        PAHWidgets->setEnabled(false);
        PAHWidgets->setToolTip(i18n("<p>Polar Alignment Helper tool requires the following:</p><p>1. German Equatorial Mount</p><p>2. FOV &gt;"
                                    " 0.5 degrees</p><p>For small FOVs, use the Legacy Polar Alignment Tool.</p>"));
        FOVDisabledLabel->show();
    }

    if (opsAstrometry->kcfg_AstrometryUseImageScale->isChecked())
    {
        int unitType = opsAstrometry->kcfg_AstrometryImageScaleUnits->currentIndex();

        // Degrees
        if (unitType == 0)
        {
            double fov_low = qMin(fov_x/60, fov_y/60);
            double fov_high= qMax(fov_x/60, fov_y/60);
            opsAstrometry->kcfg_AstrometryImageScaleLow->setValue(fov_low);
            opsAstrometry->kcfg_AstrometryImageScaleHigh->setValue(fov_high);

            Options::setAstrometryImageScaleLow(fov_low);
            Options::setAstrometryImageScaleHigh(fov_high);
        }
        // Arcmins
        else if (unitType == 1)
        {
            double fov_low = qMin(fov_x, fov_y);
            double fov_high= qMax(fov_x, fov_y);
            opsAstrometry->kcfg_AstrometryImageScaleLow->setValue(fov_low);
            opsAstrometry->kcfg_AstrometryImageScaleHigh->setValue(fov_high);

            Options::setAstrometryImageScaleLow(fov_low);
            Options::setAstrometryImageScaleHigh(fov_high);
        }
        // Arcsec per pixel
        else
        {
            opsAstrometry->kcfg_AstrometryImageScaleLow->setValue(fov_pixscale*0.9);
            opsAstrometry->kcfg_AstrometryImageScaleHigh->setValue(fov_pixscale*1.1);

            // 10% boundary
            Options::setAstrometryImageScaleLow(fov_pixscale*0.9);
            Options::setAstrometryImageScaleHigh(fov_pixscale*1.1);
        }
    }
}

QStringList Align::generateOptions(const QVariantMap & optionsMap)
{
    // -O overwrite
    // -3 Expected RA
    // -4 Expected DEC
    // -5 Radius (deg)
    // -L lower scale of image in arcminutes
    // -H upper scale of image in arcmiutes
    // -u aw set scale to be in arcminutes
    // -W solution.wcs name of solution file
    // apog1.jpg name of target file to analyze
    //solve-field -O -3 06:40:51 -4 +09:49:53 -5 1 -L 40 -H 100 -u aw -W solution.wcs apod1.jpg

    QStringList solver_args;

    // Start with always-used arguments
    solver_args << "-O" << "--no-plots";

    // Now go over boolean options

    // noverify
    if (optionsMap.contains("noverify"))
        solver_args << "--no-verify";

    // noresort
    if (optionsMap.contains("resort"))
        solver_args << "--resort";

    // fits2fits
    if (optionsMap.contains("nofits2fits"))
        solver_args << "--no-fits2fits";

    // downsample
    if (optionsMap.contains("downsample"))
        solver_args << "--downsample" << QString::number(optionsMap.value("downsample", 2).toInt());

    // image scale low
    if (optionsMap.contains("scaleL"))
        solver_args << "-L" << QString::number(optionsMap.value("scaleL").toDouble());

    // image scale high
    if (optionsMap.contains("scaleH"))
        solver_args << "-H" << QString::number(optionsMap.value("scaleH").toDouble());

    // image scale units
    if (optionsMap.contains("scaleUnits"))
        solver_args << "-u" << optionsMap.value("scaleUnits").toString();

    // RA
    if (optionsMap.contains("ra"))
        solver_args << "-3" << QString::number(optionsMap.value("ra").toDouble());

    // DE
    if (optionsMap.contains("de"))
        solver_args << "-4" << QString::number(optionsMap.value("de").toDouble());

    // Radius
    if (optionsMap.contains("radius"))
        solver_args << "-5" << QString::number(optionsMap.value("radius").toDouble());

    // Custom
    if (optionsMap.contains("custom"))
        solver_args << optionsMap.value("custom").toString();

    return solver_args;
}

//This will generate the high and low scale of the imager field size based on the stated units.
void Align::generateFOVBounds(double fov_h, double fov_v, QString &fov_low, QString &fov_high)
{
    double fov_lower, fov_upper;
    // let's stretch the boundaries by 5%
    fov_lower = ((fov_h < fov_v) ? (fov_h * 0.95) : (fov_v * 0.95));
    fov_upper = ((fov_h > fov_v) ? (fov_h * 1.05) : (fov_v * 1.05));

    //No need to do anything if they are aw, since that is the default
    fov_low  = QString::number(fov_lower);
    fov_high = QString::number(fov_upper);
}

void Align::generateArgs()
{
    // -O overwrite
    // -3 Expected RA
    // -4 Expected DEC
    // -5 Radius (deg)
    // -L lower scale of image in arcminutes
    // -H upper scale of image in arcmiutes
    // -u aw set scale to be in arcminutes
    // -W solution.wcs name of solution file
    // apog1.jpg name of target file to analyze
    //solve-field -O -3 06:40:51 -4 +09:49:53 -5 1 -L 40 -H 100 -u aw -W solution.wcs apod1.jpg    

    QVariantMap optionsMap;

    if (Options::astrometryUseNoVerify())
        optionsMap["noverify"] = true;

    if (Options::astrometryUseResort())
        optionsMap["resort"] = true;

    if (Options::astrometryUseNoFITS2FITS())
        optionsMap["nofits2fits"] = true;

    if (Options::astrometryUseDownsample())
        optionsMap["downsample"] = Options::astrometryDownsample();

    if (Options::astrometryUseImageScale())
    {
        QString units = ImageScales[Options::astrometryImageScaleUnits()];
        if (Options::astrometryAutoUpdateImageScale())
        {
            QString fov_low,fov_high;
            double fov_w = fov_x;
            double fov_h = fov_y;

            if (units == "dw")
            {
                fov_w /= 60;
                fov_h /= 60;
            }
            else if (units == "app")
            {
                fov_w = fov_pixscale;
                fov_h = fov_pixscale;
            }

            generateFOVBounds(fov_w, fov_h, fov_low, fov_high);

            optionsMap["scaleL"] = fov_low;
            optionsMap["scaleH"] = fov_high;
            optionsMap["scaleUnits"] = units;
        }
        else
        {
            optionsMap["scaleL"] = Options::astrometryImageScaleLow();
            optionsMap["scaleH"] = Options::astrometryImageScaleHigh();
            optionsMap["scaleUnits"] = units;
        }
    }

    if (Options::astrometryUsePosition())
    {
        double ra=0,dec=0;
        currentTelescope->getEqCoords(&ra, &dec);

        optionsMap["ra"] = ra*15.0;
        optionsMap["de"] = dec;
        optionsMap["radius"] = Options::astrometryRadius();
    }

    if (Options::astrometryCustomOptions().isEmpty() == false)
        optionsMap["custom"] = Options::astrometryCustomOptions();


    QStringList solverArgs = generateOptions(optionsMap);

    QString options = solverArgs.join(" ");
    solverOptions->setText(options);
    solverOptions->setToolTip(options);
}

#if 0
void Align::checkLineEdits()
{
   bool raOk(false), decOk(false);
   raBox->createDms( false, &raOk );
   decBox->createDms( true, &decOk );
   if ( raOk && decOk )
            generateArgs();
}

void Align::copyCoordsToBoxes()
{
    raBox->setText(ScopeRAOut->text());
    decBox->setText(ScopeDecOut->text());

    checkLineEdits();
}

void Align::clearCoordBoxes()
{
    raBox->clear();
    decBox->clear();

    generateArgs();
}
#endif

bool Align::captureAndSolve()
{
    //m_isSolverComplete = false;

    if (currentCCD == NULL)
        return false;

    if (currentCCD->isConnected() == false)
    {
        appendLogText(i18n("Error: Lost connection to CCD."));
        KNotification::event( QLatin1String( "AlignFailed"), i18n("Astrometry alignment failed") );
        return false;
    }

    if (parser->init() == false)
        return false;

    if (focal_length == -1 || aperture == -1)
    {
        KMessageBox::error(0, i18n("Telescope aperture and focal length are missing. Please check your driver settings and try again."));
        return false;
    }

    if (ccd_hor_pixel == -1 || ccd_ver_pixel == -1)
    {
        KMessageBox::error(0, i18n("CCD pixel size is missing. Please check your driver settings and try again."));
        return false;
    }

    if (currentFilter != NULL && lockedFilterIndex != -1)
    {
        if (lockedFilterIndex != currentFilterIndex)
        {
            int lockedFilterPosition = lockedFilterIndex + 1;
            filterPositionPending = true;
            currentFilter->runCommand(INDI_SET_FILTER, &lockedFilterPosition);
            return true;
        }
    }

    if (currentCCD->getDriverInfo()->getClientManager()->getBLOBMode(currentCCD->getDeviceName(), "CCD1") == B_NEVER)
    {
        if (KMessageBox::questionYesNo(0, i18n("Image transfer is disabled for this camera. Would you like to enable it?")) == KMessageBox::Yes)
        {
            currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_ALSO, currentCCD->getDeviceName(), "CCD1");
            currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_ALSO, currentCCD->getDeviceName(), "CCD2");
        }
        else
        {
            return false;
        }
    }

    double seqExpose = exposureIN->value();

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    if (focusState >= FOCUS_PROGRESS)
    {
        appendLogText(i18n("Cannot capture while focus module is busy! Retrying..."));
        QTimer::singleShot(5000, this, SLOT(captureAndSolve()));
        return false;
    }

    if (targetChip->isCapturing())
    {
        appendLogText(i18n("Cannot capture while CCD exposure is in progress! Retrying..."));
        QTimer::singleShot(1000, this, SLOT(captureAndSolve()));
        return false;
    }

   alignView->setBaseSize(alignWidget->size());

   connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));
   connect(currentCCD, SIGNAL(newExposureValue(ISD::CCDChip*,double,IPState)), this, SLOT(checkCCDExposureProgress(ISD::CCDChip*,double,IPState)));

   // In case of remote solver, we set mode to UPLOAD_BOTH
   if (solverTypeGroup->checkedId() == SOLVER_REMOTE)
   {
       rememberUploadMode = currentCCD->getUploadMode();
       currentCCD->setUploadMode(ISD::CCD::UPLOAD_BOTH);

       // For solver remote we need to start solver BEFORE capture
       startSolving(QString());
   }
   else
   {
       if (currentCCD->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
       {
           rememberUploadMode = ISD::CCD::UPLOAD_LOCAL;
           currentCCD->setUploadMode(ISD::CCD::UPLOAD_CLIENT);
       }

       // Remove temporary FITS files left before by the solver
       QDir dir(QDir::tempPath());
       dir.setNameFilters(QStringList() << "fits*" << "tmp.*");
       dir.setFilter(QDir::Files);
       foreach(QString dirFile, dir.entryList())
           dir.remove(dirFile);
   }

   currentCCD->setTransformFormat(ISD::CCD::FORMAT_FITS);

   targetChip->resetFrame();
   targetChip->setBatchMode(false);
   targetChip->setCaptureMode(FITS_ALIGN);
   targetChip->setFrameType(FRAME_LIGHT);

   int bin = Options::solverBinningIndex()+1;
   targetChip->setBinning(bin, bin);

   // In case we're in refresh phase of the polar alignment helper then we use capture value from there
   if (pahStage == PAH_REFRESH)
       targetChip->capture(PAHExposure->value());
   else
       targetChip->capture(seqExpose);

   Options::setAlignExposure(seqExpose);

   solveB->setEnabled(false);
   stopB->setEnabled(true);
   pi->startAnimation();

   state = ALIGN_PROGRESS;
   emit newStatus(state);

   appendLogText(i18n("Capturing image..."));

   return true;
}

void Align::newFITS(IBLOB *bp)
{
    // Ignore guide head if there is any.
    if (!strcmp(bp->name, "CCD2"))
        return;

    disconnect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));
    disconnect(currentCCD, SIGNAL(newExposureValue(ISD::CCDChip*,double,IPState)), this, SLOT(checkCCDExposureProgress(ISD::CCDChip*,double,IPState)));

    appendLogText(i18n("Image received."));

    blobType = *(static_cast<ISD::CCD::BlobType*>(bp->aux1));
    blobFileName = QString(static_cast<char*>(bp->aux2));

    if (solverTypeGroup->checkedId() != SOLVER_REMOTE)
    {
        if (blobType == ISD::CCD::BLOB_FITS)
        {
            ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

            if (alignDarkFrameCheck->isChecked())
            {
                int x,y,w,h,binx=1,biny=1;
                targetChip->getFrame(&x,&y,&w,&h);
                targetChip->getBinning(&binx, &biny);

                FITSData *darkData       = NULL;

                uint16_t offsetX = x / binx;
                uint16_t offsetY = y / biny;

                darkData = DarkLibrary::Instance()->getDarkFrame(targetChip, exposureIN->value());

                connect(DarkLibrary::Instance(), SIGNAL(darkFrameCompleted(bool)), this, SLOT(setCaptureComplete()));
                connect(DarkLibrary::Instance(), SIGNAL(newLog(QString)), this, SLOT(appendLogText(QString)));

                if (darkData)
                    DarkLibrary::Instance()->subtract(darkData, alignView, FITS_NONE, offsetX, offsetY);
                else
                {
                    bool rc = DarkLibrary::Instance()->captureAndSubtract(targetChip, alignView, exposureIN->value(), offsetX, offsetY);
                    alignDarkFrameCheck->setChecked(rc);
                }

                return;
            }
        }

        setCaptureComplete();
    }
}

void Align::setCaptureComplete()
{
    DarkLibrary::Instance()->disconnect(this);

    if (pahStage == PAH_REFRESH)
    {
        captureAndSolve();
        return;
    }

    if (solverTypeGroup->checkedId() == SOLVER_ONLINE && Options::astrometryUseJPEG())
    {
        ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
        if (targetChip)
        {
            QString jpegFile = blobFileName + ".jpg";
            bool rc = alignView->getDisplayImage()->save(jpegFile, "JPG");
            if (rc)
                blobFileName = jpegFile;
        }
    }

    if (fov())
        fov()->setImageDisplay(alignView->getDisplayImage());

    startSolving(blobFileName);
}

void Align::setGOTOMode(int mode)
{
    gotoModeButtonGroup->button(mode)->setChecked(true);
    currentGotoMode = static_cast<GotoMode>(mode);
}

void Align::startSolving(const QString &filename, bool isGenerated)
{
    QStringList solverArgs;
    double ra,dec;

    QString options = solverOptions->text().simplified();

    if (isGenerated)
    {
        solverArgs = options.split(" ");
        // Replace RA and DE with LST & 90/-90 pole
        if (pahStage == PAH_FIRST_CAPTURE)
        {
            for (int i=0; i <solverArgs.count(); i++)
            {
                // RA
                if (solverArgs[i] == "-3")
                    solverArgs[i+1] = QString::number(KStarsData::Instance()->lst()->Degrees());
                // DE. +90 for Northern hemisphere. -90 for southern hemisphere
                else if (solverArgs[i] == "-4")
                    solverArgs[i+1] = QString::number(hemisphere == NORTH_HEMISPHERE ? 90 : -90);
            }
        }
    }
    else if (filename.endsWith("fits") || filename.endsWith("fit"))
    {
        solverArgs = getSolverOptionsFromFITS(filename);
        appendLogText(i18n("Using solver options: %1", solverArgs.join(" ")));
    }
    else
    {
        KGuiItem blindItem(i18n("Blind solver"), QString(), i18n("Blind solver takes a very long time to solve but can reliably solve any image any where in the sky given enough time."));
        KGuiItem existingItem(i18n("Use existing settings"), QString(), i18n("Mount must be pointing close to the target location and current field of view must match the image's field of view."));
        int rc = KMessageBox::questionYesNoCancel(0, i18n("No metadata is available in this image. Do you want to use the blind solver or the existing solver settings?"), i18n("Astrometry solver"),
                                         blindItem, existingItem, KStandardGuiItem::cancel(), "blind_solver_or_existing_solver_option");
        if (rc == KMessageBox::Yes)
        {
            QVariantMap optionsMap;

            if (Options::astrometryUseNoVerify())
                optionsMap["noverify"] = true;

            if (Options::astrometryUseResort())
                optionsMap["resort"] = true;

            if (Options::astrometryUseNoFITS2FITS())
                optionsMap["nofits2fits"] = true;

            if (Options::astrometryUseDownsample())
                optionsMap["downsample"] = Options::astrometryDownsample();

            solverArgs = generateOptions(optionsMap);
        }
        else if (rc == KMessageBox::No)
            solverArgs = options.split(" ");
        else
        {
            abort();
            return;
        }
    }

    currentTelescope->getEqCoords(&ra, &dec);

    if (solverIterations == 0)
    {
        targetCoord.setRA(ra);
        targetCoord.setDec(dec);
    }

    Options::setSolverType(solverTypeGroup->checkedId());
    //Options::setSolverOptions(solverOptions->text());
    Options::setGuideScopeCCDs(guideScopeCCDs);
    Options::setSolverAccuracyThreshold(accuracySpin->value());
    Options::setAlignDarkFrame(alignDarkFrameCheck->isChecked());
    Options::setSolverGotoOption(currentGotoMode);

    //m_isSolverComplete = false;
    //m_isSolverSuccessful = false;

    parser->verifyIndexFiles(fov_x, fov_y);

    solverTimer.start();

    alignTimer.start();

    if (currentGotoMode == GOTO_SLEW)
        appendLogText(i18n("Solver iteration #%1", solverIterations+1));

    state = ALIGN_PROGRESS;
    emit newStatus(state);

    parser->startSovler(filename, solverArgs, isGenerated);
}

void Align::solverFinished(double orientation, double ra, double dec, double pixscale)
{
    pi->stopAnimation();
    stopB->setEnabled(false);
    solveB->setEnabled(true);

    sOrientation = orientation;
    sRA  = ra;
    sDEC = dec;

    alignTimer.stop();

    int binx, biny;
    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
    targetChip->getBinning(&binx, &biny);

    if (Options::astrometrySolverVerbose())
        appendLogText(i18n("Solver RA (%1) DEC (%2) Orientation (%3) Pixel Scale (%4)", QString::number(ra, 'g' , 5), QString::number(dec, 'g' , 5),
                            QString::number(orientation, 'g' , 5), QString::number(pixscale, 'g' , 5)));

    if (pixscale > 0 && loadSlewState == IPS_IDLE)
    {
        double solver_focal_length = (206.264 * ccd_hor_pixel) / pixscale * binx;
        if (fabs(focal_length - solver_focal_length) > 1)
            appendLogText(i18n("Current focal length is %1 mm while computed focal length from the solver is %2 mm. Please update the mount focal length to obtain accurate results.",
                                QString::number(focal_length, 'g' , 5), QString::number(solver_focal_length, 'g' , 5)));
    }

     alignCoord.setRA0(ra/15.0);
     alignCoord.setDec0(dec);
     RotOut->setText(QString::number(orientation, 'g', 5));

     // Convert to JNow
     alignCoord.apparentCoord((long double) J2000, KStars::Instance()->data()->ut().djd());
     // Get horizontal coords
     alignCoord.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

     double raDiff = (alignCoord.ra().Degrees()-targetCoord.ra().Degrees()) * 3600;
     double deDiff = (alignCoord.dec().Degrees()-targetCoord.dec().Degrees()) * 3600;

     dms RADiff(fabs(raDiff)/3600.0), DEDiff(deDiff/3600.0);
     dRAOut->setText(QString("%1%2").arg(raDiff > 0 ? "+" : "-").arg(RADiff.toHMSString()));
     dDEOut->setText(DEDiff.toDMSString(true));

     pixScaleOut->setText(QString::number(pixscale, 'f', 2));

     emit newSolutionDeviation(raDiff, deDiff);

     targetDiff    = sqrt(raDiff*raDiff + deDiff*deDiff);

     solverFOV->setCenter(alignCoord);
     solverFOV->setRotation(sOrientation);
     solverFOV->setImageDisplay(Options::astrometrySolverOverlay());

     QString ra_dms, dec_dms;
     getFormattedCoords(alignCoord.ra().Hours(), alignCoord.dec().Degrees(), ra_dms, dec_dms);

     SolverRAOut->setText(ra_dms);
     SolverDecOut->setText(dec_dms);

     if (Options::astrometrySolverWCS())
     {
         INumberVectorProperty *ccdRotation = currentCCD->getBaseDevice()->getNumber("CCD_ROTATION");
         if (ccdRotation)
         {
             INumber *rotation = IUFindNumber(ccdRotation, "CCD_ROTATION_VALUE");
             if (rotation)
             {
                 ClientManager *clientManager = currentCCD->getDriverInfo()->getClientManager();
                 rotation->value = orientation;
                 clientManager->sendNewNumber(ccdRotation);

                 if (m_wcsSynced == false)
                 {
                     appendLogText(i18n("WCS information updated. Images captured from this point forward shall have valid WCS."));

                     // Just send telescope info in case the CCD driver did not pick up before.
                     INumberVectorProperty *telescopeInfo = currentTelescope->getBaseDevice()->getNumber("TELESCOPE_INFO");
                     if (telescopeInfo)
                        clientManager->sendNewNumber(telescopeInfo);

                     m_wcsSynced=true;
                 }
             }
         }
     }

     retries=0;

     appendLogText(i18n("Solution coordinates: RA (%1) DEC (%2) Telescope Coordinates: RA (%3) DEC (%4)", alignCoord.ra().toHMSString(), alignCoord.dec().toDMSString(), telescopeCoord.ra().toHMSString(), telescopeCoord.dec().toDMSString()));
     if (loadSlewState == IPS_IDLE && currentGotoMode == GOTO_SLEW)
     {
        dms diffDeg(targetDiff/3600.0);
        appendLogText(i18n("Target is within %1 degrees of solution coordinates.", diffDeg.toDMSString()));
     }

     if (rememberUploadMode != currentCCD->getUploadMode())
         currentCCD->setUploadMode(rememberUploadMode);

     //if (syncR->isChecked() || nothingR->isChecked() || targetDiff <= accuracySpin->value())
     // CONTINUE HERE

     switch (currentGotoMode)
     {
        case GOTO_SYNC:
         executeGOTO();
         return;
         break;

        case GOTO_SLEW:
         if (loadSlewState == IPS_BUSY || targetDiff > accuracySpin->value())
         {
             if (loadSlewState == IPS_IDLE && ++solverIterations == MAXIMUM_SOLVER_ITERATIONS)
             {
                 appendLogText(i18n("Maximum number of iterations reached. Solver failed."));
                 solverFailed();
                 return;
             }

             //executeMode();
             executeGOTO();
             return;
         }

         appendLogText(i18n("Target is within acceptable range. Astrometric solver is successful."));
         break;

        case GOTO_NOTHING:
         break;

     }

     KNotification::event( QLatin1String( "AlignSuccessful"), i18n("Astrometry alignment completed successfully") );
     state = ALIGN_COMPLETE;
     emit newStatus(state);
     solverIterations=0;

     if (pahStage != PAH_IDLE)
         processPAHStage(orientation, ra, dec, pixscale);
     else if (azStage > AZ_INIT || altStage > ALT_INIT)
         executePolarAlign();
}

void Align::solverFailed()
{
    KNotification::event( QLatin1String( "AlignFailed"), i18n("Astrometry alignment failed with errors") );

    pi->stopAnimation();
    stopB->setEnabled(false);
    solveB->setEnabled(true);

    alignTimer.stop();

    azStage  = AZ_INIT;
    altStage = ALT_INIT;

    //loadSlewMode = false;
    loadSlewState=IPS_IDLE;
    PAHFirstCaptureB->setEnabled(true);
    PAHSecondCaptureB->setEnabled(true);
    PAHThirdCaptureB->setEnabled(true);
    solverIterations=0;
    retries=0;

    //emit solverComplete(false);

    state = ALIGN_FAILED;
    emit newStatus(state);
}

void Align::abort()
{
    parser->stopSolver();
    pi->stopAnimation();
    stopB->setEnabled(false);
    solveB->setEnabled(true);

    azStage  = AZ_INIT;
    altStage = ALT_INIT;

    //loadSlewMode = false;
    loadSlewState=IPS_IDLE;
    PAHFirstCaptureB->setEnabled(true);
    PAHSecondCaptureB->setEnabled(true);
    PAHThirdCaptureB->setEnabled(true);
    solverIterations=0;
    retries=0;
    alignTimer.stop();

    //currentCCD->disconnect(this);
    disconnect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));
    disconnect(currentCCD, SIGNAL(newExposureValue(ISD::CCDChip*,double,IPState)), this, SLOT(checkCCDExposureProgress(ISD::CCDChip*,double,IPState)));

    if (rememberUploadMode != currentCCD->getUploadMode())
        currentCCD->setUploadMode(rememberUploadMode);

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    // If capture is still in progress, let's stop that.
    if (pahStage == PAH_REFRESH)
    {
        if (targetChip->isCapturing())
            targetChip->abortExposure();

        appendLogText(i18n("Refresh is complete."));
    }
    else
    {
        if (targetChip->isCapturing())
        {
            targetChip->abortExposure();
            appendLogText(i18n("Capture aborted."));
        }
        else
        {
            int elapsed = (int) round(solverTimer.elapsed()/1000.0);
            appendLogText(i18np("Solver aborted after %1 second.", "Solver aborted after %1 seconds", elapsed));
        }
    }

    state = ALIGN_ABORTED;
    emit newStatus(state);
}

QList<double> Align::getSolutionResult()
{
    QList<double> result;

    result << sOrientation << sRA << sDEC;

    return result;
}

void Align::appendLogText(const QString &text)
{
    logText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    if (Options::alignmentLogging())
        qDebug() << "Alignment: " << text;

    emit newLog();
}

void Align::clearLog()
{
    logText.clear();
    emit newLog();
}

void Align::processTelescopeNumber(INumberVectorProperty *coord)
{
    QString ra_dms, dec_dms;
    static bool slew_dirty=false;

    if (!strcmp(coord->name, "EQUATORIAL_EOD_COORD"))
    {
        getFormattedCoords(coord->np[0].value, coord->np[1].value, ra_dms, dec_dms);

        telescopeCoord.setRA(coord->np[0].value);
        telescopeCoord.setDec(coord->np[1].value);
        telescopeCoord.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

        ScopeRAOut->setText(ra_dms);
        ScopeDecOut->setText(dec_dms);

        switch (coord->s)
        {
        case IPS_OK:
        {
            // Update the boxes as the mount just finished slewing
            if (slew_dirty && Options::astrometryAutoUpdatePosition())
            {
                opsAstrometry->estRA->setText(ra_dms);
                opsAstrometry->estDec->setText(dec_dms);

                Options::setAstrometryPositionRA(coord->np[0].value*15);
                Options::setAstrometryPositionDE(coord->np[1].value);

                generateArgs();
            }

            if (slew_dirty && pahStage == PAH_FIND_CP)
            {
                slew_dirty = false;
                appendLogText(i18n("Mount completed slewing near celestial pole. Capture again to verify."));
                PAHFirstCaptureB->setEnabled(true);
                setGOTOMode(GOTO_NOTHING);
                pahStage = PAH_FIRST_CAPTURE;
                return;
            }

            if (slew_dirty && pahStage == PAH_FIRST_ROTATE)
            {
                slew_dirty = false;

                appendLogText(i18n("Mount first rotation is complete."));

                pahStage = PAH_SECOND_CAPTURE;

                PAHWidgets->setCurrentWidget(PAHSecondCapturePage);

                if (PAHAutoModeCheck->isChecked())
                    PAHSecondCaptureB->click();
            }
            else if (slew_dirty && pahStage == PAH_SECOND_ROTATE)
            {
                slew_dirty = false;

                appendLogText(i18n("Mount second rotation is complete."));

                pahStage = PAH_THIRD_CAPTURE;

                PAHWidgets->setCurrentWidget(PAHThirdCapturePage);

                if (PAHAutoModeCheck->isChecked())
                    PAHThirdCaptureB->click();
            }

            switch (state)
            {
            case ALIGN_PROGRESS:
                break;

            case ALIGN_SYNCING:
            {
                slew_dirty = false;
                if (currentGotoMode == GOTO_SLEW)
                {
                    Slew();
                    return;
                }
                else
                {
                    appendLogText(i18n("Mount is synced to solution coordinates. Astrometric solver is successful."));
                    KNotification::event( QLatin1String( "AlignSuccessful"), i18n("Astrometry alignment completed successfully") );
                    state = ALIGN_COMPLETE;
                    emit newStatus(state);
                }
            }
            break;

            case ALIGN_SLEWING:
                if (slew_dirty == false)
                    break;

                slew_dirty = false;
                if (loadSlewState == IPS_BUSY)
                {
                    loadSlewState = IPS_IDLE;

                    if (Options::alignmentLogging())
                        qDebug() << "Alignment: loadSlewState is IDLE.";

                    state = ALIGN_PROGRESS;
                    emit newStatus(state);

                    QTimer::singleShot(delaySpin->value(), this, SLOT(captureAndSolve()));
                    return;
                }
                else if(currentGotoMode == GOTO_SLEW)
                {
                    appendLogText(i18n("Slew complete. Target accuracy is not met, running solver again..."));

                    state = ALIGN_PROGRESS;
                    emit newStatus(state);

                    QTimer::singleShot(delaySpin->value(), this, SLOT(captureAndSolve()));
                    return;
                }
                break;

            default:
            {
                slew_dirty = false;
            }
            break;
            }
        }
        break;

        case IPS_BUSY:
        {
            slew_dirty = true;

        }
        break;

        case IPS_ALERT:
        {
            if (state == ALIGN_SYNCING || state == ALIGN_SLEWING)
            {
                if (state == ALIGN_SYNCING)
                    appendLogText(i18n("Syncing failed!"));
                else
                    appendLogText(i18n("Slewing failed!"));

                if (++retries == 3)
                {
                    abort();
                    return;
                }
                else
                {
                    if (currentGotoMode == GOTO_SLEW)
                        Slew();
                    else
                        Sync();
                }
            }

            return;
        }
        break;

        default:
        break;
        }


        /*if (Options::alignmentLogging())
            qDebug() << "Alignment: State is " << Ekos::getAlignStatusString(state) << " isSlewing? " << currentTelescope->isSlewing() << " slew Dirty? " << slew_dirty
                     << " Current GOTO Mode? " << currentGotoMode << " LoadSlewState? " << pstateStr(loadSlewState);*/

        switch (azStage)
        {
        case AZ_SYNCING:
            if (currentTelescope->isSlewing())
                azStage=AZ_SLEWING;
            break;

        case AZ_SLEWING:
            if (currentTelescope->isSlewing() == false)
            {
                azStage = AZ_SECOND_TARGET;
                measureAzError();
            }
            break;

        case AZ_CORRECTING:
            if (currentTelescope->isSlewing() == false)
            {
                appendLogText(i18n("Slew complete. Please adjust azimuth knob until the target is in the center of the view."));
                azStage = AZ_INIT;
            }
            break;

        default:
            break;
        }

        switch (altStage)
        {
        case ALT_SYNCING:
            if (currentTelescope->isSlewing())
                altStage = ALT_SLEWING;
            break;

        case ALT_SLEWING:
            if (currentTelescope->isSlewing() == false)
            {
                altStage = ALT_SECOND_TARGET;
                measureAltError();
            }
            break;

        case ALT_CORRECTING:
            if (currentTelescope->isSlewing() == false)
            {
                appendLogText(i18n("Slew complete. Please adjust altitude knob until the target is in the center of the view."));
                altStage = ALT_INIT;
            }
            break;
        default:
            break;
        }

    }


    // N.B. EkosManager already mananges TELESCOPE_INFO, why here again?
    //if (!strcmp(coord->name, "TELESCOPE_INFO"))
        //syncTelescopeInfo();

}

void Align::executeGOTO()
{
    if (loadSlewState == IPS_BUSY)
    {
        //if (loadSlewIterations == loadSlewIterationsSpin->value())
            //loadSlewCoord = alignCoord;

        //targetCoord = loadSlewCoord;
        targetCoord = alignCoord;
        SlewToTarget();
    }
    else if (currentGotoMode == GOTO_SYNC)
        Sync();
    else if (currentGotoMode == GOTO_SLEW)
        SlewToTarget();
}

void Align::Sync()
{
    state = ALIGN_SYNCING;

    if (currentTelescope->Sync(&alignCoord))
    {
        emit newStatus(state);
        appendLogText(i18n("Syncing to RA (%1) DEC (%2)", alignCoord.ra().toHMSString(), alignCoord.dec().toDMSString()));
    }
    else
    {
        state = ALIGN_IDLE;
        emit newStatus(state);
        appendLogText(i18n("Syncing failed."));
    }
}

void Align::Slew()
{
    state = ALIGN_SLEWING;
    emit newStatus(state);

    currentTelescope->Slew(&targetCoord);

    appendLogText(i18n("Slewing to target coordinates: RA (%1) DEC (%2).", targetCoord.ra().toHMSString(), targetCoord.dec().toDMSString()));
}

void Align::SlewToTarget()
{
    if (canSync && loadSlewState == IPS_IDLE)
    {
        Sync();
        return;
    }

    Slew();
}

void Align::executePolarAlign()
{
    appendLogText(i18n("Processing solution for polar alignment..."));

    switch (azStage)
    {
        case AZ_FIRST_TARGET:
        case AZ_FINISHED:
            measureAzError();
            break;

        default:
            break;
    }

    switch (altStage)
    {
        case ALT_FIRST_TARGET:
        case ALT_FINISHED:
            measureAltError();
            break;

        default:
            break;
    }
}


void Align::measureAzError()
{
    static double initRA=0, initDEC=0, finalRA=0, finalDEC=0, initAz=0;

    if (pahStage != PAH_IDLE &&
        (KMessageBox::warningContinueCancel(KStars::Instance(),
         i18n("Polar Alignment Helper is still active. Do you want to continue using legacy polar alignment tool?"))!=KMessageBox::Continue))
        return;

    pahStage = PAH_IDLE;

    if (Options::alignmentLogging())
        qDebug() << "Polar Alignment: Measureing Azimuth Error...";

    switch (azStage)
    {
        case AZ_INIT:

        // Display message box confirming user point scope near meridian and south

        if (KMessageBox::warningContinueCancel( 0, hemisphere == NORTH_HEMISPHERE
                                                   ? i18n("Point the telescope at the southern meridian. Press continue when ready.")
                                                   : i18n("Point the telescope at the northern meridian. Press continue when ready.")
                                                , i18n("Polar Alignment Measurement"), KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                                                "ekos_measure_az_error")!=KMessageBox::Continue)
            return;

        appendLogText(i18n("Solving first frame near the meridian."));
        azStage = AZ_FIRST_TARGET;
        solveB->click();
        break;

      case AZ_FIRST_TARGET:
        // start solving there, find RA/DEC
        initRA   = alignCoord.ra().Degrees();
        initDEC  = alignCoord.dec().Degrees();
        initAz   = alignCoord.az().Degrees();

        if (Options::alignmentLogging())
            qDebug() << "Polar Alignment: initRA " << alignCoord.ra().toHMSString() << " initDEC " << alignCoord.dec().toDMSString() <<
                        " initlAz " << alignCoord.az().toDMSString() << " initAlt " << alignCoord.alt().toDMSString();

        // Now move 30 arcminutes in RA
        if (canSync)
        {
            azStage = AZ_SYNCING;
            currentTelescope->Sync(initRA/15.0, initDEC);
            currentTelescope->Slew((initRA - RAMotion)/15.0, initDEC);
        }
        // If telescope doesn't sync, we slew relative to its current coordinates
        else
        {
            azStage = AZ_SLEWING;
            currentTelescope->Slew(telescopeCoord.ra().Hours() - RAMotion/15.0, telescopeCoord.dec().Degrees());
        }

        appendLogText(i18n("Slewing 30 arcminutes in RA..."));
        break;

      case AZ_SECOND_TARGET:
        // We reached second target now
        // Let now solver for RA/DEC
        appendLogText(i18n("Solving second frame near the meridian."));
        azStage = AZ_FINISHED;
        solveB->click();
        break;


      case AZ_FINISHED:
        // Measure deviation in DEC
        // Call function to report error
        // set stage to AZ_FIRST_TARGET again
        appendLogText(i18n("Calculating azimuth alignment error..."));
        finalRA   = alignCoord.ra().Degrees();
        finalDEC  = alignCoord.dec().Degrees();

        if (Options::alignmentLogging())
            qDebug() << "Polar Alignment: finalRA " << alignCoord.ra().toHMSString() << " finalDEC " << alignCoord.dec().toDMSString() <<
                        " finalAz " << alignCoord.az().toDMSString() << " finalAlt " << alignCoord.alt().toDMSString();

        // Slew back to original position
        if (canSync)
            currentTelescope->Slew(initRA/15.0, initDEC);
        else
        {
            currentTelescope->Slew(telescopeCoord.ra().Hours() + RAMotion/15.0, telescopeCoord.dec().Degrees());
        }

        appendLogText(i18n("Slewing back to original position..."));

        calculatePolarError(initRA, initDEC, finalRA, finalDEC, initAz);

        azStage = AZ_INIT;
        break;

    default:
        break;

    }

}

void Align::measureAltError()
{
    static double initRA=0, initDEC=0, finalRA=0, finalDEC=0, initAz=0;

    if (pahStage != PAH_IDLE &&
        (KMessageBox::warningContinueCancel(KStars::Instance(),
         i18n("Polar Alignment Helper is still active. Do you want to continue using legacy polar alignment tool?"))!=KMessageBox::Continue))
        return;

    pahStage = PAH_IDLE;

    if (Options::alignmentLogging())
        qDebug() << "Polar Alignment: Measureing Altitude Error...";

    switch (altStage)
    {
        case ALT_INIT:

        // Display message box confirming user point scope near meridian and south

        if (KMessageBox::warningContinueCancel( 0, i18n("Point the telescope to the eastern or western horizon with a minimum altitude of 20 degrees. Press continue when ready.")
                                                , i18n("Polar Alignment Measurement"), KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                                                "ekos_measure_alt_error")!=KMessageBox::Continue)
            return;

        appendLogText(i18n("Solving first frame."));
        altStage = ALT_FIRST_TARGET;
        solveB->click();
        break;

      case ALT_FIRST_TARGET:
        // start solving there, find RA/DEC
        initRA   = alignCoord.ra().Degrees();
        initDEC  = alignCoord.dec().Degrees();
        initAz   = alignCoord.az().Degrees();

        if (Options::alignmentLogging())
            qDebug() << "Polar Alignment: initRA " << alignCoord.ra().toHMSString() << " initDEC " << alignCoord.dec().toDMSString() <<
                        " initlAz " << alignCoord.az().toDMSString() << " initAlt " << alignCoord.alt().toDMSString();

        // Now move 30 arcminutes in RA
        if (canSync)
        {
            altStage = ALT_SYNCING;
            currentTelescope->Sync(initRA/15.0, initDEC);
            currentTelescope->Slew((initRA - RAMotion)/15.0, initDEC);

        }
        // If telescope doesn't sync, we slew relative to its current coordinates
        else
        {
            altStage = ALT_SLEWING;
            currentTelescope->Slew(telescopeCoord.ra().Hours() - RAMotion/15.0, telescopeCoord.dec().Degrees());
        }


        appendLogText(i18n("Slewing 30 arcminutes in RA..."));
        break;

      case ALT_SECOND_TARGET:
        // We reached second target now
        // Let now solver for RA/DEC
        appendLogText(i18n("Solving second frame."));
        altStage = ALT_FINISHED;
        solveB->click();
        break;


      case ALT_FINISHED:
        // Measure deviation in DEC
        // Call function to report error
        appendLogText(i18n("Calculating altitude alignment error..."));
        finalRA   = alignCoord.ra().Degrees();
        finalDEC  = alignCoord.dec().Degrees();

        if (Options::alignmentLogging())
            qDebug() << "Polar Alignment: finalRA " << alignCoord.ra().toHMSString() << " finalDEC " << alignCoord.dec().toDMSString() <<
                        " finalAz " << alignCoord.az().toDMSString() << " finalAlt " << alignCoord.alt().toDMSString();

        // Slew back to original position
        if (canSync)
            currentTelescope->Slew(initRA/15.0, initDEC);
        // If telescope doesn't sync, we slew relative to its current coordinates
        else
        {
            currentTelescope->Slew(telescopeCoord.ra().Hours() + RAMotion/15.0, telescopeCoord.dec().Degrees());
        }

        appendLogText(i18n("Slewing back to original position..."));

        calculatePolarError(initRA, initDEC, finalRA, finalDEC, initAz);

        altStage = ALT_INIT;
        break;

    default:
        break;

    }

}

void Align::calculatePolarError(double initRA, double initDEC, double finalRA, double finalDEC, double initAz)
{
    double raMotion = finalRA - initRA;
    decDeviation = finalDEC - initDEC;

    // East/West of meridian
    int horizon    = (initAz > 0 && initAz <= 180) ? 0 : 1;

    // How much time passed siderrally form initRA to finalRA?
    //double RATime = fabs(raMotion / SIDRATE) / 60.0;

    // 2016-03-30: Diff in RA is sufficient for time difference
    // raMotion in degrees. RATime in minutes.
    double RATime = fabs(raMotion) * 60.0;

    // Equation by Frank Berret (Measuring Polar Axis Alignment Error, page 4)
    // In degrees
    double deviation = (3.81 * (decDeviation * 3600) ) / ( RATime * cos(initDEC * dms::DegToRad)) / 60.0;
    dms devDMS(fabs(deviation));

    KLocalizedString deviationDirection;

    switch (hemisphere)
    {
        // Northern hemisphere
        case NORTH_HEMISPHERE:
        if (azStage == AZ_FINISHED)
        {
            if (decDeviation > 0)
                deviationDirection = ki18n("%1 too far east");
            else
                deviationDirection = ki18n("%1 too far west");
        }
        else if (altStage == ALT_FINISHED)
        {
            switch (horizon)
            {
                // East
                case 0:
                if (decDeviation > 0)
                    deviationDirection = ki18n("%1 too far high");
                else
                    deviationDirection = ki18n("%1 too far low");

                break;

                // West
                case 1:
                if (decDeviation > 0)
                    deviationDirection = ki18n("%1 too far low");
                else
                    deviationDirection = ki18n("%1 too far high");
                break;

                default:
                break;
            }
        }
        break;

        // Southern hemisphere
        case SOUTH_HEMISPHERE:
        if (azStage == AZ_FINISHED)
        {
            if (decDeviation > 0)
                deviationDirection = ki18n("%1 too far west");
            else
                deviationDirection = ki18n("%1 too far east");
        }
        else if (altStage == ALT_FINISHED)
        {
            switch (horizon)
            {
                // East
                case 0:
                if (decDeviation > 0)
                    deviationDirection = ki18n("%1 too far low");
                else
                    deviationDirection = ki18n("%1 too far high");
                break;

                // West
                case 1:
                if (decDeviation > 0)
                    deviationDirection = ki18n("%1 too far high");
                else
                    deviationDirection = ki18n("%1 too far low");
                break;

                default:
                break;
            }
        }
        break;

       default:
        break;

    }

    if (Options::alignmentLogging())
    {
        qDebug() << "Polar Alignment: Hemisphere is " << ((hemisphere == NORTH_HEMISPHERE) ? "North" : "South") << " --- initAz " << initAz;
        qDebug() << "Polar Alignment: initRA " << initRA << " initDEC " << initDEC << " finalRA " << finalRA << " finalDEC " << finalDEC;
        qDebug() << "Polar Alignment: decDeviation " << decDeviation*3600 << " arcsec " << " RATime " << RATime << " minutes";
        qDebug() << "Polar Alignment: Raw Deviaiton " << deviation << " degrees.";
    }

    if (azStage == AZ_FINISHED)
    {
        azError->setText(deviationDirection.subs(QString("%1").arg(devDMS.toDMSString())).toString());
        //azError->setText(deviationDirection.subs(QString("%1")azDMS.toDMSString());
        azDeviation = deviation * (decDeviation > 0 ? 1 : -1);

        if (Options::alignmentLogging())
            qDebug() << "Polar Alignment: Azimuth Deviation " << azDeviation << " degrees.";

        correctAzB->setEnabled(true);
    }
    if (altStage == ALT_FINISHED)
    {
        //altError->setText(deviationDirection.subs(QString("%1").arg(fabs(deviation), 0, 'g', 3)).toString());
        altError->setText(deviationDirection.subs(QString("%1").arg(devDMS.toDMSString())).toString());
        altDeviation = deviation * (decDeviation > 0 ? 1 : -1);

        if (Options::alignmentLogging())
            qDebug() << "Polar Alignment: Altitude Deviation " << altDeviation << " degrees.";

        correctAltB->setEnabled(true);
    }
}

void Align::correctAltError()
{
    double newRA, newDEC;

    SkyPoint currentCoord (telescopeCoord);
    dms      targetLat;

    if (Options::alignmentLogging())
    {
        qDebug() << "Polar Alignment: Correcting Altitude Error...";
        qDebug() << "Polar Alignment: Current Mount RA " << currentCoord.ra().toHMSString() << " DEC " << currentCoord.dec().toDMSString() <<
                    "Az " << currentCoord.az().toDMSString() << " Alt " << currentCoord.alt().toDMSString();
    }

    // An error in polar alignment altitude reflects a deviation in the latitude of the mount from actual latitude of the site
    // Calculating the latitude accounting for the altitude deviation. This is the latitude at which the altitude deviation should be zero.
    targetLat.setD(KStars::Instance()->data()->geo()->lat()->Degrees() + altDeviation);

    // Calculate the Az/Alt of the mount if it were located at the corrected latitude
    currentCoord.EquatorialToHorizontal(KStars::Instance()->data()->lst(), &targetLat );

    // Convert corrected Az/Alt to RA/DEC given the local sideral time and current (not corrected) latitude
    currentCoord.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());

    // New RA/DEC should reflect the position in the sky at which the polar alignment altitude error is minimal.
    newRA  = currentCoord.ra().Hours();
    newDEC = currentCoord.dec().Degrees();

    altStage = ALT_CORRECTING;

    if (Options::alignmentLogging())
    {
        qDebug() << "Polar Alignment: Target Latitude = Latitude " << KStars::Instance()->data()->geo()->lat()->Degrees() << " + Altitude Deviation " << altDeviation << " = " << targetLat.Degrees();
        qDebug() << "Polar Alignment: Slewing to calibration position...";
    }

    currentTelescope->Slew(newRA, newDEC);

    appendLogText(i18n("Slewing to calibration position, please wait until telescope completes slewing."));
}

void Align::correctAzError()
{
    double newRA, newDEC, currentAlt, currentAz;

    SkyPoint currentCoord (telescopeCoord);

    if (Options::alignmentLogging())
    {
        qDebug() << "Polar Alignment: Correcting Azimuth Error...";
        qDebug() << "Polar Alignment: Current Mount RA " << currentCoord.ra().toHMSString() << " DEC " << currentCoord.dec().toDMSString() <<
                    "Az " << currentCoord.az().toDMSString() << " Alt " << currentCoord.alt().toDMSString();
        qDebug() << "Polar Alignment: Target Azimuth = Current Azimuth " << currentCoord.az().Degrees() << " + Azimuth Deviation " << azDeviation << " = " << currentCoord.az().Degrees() + azDeviation;
    }

    // Get current horizontal coordinates of the mount
    currentCoord.EquatorialToHorizontal(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());

    // Keep Altitude as it is and change Azimuth to account for the azimuth deviation
    // The new sky position should be where the polar alignment azimuth error is minimal
    currentAlt = currentCoord.alt().Degrees();
    currentAz  = currentCoord.az().Degrees() + azDeviation;

    // Update current Alt and Azimuth to new values
    currentCoord.setAlt(currentAlt);
    currentCoord.setAz(currentAz);

    // Convert Alt/Az back to equatorial coordinates
    currentCoord.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());

    // Get new RA and DEC
    newRA  = currentCoord.ra().Hours();
    newDEC = currentCoord.dec().Degrees();

    azStage = AZ_CORRECTING;

    if (Options::alignmentLogging())
        qDebug() << "Polar Alignment: Slewing to calibration position...";

    currentTelescope->Slew(newRA, newDEC);

    appendLogText(i18n("Slewing to calibration position, please wait until telescope completes slewing."));

}

void Align::getFormattedCoords(double ra, double dec, QString &ra_str, QString &dec_str)
{
    dms ra_s,dec_s;
    ra_s.setH(ra);
    dec_s.setD(dec);

    ra_str = QString("%1:%2:%3").arg(ra_s.hour(), 2, 10, QChar('0')).arg(ra_s.minute(), 2, 10, QChar('0')).arg(ra_s.second(), 2, 10, QChar('0'));
    if (dec_s.Degrees() < 0)
        dec_str = QString("-%1:%2:%3").arg(abs(dec_s.degree()), 2, 10, QChar('0')).arg(abs(dec_s.arcmin()), 2, 10, QChar('0')).arg(dec_s.arcsec(), 2, 10, QChar('0'));
    else
        dec_str = QString("%1:%2:%3").arg(dec_s.degree(), 2, 10, QChar('0')).arg(dec_s.arcmin(), 2, 10, QChar('0')).arg(dec_s.arcsec(), 2, 10, QChar('0'));
}

void Align::loadAndSlew(QString fileURL)
{
    if (fileURL.isEmpty())
        fileURL = QFileDialog::getOpenFileName(KStars::Instance(), i18n("Load Image"), dirPath, "Images (*.fits *.fit *.jpg *.jpeg)");

    if (fileURL.isEmpty())
        return;

    QFileInfo fileInfo(fileURL);
    dirPath = fileInfo.absolutePath();

    //loadSlewMode = true;
    loadSlewState=IPS_BUSY;

    slewR->setChecked(true);
    currentGotoMode = GOTO_SLEW;

    solveB->setEnabled(false);
    stopB->setEnabled(true);
    pi->startAnimation();

    startSolving(fileURL, false);
}

void Align::setExposure(double value)
{
    exposureIN->setValue(value);
}

void Align::setBinningIndex(int binIndex)
{
   Options::setSolverBinningIndex(binIndex);

   // If sender is not our combo box, then we need to update the combobox itself
   if ( dynamic_cast<QComboBox*>(sender()) != binningCombo)
   {
       binningCombo->blockSignals(true);
       binningCombo->setCurrentIndex(binIndex);
       binningCombo->blockSignals(false);
   }

   // Need to calculate FOV and args for APP
   if (Options::astrometryImageScaleUnits() == OpsAstrometry::SCALE_ARCSECPERPIX)
   {
       calculateFOV();
       generateArgs();
   }

}

void Align::setSolverArguments(const QString & value)
{
    solverOptions->setText(value);
}

void Align::setSolverSearchCoords(double ra, double dec)
{    
    dms RA, DEC;
    RA.setH(ra);
    DEC.setD(dec);

    //TODO
    //raBox->setText(RA.toHMSString());
    //decBox->setText(DEC.toDMSString());
}

void Align::setUseOAGT(bool enabled)
{
    FOVScopeCombo->setCurrentIndex(enabled ? ISD::CCD::TELESCOPE_GUIDE : ISD::CCD::TELESCOPE_PRIMARY);
}

FOV* Align::fov()
{
    if (sOrientation == -1)
        return NULL;
    else
        return solverFOV;

}

void Align::setLockedFilter(ISD::GDInterface *filter, int lockedPosition)
{
    currentFilter = filter;
    if (currentFilter)
    {
        lockedFilterIndex = lockedPosition;

        INumberVectorProperty *filterSlot = filter->getBaseDevice()->getNumber("FILTER_SLOT");
        if (filterSlot)
            currentFilterIndex = filterSlot->np[0].value-1;

        connect(currentFilter, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processFilterNumber(INumberVectorProperty*)), Qt::UniqueConnection);
    }
}

void Align::processFilterNumber(INumberVectorProperty *nvp)
{
    if (currentFilter && !strcmp(nvp->name, "FILTER_SLOT") && !strcmp(nvp->device, currentFilter->getDeviceName()))
    {
        currentFilterIndex = nvp->np[0].value - 1;

        if (filterPositionPending)
        {
            if (currentFilterIndex == lockedFilterIndex)
            {
                filterPositionPending = false;
                captureAndSolve();
            }
        }
    }
}

void Align::setWCSEnabled(bool enable)
{
    if (currentCCD == NULL)
        return;

    ISwitchVectorProperty *wcsControl = currentCCD->getBaseDevice()->getSwitch("WCS_CONTROL");

    ISwitch *wcs_enable  = IUFindSwitch(wcsControl, "WCS_ENABLE");
    ISwitch *wcs_disable = IUFindSwitch(wcsControl, "WCS_DISABLE");

    if (!wcs_enable || !wcs_disable)
        return;

    if ( (wcs_enable->s == ISS_ON && enable) || (wcs_disable->s == ISS_ON && !enable) )
        return;

    IUResetSwitch(wcsControl);
    if (enable)
    {
        appendLogText(i18n("World Coordinate System (WCS) is enabled. CCD rotation must be set either manually in the CCD driver or by solving an image before proceeding to capture any further images, otherwise the WCS information may be invalid."));
        wcs_enable->s  = ISS_ON;
    }
    else
    {
        wcs_disable->s = ISS_ON;
        m_wcsSynced=false;
        appendLogText(i18n("World Coordinate System (WCS) is disabled."));
    }

    ClientManager *clientManager = currentCCD->getDriverInfo()->getClientManager();

    clientManager->sendNewSwitch(wcsControl);
}

void Align::checkCCDExposureProgress(ISD::CCDChip *targetChip, double remaining, IPState state)
{
    INDI_UNUSED(targetChip);
    INDI_UNUSED(remaining);

    if (state == IPS_ALERT)
    {
        if (++retries == 3 && pahStage != PAH_REFRESH)
        {
            appendLogText(i18n("Capture error! Aborting..."));

            abort();
            return;
        }

        appendLogText(i18n("Restarting capture attempt #%1", retries));
        captureAndSolve();
    }
}

void Align::setFocusStatus(Ekos::FocusState state)
{
    focusState = state;
}

QStringList Align::getSolverOptionsFromFITS(const QString &filename)
{
    int status=0, fits_ccd_width, fits_ccd_height, fits_focal_length=-1, fits_binx=1, fits_biny=1;
    char comment[128], error_status[512];
    fitsfile * fptr = NULL;
    double ra=0,dec=0, fits_fov_x, fits_fov_y, fov_lower, fov_upper, fits_ccd_hor_pixel=-1, fits_ccd_ver_pixel=-1;
    QString fov_low,fov_high;
    QStringList solver_args;

    QVariantMap optionsMap;

    if (Options::astrometryUseNoVerify())
        optionsMap["noverify"] = true;

    if (Options::astrometryUseResort())
        optionsMap["resort"] = true;

    if (Options::astrometryUseNoFITS2FITS())
        optionsMap["nofits2fits"] = true;

    if (Options::astrometryUseDownsample())
        optionsMap["downsample"] = Options::astrometryDownsample();

    if (Options::astrometryCustomOptions().isEmpty() == false)
        optionsMap["custom"] = Options::astrometryCustomOptions();

    solver_args = generateOptions(optionsMap);

    if (fits_open_image(&fptr, filename.toLatin1(), READONLY, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        qWarning() << "Could not open file " << filename << "  Error: " << QString::fromUtf8(error_status);
        return solver_args;
    }

    if (fits_read_key(fptr, TINT, "NAXIS1", &fits_ccd_width, comment, &status ))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        appendLogText(i18n("FITS header: Cannot find NAXIS1."));
        return solver_args;
    }

    if (fits_read_key(fptr, TINT, "NAXIS2", &fits_ccd_height, comment, &status ))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        appendLogText(i18n("FITS header: Cannot find NAXIS2."));
        return solver_args;
    }

    bool coord_ok = true;

    if (fits_read_key(fptr, TDOUBLE, "OBJCTRA", &ra, comment, &status ))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        coord_ok=false;
        appendLogText(i18n("FITS header: Cannot find OBJCTRA. Using current mount coordinates."));
        //return solver_args;
    }

    if (coord_ok && fits_read_key(fptr, TDOUBLE, "OBJCTDEC", &dec, comment, &status ))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        coord_ok=false;
        appendLogText(i18n("FITS header: Cannot find OBJCTDEC. Using current mount coordinates."));
        //return solver_args;
    }

    if (coord_ok == false)
    {
        ra  = telescopeCoord.ra0().Hours();
        dec = telescopeCoord.dec0().Degrees();
    }

    solver_args << "-3" << QString::number(ra*15.0) << "-4" << QString::number(dec) << "-5 15";

    if (fits_read_key(fptr, TINT, "FOCALLEN", &fits_focal_length, comment, &status ))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        appendLogText(i18n("FITS header: Cannot find FOCALLEN."));
        return solver_args;
    }

    if (fits_read_key(fptr, TDOUBLE, "PIXSIZE1", &fits_ccd_hor_pixel, comment, &status ))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        appendLogText(i18n("FITS header: Cannot find PIXSIZE1."));
        return solver_args;
    }

    if (fits_read_key(fptr, TDOUBLE, "PIXSIZE2", &fits_ccd_ver_pixel, comment, &status ))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        appendLogText(i18n("FITS header: Cannot find PIXSIZE2."));
        return solver_args;
    }

    fits_read_key(fptr, TINT, "XBINNING", &fits_binx, comment, &status );
    fits_read_key(fptr, TINT, "YBINNING", &fits_biny, comment, &status );

    // Calculate FOV
    fits_fov_x = 206264.8062470963552 * fits_ccd_width * fits_ccd_hor_pixel / 1000.0 / fits_focal_length * fits_binx;
    fits_fov_y = 206264.8062470963552 * fits_ccd_height * fits_ccd_ver_pixel / 1000.0 / fits_focal_length* fits_biny;

    fits_fov_x /= 60.0;
    fits_fov_y /= 60.0;

    // let's stretch the boundaries by 5%
    fov_lower = qMin(fits_fov_x, fits_fov_y);
    fov_upper = qMax(fits_fov_x, fits_fov_y);

    fov_lower *= 0.95;
    fov_upper *= 1.05;

    fov_low  = QString::number(fov_lower);
    fov_high = QString::number(fov_upper);

    solver_args << "-L" << fov_low << "-H" << fov_high << "-u" << "aw";


    return solver_args;
}

void Align::saveSettleTime()
{
    Options::setSettlingTime(delaySpin->value());
}

void Align::setCaptureStatus(CaptureState newState)
{
    switch (newState)
    {
    case CAPTURE_ALIGNING:
        QTimer::singleShot(Options::settlingTime(), this, SLOT(captureAndSolve()));
        break;

    default:
        break;
    }
}

void Align::showFITSViewer()
{
    FITSData *data = alignView->getImageData();

    if (data)
    {
        QUrl url = QUrl::fromLocalFile(data->getFilename());

        if (fv.isNull())
        {
            if (Options::singleWindowCapturedFITS())
                fv = KStars::Instance()->genericFITSViewer();
            else
            {
                fv = new FITSViewer(Options::independentWindowFITS() ? NULL : KStars::Instance());
                KStars::Instance()->getFITSViewersList().append(fv);
            }

            fv->addFITS(&url);
            FITSView *currentView = fv->getCurrentView();
            if (currentView)
                currentView->getImageData()->setAutoRemoveTemporaryFITS(false);
        }
        else
            fv->updateFITS(&url, 0);

        fv->show();
    }
}

void Align::toggleAlignWidgetFullScreen()
{
    if (alignWidget->parent() == NULL)
    {
        alignWidget->setParent(this);
        rightLayout->insertWidget(0, alignWidget);
        rightLayout->setStretch(0, 2);
        rightLayout->setStretch(1, 1);
        alignWidget->showNormal();
    }
    else
    {
        alignWidget->setParent(0);
        alignWidget->setWindowTitle(i18n("Align Frame"));
        alignWidget->setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
        alignWidget->showMaximized();
        alignWidget->show();
    }
}

void Align::startPAHProcess()
{
    pahStage = PAH_FIRST_CAPTURE;
    nothingR->setChecked(true);
    currentGotoMode = GOTO_NOTHING;

    if (Options::limitedResourcesMode())
        appendLogText(i18n("Warning: Equatorial Grid Lines will not be drawn due to limited resources mode."));

    appendLogText(i18n("Clearing mount Alignment Model..."));
    currentTelescope->clearAlignmentModel();

    PAHWidgets->setCurrentWidget(PAHFirstCapturePage);
}

void Align::restartPAHProcess()
{
    if (pahStage == PAH_IDLE)
        return;

    if (KMessageBox::questionYesNo(KStars::Instance(), i18n("Are you sure you want to restart the polar alignment process?"), i18n("Polar Alignment Assistant"),
                                   KStandardGuiItem::yes(), KStandardGuiItem::no(), "restart_PAA_process_dialog") == KMessageBox::No)
        return;

    pahStage = PAH_IDLE;

    PAHFirstCaptureB->setEnabled(true);
    PAHSecondCaptureB->setEnabled(true);
    PAHThirdCaptureB->setEnabled(true);
    PAHFirstRotateB->setEnabled(true);
    PAHSecondRotateB->setEnabled(true);
    PAHRefreshB->setEnabled(true);

    PAHWidgets->setCurrentWidget(PAHIntroPage);

    qDeleteAll(pahImageInfos);
    pahImageInfos.clear();

    correctionVector = QLineF();
    correctionOffset = QPointF();

    alignView->setCorrectionParams(correctionVector);
    alignView->setCorrectionOffset(correctionOffset);
    alignView->setRACircle(QVector3D());

    disconnect(alignView, SIGNAL(trackingStarSelected(int,int)), this, SLOT(setPAHCorrectionOffset(int,int)));
}

void Align::rotatePAH()
{
    double raDiff = (pahStage == PAH_FIRST_ROTATE) ? PAHFirstRotationSpin->value() : PAHSecondRotationSpin->value();
    bool westMeridian = (pahStage == PAH_FIRST_ROTATE) ? PAHFirstWestMeridianR->isChecked() : PAHSecondWestMeridianR->isChecked();

    // North
    if (hemisphere == NORTH_HEMISPHERE)
    {
        // West
        if (westMeridian)
            raDiff *= -1;
        // East
        else
            raDiff *= 1;
    }
    // South
    else
    {
        // West
        if (westMeridian)
            raDiff *= 1;
        // East
        else
            raDiff *= -1;
    }

    SkyPoint targetPAH;

    double newTelescopeRA = (telescopeCoord.ra().Degrees() + raDiff) / 15.0;

    targetPAH.setRA(newTelescopeRA);
    targetPAH.setDec(telescopeCoord.dec());

    // Convert to JNow
    //targetPAH.apparentCoord((long double) J2000, KStars::Instance()->data()->ut().djd());
    // Get horizontal coords
    //targetPAH.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

    currentTelescope->Slew(&targetPAH);

    appendLogText(i18n("Please wait until mount completes rotating to RA (%1) DE (%2)", targetPAH.ra().toHMSString(), targetPAH.dec().toDMSString()));
}

void Align::calculatePAHError()
{
    QVector3D RACircle;

    bool rc = findRACircle(RACircle);

    if (rc == false)
    {
        appendLogText(i18n("Failed to find a solution. Try again."));
        restartPAHProcess();
        return;
    }

    if (alignView->isEQGridShown() == false)
        alignView->toggleEQGrid();
    alignView->setRACircle(RACircle);

    FITSData *imageData = alignView->getImageData();

    QPointF RACenterPoint(RACircle.x(), RACircle.y());
    SkyPoint RACenter;
    rc = imageData->pixelToWCS(RACenterPoint, RACenter);

    if (rc == false)
    {
        appendLogText(i18n("Failed to find RA Axis center: %1.", imageData->getLastError()));
        return;
    }

    SkyPoint CP(0, (hemisphere == NORTH_HEMISPHERE) ? 90 : -90);

    RACenter.setRA(RACenter.ra0());
    RACenter.setDec(RACenter.dec0());
    dms polarError = RACenter.angularDistanceTo(&CP);

    if (Options::alignmentLogging())
    {
        qDebug() << "Alignment: RA Axis Circle X: " << RACircle.x() << " Y: " << RACircle.y() << " Radius: " << RACircle.z();
        qDebug() << "Alignment: RA Axis Location RA: " << RACenter.ra0().toHMSString() << "DE: " << RACenter.dec0().toDMSString();
        qDebug() << "Alignment: RA Axis Offset: " << polarError.toDMSString();
    }

    PAHErrorLabel->setText(polarError.toDMSString());

    correctionVector.setP1(celestialPolePoint);
    correctionVector.setP2(RACenterPoint);

    /*
    bool RAAxisInside = imageData->contains(RACenterPoint);
    bool CPPointInside= imageData->contains(celestialPolePoint);

    if (RAAxisInside == false && CPPointInside == false)
        appendLogText(i18n("Warning: Mount axis and celestial pole are outside the field of view. Correction vector may be inaccurate."));
    */

    alignView->setCorrectionParams(correctionVector);

    connect(alignView, SIGNAL(trackingStarSelected(int,int)), this, SLOT(setPAHCorrectionOffset(int,int)));
}

void Align::setPAHCorrectionOffset(int x, int y)
{
    correctionOffset.setX(x);
    correctionOffset.setY(y);

    alignView->setCorrectionOffset(correctionOffset);
}

void Align::setPAHCorrectionSelectionComplete()
{
    pahStage = PAH_PRE_REFRESH;

    PAHWidgets->setCurrentWidget(PAHRefreshPage);
}

void Align::startPAHRefreshProcess()
{
    pahStage = PAH_REFRESH;

    PAHRefreshB->setEnabled(false);

    // Hide EQ Grids if shown
    if (alignView->isEQGridShown())
        alignView->toggleEQGrid();

    // We for refresh, just capture really
    captureAndSolve();
}

void Align::setPAHRefreshComplete()
{
    abort();

    restartPAHProcess();
}

void Align::processPAHStage(double orientation, double ra, double dec, double pixscale)
{
    // Create temporary file to hold all WCS data
    QTemporaryFile tmpFile(QDir::tempPath() + "/fitswcsXXXXXX");
    tmpFile.setAutoRemove(false);
    tmpFile.open();
    QString newWCSFile = tmpFile.fileName();
    tmpFile.close();

    if (pahStage == PAH_FIND_CP)
    {
        setGOTOMode(GOTO_NOTHING);
        appendLogText(i18n("Mount is synced to celestial pole. You can now continue Polar Alignment Assistant procedure."));
        pahStage = PAH_FIRST_CAPTURE;
        return;
    }

    bool rc = false;

    if (pahStage == PAH_FIRST_CAPTURE)
    {
        rc = alignView->createWCSFile(newWCSFile, orientation, ra, dec, pixscale);
        if (rc == false)
        {
            appendLogText(i18n("Error creating WCS file: %1", alignView->getImageData()->getLastError()));
            return;
        }

        // Set First PAH Center
        PAHImageInfo *solution = new PAHImageInfo();
        solution->skyCenter.setRA0(alignCoord.ra0());
        solution->skyCenter.setDec0(alignCoord.dec0());
        solution->orientation = orientation;
        solution->pixelScale  = pixscale;

        pahImageInfos.append(solution);

        // Find Celestial pole location
        SkyPoint CP(0, (hemisphere == NORTH_HEMISPHERE) ? 90 : -90);

        FITSData *imageData = alignView->getImageData();
        QPointF pixelPoint, imagePoint;

        rc = imageData->wcsToPixel(CP, pixelPoint, imagePoint);

        // TODO check if pixelPoint is located TOO far from the current position as well
        // i.e. if X > Width * 2..etc
        if (rc == false)
        {
            appendLogText(i18n("Failed to process World Coordinate System: %1. Try again.", imageData->getLastError()));
            return;
        }

        // If celestial pole out of range, ask the user if they want to move to it
        if (pixelPoint.x() < (-1*imageData->getWidth()) || pixelPoint.x() > (imageData->getWidth()*2) ||
            pixelPoint.y() < (-1*imageData->getHeight())|| pixelPoint.y() > (imageData->getHeight()*2))
        {
            if (currentTelescope->canSync() && KMessageBox::questionYesNo(0, i18n("Celestial pole is located outside of the field of view. Would you like to sync and slew the telescope to the celestial pole? WARNING: Slewing near poles may cause your mount to end up in unsafe position. Proceed with caution.")) == KMessageBox::Yes)
            {
                pahStage = PAH_FIND_CP;
                targetCoord.setRA(KStarsData::Instance()->lst()->Hours());
                targetCoord.setDec(CP.dec().Degrees() > 0 ? 89.5 : -89.5);

                qDeleteAll(pahImageInfos);
                pahImageInfos.clear();

                setGOTOMode(GOTO_SLEW);
                Sync();
                return;
            }
            else
                appendLogText(i18n("Warning: Celestial pole is located outside the field of view. Move the mount closer to the celestial pole."));
        }

        pahStage = PAH_FIRST_ROTATE;
        PAHWidgets->setCurrentWidget(PAHFirstRotatePage);
    }
    else if (pahStage == PAH_SECOND_CAPTURE)
    {
        rc= alignView->createWCSFile(newWCSFile, orientation, ra, dec, pixscale);
        if (rc == false)
        {
            appendLogText(i18n("Error creating WCS file: %1", alignView->getImageData()->getLastError()));
            // Not critical error
            //return;
        }

        // Set 2nd PAH Center
        PAHImageInfo *solution = new PAHImageInfo();
        solution->skyCenter.setRA0(alignCoord.ra0());
        solution->skyCenter.setDec0(alignCoord.dec0());
        solution->orientation = orientation;
        solution->pixelScale  = pixscale;

        pahImageInfos.append(solution);

        pahStage = PAH_SECOND_ROTATE;
        PAHWidgets->setCurrentWidget(PAHSecondRotatePage);

        if (PAHAutoModeCheck->isChecked())
            PAHSecondRotateB->click();
    }
    else if (pahStage == PAH_THIRD_CAPTURE)
    {
        rc = alignView->createWCSFile(newWCSFile, orientation, ra, dec, pixscale);
        if (rc == false)
        {
            appendLogText(i18n("Error creating WCS file: %1", alignView->getImageData()->getLastError()));
            return;
        }

        // Set Third PAH Center
        PAHImageInfo *solution = new PAHImageInfo();
        solution->skyCenter.setRA0(alignCoord.ra0());
        solution->skyCenter.setDec0(alignCoord.dec0());
        solution->orientation = orientation;
        solution->pixelScale  = pixscale;

        pahImageInfos.append(solution);

        // Find Celstial pole location
        SkyPoint CP(0, (hemisphere == NORTH_HEMISPHERE) ? 90 : -90);

        FITSData *imageData = alignView->getImageData();
        QPointF imagePoint;

        imageData->wcsToPixel(CP, celestialPolePoint, imagePoint);

        // Now find pixel locations for all recorded center coordinates in the 3rd frame reference
        imageData->wcsToPixel(pahImageInfos[0]->skyCenter, pahImageInfos[0]->pixelCenter, imagePoint);
        imageData->wcsToPixel(pahImageInfos[1]->skyCenter, pahImageInfos[1]->pixelCenter, imagePoint);
        imageData->wcsToPixel(pahImageInfos[2]->skyCenter, pahImageInfos[2]->pixelCenter, imagePoint);

        if (Options::alignmentLogging())
        {
            qDebug() << "Alignment: P1 RA: " << pahImageInfos[0]->skyCenter.ra0().toHMSString() << "DE: " << pahImageInfos[0]->skyCenter.dec0().toDMSString();
            qDebug() << "Alignment: P2 RA: " << pahImageInfos[1]->skyCenter.ra0().toHMSString() << "DE: " << pahImageInfos[1]->skyCenter.dec0().toDMSString();
            qDebug() << "Alignment: P3 RA: " << pahImageInfos[2]->skyCenter.ra0().toHMSString() << "DE: " << pahImageInfos[2]->skyCenter.dec0().toDMSString();

            qDebug() << "Alignment: P1 X: " << pahImageInfos[0]->pixelCenter.x() << "Y: " << pahImageInfos[0]->pixelCenter.y();
            qDebug() << "Alignment: P2 X: " << pahImageInfos[1]->pixelCenter.x() << "Y: " << pahImageInfos[1]->pixelCenter.y();
            qDebug() << "Alignment: P3 X: " << pahImageInfos[2]->pixelCenter.x() << "Y: " << pahImageInfos[2]->pixelCenter.y();
        }

        // We have 3 points which uniquely defines a circle with its center representing the RA Axis
        // We have celestial pole location. So correction vector is just the vector between these two points
        calculatePAHError();

        pahStage = PAH_STAR_SELECT;
        PAHWidgets->setCurrentWidget(PAHCorrectionPage);
    }
}

void Align::syncGuideScopeCCDs()
{
    updateGuideScopeCCDs(FOVScopeCombo->currentIndex());
}

void Align::updateGuideScopeCCDs(int index)
{
    if (currentCCD == NULL)
        return;

    if (index ==  ISD::CCD::TELESCOPE_GUIDE && guideScopeCCDs.contains(currentCCD->getDeviceName()) == false)
        guideScopeCCDs.append(currentCCD->getDeviceName());
    else if (index == ISD::CCD::TELESCOPE_PRIMARY)
        guideScopeCCDs.removeOne(currentCCD->getDeviceName());

    if (guideScopeCCDs.contains(currentCCD->getDeviceName()))
        currentCCD->setTelescopeType(ISD::CCD::TELESCOPE_GUIDE);
    else
        currentCCD->setTelescopeType(ISD::CCD::TELESCOPE_PRIMARY);

    Options::setGuideScopeCCDs(guideScopeCCDs);

    syncTelescopeInfo();
}

// Function adapted from https://rosettacode.org/wiki/Circles_of_given_radius_through_two_points
Align::CircleSolution Align::findCircleSolutions(const QPointF & p1, const QPointF p2, double angle, QPair<QPointF, QPointF> &circleSolutions)
{
    QPointF solutionOne(1,1), solutionTwo(1,1);

    double radius = distance(p1, p2) / (dms::DegToRad * angle);

    if (p1 == p2)
    {
        if (angle == 0)
        {
            circleSolutions = qMakePair(p1, p2);
            appendLogText(i18n("Only one solution is found."));
            return ONE_CIRCLE_SOLUTION;
        }
        else
        {
            circleSolutions = qMakePair(solutionOne, solutionTwo);
            appendLogText(i18n("Infinite number of solutions found."));
            return INFINITE_CIRCLE_SOLUTION;
        }
    }

    QPointF center( p1.x()/2 + p2.x()/2, p1.y()/2 + p2.y()/2);

    double halfDistance = distance(center, p1);

    if (halfDistance > radius)
    {
        circleSolutions = qMakePair(solutionOne, solutionTwo);
        appendLogText(i18n("No solution is found. Points are too far away"));
        return NO_CIRCLE_SOLUTION;
    }

    if (halfDistance - radius == 0)
    {
        circleSolutions = qMakePair(center, solutionTwo);
        appendLogText(i18n("Only one solution is found."));
        return ONE_CIRCLE_SOLUTION;
    }

    double root = std::hypotf(radius, halfDistance) / distance (p1, p2);

    solutionOne.setX(center.x() + root * (p1.y() - p2.y()));
    solutionOne.setY(center.y() + root * (p2.x() - p1.x()));

    solutionTwo.setX(center.x() - root * (p1.y() - p2.y()));
    solutionTwo.setY(center.y() - root * (p2.x() - p1.x()));

    circleSolutions = qMakePair(solutionOne, solutionTwo);

    return TWO_CIRCLE_SOLUTION;
}

double Align::distance(const QPointF & p1, const QPointF & p2)
{
    return std::hypotf(p2.x()-p1.x(), p2.y()-p1.y());
}

bool Align::findRACircle(QVector3D & RACircle)
{
    bool rc = false;

    QPointF p1 = pahImageInfos[0]->pixelCenter;
    QPointF p2 = pahImageInfos[1]->pixelCenter;
    QPointF p3 = pahImageInfos[2]->pixelCenter;

    if (!isPerpendicular(p1, p2, p3))
        rc = calcCircle(p1, p2, p3, RACircle);
    else if (!isPerpendicular(p1, p3, p2) )
        rc = calcCircle(p1, p3, p2, RACircle);
    else if (!isPerpendicular(p2, p1, p3) )
        rc = calcCircle(p2, p1, p3, RACircle);
    else if (!isPerpendicular(p2, p3, p1) )
        rc = calcCircle(p2, p3, p1, RACircle);
    else if (!isPerpendicular(p3, p2, p1) )
        rc = calcCircle(p3, p2, p1, RACircle);
    else if (!isPerpendicular(p3, p1, p2) )
        rc = calcCircle(p3, p1, p2, RACircle);
    else {
        //TRACE("\nThe three pts are perpendicular to axis\n");
        return false;
         }

    return rc;
}

bool Align::isPerpendicular(const QPointF &p1, const QPointF &p2, const QPointF &p3)
// Check the given point are perpendicular to x or y axis
{
    double yDelta_a= p2.y() - p1.y();
    double xDelta_a= p2.x() - p1.x();
    double yDelta_b= p3.y() - p2.y();
    double xDelta_b= p3.x() - p2.x();


    // checking whether the line of the two pts are vertical
    if (fabs(xDelta_a) <= 0.000000001 && fabs(yDelta_b) <= 0.000000001)
    {
        //TRACE("The points are pependicular and parallel to x-y axis\n");
        return false;
    }

    if (fabs(yDelta_a) <= 0.0000001)
    {
        //TRACE(" A line of two point are perpendicular to x-axis 1\n");
        return true;
    }
    else if (fabs(yDelta_b) <= 0.0000001)
    {
        //TRACE(" A line of two point are perpendicular to x-axis 2\n");
        return true;
    }
    else if (fabs(xDelta_a)<= 0.000000001)
    {
        //TRACE(" A line of two point are perpendicular to y-axis 1\n");
        return true;
    }
    else if (fabs(xDelta_b)<= 0.000000001)
    {
        //TRACE(" A line of two point are perpendicular to y-axis 2\n");
        return true;
    }
    else
        return false;
}

bool Align::calcCircle(const QPointF &p1, const QPointF &p2, const QPointF &p3, QVector3D & RACircle)
{
    double yDelta_a= p2.y() - p1.y();
    double xDelta_a= p2.x() - p1.x();
    double yDelta_b= p3.y() - p2.y();
    double xDelta_b= p3.x() - p2.x();

    if (fabs(xDelta_a) <= 0.000000001 && fabs(yDelta_b) <= 0.000000001)
    {
        RACircle.setX(0.5*(p2.x() + p3.x()));
        RACircle.setY(0.5*(p1.y() + p2.y()));
        QPointF center(RACircle.x(), RACircle.y());
        RACircle.setZ(distance(center,p1));
        return true;
    }

    // IsPerpendicular() assure that xDelta(s) are not zero
    double aSlope=yDelta_a/xDelta_a; //
    double bSlope=yDelta_b/xDelta_b;
    if (fabs(aSlope-bSlope) <= 0.000000001)
    {	// checking whether the given points are colinear.
        //TRACE("The three ps are colinear\n");
        return false;
    }

    // calc center
    RACircle.setX((aSlope*bSlope*(p1.y() - p3.y()) + bSlope*(p1.x() + p2.x()) - aSlope*(p2.x()+p3.x()) )/(2* (bSlope-aSlope) ) );
    RACircle.setY(-1*(RACircle.x() - (p1.x()+p2.x())/2)/aSlope +  (p1.y()+p2.y())/2 );
    QPointF center(RACircle.x(), RACircle.y());

    RACircle.setZ(distance(center,p1));
    return true;
}

void Align::setMountStatus(ISD::Telescope::TelescopeStatus newState)
{
    switch (newState)
    {
    case ISD::Telescope::MOUNT_PARKING:
    case ISD::Telescope::MOUNT_SLEWING:
        solveB->setEnabled(false);
        loadSlewB->setEnabled(false);
        PAHFirstCaptureB->setEnabled(false);
        PAHSecondCaptureB->setEnabled(false);
        PAHThirdCaptureB->setEnabled(false);
        break;

    default:
        if (pi->isAnimated() == false)
        {
            solveB->setEnabled(true);
            loadSlewB->setEnabled(true);
        }

        PAHFirstCaptureB->setEnabled(true);
        PAHSecondCaptureB->setEnabled(true);
        PAHThirdCaptureB->setEnabled(true);
        break;    
    }
}

}
