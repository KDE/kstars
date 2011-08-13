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
#include "pwiztypeselection.h"
#include "pwizobjectselection.h"
#include "pwizchartconfig.h"
#include "pwizfovtypeselection.h"
#include "pwizfovmanual.h"
#include "pwizchartcontents.h"
#include "pwizprint.h"
#include "kstars.h"
#include "kstars/kstarsdata.h"
#include "skymap.h"
#include "legend.h"
#include "QStackedWidget"
#include <kstandarddirs.h>

PWizWelcomeUI::PWizWelcomeUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

PrintingWizard::PrintingWizard(QWidget *parent) : KDialog(parent),
    m_KStars(KStars::Instance()), m_PrintoutType(PT_UNDEFINED),
    m_FovType(FT_UNDEFINED), m_FinderChart(0), m_LoggingForm(0), m_Document(0),
    m_FovImageSize(QSize(0, 0)), m_SkyObject(0), m_SwitchColors(false)
{
    m_Printer = new QPrinter(QPrinter::ScreenResolution);

    setupWidgets();
    setupConnections();
}

PrintingWizard::~PrintingWizard()
{}

KStarsDocument* PrintingWizard::getDocument()
{
    switch(m_PrintoutType)
    {
    case PT_FINDER_CHART:
        {
            return m_FinderChart;
            break;
        }

    case PT_LOGGING_FORM:
        {
            return m_LoggingForm;
            break;
        }

    default:
        {
            return 0;
        }
    }
}

void PrintingWizard::restart()
{
    m_FovImages.clear();
    m_Images.clear();

    m_WizardStack->setCurrentIndex(0);
}

void PrintingWizard::pointingDone(SkyObject *obj)
{
    m_WizObjectSelectionUI->setSkyObject(obj);
    show();
}

void PrintingWizard::beginFovCapture(bool switchColors)
{
    m_SwitchColors = switchColors;
    m_PrevSchemeName = m_KStars->data()->colorScheme()->fileName();
    if(m_SwitchColors)
    {
        m_KStars->loadColorScheme("chart.colors");
    }

    m_KStars->hideAllFovExceptFirst();
    m_KStars->map()->setClickedObject(m_SkyObject);
    m_KStars->map()->slotCenter();
    m_KStars->map()->setFovCaptureMode(true);
    hide();
}

void PrintingWizard::captureFov()
{
    QPixmap pmap(500, 500);
    m_SimpleFovExporter.exportFov(m_KStars->data()->getVisibleFOVs().first(), &pmap);

    m_FovImages.append(pmap);
    m_FovDescriptions.append(QString::number(m_FovImages.size()));
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

    show();
}

void PrintingWizard::slotPrevPage()
{
    m_WizardStack->setCurrentIndex(m_WizardStack->currentIndex() - 1);
    updateButtons();
}

void PrintingWizard::slotNextPage()
{
    int currentIdx = m_WizardStack->currentIndex();
    switch(currentIdx)
    {
    case 1: // Printout type selection screen
        {
            m_PrintoutType = m_WizTypeSelectionUI->getSelectedPrintoutType();

            switch(m_PrintoutType)
            {
            case PT_FINDER_CHART:
                {
                    m_WizardStack->setCurrentIndex(currentIdx + 1);
                    break;
                }

            case PT_LOGGING_FORM:
                {
                    m_WizardStack->setCurrentIndex(0);
                    break;
                }

            default: // Undefined printout type - do nothing
                {
                    return;
                }
            }

            break;
        }

    case 4: // FOV type selection screen
        {
            m_FovType = m_WizFovTypeSelectionUI->getFovExportType();

            switch(m_FovType)
            {
            case FT_MANUAL:
                {
                    m_WizardStack->setCurrentIndex(currentIdx + 1);
                    break;
                }

            case FT_STARHOPPER:
                {
                    m_WizardStack->setCurrentIndex(0);
                    break;
                }

            default: // Undefined FOV type - do nothing
                {
                    return;
                }
            }

            break;
        }

    case 6:
        {
            createDocument();
            m_WizardStack->setCurrentIndex(currentIdx + 1);
            break;
        }

    default:
        {
            m_WizardStack->setCurrentIndex(currentIdx + 1);
        }
    }

    updateButtons();
}

void PrintingWizard::setupWidgets()
{
    m_WizardStack = new QStackedWidget(this);
    setMainWidget(m_WizardStack);

    setCaption(i18n("Printing Wizard"));

    setButtons(KDialog::User1 | KDialog::User2 | KDialog::Cancel);

    setButtonGuiItem(KDialog::User1, KGuiItem(i18n("&Next") + QString(" >"), QString(), i18n("Go to next Wizard page")));
    setButtonGuiItem(KDialog::User2, KGuiItem(QString("< ") + i18n("&Back"), QString(), i18n("Go to previous Wizard page")));

    m_WizWelcomeUI = new PWizWelcomeUI(m_WizardStack);
    m_WizTypeSelectionUI = new PWizTypeSelectionUI(m_WizardStack);
    m_WizObjectSelectionUI = new PWizObjectSelectionUI(this, m_WizardStack);
    m_WizChartConfigUI = new PWizChartConfigUI(this, m_WizardStack);
    m_WizFovTypeSelectionUI = new PWizFovTypeSelectionUI(this, m_WizardStack);
    m_WizFovManualUI = new PWizFovManualUI(this, m_WizardStack);
    m_WizChartContentsUI = new PWizChartContentsUI(this, m_WizardStack);
    m_WizPrintUI = new PWizPrintUI(this, m_WizardStack);

    m_WizardStack->addWidget(m_WizWelcomeUI);
    m_WizardStack->addWidget(m_WizTypeSelectionUI);
    m_WizardStack->addWidget(m_WizObjectSelectionUI);
    m_WizardStack->addWidget(m_WizChartConfigUI);
    m_WizardStack->addWidget(m_WizFovTypeSelectionUI);
    m_WizardStack->addWidget(m_WizFovManualUI);
    m_WizardStack->addWidget(m_WizChartContentsUI);
    m_WizardStack->addWidget(m_WizPrintUI);

    QPixmap bannerImg;
    if(bannerImg.load(KStandardDirs::locate("appdata", "wzstars.png")))
    {
        m_WizWelcomeUI->banner->setPixmap(bannerImg);
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

void PrintingWizard::createDocument()
{
    switch(m_PrintoutType)
    {
    case PT_FINDER_CHART:
        {
            createFinderChart();
            break;
        }

    case PT_LOGGING_FORM:
        {
            createLoggingForm();
            break;
        }

    default:
        {
            return;
        }
    }
}

void PrintingWizard::createFinderChart()
{
    m_FinderChart = new FinderChart;

    m_FinderChart->insertTitleSubtitle(m_WizChartConfigUI->titleEdit->text(), m_WizChartConfigUI->subtitleEdit->text());

    if(!m_WizChartConfigUI->descriptionTextEdit->toPlainText().isEmpty())
    {
        m_FinderChart->insertDescription(m_WizChartConfigUI->descriptionTextEdit->toPlainText());
    }

    if(m_WizChartConfigUI->isGeoTimeChecked())
    {
        m_FinderChart->insertGeoTimeInfo(m_KStars->data()->ut(), m_KStars->data()->geo());
    }

    if(m_WizChartConfigUI->isSymbolListChecked())
    {
        Legend legend(Legend::LO_HORIZONTAL, Legend::LP_UPPER_LEFT);
        legend.setType(Legend::LT_SYMBOLS_ONLY);
        legend.setFont(QFont("Times", 8));
        legend.setSymbolSize(12);
        legend.setXSymbolSpacing(80);
        QImage legendImg(legend.calculateSize(), QImage::Format_ARGB32);
        legend.paintLegend(&legendImg);
        m_FinderChart->insertImage(legendImg, QString());
    }

    if(m_WizChartContentsUI->isLoggingFormChecked())
    {
        LoggingForm chartLogger;
        chartLogger.createFinderChartLogger();
        m_FinderChart->insertLoggingForm(&chartLogger);
    }

    for(int i = 0; i < m_FovImages.size(); i++)
    {
        m_FinderChart->insertImage(m_FovImages.at(i).toImage(), m_FovDescriptions.at(i), false);
    }

    DetailsTable detTable;
    if(m_WizChartContentsUI->isGeneralTableChecked())
    {
        detTable.createGeneralTable(m_SkyObject);
        m_FinderChart->insertDetailsTable(&detTable);
    }

    if(m_WizChartContentsUI->isPositionTableChecked())
    {
        detTable.createCoordinatesTable(m_SkyObject, m_KStars->data()->ut(), m_KStars->data()->geo());
        m_FinderChart->insertDetailsTable(&detTable);
    }

    if(m_WizChartContentsUI->isRSTTableChecked())
    {
        detTable.createRSTTAble(m_SkyObject, m_KStars->data()->ut(), m_KStars->data()->geo());
        m_FinderChart->insertDetailsTable(&detTable);
    }

    if(m_WizChartContentsUI->isAstComTableChecked())
    {
        detTable.createRSTTAble(m_SkyObject, m_KStars->data()->ut(), m_KStars->data()->geo());
        m_FinderChart->insertDetailsTable(&detTable);
    }
}

void PrintingWizard::createLoggingForm()
{

}
