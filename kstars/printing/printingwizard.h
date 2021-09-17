/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PRINTINGWIZARD_H
#define PRINTINGWIZARD_H

#include "ui_pwizwelcome.h"

#include "QDialog"
#include "simplefovexporter.h"
#include "fovsnapshot.h"
#include "QSize"

class KStars;
class PWizObjectSelectionUI;
class PWizFovBrowseUI;
class PWizFovTypeSelectionUI;
class PWizFovConfigUI;
class PWizFovManualUI;
class PWizFovShUI;
class PWizChartConfigUI;
class PWizChartContentsUI;
class PWizPrintUI;
class FinderChart;
class SkyObject;
class QStackedWidget;
class QPrinter;

/**
  * \class PWizWelcomeUI
  * \brief User interface for the first step of the Printing Wizard.
  * \author Rafał Kułaga
  */
class PWizWelcomeUI : public QFrame, public Ui::PWizWelcome
{
    Q_OBJECT
  public:
    /**
          * \brief Constructor.
          */
    explicit PWizWelcomeUI(QWidget *parent = nullptr);
};

/**
  * \class PrintingWizard
  * \brief Class representing Printing Wizard for KStars printed documents (currently only finder charts).
  * \author Rafał Kułaga
  */
class PrintingWizard : public QDialog
{
    Q_OBJECT
  public:
    /**
          * \brief Wizard steps enumeration.
          */
    enum WIZARD_STEPS
    {
        PW_WELCOME          = 0,
        PW_OBJECT_SELECTION = 1,
        PW_CHART_CONFIG     = 2,
        PW_FOV_TYPE         = 3,
        PW_FOV_CONFIG       = 4,
        PW_FOV_MANUAL       = 5,
        PW_FOV_SH           = 6,
        PW_FOV_BROWSE       = 7,
        PW_CHART_CONTENTS   = 8,
        PW_CHART_PRINT      = 9
    };

    /**
          * \brief FOV export method type enumeration.
          */
    enum FOV_TYPE
    {
        FT_MANUAL,
        FT_STARHOPPER,
        FT_UNDEFINED
    };

    /**
          * \brief Constructor.
          */
    explicit PrintingWizard(QWidget *parent = nullptr);

    /**
          * \brief Destructor.
          */
    ~PrintingWizard() override;

    /**
          * \brief Get used FOV export method.
          * \return Selected FOV export method.
          */
    FOV_TYPE getFovType() { return m_FovType; }

    /**
          * \brief Get printer used by Printing Wizard.
          * \return Used printer.
          */
    QPrinter *getPrinter() { return m_Printer; }

    /**
          * \brief Get used FinderChart document.
          * \return Used FinderChart document.
          */
    FinderChart *getFinderChart() { return m_FinderChart; }

    /**
          * \brief Get selected SkyObject, for which FinderChart is created.
          * \return Selected SkyObject.
          */
    SkyObject *getSkyObject() { return m_SkyObject; }

    /**
          * \brief Get FovSnapshot list.
          * \return Used FovSnapshot list.
          */
    QList<FovSnapshot *> *getFovSnapshotList() { return &m_FovSnapshots; }

    /**
          * \brief Get FOV snapshot image size.
          * \return Size of the FOV snapshot image.
          */
    QSize getFovImageSize() { return m_FovImageSize; }

    /**
          * \brief Get pointer to the SimpleFovExporter class instance.
          * \return Pointer to the SimpleFovExporter instance used by Printing Wizard.
          */
    SimpleFovExporter *getFovExporter() { return &m_SimpleFovExporter; }

    /**
          * \brief Get object at which star hopping will begin.
          * \return Source object for star hopper.
          */
    SkyObject *getShBeginObject() { return m_ShBeginObject; }

    /**
          * \brief Set SkyObject for which FinderChart is created.
          * \return SkyObject for which finder chart is created.
          */
    void setSkyObject(SkyObject *obj) { m_SkyObject = obj; }

    /**
          * \brief Set SkyObject at which star hopper will begin.
          * \return SkyObject at which star hopper will begin.
          */
    void setShBeginObject(SkyObject *obj) { m_ShBeginObject = obj; }

    /**
          * \brief Update Next/Previous step buttons.
          */
    void updateStepButtons();

    /**
          * \brief Set SkyMap to pointing mode and hide Printing Wizard.
          */
    void beginPointing();

    /**
          * \brief Enter star hopping begin pointing mode.
          */
    void beginShBeginPointing();

    /**
          * \brief Quit object pointing mode and set the pointed object.
          * \param obj Pointer to the SkyObject that was pointed on SkyMap.
          */
    void pointingDone(SkyObject *obj);

    /**
          * \brief Hide Printing Wizard and put SkyMap in FOV capture mode.
          */
    void beginFovCapture();

    /**
          * \brief Hide Printing Wizard and put SkyMap in FOV capture mode.
          * \param center Point at which SkyMap should be centered.
          * \param fov Field of view symbol, used to calculate zoom factor.
          */
    void beginFovCapture(SkyPoint *center, FOV *fov = nullptr);

    /**
          * \brief Capture current contents of FOV symbol.
          */
    void captureFov();

    /**
          * \brief Disable FOV capture mode.
          */
    void fovCaptureDone();

    /**
          * \brief Capture FOV snapshots using star hopper-based method.
          */
    void beginShFovCapture();

    /**
          * \brief Recapture FOV snapshot of passed index.
          * \param idx Index of the element to be recaptured.
          */
    void recaptureFov(int idx);

  private slots:
    /**
          * \brief Slot: go to the previous page of Printing Wizard.
          */
    void slotPrevPage();

    /**
          * \brief Slot: go to the next page of Printing Wizard.
          */
    void slotNextPage();

  private:
    /**
          * \brief Setup widget properties.
          */
    void setupWidgets();

    /**
          * \brief Update Previous/Next buttons.
          */
    void updateButtons();

    /**
          * \brief Private method, used to center SkyMap around passed SkyPoint, and enter FOV capture mode.
          * \param center Point at which SkyMap should be centered.
          * \param fov Field of view symbol, used to calculate zoom factor.
          */
    void slewAndBeginCapture(SkyPoint *center, FOV *fov = nullptr);

    /**
          * \brief Create finder chart using settings from all steps.
          */
    void createFinderChart();

    KStars *m_KStars;
    FinderChart *m_FinderChart;
    SkyObject *m_SkyObject;
    QStackedWidget *m_WizardStack;
    QPrinter *m_Printer;

    FOV_TYPE m_FovType;
    QSize m_FovImageSize;
    SimpleFovExporter m_SimpleFovExporter;

    QList<FovSnapshot *> m_FovSnapshots;

    SkyObject *m_ShBeginObject;
    bool m_PointingShBegin;

    bool m_SwitchColors;
    QString m_PrevSchemeName;

    bool m_RecapturingFov;
    int m_RecaptureIdx;

    QPushButton *nextB, *backB;

    PWizWelcomeUI *m_WizWelcomeUI;
    PWizObjectSelectionUI *m_WizObjectSelectionUI;
    PWizFovTypeSelectionUI *m_WizFovTypeSelectionUI;
    PWizFovConfigUI *m_WizFovConfigUI;
    PWizFovManualUI *m_WizFovManualUI;
    PWizFovShUI *m_WizFovShUI;
    PWizFovBrowseUI *m_WizFovBrowseUI;
    PWizChartConfigUI *m_WizChartConfigUI;
    PWizChartContentsUI *m_WizChartContentsUI;
    PWizPrintUI *m_WizPrintUI;
};

#endif // PRINTINGWIZARD_H
