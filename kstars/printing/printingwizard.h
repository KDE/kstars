/***************************************************************************
                          printingwizard.h  -  K Desktop Planetarium
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

#ifndef PRINTINGWIZARD_H
#define PRINTINGWIZARD_H

#include "ui_pwizwelcome.h"

#include "kdialog.h"
#include "simplefovexporter.h"
#include "QPrinter"
#include "QSize"

class KStars;
class PWizTypeSelectionUI;
class PWizObjectSelectionUI;
class PWizFovTypeSelectionUI;
class PWizFovManualUI;
class PWizChartConfigUI;
class PWizChartContentsUI;
class PWizPrintUI;
class LoggingForm;
class FinderChart;
class KStarsDocument;
class SkyObject;
class QStackedWidget;

// Printing Wizard welcome screen
class PWizWelcomeUI : public QFrame, public Ui::PWizWelcome
{
    Q_OBJECT
public:
    PWizWelcomeUI(QWidget *parent = 0);
};

class PrintingWizard : public KDialog
{
    Q_OBJECT

public:
    enum PRINTOUT_TYPE
    {
        PT_FINDER_CHART,
        PT_LOGGING_FORM,
        PT_UNDEFINED
    };

    enum FOV_TYPE
    {
        FT_MANUAL,
        FT_STARHOPPER,
        FT_UNDEFINED
    };

    PrintingWizard(QWidget *parent = 0);
    ~PrintingWizard();

    PRINTOUT_TYPE getPrintoutType() { return m_PrintoutType; }
    FOV_TYPE getFovType() { return m_FovType; }
    QPrinter* getPrinter() { return m_Printer; }
    FinderChart* getFinderChart() { return m_FinderChart; }
    LoggingForm* getLoggingForm() { return m_LoggingForm; }
    KStarsDocument* getDocument();
    SkyObject* getSkyObject() { return m_SkyObject; }

    void setPrinter(QPrinter *printer) { m_Printer = printer; }
    void setSkyObject(SkyObject *obj) { m_SkyObject = obj; }

    void restart();
    void pointingDone(SkyObject *obj);

    void beginFovCapture(bool switchColors);
    void captureFov();
    void fovCaptureDone();

private slots:
    void slotPrevPage();
    void slotNextPage();

private:
    void setupWidgets();
    void setupConnections();
    void updateButtons();

    void createDocument();
    void createFinderChart();
    void createLoggingForm();

    KStars *m_KStars;
    QStackedWidget *m_WizardStack;

    QPrinter *m_Printer;

    PRINTOUT_TYPE m_PrintoutType;
    FOV_TYPE m_FovType;
    FinderChart *m_FinderChart;
    LoggingForm *m_LoggingForm;
    KStarsDocument *m_Document;

    QSize m_FovImageSize;
    SkyObject *m_SkyObject;
    SimpleFovExporter m_SimpleFovExporter;
    QList<QPixmap> m_FovImages;
    QList<QString> m_FovDescriptions;
    QList<QPixmap> m_Images;
    QList<QString> m_ImagesDescriptions;

    bool m_SwitchColors;
    QString m_PrevSchemeName;

    PWizWelcomeUI *m_WizWelcomeUI;
    PWizTypeSelectionUI *m_WizTypeSelectionUI;
    PWizObjectSelectionUI *m_WizObjectSelectionUI;
    PWizFovTypeSelectionUI *m_WizFovTypeSelectionUI;
    PWizFovManualUI *m_WizFovManualUI;
    PWizChartConfigUI *m_WizChartConfigUI;
    PWizChartContentsUI *m_WizChartContentsUI;
    PWizPrintUI *m_WizPrintUI;
};

#endif // PRINTINGWIZARD_H
