/***************************************************************************
                          kstarsactions.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Feb 25 2002
    copyright            : (C) 2002 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// This file contains function definitions for Actions declared in kstars.h

#include "kstars.h"

#include "imageexporter.h"
#include "kstarsdata.h"
#include "kstars_debug.h"
#include "ksnotification.h"
#include "kswizard.h"
#include "Options.h"
#include "skymap.h"
#include "dialogs/exportimagedialog.h"
#include "dialogs/finddialog.h"
#include "dialogs/focusdialog.h"
#include "dialogs/fovdialog.h"
#include "dialogs/locationdialog.h"
#include "dialogs/timedialog.h"
#include "oal/execute.h"
#include "oal/equipmentwriter.h"
#include "oal/observeradd.h"
#include "options/opsadvanced.h"
#include "options/opscatalog.h"
#include "options/opscolors.h"
#include "options/opsguides.h"
#include "options/opssatellites.h"
#include "options/opssolarsystem.h"
#include "options/opssupernovae.h"
#include "printing/printingwizard.h"
#include "projections/projector.h"
#include "skycomponents/asteroidscomponent.h"
#include "skycomponents/cometscomponent.h"
#include "skycomponents/satellitescomponent.h"
#include "skycomponents/skymapcomposite.h"
#include "skycomponents/solarsystemcomposite.h"
#include "skycomponents/supernovaecomponent.h"
#include "tools/adddeepskyobject.h"
#include "tools/altvstime.h"
#include "tools/astrocalc.h"
#include "tools/eyepiecefield.h"
#include "tools/flagmanager.h"
#include "tools/horizonmanager.h"
#include "tools/observinglist.h"
#include "tools/planetviewer.h"
#include "tools/jmoontool.h"
#include "tools/scriptbuilder.h"
#include "tools/skycalendar.h"
#include "tools/wutdialog.h"
#include "tools/polarishourangle.h"
#include "tools/whatsinteresting/wiequipsettings.h"
#include "tools/whatsinteresting/wilpsettings.h"
#include "tools/whatsinteresting/wiview.h"
#include "hips/hipsmanager.h"

#ifdef HAVE_INDI
#include <basedevice.h>
//#include "indi/telescopewizardprocess.h"
#include "indi/opsindi.h"
#include "indi/drivermanager.h"
#include "indi/guimanager.h"
#include "indi/indilistener.h"
#endif

#ifdef HAVE_CFITSIO
#include "fitsviewer/fitsviewer.h"
#include "fitsviewer/opsfits.h"
#ifdef HAVE_INDI
#include "ekos/manager.h"
#include "ekos/opsekos.h"
#endif
#endif

#include "xplanet/opsxplanet.h"

#ifdef HAVE_NOTIFYCONFIG
#include <KNotifyConfigWidget>
#endif
#include <KActionCollection>
#include <KActionMenu>
#include <KTipDialog>
#include <KToggleAction>
#include <kns3/downloaddialog.h>

#include <QQuickWindow>
#include <QQuickView>


#ifdef _WIN32
#include <windows.h>
#undef interface
#endif
#include <sys/stat.h>

/** ViewToolBar Action.  All of the viewToolBar buttons are connected to this slot. **/

void KStars::slotViewToolBar()
{
    KToggleAction *a   = (KToggleAction *)sender();
    KConfigDialog *kcd = KConfigDialog::exists("settings");

    if (a == actionCollection()->action("show_stars"))
    {
        Options::setShowStars(a->isChecked());
        if (kcd)
        {
            opcatalog->kcfg_ShowStars->setChecked(a->isChecked());
        }
    }
    else if (a == actionCollection()->action("show_deepsky"))
    {
        Options::setShowDeepSky(a->isChecked());
        if (kcd)
        {
            opcatalog->kcfg_ShowDeepSky->setChecked(a->isChecked());
        }
    }
    else if (a == actionCollection()->action("show_planets"))
    {
        Options::setShowSolarSystem(a->isChecked());
        if (kcd)
        {
            opsolsys->kcfg_ShowSolarSystem->setChecked(a->isChecked());
        }
    }
    else if (a == actionCollection()->action("show_clines"))
    {
        Options::setShowCLines(a->isChecked());
        if (kcd)
        {
            opguides->kcfg_ShowCLines->setChecked(a->isChecked());
        }
    }
    else if (a == actionCollection()->action("show_cnames"))
    {
        Options::setShowCNames(a->isChecked());
        if (kcd)
        {
            opguides->kcfg_ShowCNames->setChecked(a->isChecked());
        }
    }
    else if (a == actionCollection()->action("show_cbounds"))
    {
        Options::setShowCBounds(a->isChecked());
        if (kcd)
        {
            opguides->kcfg_ShowCBounds->setChecked(a->isChecked());
        }
    }
    else if (a == actionCollection()->action("show_constellationart"))
    {
        Options::setShowConstellationArt(a->isChecked());
        if (kcd)
        {
            opguides->kcfg_ShowConstellationArt->setChecked(a->isChecked());
        }
    }
    else if (a == actionCollection()->action("show_mw"))
    {
        Options::setShowMilkyWay(a->isChecked());
        if (kcd)
        {
            opguides->kcfg_ShowMilkyWay->setChecked(a->isChecked());
        }
    }
    else if (a == actionCollection()->action("show_equatorial_grid"))
    {
        // if autoSelectGrid is selected and the user clicked the
        // show_equatorial_grid button, he probably wants us to disable
        // the autoSelectGrid and display the equatorial grid.
        Options::setAutoSelectGrid(false);
        Options::setShowEquatorialGrid(a->isChecked());
        if (kcd)
        {
            opguides->kcfg_ShowEquatorialGrid->setChecked(a->isChecked());
            opguides->kcfg_AutoSelectGrid->setChecked(false);
        }
    }
    else if (a == actionCollection()->action("show_horizontal_grid"))
    {
        Options::setAutoSelectGrid(false);
        Options::setShowHorizontalGrid(a->isChecked());
        if (kcd)
        {
            opguides->kcfg_ShowHorizontalGrid->setChecked(a->isChecked());
            opguides->kcfg_AutoSelectGrid->setChecked(false);
        }
    }
    else if (a == actionCollection()->action("show_horizon"))
    {
        Options::setShowGround(a->isChecked());
        if (!a->isChecked() && Options::useRefraction())
        {
            QString caption = i18n("Refraction effects disabled");
            QString message = i18n("When the horizon is switched off, refraction effects are temporarily disabled.");

            KMessageBox::information(this, message, caption, "dag_refract_hide_ground");
        }
        if (kcd)
        {
            opguides->kcfg_ShowGround->setChecked(a->isChecked());
        }
    }
    else if (a == actionCollection()->action("show_flags"))
    {
        Options::setShowFlags(a->isChecked());
        if (kcd)
        {
            opguides->kcfg_ShowFlags->setChecked(a->isChecked());
        }
    }
    else if (a == actionCollection()->action("show_satellites"))
    {
        Options::setShowSatellites(a->isChecked());
        if (kcd)
        {
            opssatellites->kcfg_ShowSatellites->setChecked(a->isChecked());
        }
    }
    else if (a == actionCollection()->action("show_supernovae"))
    {
        Options::setShowSupernovae(a->isChecked());
        if (kcd)
        {
            opssupernovae->kcfg_ShowSupernovae->setChecked(a->isChecked());
        }
    }

    // update time for all objects because they might be not initialized
    // it's needed when using horizontal coordinates
    data()->setFullTimeUpdate();
    updateTime();

    map()->forceUpdate();
}

void KStars::slotINDIToolBar()
{
#ifdef HAVE_INDI
    KToggleAction *a = qobject_cast<KToggleAction *>(sender());

    if (a == actionCollection()->action("show_control_panel"))
    {
        if (a->isChecked())
        {
            GUIManager::Instance()->raise();
            GUIManager::Instance()->activateWindow();
            GUIManager::Instance()->showNormal();
        }
        else
            GUIManager::Instance()->hide();
    }
    else if (a == actionCollection()->action("show_ekos"))
    {
        if (a->isChecked())
        {
            Ekos::Manager::Instance()->raise();
            Ekos::Manager::Instance()->activateWindow();
            Ekos::Manager::Instance()->showNormal();
        }
        else
            Ekos::Manager::Instance()->hide();
    }
    else if (a == actionCollection()->action("lock_telescope"))
    {
        if (INDIListener::Instance()->size() == 0)
        {
            KSNotification::sorry(i18n("KStars did not find any active telescopes."));
            return;
        }

        ISD::GDInterface *oneScope = nullptr;

        foreach (ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
        {
            INDI::BaseDevice *bd = gd->getBaseDevice();

            if (gd->getType() != KSTARS_TELESCOPE)
                continue;

            if (bd == nullptr)
                continue;

            if (bd->isConnected() == false)
            {
                KMessageBox::error(
                    nullptr, i18n("Telescope %1 is offline. Please connect and retry again.", gd->getDeviceName()));
                return;
            }

            oneScope = gd;
            break;
        }

        if (oneScope == nullptr)
        {
            KSNotification::sorry(i18n("KStars did not find any active telescopes."));
            return;
        }

        if (a->isChecked())
            oneScope->runCommand(INDI_CENTER_LOCK);
        else
            oneScope->runCommand(INDI_CENTER_UNLOCK);
    }
    else if (a == actionCollection()->action("show_fits_viewer"))
    {
        if (m_FITSViewers.isEmpty())
        {
            a->setEnabled(false);
            return;
        }

        if (a->isChecked())
        {
            for (QPointer<FITSViewer> view : m_FITSViewers)
            {
                if (view->getTabs().empty() == false)
                {
                    view->raise();
                    view->activateWindow();
                    view->showNormal();
                }
            }
        }
        else
        {
            for (QPointer<FITSViewer> view : m_FITSViewers)
            {
                view->hide();
            }
        }
    }
    else if (a == actionCollection()->action("show_mount_box"))
    {
        Ekos::Manager::Instance()->mountModule()->toggleMountToolBox();
    }
    else if (a == actionCollection()->action("show_sensor_fov"))
    {
        Options::setShowSensorFOV(a->isChecked());
        for (auto oneFOV : data()->getTransientFOVs())
        {
            if (oneFOV->objectName() == "sensor_fov")
                oneFOV->setProperty("visible", a->isChecked());
        }
    }

#endif
}

void KStars::slotSetTelescopeEnabled(bool enable)
{
    telescopeGroup->setEnabled(enable);
    if (enable == false)
    {
        for(QAction *a : telescopeGroup->actions())
        {
            a->setChecked(false);
        }
    }
}

void KStars::slotSetDomeEnabled(bool enable)
{
    domeGroup->setEnabled(enable);
    if (enable == false)
    {
        for(QAction *a : domeGroup->actions())
        {
            a->setChecked(false);
        }
    }
}

/** Major Dialog Window Actions **/

void KStars::slotCalculator()
{
    if (!m_AstroCalc)
        m_AstroCalc = new AstroCalc(this);
    m_AstroCalc->show();
}

void KStars::slotWizard()
{
    QPointer<KSWizard> wizard = new KSWizard(this);
    if (wizard->exec() == QDialog::Accepted)
    {
        Options::setRunStartupWizard(false); //don't run on startup next time
        updateLocationFromWizard(*(wizard->geo()));
    }
    delete wizard;
}

void KStars::updateLocationFromWizard(const GeoLocation &geo)
{
    data()->setLocation(geo);
    // adjust local time to keep UT the same.
    // create new LT without DST offset
    KStarsDateTime ltime = data()->geo()->UTtoLT(data()->ut());

    // reset timezonerule to compute next dst change
    data()->geo()->tzrule()->reset_with_ltime(ltime, data()->geo()->TZ0(), data()->isTimeRunningForward());

    // reset next dst change time
    data()->setNextDSTChange(data()->geo()->tzrule()->nextDSTChange());

    // reset local sideral time
    data()->syncLST();

    // Make sure Numbers, Moon, planets, and sky objects are updated immediately
    data()->setFullTimeUpdate();

    // If the sky is in Horizontal mode and not tracking, reset focus such that
    // Alt/Az remain constant.
    if (!Options::isTracking() && Options::useAltAz())
    {
        map()->focus()->HorizontalToEquatorial(data()->lst(), data()->geo()->lat());
    }

    // recalculate new times and objects
    data()->setSnapNextFocus();
    updateTime();
}

void KStars::slotDownload()
{
    // 2017-07-04: Explicitly load kstars.knsrc from resources file
    QPointer<KNS3::DownloadDialog> dlg(new KNS3::DownloadDialog(":/kconfig/kstars.knsrc", this));
    dlg->exec();

    // Get the list of all the installed entries.
    KNS3::Entry::List installed_entries;
    KNS3::Entry::List changed_entries;
    if (dlg)
    {
        installed_entries = dlg->installedEntries();
        changed_entries   = dlg->changedEntries();
    }

    delete dlg;

    foreach (const KNS3::Entry &entry, installed_entries)
    {
        foreach (const QString &name, entry.installedFiles())
        {
            if (name.endsWith(QLatin1String(".cat")))
            {
                data()->catalogdb()->AddCatalogContents(name);
                // To start displaying the custom catalog, add it to SkyMapComposite

                QString catalogName = data()->catalogdb()->GetCatalogName(name);
                Options::setShowCatalogNames(Options::showCatalogNames() << catalogName);
                Options::setCatalogFile(Options::catalogFile() << name);
                Options::setShowCatalog(Options::showCatalog() << 1);
            }
        }

        KStars::Instance()->data()->skyComposite()->reloadDeepSky();
        // update time for all objects because they might be not initialized
        // it's needed when using horizontal coordinates
        KStars::Instance()->data()->setFullTimeUpdate();
        KStars::Instance()->updateTime();
        KStars::Instance()->map()->forceUpdate();
    }

    foreach (const KNS3::Entry &entry, changed_entries)
    {
        foreach (const QString &name, entry.uninstalledFiles())
        {
            if (name.endsWith(QLatin1String(".cat")))
            {
                data()->catalogdb()->RemoveCatalog(name);
                // To start displaying the custom catalog, add it to SkyMapComposite
                QStringList catFile = Options::catalogFile();
                catFile.removeOne(name);
                Options::setCatalogFile(catFile);
            }
        }
    }
}

void KStars::slotAVT()
{
    if (!m_AltVsTime)
        m_AltVsTime = new AltVsTime(this);
    m_AltVsTime->show();
}

void KStars::slotWUT()
{
    if (!m_WUTDialog)
        m_WUTDialog = new WUTDialog(this);
    m_WUTDialog->show();
}

//FIXME Port to QML2
//#if 0
void KStars::slotWISettings()
{
    if (!m_WIView)
        slotToggleWIView();
    if (m_WIView && !m_wiDock->isVisible())
        slotToggleWIView();

    if (KConfigDialog::showDialog("wisettings"))
    {
        m_WIEquipmentSettings->populateScopeListWidget();
        return;
    }

    KConfigDialog *dialog = new KConfigDialog(this, "wisettings", Options::self());

    connect(dialog, SIGNAL(settingsChanged(QString)), this, SLOT(slotApplyWIConfigChanges()));

    m_WISettings          = new WILPSettings(this);
    m_WIEquipmentSettings = new WIEquipSettings();
    dialog->addPage(m_WISettings, i18n("Light Pollution Settings"));
    dialog->addPage(m_WIEquipmentSettings, i18n("Equipment Settings - Equipment Type and Parameters"));
    dialog->exec();
    if (m_WIEquipmentSettings)
        m_WIEquipmentSettings->setAperture(); //Something isn't working with this!
}

void KStars::slotToggleWIView()
{
    if (KStars::Closing)
        return;

    if (!m_WIView)
    {
        m_WIView = new WIView(nullptr);
        m_wiDock = new QDockWidget(this);
        m_wiDock->setStyleSheet("QDockWidget::title{background-color:black;}");
        m_wiDock->setObjectName("What's Interesting");
        m_wiDock->setAllowedAreas(Qt::RightDockWidgetArea);
        QWidget *container = QWidget::createWindowContainer(m_WIView->getWIBaseView());
        m_wiDock->setWidget(container);
        m_wiDock->setMinimumWidth(400);
        addDockWidget(Qt::RightDockWidgetArea, m_wiDock);
        connect(m_wiDock, SIGNAL(visibilityChanged(bool)), actionCollection()->action("show_whatsinteresting"),
                SLOT(setChecked(bool)));
        m_wiDock->setVisible(true);
    }
    else
    {
        m_wiDock->setVisible(!m_wiDock->isVisible());
    }
}

void KStars::slotCalendar()
{
    if (!m_SkyCalendar)
        m_SkyCalendar = new SkyCalendar(this);
    m_SkyCalendar->show();
}

void KStars::slotGlossary()
{
    // 	GlossaryDialog *dlg = new GlossaryDialog( this, true );
    // 	QString glossaryfile =data()->stdDirs->findResource( "data", "kstars/glossary.xml" );
    // 	QUrl u = glossaryfile;
    // 	Glossary *g = new Glossary( u );
    // 	g->setName( i18n( "Knowledge" ) );
    // 	dlg->addGlossary( g );
    // 	dlg->show();
}

void KStars::slotScriptBuilder()
{
    if (!m_ScriptBuilder)
        m_ScriptBuilder = new ScriptBuilder(this);
    m_ScriptBuilder->show();
}

void KStars::slotSolarSystem()
{
    if (!m_PlanetViewer)
        m_PlanetViewer = new PlanetViewer(this);
    m_PlanetViewer->show();
}

void KStars::slotJMoonTool() {
    if (!m_JMoonTool)
        m_JMoonTool = new JMoonTool(this);
    m_JMoonTool->show();
}


void KStars::slotMoonPhaseTool()
{
    //FIXME Port to KF5
    //if( ! mpt ) mpt = new MoonPhaseTool( this );
    //mpt->show();
}

void KStars::slotFlagManager()
{
    if (!m_FlagManager)
        m_FlagManager = new FlagManager(this);
    m_FlagManager->show();
}

#if 0
void KStars::slotTelescopeWizard()
{
#ifdef HAVE_INDI
#ifndef Q_OS_WIN

    QString indiServerDir = Options::indiServer();

#ifdef Q_OS_OSX
    if (Options::indiServerIsInternal())
        indiServerDir = QCoreApplication::applicationDirPath() + "/indi";
    else
        indiServerDir = QFileInfo(Options::indiServer()).dir().path();
#endif

    QStringList paths;
    paths << "/usr/bin"
          << "/usr/local/bin" << indiServerDir;

    if (QStandardPaths::findExecutable("indiserver").isEmpty())
    {
        if (QStandardPaths::findExecutable("indiserver", paths).isEmpty())
        {
            KSNotification::error(i18n("Unable to find INDI server. Please make sure the package that provides "
                                       "the 'indiserver' binary is installed."));
            return;
        }
    }
#endif

    QPointer<telescopeWizardProcess> twiz = new telescopeWizardProcess(this);
    twiz->exec();
    delete twiz;
#endif
}
#endif

void KStars::slotINDIPanel()
{
#ifdef HAVE_INDI
#ifndef Q_OS_WIN

    QString indiServerDir = Options::indiServer();

#ifdef Q_OS_OSX
    if (Options::indiServerIsInternal())
        indiServerDir = QCoreApplication::applicationDirPath() + "/indi";
    else
        indiServerDir = QFileInfo(Options::indiServer()).dir().path();
#endif

    QStringList paths;
    paths << "/usr/bin"
          << "/usr/local/bin" << indiServerDir;

    if (QStandardPaths::findExecutable("indiserver").isEmpty())
    {
        if (QStandardPaths::findExecutable("indiserver", paths).isEmpty())
        {
            KSNotification::error(i18n("Unable to find INDI server. Please make sure the package that provides "
                                       "the 'indiserver' binary is installed."));
            return;
        }
    }
#endif
    GUIManager::Instance()->updateStatus(true);
#endif
}

void KStars::slotINDIDriver()
{
#ifdef HAVE_INDI
#ifndef Q_OS_WIN

    if (KMessageBox::warningContinueCancel(
                nullptr,
                i18n("INDI Device Manager should only be used by advanced technical users. It cannot be used with Ekos. Do you still want to open INDI device manager?"),
                i18n("INDI Device Manager"),
                KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                "indi_device_manager_warning") == KMessageBox::Cancel)
        return;

    QString indiServerDir = Options::indiServer();

#ifdef Q_OS_OSX
    if (Options::indiServerIsInternal())
        indiServerDir = QCoreApplication::applicationDirPath() + "/indi";
    else
        indiServerDir = QFileInfo(Options::indiServer()).dir().path();
#endif

    QStringList paths;
    paths << "/usr/bin"
          << "/usr/local/bin" << indiServerDir;

    if (QStandardPaths::findExecutable("indiserver").isEmpty())
    {
        if (QStandardPaths::findExecutable("indiserver", paths).isEmpty())
        {
            KSNotification::error(i18n("Unable to find INDI server. Please make sure the package that provides "
                                       "the 'indiserver' binary is installed."));
            return;
        }
    }
#endif

    DriverManager::Instance()->raise();
    DriverManager::Instance()->activateWindow();
    DriverManager::Instance()->showNormal();

#endif
}

void KStars::slotEkos()
{
#ifdef HAVE_CFITSIO
#ifdef HAVE_INDI

#ifndef Q_OS_WIN

    QString indiServerDir = Options::indiServer();

#ifdef Q_OS_OSX
    if (Options::indiServerIsInternal())
        indiServerDir = QCoreApplication::applicationDirPath() + "/indi";
    else
        indiServerDir = QFileInfo(Options::indiServer()).dir().path();
#endif

    QStringList paths;
    paths << "/usr/bin"
          << "/usr/local/bin" << indiServerDir;

    if (QStandardPaths::findExecutable("indiserver").isEmpty())
    {
        if (QStandardPaths::findExecutable("indiserver", paths).isEmpty())
        {
            KSNotification::error(i18n("Unable to find INDI server. Please make sure the package that provides "
                                       "the 'indiserver' binary is installed."));
            return;
        }
    }
#endif

    if (Ekos::Manager::Instance()->isVisible() && Ekos::Manager::Instance()->isActiveWindow())
    {
        Ekos::Manager::Instance()->hide();
    }
    else
    {
        Ekos::Manager::Instance()->raise();
        Ekos::Manager::Instance()->activateWindow();
        Ekos::Manager::Instance()->showNormal();
    }

#endif
#endif
}

void KStars::slotINDITelescopeTrack()
{
#ifdef HAVE_INDI
    if (m_KStarsData == nullptr || INDIListener::Instance() == nullptr)
        return;

    for (auto *gd : INDIListener::Instance()->getDevices())
    {
        ISD::Telescope* telescope = dynamic_cast<ISD::Telescope*>(gd);

        if (telescope != nullptr && telescope->isConnected())
        {
            KToggleAction *a = qobject_cast<KToggleAction*>(sender());

            if (a != nullptr)
            {
                telescope->setTrackEnabled(a->isChecked());
                return;
            }
        }
    }
#endif
}

void KStars::slotINDITelescopeSlew(bool focused_object)
{
#ifdef HAVE_INDI
    if (m_KStarsData == nullptr || INDIListener::Instance() == nullptr)
        return;

    for (auto *gd : INDIListener::Instance()->getDevices())
    {
        ISD::Telescope* telescope = dynamic_cast<ISD::Telescope*>(gd);

        if (telescope != nullptr && telescope->isConnected())
        {
            if (focused_object)
            {
                if (m_SkyMap->focusObject() != nullptr)
                    telescope->Slew(m_SkyMap->focusObject());
            }
            else
                telescope->Slew(m_SkyMap->mousePoint());

            return;
        }
    }
#else
    Q_UNUSED(focused_object)
#endif
}

void KStars::slotINDITelescopeSlewMousePointer()
{
#ifdef HAVE_INDI
    slotINDITelescopeSlew(false);
#endif
}

void KStars::slotINDITelescopeSync(bool focused_object)
{
#ifdef HAVE_INDI
    if (m_KStarsData == nullptr || INDIListener::Instance() == nullptr)
        return;

    for (auto *gd : INDIListener::Instance()->getDevices())
    {
        ISD::Telescope* telescope = dynamic_cast<ISD::Telescope*>(gd);

        if (telescope != nullptr && telescope->isConnected() && telescope->canSync())
        {
            if (focused_object)
            {
                if (m_SkyMap->focusObject() != nullptr)
                    telescope->Sync(m_SkyMap->focusObject());
            }
            else
                telescope->Sync(m_SkyMap->mousePoint());

            return;
        }
    }
#else
    Q_UNUSED(focused_object)
#endif
}

void KStars::slotINDITelescopeSyncMousePointer()
{
#ifdef HAVE_INDI
    slotINDITelescopeSync(false);
#endif
}

void KStars::slotINDITelescopeAbort()
{
#ifdef HAVE_INDI
    if (m_KStarsData == nullptr || INDIListener::Instance() == nullptr)
        return;

    for (auto *gd : INDIListener::Instance()->getDevices())
    {
        ISD::Telescope* telescope = dynamic_cast<ISD::Telescope*>(gd);

        if (telescope != nullptr && telescope->isConnected())
        {
            telescope->Abort();
            return;
        }
    }
#endif
}

void KStars::slotINDITelescopePark()
{
#ifdef HAVE_INDI
    if (m_KStarsData == nullptr || INDIListener::Instance() == nullptr)
        return;

    for (auto *gd : INDIListener::Instance()->getDevices())
    {
        ISD::Telescope* telescope = dynamic_cast<ISD::Telescope*>(gd);

        if (telescope != nullptr && telescope->isConnected() && telescope->canPark())
        {
            telescope->Park();
            return;
        }
    }
#endif
}

void KStars::slotINDITelescopeUnpark()
{
#ifdef HAVE_INDI
    if (m_KStarsData == nullptr || INDIListener::Instance() == nullptr)
        return;

    for (auto *gd : INDIListener::Instance()->getDevices())
    {
        ISD::Telescope* telescope = dynamic_cast<ISD::Telescope*>(gd);

        if (telescope != nullptr && telescope->isConnected() && telescope->canPark())
        {
            telescope->UnPark();
            return;
        }
    }
#endif
}

void KStars::slotINDIDomePark()
{
#ifdef HAVE_INDI
    if (m_KStarsData == nullptr || INDIListener::Instance() == nullptr)
        return;

    for (auto *gd : INDIListener::Instance()->getDevices())
    {
        ISD::Dome* dome = dynamic_cast<ISD::Dome*>(gd);

        if (dome != nullptr && dome->isConnected() && dome->canPark())
        {
            dome->Park();
            return;
        }
    }
#endif
}

void KStars::slotINDIDomeUnpark()
{
#ifdef HAVE_INDI
    if (m_KStarsData == nullptr || INDIListener::Instance() == nullptr)
        return;

    for (auto *gd : INDIListener::Instance()->getDevices())
    {
        ISD::Dome* dome = dynamic_cast<ISD::Dome*>(gd);

        if (dome != nullptr && dome->isConnected() && dome->canPark())
        {
            dome->UnPark();
            return;
        }
    }
#endif
}

void KStars::slotGeoLocator()
{
    QPointer<LocationDialog> locationdialog = new LocationDialog(this);
    if (locationdialog->exec() == QDialog::Accepted)
    {
        GeoLocation *newLocation = locationdialog->selectedCity();
        if (newLocation)
        {
            // set new location in options
            data()->setLocation(*newLocation);

            // adjust local time to keep UT the same.
            // create new LT without DST offset
            KStarsDateTime ltime = newLocation->UTtoLT(data()->ut());

            // reset timezonerule to compute next dst change
            newLocation->tzrule()->reset_with_ltime(ltime, newLocation->TZ0(), data()->isTimeRunningForward());

            // reset next dst change time
            data()->setNextDSTChange(newLocation->tzrule()->nextDSTChange());

            // reset local sideral time
            data()->syncLST();

            // Make sure Numbers, Moon, planets, and sky objects are updated immediately
            data()->setFullTimeUpdate();

            // If the sky is in Horizontal mode and not tracking, reset focus such that
            // Alt/Az remain constant.
            if (!Options::isTracking() && Options::useAltAz())
            {
                map()->focus()->HorizontalToEquatorial(data()->lst(), data()->geo()->lat());
            }

            // recalculate new times and objects
            data()->setSnapNextFocus();
            updateTime();
        }
    }
    delete locationdialog;
}

void KStars::slotViewOps()
{
    // An instance of your dialog could be already created and could be cached,
    // in which case you want to display the cached dialog instead of creating
    // another one
    prepareOps()->show();
}

KConfigDialog* KStars::prepareOps()
{
    KConfigDialog *dialog = KConfigDialog::exists("settings");
    if (nullptr != dialog)
        return dialog;

    // KConfigDialog didn't find an instance of this dialog, so lets create it :
    dialog = new KConfigDialog(this, "settings", Options::self());

    // For some reason the dialog does not resize to contents
    // so we set initial 'resonable' size here. Any better way to do this?
    dialog->resize(800, 600);
#ifdef Q_OS_OSX
    dialog->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    connect(dialog, SIGNAL(settingsChanged(QString)), this, SLOT(slotApplyConfigChanges()));

    opcatalog     = new OpsCatalog();
    opguides      = new OpsGuides();
    opsolsys      = new OpsSolarSystem();
    opssatellites = new OpsSatellites();
    opssupernovae = new OpsSupernovae();
    opcolors      = new OpsColors();
    opadvanced    = new OpsAdvanced();

    KPageWidgetItem *page;

    page = dialog->addPage(opcatalog, i18n("Catalogs"), "kstars_catalog");
    page->setIcon(QIcon::fromTheme("kstars_catalog"));

    page = dialog->addPage(opsolsys, i18n("Solar System"), "kstars_solarsystem");
    page->setIcon(QIcon::fromTheme("kstars_solarsystem"));

    page = dialog->addPage(opssatellites, i18n("Satellites"), "kstars_satellites");
    page->setIcon(QIcon::fromTheme("kstars_satellites"));

    page = dialog->addPage(opssupernovae, i18n("Supernovae"), "kstars_supernovae");
    page->setIcon(QIcon::fromTheme("kstars_supernovae"));

    page = dialog->addPage(opguides, i18n("Guides"), "kstars_guides");
    page->setIcon(QIcon::fromTheme("kstars_guides"));

    page = dialog->addPage(opcolors, i18n("Colors"), "kstars_colors");
    page->setIcon(QIcon::fromTheme("kstars_colors"));

#ifdef HAVE_CFITSIO
    opsfits = new OpsFITS();
    page    = dialog->addPage(opsfits, i18n("FITS"), "kstars_fitsviewer");
    page->setIcon(QIcon::fromTheme("kstars_fitsviewer"));
#endif

#ifdef HAVE_INDI
    opsindi = new OpsINDI();
    page    = dialog->addPage(opsindi, i18n("INDI"), "kstars_indi");
    page->setIcon(QIcon::fromTheme("kstars_indi"));
#ifdef HAVE_CFITSIO
    opsekos                     = new OpsEkos();
    KPageWidgetItem *ekosOption = dialog->addPage(opsekos, i18n("Ekos"), "kstars_ekos");
    ekosOption->setIcon(QIcon::fromTheme("kstars_ekos"));
    if (m_EkosManager)
        m_EkosManager->setOptionsWidget(ekosOption);
#endif

#endif

    opsxplanet = new OpsXplanet(this);
    page       = dialog->addPage(opsxplanet, i18n("Xplanet"), "kstars_xplanet");
    page->setIcon(QIcon::fromTheme("kstars_xplanet"));

    page = dialog->addPage(opadvanced, i18n("Advanced"), "kstars_advanced");
    page->setIcon(QIcon::fromTheme("kstars_advanced"));

    return dialog;
}

void KStars::slotApplyConfigChanges()
{
    Options::self()->save();

    applyConfig();

    //data()->setFullTimeUpdate();
    //map()->forceUpdate();
}

void KStars::slotApplyWIConfigChanges()
{
    Options::self()->save();
    applyConfig();
    m_WIView->updateObservingConditions();
    m_WIView->onReloadIconClicked();
}

void KStars::slotSetTime()
{
    QPointer<TimeDialog> timedialog = new TimeDialog(data()->lt(), data()->geo(), this);

    if (timedialog->exec() == QDialog::Accepted)
    {
        data()->changeDateTime(data()->geo()->LTtoUT(timedialog->selectedDateTime()));

        if (Options::useAltAz())
        {
            if (map()->focusObject())
            {
                map()->focusObject()->EquatorialToHorizontal(data()->lst(), data()->geo()->lat());
                map()->setFocus(map()->focusObject());
            }
            else
                map()->focus()->HorizontalToEquatorial(data()->lst(), data()->geo()->lat());
        }

        map()->forceUpdateNow();

        //If focusObject has a Planet Trail, clear it and start anew.
        KSPlanetBase *planet = dynamic_cast<KSPlanetBase *>(map()->focusObject());
        if (planet && planet->hasTrail())
        {
            planet->clearTrail();
            planet->addToTrail();
        }
    }
    delete timedialog;
}

//Set Time to CPU clock
void KStars::slotSetTimeToNow()
{
    data()->changeDateTime(KStarsDateTime::currentDateTimeUtc());

    if (Options::useAltAz())
    {
        if (map()->focusObject())
        {
            map()->focusObject()->EquatorialToHorizontal(data()->lst(), data()->geo()->lat());
            map()->setFocus(map()->focusObject());
        }
        else
            map()->focus()->HorizontalToEquatorial(data()->lst(), data()->geo()->lat());
    }

    map()->forceUpdateNow();

    //If focusObject has a Planet Trail, clear it and start anew.
    KSPlanetBase *planet = dynamic_cast<KSPlanetBase *>(map()->focusObject());
    if (planet && planet->hasTrail())
    {
        planet->clearTrail();
        planet->addToTrail();
    }
}

void KStars::slotFind()
{
    //clearCachedFindDialog();
    SkyObject *targetObject = nullptr;
    if (FindDialog::Instance()->exec() == QDialog::Accepted && (targetObject = FindDialog::Instance()->targetObject()))
    {
        map()->setClickedObject(targetObject);
        map()->setClickedPoint(map()->clickedObject());
        map()->slotCenter();
    }

    // check if data has changed while dialog was open
    //if (DialogIsObsolete)
    //    clearCachedFindDialog();
}

void KStars::slotOpenFITS()
{
#ifdef HAVE_CFITSIO

    static QUrl path = QUrl::fromLocalFile(QDir::homePath());
    QUrl fileURL =
        QFileDialog::getOpenFileUrl(KStars::Instance(), i18n("Open FITS"), path, "FITS (*.fits *.fits.fz *.fit *.fts)");

    if (fileURL.isEmpty())
        return;

    // Remember last directory
    path.setUrl(fileURL.url(QUrl::RemoveFilename));

    QPointer<FITSViewer> fv = new FITSViewer((Options::independentWindowFITS()) ? nullptr : this);

    connect(fv, &FITSViewer::loaded, [ &, fv]()
    {
        addFITSViewer(fv);
        fv->show();
    });

    fv->addFITS(fileURL, FITS_NORMAL, FITS_NONE, QString(), false);
#endif
}

void KStars::slotExportImage()
{
    //TODO Check this
    //For remote files, this returns
    //QFileInfo::absolutePath: QFileInfo::absolutePath: Constructed with empty filename
    //As of 2014-07-19
    //QUrl fileURL = KFileDialog::getSaveUrl( QDir::homePath(), "image/png image/jpeg image/gif image/x-portable-pixmap image/bmp image/svg+xml" );
    QUrl fileURL = QFileDialog::getSaveFileUrl(KStars::Instance(), i18n("Export Image"), QUrl(),
                   "Images (*.png *.jpeg *.gif *.bmp *.svg)");

    //User cancelled file selection dialog - abort image export
    if (fileURL.isEmpty())
    {
        return;
    }

    //Warn user if file exists!
    if (QFile::exists(fileURL.toLocalFile()))
    {
        int r = KMessageBox::warningContinueCancel(
                    parentWidget(), i18n("A file named \"%1\" already exists. Overwrite it?", fileURL.fileName()),
                    i18n("Overwrite File?"), KStandardGuiItem::overwrite());
        if (r == KMessageBox::Cancel)
            return;
    }

    // execute image export dialog

    // Note: We don't let ExportImageDialog create its own ImageExporter because we want legend settings etc to be remembered between UI use and DBus scripting interface use.
    //if ( !m_ImageExporter )
    //m_ImageExporter = new ImageExporter( this );

    if (!m_ExportImageDialog)
    {
        m_ExportImageDialog = new ExportImageDialog(fileURL.toLocalFile(), QSize(map()->width(), map()->height()),
                KStarsData::Instance()->imageExporter());
    }
    else
    {
        m_ExportImageDialog->setOutputUrl(fileURL.toLocalFile());
        m_ExportImageDialog->setOutputSize(QSize(map()->width(), map()->height()));
    }

    m_ExportImageDialog->show();
}

void KStars::slotRunScript()
{
    QUrl fileURL = QFileDialog::getOpenFileUrl(
                       KStars::Instance(), QString(), QUrl(QDir::homePath()),
                       "*.kstars|" + i18nc("Filter by file type: KStars Scripts.", "KStars Scripts (*.kstars)"));
    QFile f;
    //QString fname;

    if (fileURL.isValid())
    {
        if (fileURL.isLocalFile() == false)
        {
            KSNotification::sorry(i18n("Executing remote scripts is not supported."));
            return;
        }

        f.setFileName(fileURL.toLocalFile());

        if (!f.open(QIODevice::ReadOnly))
        {
            QString message = i18n("Could not open file %1", f.fileName());
            KSNotification::sorry(message, i18n("Could Not Open File"));
            return;
        }

        QTextStream istream(&f);
        QString line;
        bool fileOK(true);

        while (!istream.atEnd())
        {
            line = istream.readLine();
            if (line.at(0) != '#' && line.left(9) != "dbus-send")
            {
                fileOK = false;
                break;
            }
        }

        if (!fileOK)
        {
            int answer;
            answer = KMessageBox::warningContinueCancel(
                         nullptr,
                         i18n("The selected script contains unrecognized elements, "
                              "indicating that it was not created using the KStars script builder. "
                              "This script may not function properly, and it may even contain malicious code. "
                              "Would you like to execute it anyway?"),
                         i18n("Script Validation Failed"), KGuiItem(i18n("Run Nevertheless")), KStandardGuiItem::cancel(),
                         "daExecuteScript");
            if (answer == KMessageBox::Cancel)
                return;
        }

        //Add statusbar message that script is running
        statusBar()->showMessage(i18n("Running script: %1", fileURL.fileName()));

        // 2017-09-19: Jasem
        // FIXME This is a hack and does not work on non-Linux systems
        // The Script Builder should generate files that can run cross-platform
        QProcess p;
        p.start(f.fileName());
        if (!p.waitForStarted())
            return;

        while (!p.waitForFinished(10))
        {
            qApp->processEvents(); //otherwise tempfile may get deleted before script completes.
            if (p.state() != QProcess::Running)
                break;
        }

        statusBar()->showMessage(i18n("Script finished."), 0);
    }
}

void KStars::slotPrint()
{
    bool switchColors(false);

    //Suggest Chart color scheme
    if (data()->colorScheme()->colorNamed("SkyColor") != QColor(255, 255, 255))
    {
        QString message = i18n("You can save printer ink by using the \"Star Chart\" "
                               "color scheme, which uses a white background. Would you like to "
                               "temporarily switch to the Star Chart color scheme for printing?");

        int answer = KMessageBox::questionYesNoCancel(
                         nullptr, message, i18n("Switch to Star Chart Colors?"), KGuiItem(i18n("Switch Color Scheme")),
                         KGuiItem(i18n("Do Not Switch")), KStandardGuiItem::cancel(), "askAgainPrintColors");

        if (answer == KMessageBox::Cancel)
            return;
        if (answer == KMessageBox::Yes)
            switchColors = true;
    }

    printImage(true, switchColors);
}

void KStars::slotPrintingWizard()
{
    if (m_PrintingWizard)
    {
        delete m_PrintingWizard;
    }

    m_PrintingWizard = new PrintingWizard(this);
    m_PrintingWizard->show();
}

void KStars::slotToggleTimer()
{
    if (data()->clock()->isActive())
    {
        data()->clock()->stop();
        updateTime();
    }
    else
    {
        if (fabs(data()->clock()->scale()) > Options::slewTimeScale())
            data()->clock()->setManualMode(true);
        data()->clock()->start();
        if (data()->clock()->isManualMode())
            map()->forceUpdate();
    }

    // Update clock state in options
    Options::setRunClock(data()->clock()->isActive());
}

void KStars::slotStepForward()
{
    if (data()->clock()->isActive())
        data()->clock()->stop();
    data()->clock()->manualTick(true);
    map()->forceUpdate();
}

void KStars::slotStepBackward()
{
    if (data()->clock()->isActive())
        data()->clock()->stop();
    data()->clock()->manualTick(true, true);
    map()->forceUpdate();
}

//Pointing
void KStars::slotPointFocus()
{
    // In the following cases, we set slewing=true in order to disengage tracking
    map()->stopTracking();

    if (sender() == actionCollection()->action("zenith"))
        map()->setDestinationAltAz(dms(90.0), map()->focus()->az(), Options::useRefraction());
    else if (sender() == actionCollection()->action("north"))
        map()->setDestinationAltAz(dms(15.0), dms(0.0001), Options::useRefraction());
    else if (sender() == actionCollection()->action("east"))
        map()->setDestinationAltAz(dms(15.0), dms(90.0), Options::useRefraction());
    else if (sender() == actionCollection()->action("south"))
        map()->setDestinationAltAz(dms(15.0), dms(180.0), Options::useRefraction());
    else if (sender() == actionCollection()->action("west"))
        map()->setDestinationAltAz(dms(15.0), dms(270.0), Options::useRefraction());
}

void KStars::slotTrack()
{
    if (Options::isTracking())
    {
        Options::setIsTracking(false);
        actionCollection()->action("track_object")->setText(i18n("Engage &Tracking"));
        actionCollection()
        ->action("track_object")
        ->setIcon(QIcon::fromTheme("document-decrypt"));

        KSPlanetBase *planet = dynamic_cast<KSPlanetBase *>(map()->focusObject());
        if (planet && data()->temporaryTrail)
        {
            planet->clearTrail();
            data()->temporaryTrail = false;
        }

        map()->setClickedObject(nullptr);
        map()->setFocusObject(nullptr); //no longer tracking focusObject
        map()->setFocusPoint(nullptr);
    }
    else
    {
        map()->setClickedPoint(map()->focus());
        map()->setClickedObject(nullptr);
        map()->setFocusObject(nullptr); //no longer tracking focusObject
        map()->setFocusPoint(map()->clickedPoint());
        Options::setIsTracking(true);
        actionCollection()->action("track_object")->setText(i18n("Stop &Tracking"));
        actionCollection()
        ->action("track_object")
        ->setIcon(QIcon::fromTheme("document-encrypt"));
    }

    map()->forceUpdate();
}

void KStars::slotManualFocus()
{
    QPointer<FocusDialog> focusDialog = new FocusDialog();

    // JM 2019-09-04: Should default to RA/DE always
    //    if (Options::useAltAz())
    //        focusDialog->activateAzAltPage();

    if (focusDialog->exec() == QDialog::Accepted)
    {
        //If the requested position is very near the pole, we need to point first
        //to an intermediate location just below the pole in order to get the longitudinal
        //position (RA/Az) right.

        // Do not access (RA0, Dec0) of focusDialog->point() as it can be of unknown epoch.
        // (RA, Dec) should be synced to JNow
        // -- asimha (2020-07-06)
        double realAlt(focusDialog->point()->alt().Degrees());
        double realDec(focusDialog->point()->dec().Degrees());
        if (Options::useAltAz() && realAlt > 89.0)
        {
            focusDialog->point()->setAlt(89.0);
            focusDialog->point()->HorizontalToEquatorial(data()->lst(), data()->geo()->lat());
        }
        if (!Options::useAltAz() && realDec > 89.0)
        {
            focusDialog->point()->setDec(89.0);
            focusDialog->point()->EquatorialToHorizontal(data()->lst(), data()->geo()->lat());
        }

        map()->setClickedPoint(focusDialog->point());

        if (Options::isTracking())
            slotTrack();

        map()->slotCenter();

        //The slew takes some time to complete, and this often causes the final focus point to be slightly
        //offset from the user's requested coordinates (because EquatorialToHorizontal() is called
        //throughout the process, which depends on the sidereal time).  So we now "polish" the final
        //position by resetting the final focus to the focusDialog point.
        //
        //Also, if the requested position was within 1 degree of the coordinate pole, this will
        //automatically correct the final pointing from the intermediate offset position to the final position
        data()->setSnapNextFocus();
        if (Options::useAltAz())
        {
            // N.B. We have applied unrefract() in focusDialog
            map()->setDestinationAltAz(focusDialog->point()->alt(), focusDialog->point()->az(), false);
        }
        else
        {
            map()->setDestination(focusDialog->point()->ra(), focusDialog->point()->dec());
        }

        //Now, if the requested point was near a pole, we need to reset the Alt/Dec of the focus.
        if (Options::useAltAz() && realAlt > 89.0)
            map()->focus()->setAlt(realAlt);
        if (!Options::useAltAz() && realDec > 89.0)
            map()->focus()->setDec(realAlt);

        //Don't track if we set Alt/Az coordinates.  This way, Alt/Az remain constant.
        if (focusDialog->usedAltAz())
            map()->stopTracking();
    }
    delete focusDialog;
}

void KStars::slotZoomChanged()
{
    // Enable/disable actions
    actionCollection()->action("zoom_out")->setEnabled(Options::zoomFactor() > MINZOOM);
    actionCollection()->action("zoom_in")->setEnabled(Options::zoomFactor() < MAXZOOM);
    // Update status bar
    map()->setupProjector(); // this needs to be run redundantly, so that the FOV returned below is up-to-date.
    float fov                      = map()->projector()->fov();
    KLocalizedString fovi18nstring = ki18nc("approximate field of view", "Approximate FOV: %1 degrees");
    if (fov < 1.0)
    {
        fov           = fov * 60.0;
        fovi18nstring = ki18nc("approximate field of view", "Approximate FOV: %1 arcminutes");
    }
    if (fov < 1.0)
    {
        fov           = fov * 60.0;
        fovi18nstring = ki18nc("approximate field of view", "Approximate FOV: %1 arcseconds");
    }
    QString fovstring = fovi18nstring.subs(QString::number(fov, 'f', 1)).toString();

    statusBar()->showMessage(fovstring, 0);
}

void KStars::slotSetZoom()
{
    bool ok;
    double currentAngle = map()->width() / (Options::zoomFactor() * dms::DegToRad);
    double minAngle     = map()->width() / (MAXZOOM * dms::DegToRad);
    double maxAngle     = map()->width() / (MINZOOM * dms::DegToRad);

    double angSize = QInputDialog::getDouble(
                         nullptr,
                         i18nc("The user should enter an angle for the field-of-view of the display",
                               "Enter Desired Field-of-View Angle"),
                         i18n("Enter a field-of-view angle in degrees: "), currentAngle, minAngle, maxAngle, 1, &ok);

    if (ok)
    {
        map()->setZoomFactor(map()->width() / (angSize * dms::DegToRad));
    }
}

void KStars::slotCoordSys()
{
    if (Options::useAltAz())
    {
        Options::setUseAltAz(false);
        if (Options::useRefraction())
        {
            if (map()->focusObject()) //simply update focus to focusObject's position
                map()->setFocus(map()->focusObject());
            else //need to recompute focus for unrefracted position
            {
                // FIXME: Changed focus()->alt() to be unrefracted by convention; is this still necessary? -- asimha 2020/07/05
                map()->setFocusAltAz(map()->focus()->alt(), map()->focus()->az());
                map()->focus()->HorizontalToEquatorial(data()->lst(), data()->geo()->lat());
            }
        }
        actionCollection()->action("coordsys")->setText(i18n("Switch to horizonal view (Horizontal &Coordinates)"));
    }
    else
    {
        Options::setUseAltAz(true);
        if (Options::useRefraction())
        {
            // FIXME: Changed focus()->alt() to be unrefracted by convention; is this still necessary? -- asimha 2020/07/05
            map()->setFocusAltAz(map()->focus()->alt(), map()->focus()->az());
        }
        actionCollection()->action("coordsys")->setText(i18n("Switch to star globe view (Equatorial &Coordinates)"));
    }
    map()->forceUpdate();
}

void KStars::slotMapProjection()
{
    if (sender() == actionCollection()->action("project_lambert"))
        Options::setProjection(Projector::Lambert);
    if (sender() == actionCollection()->action("project_azequidistant"))
        Options::setProjection(Projector::AzimuthalEquidistant);
    if (sender() == actionCollection()->action("project_orthographic"))
        Options::setProjection(Projector::Orthographic);
    if (sender() == actionCollection()->action("project_equirectangular"))
        Options::setProjection(Projector::Equirectangular);
    if (sender() == actionCollection()->action("project_stereographic"))
        Options::setProjection(Projector::Stereographic);
    if (sender() == actionCollection()->action("project_gnomonic"))
        Options::setProjection(Projector::Gnomonic);

    //DEBUG
    qCDebug(KSTARS) << "Projection system: " << Options::projection();

    m_SkyMap->forceUpdate();
}

//Settings Menu:
void KStars::slotColorScheme()
{
    //use mid(3) to exclude the leading "cs_" prefix from the action name
    QString filename = QString(sender()->objectName()).mid(3) + ".colors";
    loadColorScheme(filename);
}

void KStars::slotTargetSymbol(bool flag)
{
    qDebug() << QString("slotTargetSymbol: %1 %2").arg(sender()->objectName()).arg(flag);

    QStringList names = Options::fOVNames();
    if (flag)
    {
        // Add FOV to list
        names.append(sender()->objectName());
    }
    else
    {
        // Remove FOV from list
        int ix = names.indexOf(sender()->objectName());
        if (ix >= 0)
            names.removeAt(ix);
    }
    Options::setFOVNames(names);

    // Sync visibleFOVs with fovNames
    data()->syncFOV();

    map()->forceUpdate();
}

void KStars::slotHIPSSource()
{
    QAction *selectedAction = qobject_cast<QAction*>(sender());
    Q_ASSERT(selectedAction != nullptr);

    QString selectedSource = selectedAction->text().remove('&');

    // selectedSource could be translated, while we need to send only Latin "None"
    // to Hips manager.
    if (selectedSource == i18n("None"))
        HIPSManager::Instance()->setCurrentSource("None");
    else
        HIPSManager::Instance()->setCurrentSource(selectedSource);

    map()->forceUpdate();
}

void KStars::slotFOVEdit()
{
    QPointer<FOVDialog> fovdlg = new FOVDialog(this);
    if (fovdlg->exec() == QDialog::Accepted)
    {
        FOVManager::save();
        repopulateFOV();
    }
    delete fovdlg;
}

void KStars::slotObsList()
{
    m_KStarsData->observingList()->show();
}

void KStars::slotEquipmentWriter()
{
    QPointer<EquipmentWriter> equipmentdlg = new EquipmentWriter();
    equipmentdlg->loadEquipment();
    equipmentdlg->exec();
    delete equipmentdlg;
}

void KStars::slotObserverManager()
{
    QPointer<ObserverAdd> m_observerAdd = new ObserverAdd();
    m_observerAdd->exec();
    delete m_observerAdd;
}

void KStars::slotHorizonManager()
{
    if (!m_HorizonManager)
    {
        m_HorizonManager = new HorizonManager(this);
        connect(m_SkyMap, SIGNAL(positionClicked(SkyPoint*)), m_HorizonManager, SLOT(addSkyPoint(SkyPoint*)));
    }

    m_HorizonManager->show();
}

void KStars::slotEyepieceView(SkyPoint *sp, const QString &imagePath)
{
    if (!m_EyepieceView)
        m_EyepieceView = new EyepieceField(this);

    // FIXME: Move FOV choice into the Eyepiece View tool itself.
    bool ok        = true;
    const FOV *fov = nullptr;
    if (!data()->getAvailableFOVs().isEmpty())
    {
        // Ask the user to choose from a list of available FOVs.
        //int index;
        const FOV *f;
        QMap<QString, const FOV *> nameToFovMap;
        foreach (f, data()->getAvailableFOVs())
        {
            nameToFovMap.insert(f->name(), f);
        }
        nameToFovMap.insert(i18n("Attempt to determine from image"), nullptr);
        fov = nameToFovMap[QInputDialog::getItem(this, i18n("Eyepiece View: Choose a field-of-view"),
                                                       i18n("FOV to render eyepiece view for:"), nameToFovMap.uniqueKeys(), 0,
                                                       false, &ok)];
    }
    if (ok)
        m_EyepieceView->showEyepieceField(sp, fov, imagePath);
}

void KStars::slotExecute()
{
    KStarsData::Instance()->executeSession()->init();
    KStarsData::Instance()->executeSession()->show();
}

void KStars::slotPolarisHourAngle()
{
    QPointer<PolarisHourAngle> pHourAngle = new PolarisHourAngle(this);
    pHourAngle->exec();
}

//Help Menu
void KStars::slotTipOfDay()
{
    KTipDialog::showTip(this, "kstars/tips", true);
}

// Toggle to and from full screen mode
void KStars::slotFullScreen()
{
    if (topLevelWidget()->isFullScreen())
    {
        topLevelWidget()->setWindowState(topLevelWidget()->windowState() & ~Qt::WindowFullScreen); // reset
    }
    else
    {
        topLevelWidget()->setWindowState(topLevelWidget()->windowState() | Qt::WindowFullScreen); // set
    }
}

void KStars::slotClearAllTrails()
{
    //Exclude object with temporary trail
    SkyObject *exOb(nullptr);
    if (map()->focusObject() && map()->focusObject()->isSolarSystem() && data()->temporaryTrail)
    {
        exOb = map()->focusObject();
    }

    TrailObject::clearTrailsExcept(exOb);

    map()->forceUpdate();
}

//toggle display of GUI Items on/off
void KStars::slotShowGUIItem(bool show)
{
    //Toolbars
    if (sender() == actionCollection()->action("show_statusBar"))
    {
        Options::setShowStatusBar(show);
        statusBar()->setVisible(show);
    }

    if (sender() == actionCollection()->action("show_sbAzAlt"))
    {
        Options::setShowAltAzField(show);
        if (!show)
            AltAzField.hide();
        else
            AltAzField.show();
    }

    if (sender() == actionCollection()->action("show_sbRADec"))
    {
        Options::setShowRADecField(show);
        if (!show)
            RADecField.hide();
        else
            RADecField.show();
    }

    if (sender() == actionCollection()->action("show_sbJ2000RADec"))
    {
        Options::setShowJ2000RADecField(show);
        if (!show)
            J2000RADecField.hide();
        else
            J2000RADecField.show();
    }
}
void KStars::addColorMenuItem(const QString &name, const QString &actionName)
{
    KToggleAction *kta = actionCollection()->add<KToggleAction>(actionName);
    kta->setText(name);
    kta->setObjectName(actionName);
    kta->setActionGroup(cschemeGroup);

    colorActionMenu->addAction(kta);

    KConfigGroup cg = KSharedConfig::openConfig()->group("Colors");
    if (actionName.mid(3) == cg.readEntry("ColorSchemeFile", "moonless-night.colors").remove(".colors"))
    {
        kta->setChecked(true);
    }

    connect(kta, SIGNAL(toggled(bool)), this, SLOT(slotColorScheme()));
}

void KStars::removeColorMenuItem(const QString &actionName)
{
    qCDebug(KSTARS) << "removing " << actionName;
    colorActionMenu->removeAction(actionCollection()->action(actionName));
}

void KStars::slotAboutToQuit()
{
    // Delete skymap. This required to run destructors and save
    // current state in the option.
    delete m_SkyMap;

    //Store Window geometry in Options object
    Options::setWindowWidth(width());
    Options::setWindowHeight(height());

    //explicitly save the colorscheme data to the config file
    data()->colorScheme()->saveToConfig();

    //synch the config file with the Config object
    writeConfig();

    //Terminate Child Processes if on OS X
#ifdef Q_OS_OSX
    QProcess *quit = new QProcess(this);
    quit->start("killall kdeinit5");
    quit->waitForFinished(1000);
    quit->start("killall klauncher");
    quit->waitForFinished(1000);
    quit->start("killall kioslave");
    quit->waitForFinished(1000);
    quit->start("killall kio_http_cache_cleaner");
    quit->waitForFinished(1000);
    delete quit;
#endif
}

void KStars::slotShowPositionBar(SkyPoint *p)
{
    if (Options::showAltAzField())
    {
        dms a = p->alt();
        if (Options::useAltAz())
            a = p->altRefracted();
        QString s = QString("%1, %2").arg(p->az().toDMSString(true), //true: force +/- symbol
                                          a.toDMSString(true));      //true: force +/- symbol
        //statusBar()->changeItem( s, 1 );
        AltAzField.setText(s);
    }
    if (Options::showRADecField())
    {
        KStarsDateTime lastUpdate;
        lastUpdate.setDJD(KStarsData::Instance()->updateNum()->getJD());
        QString sEpoch = QString::number(lastUpdate.epoch(), 'f', 1);
        QString s      = QString("%1, %2 (J%3)")
                         .arg(p->ra().toHMSString(), p->dec().toDMSString(true), sEpoch); //true: force +/- symbol
        //statusBar()->changeItem( s, 2 );
        RADecField.setText(s);
    }

    if (Options::showJ2000RADecField())
    {
        SkyPoint p0;
        //p0        = p->deprecess(KStarsData::Instance()->updateNum()); // deprecess to update RA0/Dec0 from RA/Dec
        p0 = p->catalogueCoord(KStarsData::Instance()->updateNum()->julianDay());
        QString s = QString("%1, %2 (J2000)")
                    .arg(p0.ra().toHMSString(),
                         p0.dec().toDMSString(true)); //true: force +/- symbol
        //statusBar()->changeItem( s, 2 );
        J2000RADecField.setText(s);
    }
}

void KStars::slotUpdateComets(bool isAutoUpdate)
{
    data()->skyComposite()->solarSystemComposite()->cometsComponent()->updateDataFile(isAutoUpdate);
}

void KStars::slotUpdateAsteroids(bool isAutoUpdate)
{
    data()->skyComposite()->solarSystemComposite()->asteroidsComponent()->updateDataFile(isAutoUpdate);
}

void KStars::slotUpdateSupernovae()
{
    data()->skyComposite()->supernovaeComponent()->slotTriggerDataFileUpdate();
}

void KStars::slotUpdateSatellites()
{
    data()->skyComposite()->satellites()->updateTLEs();
}

void KStars::slotAddDeepSkyObject()
{
    if (!m_addDSODialog)
    {
        Q_ASSERT(data() && data()->skyComposite() && data()->skyComposite()->manualAdditionsComponent());
        m_addDSODialog = new AddDeepSkyObject(this, data()->skyComposite()->manualAdditionsComponent());
    }
    m_addDSODialog->show();
}

void KStars::slotConfigureNotifications()
{
#ifdef HAVE_NOTIFYCONFIG
    KNotifyConfigWidget::configure(this);
#endif
}
