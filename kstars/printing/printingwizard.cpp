/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "printingwizard.h"

#include <QStackedWidget>
#include <QPrinter>
#include <QStandardPaths>

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
#include "projections/projector.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "legend.h"
#include "shfovexporter.h"
#include "Options.h"
#include "kspaths.h"

PWizWelcomeUI::PWizWelcomeUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

PrintingWizard::PrintingWizard(QWidget *parent)
    : QDialog(parent), m_KStars(KStars::Instance()), m_FinderChart(nullptr), m_SkyObject(nullptr), m_FovType(FT_UNDEFINED),
      m_FovImageSize(QSize(500, 500)), m_ShBeginObject(nullptr), m_PointingShBegin(false), m_SwitchColors(false),
      m_RecapturingFov(false), m_RecaptureIdx(-1)
{
    m_Printer = new QPrinter(QPrinter::ScreenResolution);

    setupWidgets();
}

PrintingWizard::~PrintingWizard()
{
    // Clean up
    if (m_Printer)
    {
        delete m_Printer;
    }
    if (m_FinderChart)
    {
        delete m_FinderChart;
    }

    qDeleteAll(m_FovSnapshots);
}

void PrintingWizard::updateStepButtons()
{
    switch (m_WizardStack->currentIndex())
    {
        case PW_OBJECT_SELECTION: // object selection
        {
            nextB->setEnabled(m_SkyObject != nullptr);
            break;
        }
    }
}

void PrintingWizard::beginPointing()
{
    // If there is sky object already selected, center sky map around it
    if (m_SkyObject)
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

    if (m_ShBeginObject)
    {
        m_KStars->map()->setClickedObject(m_SkyObject);
        m_KStars->map()->slotCenter();
    }

    m_KStars->map()->setObjectPointingMode(true);
    hide();
}

void PrintingWizard::pointingDone(SkyObject *obj)
{
    if (m_PointingShBegin)
    {
        m_ShBeginObject = obj;
        m_WizFovShUI->setBeginObject(obj);
        m_PointingShBegin = false;
    }
    else
    {
        m_SkyObject = obj;
        m_WizObjectSelectionUI->setSkyObject(obj);
    }

    show();
}

void PrintingWizard::beginFovCapture()
{
    if (m_SkyObject)
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
    if (m_KStars->data()->getVisibleFOVs().isEmpty())
    {
        return;
    }

    QPixmap pixmap(m_FovImageSize);
    m_SimpleFovExporter.exportFov(m_KStars->data()->getVisibleFOVs().first(), &pixmap);
    if (m_WizFovConfigUI->isLegendEnabled())
    {
        // Set legend position, orientation and type
        Legend legend(m_WizFovConfigUI->getLegendOrientation(), m_WizFovConfigUI->getLegendPosition());
        legend.setType(m_WizFovConfigUI->getLegendType());

        // Check if alpha blending is enabled
        if (m_WizFovConfigUI->isAlphaBlendingEnabled())
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

    if (m_RecapturingFov)
    {
        delete m_FovSnapshots.at(m_RecaptureIdx);
        m_FovSnapshots.replace(m_RecaptureIdx, snapshot);
        m_KStars->map()->setFovCaptureMode(false);
        m_RecapturingFov = false;
        m_RecaptureIdx   = -1;
        fovCaptureDone();
    }
    else
    {
        m_FovSnapshots.append(snapshot);
    }
}

void PrintingWizard::fovCaptureDone()
{
    //Restore old color scheme if necessary
    //(if printing was aborted, the ColorScheme is still restored)
    if (m_SwitchColors)
    {
        m_KStars->loadColorScheme(m_PrevSchemeName);
        m_KStars->map()->forceUpdate();
    }

    if (m_RecapturingFov)
    {
        m_RecapturingFov = false;
        m_RecaptureIdx   = -1;
    }

    show();
}

void PrintingWizard::beginShFovCapture()
{
    if (!m_ShBeginObject)
    {
        return;
    }

    ShFovExporter exporter(this, KStars::Instance()->map());

    // Get selected FOV symbol
    double fovArcmin(0);
    foreach (FOV *fov, KStarsData::Instance()->getAvailableFOVs())
    {
        if (fov->name() == m_WizFovShUI->getFovName())
        {
            fovArcmin = qMin(fov->sizeX(), fov->sizeY());
            break;
        }
    }

    // Calculate path and check if it's not empty
    if (!exporter.calculatePath(*m_SkyObject, *m_ShBeginObject, fovArcmin / 60, m_WizFovShUI->getMaglim()))
    {
        KMessageBox::information(this,
                                 i18n("Star hopper returned empty path. We advise you to change star hopping settings "
                                      "or use manual capture mode."),
                                 i18n("Star hopper failed to find path"));
        return;
    }

    // If FOV shape should be overridden, do this now
    m_SimpleFovExporter.setFovShapeOverriden(m_WizFovConfigUI->isFovShapeOverriden());
    m_SimpleFovExporter.setFovSymbolDrawn(m_WizFovConfigUI->isFovShapeOverriden());

    // If color scheme should be switched, save previous scheme name and switch to "sky chart" color scheme
    m_SwitchColors   = m_WizFovConfigUI->isSwitchColorsEnabled();
    m_PrevSchemeName = m_KStars->data()->colorScheme()->fileName();
    if (m_SwitchColors)
    {
        m_KStars->loadColorScheme("chart.colors");
    }

    // Save previous FOV symbol names and switch to symbol selected by user
    QStringList prevFovNames = Options::fOVNames();
    Options::setFOVNames(QStringList(m_WizFovShUI->getFovName()));
    KStarsData::Instance()->syncFOV();
    if (KStarsData::Instance()->getVisibleFOVs().isEmpty())
    {
        return;
    }

    // Hide Printing Wizard
    hide();

    // Draw and export path
    exporter.exportPath();

    // Restore old color scheme if necessary
    if (m_SwitchColors)
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
    m_RecaptureIdx   = idx;

    // Begin FOV snapshot capture
    SkyPoint p = m_FovSnapshots.at(m_RecaptureIdx)->getCentralPoint();
    slewAndBeginCapture(&p, m_FovSnapshots.at(m_RecaptureIdx)->getFov());
}

void PrintingWizard::slotPrevPage()
{
    int currentIdx = m_WizardStack->currentIndex();
    switch (currentIdx)
    {
        case PW_FOV_BROWSE:
        {
            switch (m_FovType)
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
    switch (currentIdx)
    {
        case PW_FOV_TYPE:
        {
            m_FovType = m_WizFovTypeSelectionUI->getFovExportType();
            m_WizardStack->setCurrentIndex(PW_FOV_CONFIG);
            break;
        }

        case PW_FOV_CONFIG:
        {
            switch (m_FovType)
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
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    m_WizardStack = new QStackedWidget(this);

    setWindowTitle(i18nc("@title:window", "Printing Wizard"));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_WizardStack);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    nextB = new QPushButton(i18n("&Next >"));
    nextB->setToolTip(i18n("Go to next Wizard page"));
    nextB->setDefault(true);
    backB = new QPushButton(i18n("< &Back"));
    backB->setToolTip(i18n("Go to previous Wizard page"));
    backB->setEnabled(false);

    buttonBox->addButton(backB, QDialogButtonBox::ActionRole);
    buttonBox->addButton(nextB, QDialogButtonBox::ActionRole);

    connect(nextB, SIGNAL(clicked()), this, SLOT(slotNextPage()));
    connect(backB, SIGNAL(clicked()), this, SLOT(slotPrevPage()));

    // Create step widgets
    m_WizWelcomeUI          = new PWizWelcomeUI(m_WizardStack);
    m_WizObjectSelectionUI  = new PWizObjectSelectionUI(this, m_WizardStack);
    m_WizChartConfigUI      = new PWizChartConfigUI(this);
    m_WizFovTypeSelectionUI = new PWizFovTypeSelectionUI(this, m_WizardStack);
    m_WizFovConfigUI        = new PWizFovConfigUI(m_WizardStack);
    m_WizFovManualUI        = new PWizFovManualUI(this, m_WizardStack);
    m_WizFovShUI            = new PWizFovShUI(this, m_WizardStack);
    m_WizFovBrowseUI        = new PWizFovBrowseUI(this, m_WizardStack);
    m_WizChartContentsUI    = new PWizChartContentsUI(this, m_WizardStack);
    m_WizPrintUI            = new PWizPrintUI(this, m_WizardStack);

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
    if (bannerImg.load(KSPaths::locate(QStandardPaths::AppDataLocation, "wzstars.png")))
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

    backB->setEnabled(false);
}

void PrintingWizard::updateButtons()
{
    nextB->setEnabled(m_WizardStack->currentIndex() < m_WizardStack->count() - 1);
    backB->setEnabled(m_WizardStack->currentIndex() > 0);
}

void PrintingWizard::slewAndBeginCapture(SkyPoint *center, FOV *fov)
{
    if (!center)
    {
        return;
    }

    // If pointer to FOV is passed...
    if (fov)
    {
        // Switch to appropriate FOV symbol
        Options::setFOVNames(QStringList(fov->name()));
        m_KStars->data()->syncFOV();

        // Adjust map's zoom level
        double zoom = m_FovImageSize.width() > m_FovImageSize.height() ?
                          SimpleFovExporter::calculateZoomLevel(m_FovImageSize.width(), fov->sizeX()) :
                          SimpleFovExporter::calculateZoomLevel(m_FovImageSize.height(), fov->sizeY());
        m_KStars->map()->setZoomFactor(zoom);
    }

    m_SimpleFovExporter.setFovShapeOverriden(m_WizFovConfigUI->isFovShapeOverriden());
    m_SimpleFovExporter.setFovSymbolDrawn(m_WizFovConfigUI->isFovShapeOverriden());

    m_SwitchColors   = m_WizFovConfigUI->isSwitchColorsEnabled();
    m_PrevSchemeName = m_KStars->data()->colorScheme()->fileName();
    if (m_SwitchColors)
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
    if (m_FinderChart)
    {
        delete m_FinderChart;
    }
    m_FinderChart = new FinderChart;

    // Insert title and subtitle
    m_FinderChart->insertTitleSubtitle(m_WizChartConfigUI->titleEdit->text(), m_WizChartConfigUI->subtitleEdit->text());

    // Insert description
    if (!m_WizChartConfigUI->descriptionTextEdit->toPlainText().isEmpty())
    {
        m_FinderChart->insertDescription(m_WizChartConfigUI->descriptionTextEdit->toPlainText());
    }

    // Insert simple finder chart logging form
    if (m_WizChartContentsUI->isLoggingFormChecked())
    {
        LoggingForm chartLogger;
        chartLogger.createFinderChartLogger();
        m_FinderChart->insertSectionTitle(i18n("Logging Form"));
        m_FinderChart->insertLoggingForm(&chartLogger);
    }

    m_FinderChart->insertSectionTitle(i18n("Field of View Snapshots"));

    // Insert FOV images and descriptions
    for (int i = 0; i < m_FovSnapshots.size(); i++)
    {
        FOV *fov = m_FovSnapshots.at(i)->getFov();
        QString fovDescription =
            i18nc("%1 = FOV index, %2 = FOV count, %3 = FOV name, %4 = FOV X size, %5 = FOV Y size",
                  "FOV (%1/%2): %3 (%4' x %5')", QString::number(i + 1), QString::number(m_FovSnapshots.size()),
                  fov->name(), QString::number(fov->sizeX()), QString::number(fov->sizeY())) +
            "\n";
        m_FinderChart->insertImage(m_FovSnapshots.at(i)->getPixmap().toImage(),
                                   fovDescription + m_FovSnapshots.at(i)->getDescription(), true);
    }

    if (m_WizChartContentsUI->isGeneralTableChecked() || m_WizChartContentsUI->isPositionTableChecked() ||
        m_WizChartContentsUI->isRSTTableChecked() || m_WizChartContentsUI->isAstComTableChecked())
    {
        m_FinderChart->insertSectionTitle(i18n("Details About Object"));
        m_FinderChart->insertGeoTimeInfo(KStarsData::Instance()->ut(), KStarsData::Instance()->geo());
    }

    // Insert details table : general
    DetailsTable detTable;
    if (m_WizChartContentsUI->isGeneralTableChecked())
    {
        detTable.createGeneralTable(m_SkyObject);
        m_FinderChart->insertDetailsTable(&detTable);
    }

    // Insert details table : position
    if (m_WizChartContentsUI->isPositionTableChecked())
    {
        detTable.createCoordinatesTable(m_SkyObject, m_KStars->data()->ut(), m_KStars->data()->geo());
        m_FinderChart->insertDetailsTable(&detTable);
    }

    // Insert details table : RST
    if (m_WizChartContentsUI->isRSTTableChecked())
    {
        detTable.createRSTTAble(m_SkyObject, m_KStars->data()->ut(), m_KStars->data()->geo());
        m_FinderChart->insertDetailsTable(&detTable);
    }

    // Insert details table : Asteroid/Comet
    if (m_WizChartContentsUI->isAstComTableChecked())
    {
        detTable.createAsteroidCometTable(m_SkyObject);
        m_FinderChart->insertDetailsTable(&detTable);
    }
}
