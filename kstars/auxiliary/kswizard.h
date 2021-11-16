/*
    SPDX-FileCopyrightText: 2004 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>
#include <QDialogButtonBox>
#include <QProcess>
#include <QPlainTextEdit>
#include <qsystemdetection.h>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#include "ui_wizwelcome.h"
#include "ui_wizlocation.h"
#include "ui_wizdownload.h"
#include "ui_wizdata.h"
#include "QProgressIndicator.h"

class GeoLocation;
class QStackedWidget;

class WizWelcomeUI : public QFrame, public Ui::WizWelcome
{
        Q_OBJECT

    public:
        explicit WizWelcomeUI(QWidget *parent = nullptr);
};

class WizLocationUI : public QFrame, public Ui::WizLocation
{
        Q_OBJECT

    public:
        explicit WizLocationUI(QWidget *parent = nullptr);
};

class WizDataUI : public QFrame, public Ui::WizData
{
        Q_OBJECT

    public:
        explicit WizDataUI(QWidget *parent = nullptr);
};

class WizDownloadUI : public QFrame, public Ui::WizDownload
{
        Q_OBJECT

    public:
        explicit WizDownloadUI(QWidget *parent = nullptr);
};

/**
 * @class KSWizard
 * The Startup Wizard will be automatically opened when KStars runs
 * for the first time.  It allows the user to set up some basic parameters:
 * @li Geographic Location
 * @li Download extra data files
 *
 * @author Jason Harris
 * @version 1.0
 */
class KSWizard : public QDialog
{
        Q_OBJECT
    public:
        /**
         * Constructor
         * @param parent Pointer to the parent widget
         */
        explicit KSWizard(QWidget *parent = nullptr);

        // Do NOT delete members of filteredCityList! They are not created by KSWizard.
        ~KSWizard() override = default;

        /** @return pointer to the geographic location selected by the user */
        const GeoLocation *geo() const
        {
            return Geo;
        }

    private slots:
        void slotNextPage();
        void slotPrevPage();

        /**
         * Set the geo pointer to the user's selected city, and display
         * its longitude and latitude in the window.
         * @note called when the highlighted city in the list box changes
         */
        void slotChangeCity();

        /**
         * Display only those cities which meet the user's search criteria in the city list box.
         * @note called when one of the name filters is modified
         */
        void slotFilterCities();

        void slotDownload();

        void slotInstallGSC();

        void slotExtractGSC();

        void slotGSCInstallerFinished();

        void slotUpdateDataButtons();

        void slotOpenOrCopyKStarsDataDirectory();

    private:
        /**
         * @short Initialize the geographic location page.
         * Populate the city list box, and highlight the current location in the list.
         */
        void initGeoPage();

        /** @short set enabled/disable state of Next/Prev buttins based on current page */
        void setButtonsEnabled();

#ifdef Q_OS_OSX

        bool GSCExists();
        bool dataDirExists();


        QProgressIndicator *gscMonitor { nullptr };
        QTimer *downloadMonitor { nullptr };
        QString gscZipPath;

#endif

        QStackedWidget *wizardStack { nullptr };
        WizWelcomeUI *welcome { nullptr };
        WizLocationUI *location { nullptr };
        WizDataUI *data { nullptr };
        QPushButton *nextB { nullptr };
        QPushButton *backB { nullptr };
        QPushButton *completeB { nullptr };
        QDialogButtonBox *buttonBox { nullptr };
        GeoLocation *Geo { nullptr };
        QList<GeoLocation *> filteredCityList;
};
