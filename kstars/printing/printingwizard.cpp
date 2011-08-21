/***************************************************************************
                          printingwizard.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Tue Aug 2 2011
    copyright            : (C) 2011 by Rafał Kułaga
    email                : rl.kulaga@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "printingwizard.h"

#include "finderchart.h"
#include "loggingform.h"
#include "detailstable.h"
#include "pwizobjectselection.h"
#include "pwizchartconfig.h"
#include "pwizfovbrowse.h"
#include "pwizfovconfig.h"
#include "pwizfovtypeselection.h"
#include "pwizfovmanual.h"
#include "pwizfovsh.h"
#include "pwizchartcontents.h"
#include "pwizprint.h"
#include "kstars/projections/projector.h"
#include "kstars.h"
#include "kstars/kstarsdata.h"
#include "skymap.h"
#include "legend.h"
#include "QStackedWidget"
#include "QPrinter"
#include "kstandarddirs.h"
#include "shfovexporter.h"
#include "Options.h"

PWizWelcomeUI::PWizWelcomeUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

PrintingWizard::PrintingWizard(QWidget *parent) : KDialog(parent),
    m_KStars(KStars::Instance()), m_FinderChart(0), m_SkyObject(0),
    m_FovType(FT_UNDEFINED), m_FovImageSize(QSize(500, 500)), m_ShBeginObject(0),
    m_PointingShBegin(false), m_SwitchColors(false), m_RecapturingFov(false),
    m_RecaptureIdx(-1)
{
    m_Printer = new QPrinter(QPrinter::ScreenResolution);

    setupWidgets();
    setupConnections();
}

PrintingWizard::~PrintingWizard()
{
    // Clean up
    if(m_Printer) {
        delete m_Printer;
    } if(m_FinderChart) {
        delete m_FinderChart;
    }

    qDeleteAll(m_FovSnapshots);
}

void PrintingWizard::updateStepButtons()
{
    switch(m_WizardStack->currentIndex())
    {
    case PW_OBJECT_SELECTION: // object selection
        {
            enableButton(KDialog::User1, m_SkyObject);
            break;
        }
    }
}

void PrintingWizard::beginPointing()
{
    // If there is sky object already selected, center sky map around it
    if(m_SkyObject)
    {
        m_KStars->map()->setClickedObject(m_SkyObject);
        m_KStars->map()->slotCenter();
    }

    m_KStars->map()->setObjectPointingMode(true);
    hide();
}

void PrintingWizard::beginShBeginPointing()
{
    m_PointingShBegin = true;

    if(m_ShBeginObject)
    {
        m_KStars->map()->setClickedObject(m_SkyObject);
        m_KStars->map()->slotCenter();
    }

    m_KStars->map()->setObjectPointingMode(true);
    hide();
}

void PrintingWizard::pointingDone(SkyObject *obj)
{
    if(m_PointingShBegin) {
        m_ShBeginObject = obj;
        m_WizFovShUI->setBeginObject(obj);
        m_PointingShBegin = false;
    } else {
        m_SkyObject = obj;
        m_WizObjectSelectionUI->setSkyObject(obj);
    }

    show();
}

void PrintingWizard::beginFovCapture()
{
    if(m_SkyObject)
    {
        slewAndBeginCapture(m_SkyObject);
    }
}

void PrintingWizard::beginFovCapture(SkyPoint *center, FOV *fov)
{
    slewAndBeginCapture(center, fov);
}

void PrintingWizard::captureFov()
{
    if(m_KStars->data()->getVisibleFOVs().isEmpty())
    {
        return;
    }

    QPixmap pixmap(m_FovImageSize);
    m_SimpleFovExporter.exportFov(m_KStars->data()->getVisibleFOVs().first(), &pixmap);
    if(m_WizFovConfigUI->isLegendEnabled())
    {
        // Set legend position, orientation and type
        Legend legend(m_WizFovConfigUI->getLegendOrientation(), m_WizFovConfigUI->getLegendPosition());
        legend.setType(m_WizFovConfigUI->getLegendType());

        // Check if alpha blending is enabled
        if(m_WizFovConfigUI->isAlphaBlendingEnabled())
        {
            QColor bgColor = legend.getBgColor();
            bgColor.setAlpha(200);
            legend.setBgColor(bgColor);
        }

        // Paint legend
        legend.paintLegend(&pixmap);
    }
    FovSnapshot *snapshot = new FovSnapshot(pixmap, QString(), m_KStars->data()->getVisibleFOVs().first(),
                                            m_KStars->map()->getCenterPoint());

    if(m_RecapturingFov) {
        delete m_FovSnapshots.at(m_RecaptureIdx);
        m_FovSnapshots.replace(m_RecaptureIdx, snapshot);
        m_KStars->map()->setFovCaptureMode(false);
        m_RecapturingFov = false;
        m_RecaptureIdx = -1;
        fovCaptureDone();
    } else {
        m_FovSnapshots.append(snapshot);
    }
}

void PrintingWizard::fovCaptureDone()
{
    //Restore old color scheme if necessary
    //(if printing was aborted, the ColorScheme is still restored)
    if(m_SwitchColors)
    {
        m_KStars->loadColorScheme(m_PrevSchemeName);
        m_KStars->map()->forceUpdate();
    }

    if(m_RecapturingFov)
    {
        m_RecapturingFov = false;
        m_RecaptureIdx = -1;
    }

    show();
}

void PrintingWizard::beginShFovCapture()
{
    if(!m_ShBeginObject)
    {
        return;
    }

    ShFovExporter exporter(this, KStars::Instance()->map());

    // Get selected FOV symbol
    double fovArcmin(0);
    foreach(FOV *fov, KStarsData::Instance()->getAvailableFOVs())
    {
        if(fov->name() == m_WizFovShUI->getFovName())
        {
            fovArcmin = qMin(fov->sizeX(), fov->sizeY());
            break;
        }
    }

    // Calculate path and check if it's not empty
    if(!exporter.calculatePath(*m_SkyObject, *m_ShBeginObject, fovArcmin / 60, m_WizFovShUI->getMaglim()))
    {
        KMessageBox::information(this, i18n("Star hopper returned empty path. We advise you to change star hopping settings or use manual capture mode."),
                                 i18n("Star hopper failed to find path"));
        return;
    }

    // If FOV shape should be overriden, do this now
    m_SimpleFovExporter.setFovShapeOverriden(m_WizFovConfigUI->isFovShapeOverriden());
    m_SimpleFovExporter.setFovSymbolDrawn(m_WizFovConfigUI->isFovShapeOverriden());

    // If color scheme should be switched, save previous scheme name and switch to "sky chart" color scheme
    m_SwitchColors = m_WizFovConfigUI->isSwitchColorsEnabled();
    m_PrevSchemeName = m_KStars->data()->colorScheme()->fileName();
    if(m_SwitchColors)
    {
        m_KStars->loadColorScheme("chart.colors");
    }

    // Save previous FOV symbol names and switch to symbol selected by user
    QStringList prevFovNames = Options::fOVNames();
    Options::setFOVNames(QStringList(m_WizFovShUI->getFovName()));
    KStarsData::Instance()->syncFOV();
    if(KStarsData::Instance()->getVisibleFOVs().isEmpty())
    {
        return;
    }

    // Hide Printing Wizard
    hide();

    // Draw and export path
    exporter.exportPath();

    // Restore old color scheme if necessary
    if(m_SwitchColors)
    {
        m_KStars->loadColorScheme(m_PrevSchemeName);
        m_KStars->map()->forceUpdate();
    }

    // Update skymap
    m_KStars->map()->forceUpdate(true);

    // Restore previous FOV symbol names
    Options::setFOVNames(prevFovNames);
    KStarsData::Instance()->syncFOV();

    //FIXME: this is _dirty_ workaround to get PrintingWizard displayed in its previous position.
    QTimer::singleShot(50, this, SLOT(show()));
}

void PrintingWizard::recaptureFov(int idx)
{
    // Set recapturing flag and index of the FOV snapshot to replace
    m_RecapturingFov = true;
    m_RecaptureIdx = idx;

    // Begin FOV snapshot capture
    slewAndBeginCapture(&m_FovSnapshots.at(m_RecaptureIdx)->getCentralPoint(), m_FovSnapshots.at(m_RecaptureIdx)->getFov());
}

void PrintingWizard::slotPrevPage()
{
    int currentIdx = m_WizardStack->currentIndex();
    switch(currentIdx)
    {
    case PW_FOV_BROWSE:
        {
            switch(m_FovType)
            {
            case FT_MANUAL:
                {
                    m_WizardStack->setCurrentIndex(PW_FOV_MANUAL);
                    break;
                }

            case FT_STARHOPPER:
                {
                    m_WizardStack->setCurrentIndex(PW_FOV_SH);
                    break;
                }

            default:
                {
                    return;
                }
            }

            break;
        }

    case PW_FOV_SH:
        {
            m_WizardStack->setCurrentIndex(PW_FOV_CONFIG);
            break;
        }

    default:
        {
            m_WizardStack->setCurrentIndex(currentIdx - 1);
            break;
        }
    }

    updateStepButtons();
    updateButtons();
}

void PrintingWizard::slotNextPage()
{
    int currentIdx = m_WizardStack->currentIndex();
    switch(currentIdx)
    {
    case PW_FOV_TYPE:
        {
            m_FovType = m_WizFovTypeSelectionUI->getFovExportType();
            m_WizardStack->setCurrentIndex(PW_FOV_CONFIG);
            break;
        }

    case PW_FOV_CONFIG:
        {
            switch(m_FovType)
            {
            case FT_MANUAL:
                {
                    m_WizardStack->setCurrentIndex(PW_FOV_MANUAL);
                    break;
                }

            case FT_STARHOPPER:
                {
                    m_WizardStack->setCurrentIndex(PW_FOV_SH);
                    break;
                }

            default: // Undefined FOV type - do nothing
                {
                    return;
                }
            }

            break;
        }

    case PW_FOV_MANUAL:
        {
            m_WizardStack->setCurrentIndex(PW_FOV_BROWSE);
            break;
        }

    case PW_FOV_BROWSE:
        {
            m_WizChartContentsUI->entered();
            m_WizardStack->setCurrentIndex(PW_CHART_CONTENTS);
            break;
        }

    case PW_CHART_CONTENTS:
        {
            createFinderChart();
            m_WizardStack->setCurrentIndex(PW_CHART_PRINT);
            break;
        }

    default:
        {
            m_WizardStack->setCurrentIndex(currentIdx + 1);
        }
    }

    updateButtons();
    updateStepButtons();
}

void PrintingWizard::setupWidgets()
{
    m_WizardStack = new QStackedWidget(this);
    setMainWidget(m_WizardStack);

    setCaption(i18n("Printing Wizard"));

    setButtons(KDialog::User1 | KDialog::User2 | KDialog::Close);

    setButtonGuiItem(KDialog::User1, KGuiItem(i18n("&Next") + QString(" >"), QString(), i18n("Go to next Wizard page")));
    setButtonGuiItem(KDialog::User2, KGuiItem(QString("< ") + i18n("&Back"), QString(), i18n("Go to previous Wizard page")));

    // Create step widgets
    m_WizWelcomeUI = new PWizWelcomeUI(m_WizardStack);
    m_WizObjectSelectionUI = new PWizObjectSelectionUI(this, m_WizardStack);
    m_WizChartConfigUI = new PWizChartConfigUI(this);
    m_WizFovTypeSelectionUI = new PWizFovTypeSelectionUI(this, m_WizardStack);
    m_WizFovConfigUI = new PWizFovConfigUI(m_WizardStack);
    m_WizFovManualUI = new PWizFovManualUI(this, m_WizardStack);
    m_WizFovShUI = new PWizFovShUI(this, m_WizardStack);
    m_WizFovBrowseUI = new PWizFovBrowseUI(this, m_WizardStack);
    m_WizChartContentsUI = new PWizChartContentsUI(this, m_WizardStack);
    m_WizPrintUI = new PWizPrintUI(this, m_WizardStack);

    // Add step widgets to m_WizardStack
    m_WizardStack->addWidget(m_WizWelcomeUI);
    m_WizardStack->addWidget(m_WizObjectSelectionUI);
    m_WizardStack->addWidget(m_WizChartConfigUI);
    m_WizardStack->addWidget(m_WizFovTypeSelectionUI);
    m_WizardStack->addWidget(m_WizFovConfigUI);
    m_WizardStack->addWidget(m_WizFovManualUI);
    m_WizardStack->addWidget(m_WizFovShUI);
    m_WizardStack->addWidget(m_WizFovBrowseUI);
    m_WizardStack->addWidget(m_WizChartContentsUI);
    m_WizardStack->addWidget(m_WizPrintUI);

    // Set banner images for steps
    QPixmap bannerImg;
    if(bannerImg.load(KStandardDirs::locate("appdata", "wzstars.png")))
    {
        m_WizWelcomeUI->banner->setPixmap(bannerImg);
        m_WizObjectSelectionUI->banner->setPixmap(bannerImg);
        m_WizChartConfigUI->banner->setPixmap(bannerImg);
        m_WizFovTypeSelectionUI->banner->setPixmap(bannerImg);
        m_WizFovConfigUI->banner->setPixmap(bannerImg);
        m_WizFovManualUI->banner->setPixmap(bannerImg);
        m_WizFovShUI->banner->setPixmap(bannerImg);
        m_WizFovBrowseUI->banner->setPixmap(bannerImg);
        m_WizChartContentsUI->banner->setPixmap(bannerImg);
        m_WizPrintUI->banner->setPixmap(bannerImg);
    }

    enableButton(KDialog::User2, false);
}

void PrintingWizard::setupConnections()
{
    connect(this, SIGNAL(user1Clicked()), this, SLOT(slotNextPage()));
    connect(this, SIGNAL(user2Clicked()), this, SLOT(slotPrevPage()));
}

void PrintingWizard::updateButtons()
{
    enableButton(KDialog::User1, m_WizardStack->currentIndex() < m_WizardStack->count() - 1);
    enableButton(KDialog::User2, m_WizardStack->currentIndex() > 0);
}

void PrintingWizard::slewAndBeginCapture(SkyPoint *center, FOV *fov)
{
    if(!center)
    {
        return;
    }

    // If pointer to FOV is passed...
    if(fov)
    {
        // Switch to appropriate FOV symbol
        Options::setFOVNames(QStringList(fov->name()));
        m_KStars->data()->syncFOV();

        // Adjust map's zoom level
        double zoom = m_FovImageSize.width() > m_FovImageSize.height() ? SimpleFovExporter::calculateZoomLevel(m_FovImageSize.width(), fov->sizeX()) :
                                                                         SimpleFovExporter::calculateZoomLevel(m_FovImageSize.height(), fov->sizeY());
        m_KStars->map()->setZoomFactor(zoom);
    }

    m_SimpleFovExporter.setFovShapeOverriden(m_WizFovConfigUI->isFovShapeOverriden());
    m_SimpleFovExporter.setFovSymbolDrawn(m_WizFovConfigUI->isFovShapeOverriden());

    m_SwitchColors = m_WizFovConfigUI->isSwitchColorsEnabled();
    m_PrevSchemeName = m_KStars->data()->colorScheme()->fileName();
    if(m_SwitchColors)
    {
        m_KStars->loadColorScheme("chart.colors");
    }

    m_KStars->hideAllFovExceptFirst();
    m_KStars->map()->setClickedPoint(center);
    m_KStars->map()->slotCenter();
    m_KStars->map()->setFovCaptureMode(true);
    hide();
}

void PrintingWizard::createFinderChart()
{
    // Delete old (if needed) and create new FinderChart
    if(m_FinderChart)
    {
        delete m_FinderChart;
    }
    m_FinderChart = new FinderChart;

    // Insert title and subtitle
    m_FinderChart->insertTitleSubtitle(m_WizChartConfigUI->titleEdit->text(), m_WizChartConfigUI->subtitleEdit->text());

    // Insert description
    if(!m_WizChartConfigUI->descriptionTextEdit->toPlainText().isEmpty())
    {
        m_FinderChart->insertDescription(m_WizChartConfigUI->descriptionTextEdit->toPlainText());
    }

    // Insert simple finder chart logging form
    if(m_WizChartContentsUI->isLoggingFormChecked())
    {
        LoggingForm chartLogger;
        chartLogger.createFinderChartLogger();
        m_FinderChart->insertLoggingForm(&chartLogger);
    }

    // Insert FOV images and descriptions
    for(int i = 0; i < m_FovSnapshots.size(); i++)
    {
        m_FinderChart->insertImage(m_FovSnapshots.at(i)->getPixmap().toImage(), m_FovSnapshots.at(i)->getDescription(), false);
    }

    // Insert details table : general
    DetailsTable detTable;
    if(m_WizChartContentsUI->isGeneralTableChecked())
    {
        detTable.createGeneralTable(m_SkyObject);
        m_FinderChart->insertDetailsTable(&detTable);
    }

    // Insert details table : position
    if(m_WizChartContentsUI->isPositionTableChecked())
    {
        detTable.createCoordinatesTable(m_SkyObject, m_KStars->data()->ut(), m_KStars->data()->geo());
        m_FinderChart->insertDetailsTable(&detTable);
    }

    // Insert details table : RST
    if(m_WizChartContentsUI->isRSTTableChecked())
    {
        detTable.createRSTTAble(m_SkyObject, m_KStars->data()->ut(), m_KStars->data()->geo());
        m_FinderChart->insertDetailsTable(&detTable);
    }

    // Insert details table : Asteroid/Comet
    if(m_WizChartContentsUI->isAstComTableChecked())
    {
        detTable.createAsteroidCometTable(m_SkyObject);
        m_FinderChart->insertDetailsTable(&detTable);
    }
}
