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
#include "QSize"

class KStars;
class PWizObjectSelectionUI;
class PWizFovTypeSelectionUI;
class PWizFovManualUI;
class PWizChartConfigUI;
class PWizChartContentsUI;
class PWizPrintUI;
class FinderChart;
class KStarsDocument;
class SkyObject;
class QStackedWidget;
class QPrinter;

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
    enum FOV_TYPE
    {
        FT_MANUAL,
        FT_STARHOPPER,
        FT_UNDEFINED
    };

    PrintingWizard(QWidget *parent = 0);
    ~PrintingWizard();

    FOV_TYPE getFovType() { return m_FovType; }
    QPrinter* getPrinter() { return m_Printer; }
    FinderChart* getFinderChart() { return m_FinderChart; }
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

    KStars *m_KStars;
    FinderChart *m_FinderChart;
    SkyObject *m_SkyObject;
    QStackedWidget *m_WizardStack;
    QPrinter *m_Printer;

    FOV_TYPE m_FovType;
    QSize m_FovImageSize;
    SimpleFovExporter m_SimpleFovExporter;
    QList<QPixmap> m_FovImages;
    QList<QString> m_FovDescriptions;
    QList<QPixmap> m_Images;
    QList<QString> m_ImagesDescriptions;

    bool m_SwitchColors;
    QString m_PrevSchemeName;

    PWizWelcomeUI *m_WizWelcomeUI;
    PWizObjectSelectionUI *m_WizObjectSelectionUI;
    PWizFovTypeSelectionUI *m_WizFovTypeSelectionUI;
    PWizFovManualUI *m_WizFovManualUI;
    PWizChartConfigUI *m_WizChartConfigUI;
    PWizChartContentsUI *m_WizChartContentsUI;
    PWizPrintUI *m_WizPrintUI;
};

#endif // PRINTINGWIZARD_H
