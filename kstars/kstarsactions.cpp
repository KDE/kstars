/*
    SPDX-FileCopyrightText: 2002 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// This file contains function definitions for Actions declared in kstars.h

#include "kstars.h"

#include "imageexporter.h"
#include "kstarsdata.h"
#include "kstars_debug.h"
#include "ksnotification.h"
#include "kswizard.h"
#include "Options.h"
#include "skymap.h"
#include "texturemanager.h"
#include "dialogs/exportimagedialog.h"
#include "dialogs/finddialog.h"
#include "dialogs/focusdialog.h"
#include "dialogs/fovdialog.h"
#include "dialogs/viewsdialog.h"
#include "dialogs/locationdialog.h"
#include "dialogs/timedialog.h"
#include "dialogs/catalogsdbui.h"
#include "oal/execute.h"
#include "oal/equipmentwriter.h"
#include "oal/observeradd.h"
#include "options/opsadvanced.h"
#include "options/opscatalog.h"
#include "options/opscolors.h"
#include "options/opsguides.h"
#include "options/opsterrain.h"
#include "options/opsimageoverlay.h"
#include "options/opsdeveloper.h"
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
#include "skycomponents/catalogscomponent.h"
#include "skycomponents/mosaiccomponent.h"
#include "skycomponents/imageoverlaycomponent.h"
#ifdef HAVE_INDI
#include "skyobjects/mosaictiles.h"
#include "indi/indidome.h"
#endif
#include "tools/altvstime.h"
#include "tools/astrocalc.h"
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
#include "catalogsdb.h"
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
#include "ekos/scheduler/framingassistantui.h"
#include "ekos/scheduler/scheduler.h"
#include "ekos/scheduler/schedulermodulestate.h"
#include "ekos/opsekos.h"
#include "ekos/mount/mount.h"
#include "tools/imagingplanner.h"
#endif
#endif

#include "xplanet/opsxplanet.h"

#ifdef HAVE_NOTIFYCONFIG
#include <KNotifyConfigWidget>
#endif
#include <KActionCollection>
#include <KActionMenu>

#include <KToggleAction>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <KNSWidgets/dialog.h>
#else
#include <kns3/downloaddialog.h>
#endif

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
            QString message = i18n("When the horizon is switched off, refraction effects "
                                   "are temporarily disabled.");

            KMessageBox::information(this, message, caption, "dag_refract_hide_ground");
        }
        if (kcd)
        {
            opguides->kcfg_ShowGround->setChecked(a->isChecked());
        }
    }
    else if (a == actionCollection()->action("simulate_daytime"))
    {
        Options::setSimulateDaytime(a->isChecked());
        if (kcd)
        {
            opguides->kcfg_SimulateDaytime->setChecked(a->isChecked());
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
        for (auto &oneDevice : INDIListener::devices())
        {
            if (!(oneDevice->getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE))
                continue;

            if (oneDevice->isConnected() == false)
            {
                KSNotification::error(i18n("Mount %1 is offline. Please connect and retry again.", oneDevice->getDeviceName()));
                return;
            }

            auto mount = oneDevice->getMount();
            if (!mount)
                continue;

            if (a->isChecked())
                mount->centerLock();
            else
                mount->centerUnlock();
            return;
        }

        KSNotification::sorry(i18n("No connected mounts found."));
        return;
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
            for (auto &view : m_FITSViewers)
            {
                if (view->tabs().empty() == false)
                {
                    view->raise();
                    view->activateWindow();
                    view->showNormal();
                }
            }
        }
        else
        {
            for (auto &view : m_FITSViewers)
            {
                view->hide();
            }
        }
    }
    else if (a == actionCollection()->action("show_mount_box"))
    {
#ifdef HAVE_CFITSIO
#ifdef HAVE_INDI
        Ekos::Manager::Instance()->mountModule()->toggleMountToolBox();
#endif
#endif
    }
    else if (a == actionCollection()->action("show_sensor_fov"))
    {
        Options::setShowSensorFOV(a->isChecked());
        for (auto &oneFOV : data()->getTransientFOVs())
        {
            if (oneFOV->objectName() == "sensor_fov")
                oneFOV->setProperty("visible", a->isChecked());
        }
    }
    else if (a == actionCollection()->action("show_mosaic_panel"))
    {
#ifdef HAVE_INDI
        Options::setShowMosaicPanel(a->isChecked());
        // TODO
        // If scheduler is not running, then we should also show the Mosaic Planner dialog.
        auto scheduler = Ekos::Manager::Instance()->schedulerModule();
        if (a->isChecked() && scheduler && scheduler->moduleState()->schedulerState() != Ekos::SCHEDULER_RUNNING)
        {
            // Only create if we don't have an instance already
            if (findChild<Ekos::FramingAssistantUI *>("FramingAssistant") == nullptr)
            {
                Ekos::FramingAssistantUI *assistant = new Ekos::FramingAssistantUI();
                assistant->setAttribute(Qt::WA_DeleteOnClose, true);
                assistant->show();
            }
        }
#endif
    }

#endif
}

void KStars::slotSetTelescopeEnabled(bool enable)
{
    telescopeGroup->setEnabled(enable);
    if (enable == false)
    {
        for (auto &a : telescopeGroup->actions())
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
        for (auto &a : domeGroup->actions())
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
        if (wizard->geo())
            updateLocationFromWizard(*(wizard->geo()));
    }
}

void KStars::updateLocationFromWizard(const GeoLocation &geo)
{
    data()->setLocation(geo);
    // adjust local time to keep UT the same.
    // create new LT without DST offset
    KStarsDateTime ltime = data()->geo()->UTtoLT(data()->ut());

    // reset timezonerule to compute next dst change
    data()->geo()->tzrule()->reset_with_ltime(ltime, data()->geo()->TZ0(),
            data()->isTimeRunningForward());

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
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    auto dlg = std::make_unique<KNSWidgets::Dialog>(":/kconfig/kstars.knsrc", this);
#else
    auto dlg = std::make_unique<KNS3::DownloadDialog>(":/kconfig/kstars.knsrc", this);
#endif

    if (!dlg)
        return;

    dlg->exec();

    // Get the list of all the installed entries.
    const auto changed_entries = dlg->changedEntries();

    CatalogsDB::DBManager manager{ CatalogsDB::dso_db_path() };
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    for (const KNSCore::Entry &entry : changed_entries)
#else
    for (const KNS3::Entry &entry : changed_entries)
#endif
    {
        if (entry.category() != "dso")
            continue;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const auto id = entry.uniqueId().toInt();
        if (entry.status() == KNSCore::Entry::Installed)
#else
        const auto id = entry.id().toInt();
        if (entry.status() == KNS3::Entry::Installed)
#endif
            for (const QString &name : entry.installedFiles())
            {
                if (name.endsWith(CatalogsDB::db_file_extension))
                {
                    const auto meta{ CatalogsDB::read_catalog_meta_from_file(name) };

                    if (!meta.first)
                    {
                        QMessageBox::critical(
                            this, i18n("Error"),
                            i18n("The catalog \"%1\" is corrupt.", entry.name()));
                        continue;
                    }

                    if (meta.second.id != id)
                    {
                        QMessageBox::critical(
                            this, i18n("Error"),
                            i18n("The catalog \"%1\" is corrupt.<br>Expected id=%2 but "
                                 "got id=%3",
                                 entry.name(), id, meta.second.id));
                        continue;
                    }

                    const auto success{ manager.import_catalog(name, true) };
                    if (!success.first)
                        QMessageBox::critical(
                            this, i18n("Error"),
                            i18n("Could not import the catalog \"%1\"<br>%2",
                                 entry.name(), success.second));
                }
            }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        if (entry.status() == KNSCore::Entry::Deleted)
#else
        if (entry.status() == KNS3::Entry::Deleted)
#endif
        {
            manager.remove_catalog(id);
        }
    }

    TextureManager::discoverTextureDirs();
    KStars::Instance()->data()->skyComposite()->reloadDeepSky();
    KStars::Instance()->data()->setFullTimeUpdate();
    KStars::Instance()->updateTime();
    KStars::Instance()->map()->forceUpdate();
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

    connect(dialog, SIGNAL(settingsChanged(QString)), this,
            SLOT(slotApplyWIConfigChanges()));

    m_WISettings          = new WILPSettings(this);
    m_WIEquipmentSettings = new WIEquipSettings();
    dialog->addPage(m_WISettings, i18n("Light Pollution Settings"));
    dialog->addPage(m_WIEquipmentSettings,
                    i18n("Equipment Settings - Equipment Type and Parameters"));
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
        connect(m_wiDock, SIGNAL(visibilityChanged(bool)),
                actionCollection()->action("show_whatsinteresting"),
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

void KStars::slotJMoonTool()
{
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

#ifdef Q_OS_MACOS
    if (Options::indiServerIsInternal())
        indiServerDir = QCoreApplication::applicationDirPath();
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

#ifdef Q_OS_MACOS
    if (Options::indiServerIsInternal())
        indiServerDir = QCoreApplication::applicationDirPath();
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
            KSNotification::error(i18n(
                                      "Unable to find INDI server. Please make sure the package that provides "
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
                i18n("INDI Device Manager should only be used by advanced technical users. "
                     "It cannot be used with Ekos. Do you still want to open INDI device "
                     "manager?"),
                i18n("INDI Device Manager"), KStandardGuiItem::cont(),
                KStandardGuiItem::cancel(),
                "indi_device_manager_warning") == KMessageBox::Cancel)
        return;

    QString indiServerDir = Options::indiServer();

#ifdef Q_OS_MACOS
    if (Options::indiServerIsInternal())
        indiServerDir = QCoreApplication::applicationDirPath();
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
            KSNotification::error(i18n(
                                      "Unable to find INDI server. Please make sure the package that provides "
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

#ifdef Q_OS_MACOS
    if (Options::indiServerIsInternal())
        indiServerDir = QCoreApplication::applicationDirPath();
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
            KSNotification::error(i18n(
                                      "Unable to find INDI server. Please make sure the package that provides "
                                      "the 'indiserver' binary is installed."));
            return;
        }
    }
#endif

    if (Ekos::Manager::Instance()->isVisible() &&
            Ekos::Manager::Instance()->isActiveWindow())
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

    for (auto &oneDevice : INDIListener::devices())
    {
        if (!(oneDevice->getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE))
            continue;

        auto mount = oneDevice->getMount();
        if (!mount || mount->isConnected() == false)
            continue;

        KToggleAction *a = qobject_cast<KToggleAction *>(sender());

        if (a != nullptr)
        {
            mount->setTrackEnabled(a->isChecked());
            return;
        }
    }
#endif
}

void KStars::slotINDITelescopeSlew(bool focused_object)
{
#ifdef HAVE_INDI
    if (m_KStarsData == nullptr || INDIListener::Instance() == nullptr)
        return;

    for (auto &oneDevice : INDIListener::devices())
    {
        if (!(oneDevice->getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE))
            continue;

        auto mount = oneDevice->getMount();
        if (!mount || mount->isConnected() == false)
            continue;
        if (focused_object)
        {
            if (m_SkyMap->focusObject() != nullptr)
                mount->Slew(m_SkyMap->focusObject());
        }
        else
            mount->Slew(m_SkyMap->mousePoint());

        return;
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

    for (auto &oneDevice : INDIListener::devices())
    {
        if (!(oneDevice->getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE))
            continue;

        auto mount = oneDevice->getMount();
        if (!mount || mount->isConnected() == false)
            continue;

        if (focused_object)
        {
            if (m_SkyMap->focusObject() != nullptr)
                mount->Sync(m_SkyMap->focusObject());
        }
        else
            mount->Sync(m_SkyMap->mousePoint());

        return;
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

    for (auto &oneDevice : INDIListener::devices())
    {
        if (!(oneDevice->getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE))
            continue;

        auto mount = oneDevice->getMount();
        if (!mount || mount->isConnected() == false)
            continue;

        mount->abort();
        return;
    }
#endif
}

void KStars::slotINDITelescopePark()
{
#ifdef HAVE_INDI
    if (m_KStarsData == nullptr || INDIListener::Instance() == nullptr)
        return;

    for (auto &oneDevice : INDIListener::devices())
    {
        if (!(oneDevice->getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE))
            continue;

        auto mount = oneDevice->getMount();
        if (!mount || mount->isConnected() == false || mount->canPark() == false)
            continue;

        mount->park();
        return;
    }
#endif
}

void KStars::slotINDITelescopeUnpark()
{
#ifdef HAVE_INDI
    if (m_KStarsData == nullptr || INDIListener::Instance() == nullptr)
        return;

    for (auto &oneDevice : INDIListener::devices())
    {
        if (!(oneDevice->getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE))
            continue;

        auto mount = oneDevice->getMount();
        if (!mount || mount->isConnected() == false || mount->canPark() == false)
            continue;

        mount->unpark();
        return;
    }
#endif
}

void KStars::slotINDIDomePark()
{
#ifdef HAVE_INDI
    if (m_KStarsData == nullptr || INDIListener::Instance() == nullptr)
        return;

    for (auto &oneDevice : INDIListener::devices())
    {
        if (!(oneDevice->getDriverInterface() & INDI::BaseDevice::DOME_INTERFACE))
            continue;

        auto dome = oneDevice->getDome();
        if (!dome || dome->isConnected() == false)
            continue;
        if (dome->canPark())
        {
            dome->park();
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

    for (auto &oneDevice : INDIListener::devices())
    {
        if (!(oneDevice->getDriverInterface() & INDI::BaseDevice::DOME_INTERFACE))
            continue;

        auto dome = oneDevice->getDome();
        if (!dome || dome->isConnected() == false)
            continue;
        if (dome->canPark())
        {
            dome->unpark();
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
            newLocation->tzrule()->reset_with_ltime(ltime, newLocation->TZ0(),
                                                    data()->isTimeRunningForward());

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
                map()->focus()->HorizontalToEquatorial(data()->lst(),
                                                       data()->geo()->lat());
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
    auto ops = prepareOps();
    ops->show();
    // Bring to the front.
    ops->raise();  // for MacOS
    ops->activateWindow(); // for Windows
}

KConfigDialog *KStars::prepareOps()
{
    KConfigDialog *dialog = KConfigDialog::exists("settings");
    if (nullptr != dialog)
        return dialog;

    // KConfigDialog didn't find an instance of this dialog, so lets create it :
    dialog = new KConfigDialog(this, "settings", Options::self());

    // For some reason the dialog does not resize to contents
    // so we set initial 'resonable' size here. Any better way to do this?
    dialog->resize(800, 600);
#ifdef Q_OS_MACOS
    dialog->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    connect(dialog, SIGNAL(settingsChanged(QString)), this,
            SLOT(slotApplyConfigChanges()));

    opcatalog     = new OpsCatalog();
    opguides      = new OpsGuides();
    opterrain     = new OpsTerrain();
    opsImageOverlay = new OpsImageOverlay();
    opsdeveloper  = new OpsDeveloper();
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

    page = dialog->addPage(opterrain, i18n("Terrain"), "kstars_terrain");
    page->setIcon(QIcon::fromTheme("kstars_terrain", QIcon(":/icons/kstars_terrain.png")));

    page = dialog->addPage(opsImageOverlay, i18n("Image Overlays"), "kstars_imageoverlay");
    page->setIcon(QIcon::fromTheme("kstars_imageoverlay", QIcon(":/icons/kstars_imageoverlay.png")));
    KStarsData::Instance()->skyComposite()->imageOverlay()->setWidgets(
        opsImageOverlay->table(), opsImageOverlay->statusDisplay(), opsImageOverlay->solvePushButton(),
        opsImageOverlay->tableTitleBox(), opsImageOverlay->solverProfile());

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
    if (Ekos::Manager::Instance())
        Ekos::Manager::Instance()->setOptionsWidget(ekosOption, opsekos);
#endif

#endif

    opsxplanet = new OpsXplanet(this);
    page       = dialog->addPage(opsxplanet, i18n("Xplanet"), "kstars_xplanet");
    page->setIcon(QIcon::fromTheme("kstars_xplanet"));

    page = dialog->addPage(opadvanced, i18n("Advanced"), "kstars_advanced");
    page->setIcon(QIcon::fromTheme("kstars_advanced"));

    page = dialog->addPage(opsdeveloper, i18n("Developer"), "kstars_developer");
    page->setIcon(QIcon::fromTheme("kstars_developer", QIcon(":/icons/kstars_developer.png")));

#ifdef Q_OS_MACOS // This is because KHelpClient doesn't seem to be working right on MacOS
    dialog->button(QDialogButtonBox::Help)->disconnect();
    connect(dialog->button(QDialogButtonBox::Help), &QPushButton::clicked, this, []()
    {
        KStars::Instance()->appHelpActivated();
    });
#endif

    return dialog;
}

void KStars::syncOps()
{
    opterrain->syncOptions();
    actionCollection()->action("toggle_terrain")
    ->setText(Options::showTerrain() ? i18n("Hide Terrain") : i18n("Show Terrain"));

    opsImageOverlay->syncOptions();
    actionCollection()->action("toggle_image_overlays")
    ->setText(Options::showImageOverlays() ? i18n("Hide Image Overlays") : i18n("Show Image Overlays"));
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
                map()->focusObject()->EquatorialToHorizontal(data()->lst(),
                        data()->geo()->lat());
                map()->setFocus(map()->focusObject());
            }
            else
                map()->focus()->HorizontalToEquatorial(data()->lst(),
                                                       data()->geo()->lat());
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
            map()->focusObject()->EquatorialToHorizontal(data()->lst(),
                    data()->geo()->lat());
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
    if (FindDialog::Instance()->exec() == QDialog::Accepted &&
            (targetObject = FindDialog::Instance()->targetObject()))
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
    auto fv = createFITSViewer();
    fv->openFile();
#endif
}

void KStars::slotBlink()
{
#ifdef HAVE_CFITSIO
    auto fv = createFITSViewer();
    fv->blink();
#endif
}

void KStars::slotExportImage()
{
    //TODO Check this
    //For remote files, this returns
    //QFileInfo::absolutePath: QFileInfo::absolutePath: Constructed with empty filename
    //As of 2014-07-19
    //QUrl fileURL = KFileDialog::getSaveUrl( QDir::homePath(), "image/png image/jpeg image/gif image/x-portable-pixmap image/bmp image/svg+xml" );
    QUrl fileURL =
        QFileDialog::getSaveFileUrl(KStars::Instance(), i18nc("@title:window", "Export Image"), QUrl(),
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
                    parentWidget(),
                    i18n("A file named \"%1\" already exists. Overwrite it?", fileURL.fileName()),
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
        m_ExportImageDialog = new ExportImageDialog(
            fileURL.toLocalFile(), QSize(map()->width(), map()->height()),
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
                       "*.kstars|" +
                       i18nc("Filter by file type: KStars Scripts.", "KStars Scripts (*.kstars)"));
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
                         i18n(
                             "The selected script contains unrecognized elements, "
                             "indicating that it was not created using the KStars script builder. "
                             "This script may not function properly, and it may even contain "
                             "malicious code. "
                             "Would you like to execute it anyway?"),
                         i18n("Script Validation Failed"), KGuiItem(i18n("Run Nevertheless")),
                         KStandardGuiItem::cancel(), "daExecuteScript");
            if (answer == KMessageBox::Cancel)
                return;
        }

        //Add statusbar message that script is running
        statusBar()->showMessage(i18n("Running script: %1", fileURL.fileName()));

        // 2017-09-19: Jasem
        // FIXME This is a hack and does not work on non-Linux systems
        // The Script Builder should generate files that can run cross-platform
        QProcess p;
        QStringList arguments;
        p.start(f.fileName(), arguments);
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
        QString message =
            i18n("You can save printer ink by using the \"Star Chart\" "
                 "color scheme, which uses a white background. Would you like to "
                 "temporarily switch to the Star Chart color scheme for printing?");

        int answer = KMessageBox::warningContinueCancel(
                         nullptr, message, i18n("Switch to Star Chart Colors?"),
                         KGuiItem(i18n("Switch Color Scheme")), KGuiItem(i18n("Do Not Switch")), "askAgainPrintColors");

        if (answer == KMessageBox::Cancel)
            return;
        if (answer == KMessageBox::Continue)
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

void KStars::slotRealTimeToogled(bool checked)
{
    KToggleAction *ta = static_cast<KToggleAction*>(actionCollection()->action("clock_realtime"));
    if (checked)
    {
        QAction *a = nullptr;
        a = actionCollection()->action("clock_startstop");
        if (a)a->setDisabled(true);
        a = actionCollection()->action("time_step_forward");
        if (a)a->setDisabled(true);
        a = actionCollection()->action("time_step_backward");
        if (a)a->setDisabled(true);
        a = actionCollection()->action("time_to_now");
        if (a)a->setDisabled(true);
        a = actionCollection()->action("time_dialog");
        if (a)a->setDisabled(true);
        m_TimeStepBox->setDisabled(true);
        if (ta)ta->setChecked(true);
    }
    else
    {
        QAction *a = nullptr;
        a = actionCollection()->action("clock_startstop");
        if (a)a->setDisabled(false);
        a = actionCollection()->action("time_step_forward");
        if (a)a->setDisabled(false);
        a = actionCollection()->action("time_step_backward");
        if (a)a->setDisabled(false);
        a = actionCollection()->action("time_to_now");
        if (a)a->setDisabled(false);
        a = actionCollection()->action("time_dialog");
        if (a)a->setDisabled(false);
        m_TimeStepBox->setDisabled(false);
        if (ta)ta->setChecked(false);
    }
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
        map()->setDestinationAltAz(dms(90.0), map()->focus()->az(),
                                   Options::useRefraction());
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
            focusDialog->point()->HorizontalToEquatorial(data()->lst(),
                    data()->geo()->lat());
        }
        if (!Options::useAltAz() && realDec > 89.0)
        {
            focusDialog->point()->setDec(89.0);
            focusDialog->point()->EquatorialToHorizontal(data()->lst(),
                    data()->geo()->lat());
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
            map()->setDestinationAltAz(focusDialog->point()->alt(),
                                       focusDialog->point()->az(), false);
        }
        else
        {
            map()->setDestination(focusDialog->point()->ra(),
                                  focusDialog->point()->dec());
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
    map()
    ->setupProjector(); // this needs to be run redundantly, so that the FOV returned below is up-to-date.
    float fov                      = map()->projector()->fov();
    KLocalizedString fovi18nstring =
        ki18nc("approximate field of view", "Approximate FOV: %1 degrees");
    if (fov < 1.0)
    {
        fov           = fov * 60.0;
        fovi18nstring =
            ki18nc("approximate field of view", "Approximate FOV: %1 arcminutes");
    }
    if (fov < 1.0)
    {
        fov           = fov * 60.0;
        fovi18nstring =
            ki18nc("approximate field of view", "Approximate FOV: %1 arcseconds");
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
                         i18n("Enter a field-of-view angle in degrees: "), currentAngle, minAngle,
                         maxAngle, 1, &ok);

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
                map()->focus()->HorizontalToEquatorial(data()->lst(),
                                                       data()->geo()->lat());
            }
        }
        actionCollection()
        ->action("coordsys")
        ->setText(i18n("Switch to Horizontal View (Horizontal &Coordinates)"));
        actionCollection()
        ->action("up_orientation")
        ->setText(i18nc("Orientation of the sky map", "North &Up"));
        actionCollection()
        ->action("down_orientation")
        ->setText(i18nc("Orientation of the sky map", "North &Down"));
        erectObserverCorrectionGroup->setEnabled(false);
    }
    else
    {
        Options::setUseAltAz(true);
        if (Options::useRefraction())
        {
            // FIXME: Changed focus()->alt() to be unrefracted by convention; is this still necessary? -- asimha 2020/07/05
            map()->setFocusAltAz(map()->focus()->alt(), map()->focus()->az());
        }
        actionCollection()
        ->action("coordsys")
        ->setText(i18n("Switch to Star Globe View (Equatorial &Coordinates)"));
        actionCollection()
        ->action("up_orientation")
        ->setText(i18nc("Orientation of the sky map", "Zenith &Up"));
        actionCollection()
        ->action("down_orientation")
        ->setText(i18nc("Orientation of the sky map", "Zenith &Down"));
        erectObserverCorrectionGroup->setEnabled(true);
    }
    actionCollection()->action("view:arbitrary")->setChecked(true);
    map()->forceUpdate();
}

void KStars::slotSkyMapOrientation()
{
    if (sender() == actionCollection()->action("up_orientation"))
    {
        Options::setSkyRotation(0.0);
    }
    else if (sender() == actionCollection()->action("down_orientation"))
    {
        Options::setSkyRotation(180.0);
    }

    Options::setMirrorSkyMap(actionCollection()->action("mirror_skymap")->isChecked());
    Options::setErectObserverCorrection(
        actionCollection()->action("erect_observer_correction_off")->isChecked() ? 0 : (
            actionCollection()->action("erect_observer_correction_left")->isChecked() ? 1 : 2));
    actionCollection()->action("view:arbitrary")->setChecked(true);
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
    loadColorScheme(sender()->objectName());
}

void KStars::slotTargetSymbol(bool flag)
{
    qDebug() << Q_FUNC_INFO << QString("slotTargetSymbol: %1 %2").arg(sender()->objectName()).arg(flag);

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

void KStars::slotApplySkyMapView(const QString &viewName)
{

    auto view = SkyMapViewManager::viewNamed(viewName);
    if (!view)
    {
        qCWarning(KSTARS) << "View named " << viewName << " not found!";
        return;
    }

    // FIXME: Ugly hack to update the menus correctly...
    // we set the opposite coordinate system setting and call slotCoordSys to toggle
    Options::setUseAltAz(!view->useAltAz);
    slotCoordSys();

    Options::setMirrorSkyMap(view->mirror);
    actionCollection()->action("mirror_skymap")->setChecked(Options::mirrorSkyMap());

    int erectObserverCorrection = 0;
    double viewAngle = view->viewAngle;
    if (view->erectObserver && view->useAltAz)
    {
        if (viewAngle > 0.)
        {
            erectObserverCorrection = 1;
            viewAngle -= 90.; // FIXME: Check
        }
        if (viewAngle < 0.)
        {
            erectObserverCorrection = 2;
            viewAngle += 90.; // FIXME: Check
        }
    }
    if (view->inverted)
    {
        viewAngle += 180.; // FIXME: Check
    }

    Options::setErectObserverCorrection(erectObserverCorrection);
    Options::setSkyRotation(dms::reduce(viewAngle));
    if (!std::isnan(view->fov))
    {
        Options::setZoomFactor(map()->width() / (3 * view->fov * dms::DegToRad));
    }
    repopulateOrientation(); // Update the menus
    qCDebug(KSTARS) << "Alt/Az: " << Options::useAltAz()
                    << "Mirror: " << Options::mirrorSkyMap()
                    << "Rotation: " << Options::skyRotation()
                    << "Erect Obs: " << Options::erectObserverCorrection()
                    << "FOV: " << view->fov;
    actionCollection()->action(QString("view:%1").arg(viewName))->setChecked(true);
    map()->forceUpdate();
}

void KStars::slotHIPSSource()
{
    QAction *selectedAction = qobject_cast<QAction *>(sender());
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

void KStars::slotEditViews()
{
    QPointer<ViewsDialog> viewsDialog = new ViewsDialog(this);
    if (viewsDialog->exec() == QDialog::Accepted)
    {
        SkyMapViewManager::save();
        repopulateViews();
    }
    delete viewsDialog;
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

void KStars::slotImagingPlanner()
{
#ifdef HAVE_INDI
    m_KStarsData->imagingPlanner()->show();
#endif
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
        connect(m_SkyMap, SIGNAL(positionClicked(SkyPoint *)), m_HorizonManager,
                SLOT(addSkyPoint(SkyPoint *)));
    }

    m_HorizonManager->show();
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

// Toggle to and from full screen mode
void KStars::slotFullScreen()
{
    if (topLevelWidget()->isFullScreen())
    {
        topLevelWidget()->setWindowState(topLevelWidget()->windowState() &
                                         ~Qt::WindowFullScreen); // reset
    }
    else
    {
        topLevelWidget()->setWindowState(topLevelWidget()->windowState() |
                                         Qt::WindowFullScreen); // set
    }
}

// Toggle showing terrain on the SkyMap.
void KStars::slotTerrain()
{
    Options::setShowTerrain(!Options::showTerrain());
    actionCollection()->action("toggle_terrain")
    ->setText(Options::showTerrain() ? i18n("Hide Terrain") : i18n("Show Terrain"));
    opterrain->syncOptions();
    KStars::Instance()->map()->forceUpdate();
}

// Toggle showing image overlays on the SkyMap.
void KStars::slotImageOverlays()
{
    Options::setShowImageOverlays(!Options::showImageOverlays());
    actionCollection()->action("toggle_image_overlays")
    ->setText(Options::showImageOverlays() ? i18n("Hide Image Overlays") : i18n("Show Image Overlays"));
    opsImageOverlay->syncOptions();
    KStars::Instance()->map()->forceUpdate();
}

void KStars::slotClearAllTrails()
{
    //Exclude object with temporary trail
    SkyObject *exOb(nullptr);
    if (map()->focusObject() && map()->focusObject()->isSolarSystem() &&
            data()->temporaryTrail)
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
        AltAzField.setHidden(!show);
    }

    if (sender() == actionCollection()->action("show_sbRADec"))
    {
        Options::setShowRADecField(show);
        RADecField.setHidden(!show);
    }

    if (sender() == actionCollection()->action("show_sbJ2000RADec"))
    {
        Options::setShowJ2000RADecField(show);
        J2000RADecField.setHidden(!show);
    }
}
void KStars::addColorMenuItem(QString name, const QString &actionName)
{
    KToggleAction *kta     = actionCollection()->add<KToggleAction>(actionName);
    const QString filename = QString(actionName).mid(3) + ".colors";
    kta->setText(name);
    kta->setObjectName(filename);
    kta->setActionGroup(cschemeGroup);

    colorActionMenu->addAction(kta);

    KConfigGroup cg = KSharedConfig::openConfig()->group("Colors");
    if (actionName.mid(3) ==
            cg.readEntry("ColorSchemeFile", "moonless-night.colors").remove(".colors"))
    {
        kta->setChecked(true);
    }

    //use mid(3) to exclude the leading "cs_" prefix from the action name
    data()->add_color_scheme(filename, name.replace("&", ""));
    connect(kta, SIGNAL(toggled(bool)), this, SLOT(slotColorScheme()));
}

void KStars::removeColorMenuItem(const QString &actionName)
{
    qCDebug(KSTARS) << "removing " << actionName;
    colorActionMenu->removeAction(actionCollection()->action(actionName));
}

void KStars::slotAboutToQuit()
{
    if (m_SkyMap == nullptr)
        return;

#ifdef HAVE_INDI
    DriverManager::Instance()->disconnectClients();
    INDIListener::Instance()->disconnect();
    GUIManager::Instance()->disconnect();
#endif

    // Delete skymap. This required to run destructors and save
    // current state in the option.
    delete m_SkyMap;
    m_SkyMap = nullptr;

    //Store Window geometry in Options object
    Options::setWindowWidth(width());
    Options::setWindowHeight(height());

    //explicitly save the colorscheme data to the config file
    data()->colorScheme()->saveToConfig();

    //synch the config file with the Config object
    writeConfig();

    //Terminate Child Processes if on OS X
#ifdef Q_OS_MACOS
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
        QString s =
            QString("%1, %2").arg(p->az().toDMSString(true), //true: force +/- symbol
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
                         .arg(p->ra().toHMSString(), p->dec().toDMSString(true),
                              sEpoch); //true: force +/- symbol
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
    data()->skyComposite()->solarSystemComposite()->cometsComponent()->updateDataFile(
        isAutoUpdate);
}

void KStars::slotUpdateAsteroids(bool isAutoUpdate)
{
    data()->skyComposite()->solarSystemComposite()->asteroidsComponent()->updateDataFile(
        isAutoUpdate);
}

void KStars::slotUpdateSupernovae()
{
    data()->skyComposite()->supernovaeComponent()->slotTriggerDataFileUpdate();
}

void KStars::slotUpdateSatellites()
{
    data()->skyComposite()->satellites()->updateTLEs();
}

void KStars::slotConfigureNotifications()
{
#ifdef HAVE_NOTIFYCONFIG
    KNotifyConfigWidget::configure(this);
#endif
}
void KStars::slotDSOCatalogGUI()
{
    auto *ui = new CatalogsDBUI{ this, CatalogsDB::dso_db_path() };
    ui->show();
    connect(ui, &QDialog::finished, this, [&](const auto)
    {
        KStars::Instance()->data()->skyComposite()->catalogsComponent()->dropCache();
    });
}
