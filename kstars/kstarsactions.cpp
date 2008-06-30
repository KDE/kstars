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

//needed in slotRunScript() for chmod() syscall (remote script downloaded to temp file)
#include <sys/stat.h>

#include <QCheckBox>
#include <QDir>
#include <QTextStream>
#include <QDialog>

#include <kdebug.h>
#include <kaction.h>
#include <kactioncollection.h>
#include <kactionmenu.h>
#include <ktoggleaction.h>
#include <klineedit.h>
#include <kiconloader.h>
#include <kio/netaccess.h>
#include <kmessagebox.h>
#include <ktemporaryfile.h>
#include <ktip.h>
#include <kstandarddirs.h>
#include <kconfigdialog.h>
#include <kfiledialog.h>
#include <kinputdialog.h>
#include <kmenu.h>
#include <kstatusbar.h>
#include <kprocess.h>
#include <kdeversion.h>
#include <ktoolbar.h>
#include <kedittoolbar.h>
#include <kicon.h>
#include <knewstuff2/engine.h>

#include "opscatalog.h"
#include "opsguides.h"
#include "opssolarsystem.h"
#include "opscolors.h"
#include "opsadvanced.h"

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "skymap.h"
#include "skyobject.h"
#include "ksplanetbase.h"
#include "ksasteroid.h"
#include "kscomet.h"
#include "ksmoon.h"
#include "simclock.h"
#include "infoboxes.h"
#include "timedialog.h"
#include "locationdialog.h"
#include "finddialog.h"
#include "focusdialog.h"
#include "fovdialog.h"
#include "kswizard.h"
#include "tools/lcgenerator.h"
#include "tools/astrocalc.h"
#include "tools/altvstime.h"
#include "tools/wutdialog.h"
#include "tools/scriptbuilder.h"
#include "tools/planetviewer.h"
#include "tools/jmoontool.h"
#include "ui_devmanager.h"
#include "imageviewer.h"
#include "indimenu.h"
#include "indidriver.h"
#include "indifitsconf.h"
#include "telescopewizardprocess.h"
#include "telescopeprop.h"

#include <config-kstars.h>

#ifdef HAVE_CFITSIO_H
#include "fitsviewer.h"
#endif

// #include "libkdeedu/kdeeduui/kdeeduglossary.h"

#include "imagesequence.h"

//This file contains function definitions for Actions declared in kstars.h

/** ViewToolBar Action.  All of the viewToolBar buttons are connected to this slot. **/

void KStars::slotViewToolBar() {
    KToggleAction *a = (KToggleAction*)sender();
    KConfigDialog *kcd = KConfigDialog::exists( "settings" );

    if ( a == actionCollection()->action( "show_stars" ) ) {
        Options::setShowStars( a->isChecked() );
        if ( kcd ) {
            opcatalog->kcfg_ShowStars->setChecked( a->isChecked() );
        }
    } else if ( a == actionCollection()->action( "show_deepsky" ) ) {
        Options::setShowDeepSky( a->isChecked() );
        if ( kcd ) {
            opcatalog->kcfg_ShowDeepSky->setChecked( a->isChecked() );
        }
    } else if ( a == actionCollection()->action( "show_planets" ) ) {
        Options::setShowSolarSystem( a->isChecked() );
        if ( kcd ) {
            opsolsys->kcfg_ShowSolarSystem->setChecked( a->isChecked() );
        }
    } else if ( a == actionCollection()->action( "show_clines" ) ) {
        Options::setShowCLines( a->isChecked() );
        if ( kcd ) {
            opguides->kcfg_ShowCLines->setChecked( a->isChecked() );
        }
    } else if ( a == actionCollection()->action( "show_cnames" ) ) {
        Options::setShowCNames( a->isChecked() );
        if ( kcd ) {
            opguides->kcfg_ShowCNames->setChecked( a->isChecked() );
        }
    } else if ( a == actionCollection()->action( "show_cbounds" ) ) {
        Options::setShowCBounds( a->isChecked() );
        if ( kcd ) {
            opguides->kcfg_ShowCBounds->setChecked( a->isChecked() );
        }
    } else if ( a == actionCollection()->action( "show_mw" ) ) {
        Options::setShowMilkyWay( a->isChecked() );
        if ( kcd ) {
            opguides->kcfg_ShowMilkyWay->setChecked( a->isChecked() );
        }
    } else if ( a == actionCollection()->action( "show_grid" ) ) {
        Options::setShowGrid( a->isChecked() );
        if ( kcd ) {
            opguides->kcfg_ShowGrid->setChecked( a->isChecked() );
        }
    } else if ( a == actionCollection()->action( "show_horizon" ) ) {
        Options::setShowGround( a->isChecked() );
        if ( kcd ) {
            opguides->kcfg_ShowGround->setChecked( a->isChecked() );
        }
    }

    // update time for all objects because they might be not initialized
    // it's needed when using horizontal coordinates
    data()->setFullTimeUpdate();
    updateTime();

    map()->forceUpdate();
}

/** Major Dialog Window Actions **/

void KStars::slotCalculator() {
    AstroCalc astrocalc (this);
    astrocalc.exec();
}

void KStars::slotWizard() {
    KSWizard wizard(this);
    if ( wizard.exec() == QDialog::Accepted ) {
        Options::setRunStartupWizard( false );  //don't run on startup next time

        data()->setLocation( wizard.geo() );

        // reset infoboxes
        infoBoxes()->geoChanged( geo() );

        // adjust local time to keep UT the same.
        // create new LT without DST offset
        KStarsDateTime ltime = geo()->UTtoLT( data()->ut() );

        // reset timezonerule to compute next dst change
        geo()->tzrule()->reset_with_ltime( ltime, geo()->TZ0(), data()->isTimeRunningForward() );

        // reset next dst change time
        data()->setNextDSTChange( geo()->tzrule()->nextDSTChange() );

        // reset local sideral time
        data()->syncLST();

        // Make sure Numbers, Moon, planets, and sky objects are updated immediately
        data()->setFullTimeUpdate();

        // If the sky is in Horizontal mode and not tracking, reset focus such that
        // Alt/Az remain constant.
        if ( ! Options::isTracking() && Options::useAltAz() ) {
            map()->focus()->HorizontalToEquatorial( LST(), geo()->lat() );
        }

        // recalculate new times and objects
        data()->setSnapNextFocus();
        updateTime();
    }
}

void KStars::slotDownload() {
    KNS::Entry::List entries = KNS::Engine::download();
    // we need to delete the entry* items in the returned list
	qDeleteAll(entries);
}

void KStars::slotLCGenerator() {
    if ( ! AAVSODialog  )
        AAVSODialog = new LCGenerator(this);

    AAVSODialog->show();
}

void KStars::slotAVT() {
    if ( ! avt ) avt = new AltVsTime(this);
    avt->show();
}

void KStars::slotWUT() {
    if ( ! wut ) wut = new WUTDialog(this);
    wut->show();
}

void KStars::slotGlossary(){
    // 	GlossaryDialog *dlg = new GlossaryDialog( this, true );
    // 	QString glossaryfile =data()->stdDirs->findResource( "data", "kstars/glossary.xml" );
    // 	KUrl u = glossaryfile;
    // 	Glossary *g = new Glossary( u );
    // 	g->setName( i18n( "Knowledge" ) );
    // 	dlg->addGlossary( g );
    // 	dlg->show();
}

void KStars::slotScriptBuilder() {
    if ( ! sb ) sb = new ScriptBuilder(this);
    sb->show();
}

void KStars::slotSolarSystem() {
    if ( ! pv ) pv = new PlanetViewer(this);
    pv->show();
}

void KStars::slotJMoonTool() {
    if ( ! jmt ) jmt = new JMoonTool(this);
    jmt->show();
}

void KStars::slotImageSequence()
{
    if (indiseq == NULL)
        indiseq = new imagesequence(this);

    if (indiseq->updateStatus())
        indiseq->show();
}

void KStars::slotTelescopeWizard()
{
    telescopeWizardProcess twiz(this);
    twiz.exec();
}

void KStars::slotTelescopeProperties()
{
    telescopeProp scopeProp(this);
    scopeProp.exec();
}

void KStars::slotINDIPanel() {
    if (indimenu == NULL)
        indimenu = new INDIMenu(this);

    indimenu->updateStatus();
}

void KStars::slotINDIDriver() {
    if (indidriver == NULL)
        indidriver = new INDIDriver(this);
    indidriver->show();
}

void KStars::slotINDIConf() {
    INDIFITSConf indioptions(this);

    indioptions.loadOptions();
    /*QStringList filterList;


    indiconf.timeCheck->setChecked( Options::indiAutoTime() );
    indiconf.GeoCheck->setChecked( Options::indiAutoGeo() );
    indiconf.crosshairCheck->setChecked( Options::indiCrosshairs() );
    indiconf.messagesCheck->setChecked ( Options::indiMessages() );
    indiconf.fitsAutoDisplayCheck->setChecked( Options::indiFITSDisplay() );
    indiconf.telPort_IN->setText ( Options::indiTelescopePort());
    indiconf.vidPort_IN->setText ( Options::indiVideoPort());

    if (Options::fitsSaveDirectory().isEmpty())
    {
      indiconf.fitsDIR_IN->setText (QDir:: homePath());
      Options::setFitsSaveDirectory( indiconf.fitsDIR_IN->text());
    }
    else
    indiconf.fitsDIR_IN->setText ( Options::fitsSaveDirectory());

    if (Options::filterAlias().empty())
    {
         filterList << "0" << "1" << "2" << "3" << "4" << "5" << "6" << "7" << "8"
                    << "9";
         indiconf.filterCombo->insertStringList(filterList);
    }*/

    if (indioptions.exec() == QDialog::Accepted)
    {
        /*Options::setIndiAutoTime( indiconf.timeCheck->isChecked() );
        Options::setIndiAutoGeo( indiconf.GeoCheck->isChecked() );
        Options::setIndiCrosshairs( indiconf.crosshairCheck->isChecked() );
        Options::setIndiMessages( indiconf.messagesCheck->isChecked() );
        Options::setIndiFITSDisplay (indiconf.AutoDisplayCheck->isChecked());
        Options::setIndiTelescopePort ( indiconf.telPort_IN->text());
        Options::setIndiVideoPort( indiconf.vidPort_IN->text());
        Options::setFitsSaveDirectory( indiconf.fitsDIR_IN->text());*/
        indioptions.saveOptions();

        map()->forceUpdateNow();
    }
}

void KStars::slotGeoLocator() {
    LocationDialog locationdialog (this);
    if ( locationdialog.exec() == QDialog::Accepted ) {
        GeoLocation *newLocation = locationdialog.selectedCity();
        if ( newLocation ) {
            // set new location in options
            data()->setLocation( *newLocation );

            // reset infoboxes
            infoBoxes()->geoChanged( newLocation );

            // adjust local time to keep UT the same.
            // create new LT without DST offset
            KStarsDateTime ltime = newLocation->UTtoLT( data()->ut() );

            // reset timezonerule to compute next dst change
            newLocation->tzrule()->reset_with_ltime( ltime, newLocation->TZ0(), data()->isTimeRunningForward() );

            // reset next dst change time
            data()->setNextDSTChange( newLocation->tzrule()->nextDSTChange() );

            // reset local sideral time
            data()->syncLST();

            // Make sure Numbers, Moon, planets, and sky objects are updated immediately
            data()->setFullTimeUpdate();

            // If the sky is in Horizontal mode and not tracking, reset focus such that
            // Alt/Az remain constant.
            if ( ! Options::isTracking() && Options::useAltAz() ) {
                map()->focus()->HorizontalToEquatorial( LST(), geo()->lat() );
            }

            // recalculate new times and objects
            data()->setSnapNextFocus();
            updateTime();
        }
    }
}

void KStars::slotApplyToolbarConfig() {
    //DEBUG
    kDebug() << "Recreating GUI...";

    createGUI();
    applyMainWindowSettings( KGlobal::config()->group( "MainWindow" ) );
}

void KStars::slotViewOps() {
    //An instance of your dialog could be already created and could be cached,
    //in which case you want to display the cached dialog instead of creating
    //another one
    if ( KConfigDialog::showDialog( "settings" ) ) return;

    //KConfigDialog didn't find an instance of this dialog, so lets create it :
    KConfigDialog* dialog = new KConfigDialog( this, "settings",
                            Options::self() );

    connect( dialog, SIGNAL( settingsChanged( const QString &) ), this, SLOT( slotApplyConfigChanges() ) );

    opcatalog    = new OpsCatalog( this );
    opguides     = new OpsGuides( this );
    opsolsys = new OpsSolarSystem( this );
    opcolors     = new OpsColors( this );
    opadvanced  = new OpsAdvanced( this );

    dialog->addPage(opcatalog, i18n("Catalogs"), "kstars_catalog");
    dialog->addPage(opsolsys, i18n("Solar System"), "kstars_solarsystem");
    dialog->addPage(opguides, i18n("Guides"), "kstars_guides");
    dialog->addPage(opcolors, i18n("Colors"), "kstars_colors");
    dialog->addPage(opadvanced, i18n("Advanced"), "kstars_advanced");
    dialog->setHelp(QString(), "kstars");
    dialog->show();
}

void KStars::slotApplyConfigChanges() {
    Options::self()->writeConfig();
    applyConfig();
    data()->setFullTimeUpdate();
    map()->forceUpdate();
}

void KStars::slotSetTime() {
    TimeDialog timedialog ( data()->lt(), geo(), this );

    if ( timedialog.exec() == QDialog::Accepted ) {
        data()->changeDateTime( geo()->LTtoUT( timedialog.selectedDateTime() ) );

        if ( Options::useAltAz() ) {
            if ( map()->focusObject() ) {
                map()->focusObject()->EquatorialToHorizontal( LST(), geo()->lat() );
                map()->setFocus( map()->focusObject() );
            } else
                map()->focus()->HorizontalToEquatorial( LST(), geo()->lat() );
        }

        map()->forceUpdateNow();

        //If focusObject has a Planet Trail, clear it and start anew.
        if ( map()->focusObject() && map()->focusObject()->isSolarSystem() &&
                ((KSPlanetBase*)map()->focusObject())->hasTrail() ) {
            ((KSPlanetBase*)map()->focusObject())->clearTrail();
            ((KSPlanetBase*)map()->focusObject())->addToTrail();
        }
    }
}

//Set Time to CPU clock
void KStars::slotSetTimeToNow() {
    data()->changeDateTime( KStarsDateTime::currentUtcDateTime() );

    if ( Options::useAltAz() ) {
        if ( map()->focusObject() ) {
            map()->focusObject()->EquatorialToHorizontal( LST(), geo()->lat() );
            map()->setFocus( map()->focusObject() );
        } else
            map()->focus()->HorizontalToEquatorial( LST(), geo()->lat() );
    }

    map()->forceUpdateNow();

    //If focusObject has a Planet Trail, clear it and start anew.
    if ( map()->focusObject() && map()->focusObject()->isSolarSystem() &&
            ((KSPlanetBase*)map()->focusObject())->hasTrail() ) {
        ((KSPlanetBase*)map()->focusObject())->clearTrail();
        ((KSPlanetBase*)map()->focusObject())->addToTrail();
    }
}

void KStars::slotFind() {
    clearCachedFindDialog();
    if ( !findDialog ) {	  // create new dialog if no dialog is existing
        findDialog = new FindDialog( this );
    }

    if ( !findDialog ) kWarning() << i18n( "KStars::slotFind() - Not enough memory for dialog" ) ;

    if ( findDialog->exec() == QDialog::Accepted && findDialog->selectedObject() ) {
        map()->setClickedObject( findDialog->selectedObject() );
        map()->setClickedPoint( map()->clickedObject() );
        map()->slotCenter();
    }

    // check if data has changed while dialog was open
    if ( DialogIsObsolete ) clearCachedFindDialog();
}

/** Menu Slots **/

//File
void KStars::newWindow() {
    new KStars(true);
}

void KStars::closeWindow() {
    show();
    close();
}

void KStars::slotOpenFITS()
{
#ifdef HAVE_CFITSIO_H

    KUrl fileURL = KFileDialog::getOpenUrl( QDir::homePath(), "*.fits *.fit *.fts|Flexible Image Transport System" );

    if (fileURL.isEmpty())
        return;

    FITSViewer * fv = new FITSViewer(&fileURL, this);
    fv->show();
#endif
}

void KStars::slotExportImage() {
    KUrl fileURL = KFileDialog::getSaveUrl( QDir::homePath(), "image/png image/jpeg image/gif image/x-portable-pixmap image/bmp" );

    //Warn user if file exists!
    if (QFile::exists(fileURL.path()))
    {
        int r=KMessageBox::warningContinueCancel(static_cast<QWidget *>(parent()),
                i18n( "A file named \"%1\" already exists. "
                      "Overwrite it?" , fileURL.fileName()),
                i18n( "Overwrite File?" ),
                KStandardGuiItem::overwrite() );

        if(r==KMessageBox::Cancel) return;
    }

    exportImage( fileURL.url(), map()->width(), map()->height() );
}

void KStars::slotRunScript() {
    KUrl fileURL = KFileDialog::getOpenUrl( QDir::homePath(), "*.kstars|KStars Scripts (*.kstars)" );
    QFile f;
    QString fname;

    if ( fileURL.isValid() ) {
        if ( ! fileURL.isLocalFile() ) {
            //Warn the user about executing remote code.
            QString message = i18n( "Warning:  You are about to execute a remote shell script on your machine. " );
            message += i18n( "If you absolutely trust the source of this script, press Continue to execute the script; " );
            message += i18n( "to save the file without executing it, press Save; " );
            message += i18n( "to cancel the download, press Cancel. " );

            int result = KMessageBox::warningYesNoCancel( 0, message, i18n( "Really Execute Remote Script?" ),
                         KStandardGuiItem::cont(), KStandardGuiItem::save() );

            if ( result == KMessageBox::Cancel ) return;
            if ( result == KMessageBox::No ) { //save file
                KUrl saveURL = KFileDialog::getSaveUrl( QDir::homePath(), "*.kstars|KStars Scripts (*.kstars)" );
                KTemporaryFile tmpfile;
                tmpfile.open();

                while ( ! saveURL.isValid() ) {
                    message = i18n( "Save location is invalid. Try another location?" );
                    if ( KMessageBox::warningYesNo( 0, message, i18n( "Invalid Save Location" ), KGuiItem(i18n("Try Another")), KGuiItem(i18n("Do Not Try")) ) == KMessageBox::No ) return;
                    saveURL = KFileDialog::getSaveUrl( QDir::homePath(), "*.kstars|KStars Scripts (*.kstars)" );
                }

                if ( saveURL.isLocalFile() ) {
                    fname = saveURL.path();
                } else {
                    fname = tmpfile.fileName();
                }

                if( KIO::NetAccess::download( fileURL, fname, this ) ) {
                    chmod( fname.toAscii(), S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH ); //make it executable

                    if ( tmpfile.fileName() == fname ) { //upload to remote location
                        if ( ! KIO::NetAccess::upload( tmpfile.fileName(), fileURL, this ) ) {
                            QString message = i18n( "Could not upload image to remote location: %1", fileURL.prettyUrl() );
                            KMessageBox::sorry( 0, message, i18n( "Could not upload file" ) );
                        }
                    }
                } else {
                    KMessageBox::sorry( 0, i18n( "Could not download the file." ), i18n( "Download Error" ) );
                }

                return;
            }
        }

        //Damn the torpedos and full speed ahead, we're executing the script!
        KTemporaryFile tmpfile;
        tmpfile.open();

        if ( ! fileURL.isLocalFile() ) {
            fname = tmpfile.fileName();
            if( KIO::NetAccess::download( fileURL, fname, this ) ) {
                chmod( fname.toAscii(), S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH );
                f.setFileName( fname );
            }
        } else {
            f.setFileName( fileURL.path() );
        }

        if ( !f.open( QIODevice::ReadOnly) ) {
            QString message = i18n( "Could not open file %1", f.fileName() );
            KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
            return;
        }

        // Before we run the script, make sure that it's safe.  Each line must either begin with "#"
        // or begin with "dbus-send". INDI scripts are much more complicated, so this simple test is not
        // suitable. INDI Scripting will return in KDE 4.1

        QTextStream istream(&f);
        QString line;
        bool fileOK( true );

        while (  ! istream.atEnd() ) {
            line = istream.readLine();
            if ( line.left(1) != "#" && line.left(9) != "dbus-send")
            {
                fileOK = false;
                break;
            }
        }

        if ( ! fileOK )
        {
            int answer;
            answer = KMessageBox::warningContinueCancel( 0, i18n( "The selected script contains unrecognized elements,"
                     "indicating that it was not created using the KStars script builder. "
                     "This script may not function properly, and it may even contain malicious code. "
                     "Would you like to execute it anyway?" ),
                     i18n( "Script Validation Failed" ), KGuiItem( i18n( "Run Nevertheless" ) ), KStandardGuiItem::cancel(), "daExecuteScript" );
            if ( answer == KMessageBox::Cancel ) return;
        }

        //Add statusbar message that script is running
        statusBar()->changeItem( i18n( "Running script: %1", fileURL.fileName() ), 0 );

        KProcess p;
        p << f.fileName();
        p.start();
        if( !p.waitForStarted() )
            return;

        while ( !p.waitForFinished( 10 ) )
        {
            qApp->processEvents(); //otherwise tempfile may get deleted before script completes.
            if( p.state() != QProcess::Running )
                break;
        }

        statusBar()->changeItem( i18n( "Script finished."), 0 );
    }
}

void KStars::slotPrint() {
    bool switchColors(false);

    //Suggest Chart color scheme
    if ( data()->colorScheme()->colorNamed( "SkyColor" ) != "#FFFFFF" ) {
        QString message = i18n( "You can save printer ink by using the \"Star Chart\" "
                                "color scheme, which uses a white background. Would you like to "
                                "temporarily switch to the Star Chart color scheme for printing?" );

        int answer;
        answer = KMessageBox::questionYesNoCancel( 0, message, i18n( "Switch to Star Chart Colors?" ),
                 KGuiItem(i18n("Switch Color Scheme")), KGuiItem(i18n("Do Not Switch")), KStandardGuiItem::cancel(), "askAgainPrintColors" );

        if ( answer == KMessageBox::Cancel ) return;
        if ( answer == KMessageBox::Yes ) switchColors = true;
    }

    printImage( true, switchColors );
}

void KStars::slotToggleTimer() {
    if ( data()->clock()->isActive() ) {
        data()->clock()->stop();
        updateTime();
    } else {
        if ( fabs( data()->clock()->scale() ) > Options::slewTimeScale() )
            data()->clock()->setManualMode( true );
        data()->clock()->start();
        if ( data()->clock()->isManualMode() ) map()->forceUpdate();
    }
}

//Pointing
void KStars::slotPointFocus() {

    if ( sender() == actionCollection()->action("zenith") )
        map()->invokeKey( Qt::Key_Z );
    else if ( sender() == actionCollection()->action("north") )
        map()->invokeKey( Qt::Key_N );
    else if ( sender() == actionCollection()->action("east") )
        map()->invokeKey( Qt::Key_E );
    else if ( sender() == actionCollection()->action("south") )
        map()->invokeKey( Qt::Key_S );
    else if ( sender() == actionCollection()->action("west") )
        map()->invokeKey( Qt::Key_W );
}

void KStars::slotTrack() {
    if ( Options::isTracking() ) {
        Options::setIsTracking( false );
        actionCollection()->action("track_object")->setText( i18n( "Engage &Tracking" ) );
        actionCollection()->action("track_object")->setIcon( KIcon("document-decrypt") );
        if ( map()->focusObject() && map()->focusObject()->isSolarSystem() && data()->temporaryTrail ) {
            ((KSPlanetBase*)map()->focusObject())->clearTrail();
            data()->temporaryTrail = false;
        }

        map()->setClickedObject( NULL );
        map()->setFocusObject( NULL );//no longer tracking focusObject
        map()->setFocusPoint( NULL );
    } else {
        map()->setClickedPoint( map()->focus() );
        map()->setClickedObject( NULL );
        map()->setFocusObject( NULL );//no longer tracking focusObject
        map()->setFocusPoint( map()->clickedPoint() );
        Options::setIsTracking( true );
        actionCollection()->action("track_object")->setText( i18n( "Stop &Tracking" ) );
        actionCollection()->action("track_object")->setIcon( KIcon("document-encrypt") );
    }

    map()->forceUpdate();
}

void KStars::slotManualFocus() {
    FocusDialog focusDialog( this ); // = new FocusDialog( this );
    if ( Options::useAltAz() ) focusDialog.activateAzAltPage();

    if ( focusDialog.exec() == QDialog::Accepted ) {
        //DEBUG
        kDebug() << "focusDialog point: " << &focusDialog;

        //If the requested position is very near the pole, we need to point first
        //to an intermediate location just below the pole in order to get the longitudinal
        //position (RA/Az) right.
        double realAlt( focusDialog.point().alt()->Degrees() );
        double realDec( focusDialog.point().dec()->Degrees() );
        if ( Options::useAltAz() && realAlt > 89.0 ) {
            focusDialog.point().setAlt( 89.0 );
            focusDialog.point().HorizontalToEquatorial( LST(), geo()->lat() );
        }
        if ( ! Options::useAltAz() && realDec > 89.0 ) {
            focusDialog.point().setDec( 89.0 );
            focusDialog.point().EquatorialToHorizontal( LST(), geo()->lat() );
        }

        map()->setClickedPoint( & focusDialog.point() );
        if ( Options::isTracking() ) slotTrack();

        map()->slotCenter();

        //The slew takes some time to complete, and this often causes the final focus point to be slightly
        //offset from the user's requested coordinates (because EquatorialToHorizontal() is called
        //throughout the process, which depends on the sidereal time).  So we now "polish" the final
        //position by resetting the final focus to the focusDialog point.
        //
        //Also, if the requested position was within 1 degree of the coordinate pole, this will
        //automatically correct the final pointing from the intermediate offset position to the final position
        if ( Options::useAltAz() ) {
            data()->setSnapNextFocus();
            map()->setDestinationAltAz( focusDialog.point().alt()->Degrees(), focusDialog.point().az()->Degrees() );
        } else {
            data()->setSnapNextFocus();
            map()->setDestination( focusDialog.point().ra()->Hours(), focusDialog.point().dec()->Degrees() );
        }

        //Now, if the requested point was near a pole, we need to reset the Alt/Dec of the focus.
        if ( Options::useAltAz() && realAlt > 89.0 ) map()->focus()->setAlt( realAlt );
        if ( ! Options::useAltAz() && realDec > 89.0 ) map()->focus()->setDec( realAlt );

        //Don't track if we set Alt/Az coordinates.  This way, Alt/Az remain constant.
        if ( focusDialog.usedAltAz() ) map()->stopTracking();
    }
}

//View Menu
void KStars::slotZoomIn() {
    actionCollection()->action("zoom_out")->setEnabled (true);
    if ( Options::zoomFactor() < MAXZOOM )
        Options::setZoomFactor( Options::zoomFactor()*DZOOM );

    if ( Options::zoomFactor() >= MAXZOOM ) {
        Options::setZoomFactor( MAXZOOM );
        actionCollection()->action("zoom_in")->setEnabled (false);
    }

    map()->forceUpdate();
}

void KStars::slotZoomOut() {
    actionCollection()->action("zoom_in")->setEnabled (true);
    if ( Options::zoomFactor() > MINZOOM )
        Options::setZoomFactor( Options::zoomFactor()/DZOOM );

    if ( Options::zoomFactor() <= MINZOOM ) {
        Options::setZoomFactor( MINZOOM );
        actionCollection()->action("zoom_out")->setEnabled (false);
    }

    map()->forceUpdate();
}

void KStars::slotDefaultZoom() {
    Options::setZoomFactor( DEFAULTZOOM );
    map()->forceUpdate();

    if ( Options::zoomFactor() > MINZOOM )
        actionCollection()->action("zoom_out")->setEnabled (true);
    if ( Options::zoomFactor() < MAXZOOM )
        actionCollection()->action("zoom_in")->setEnabled (true);
}

void KStars::slotSetZoom() {
    bool ok( false );
    double currentAngle = map()->width() / ( Options::zoomFactor() * dms::DegToRad );
    double angSize = currentAngle;
    double minAngle = map()->width() / ( MAXZOOM * dms::DegToRad );
    double maxAngle = map()->width() / ( MINZOOM * dms::DegToRad );

    angSize = KInputDialog::getDouble( i18nc( "The user should enter an angle for the field-of-view of the display",
                                       "Enter Desired Field-of-View Angle" ), i18n( "Enter a field-of-view angle in degrees: " ),
                                       currentAngle, minAngle, maxAngle, 0.1, 1, &ok );

    if ( ok ) {
        Options::setZoomFactor( map()->width() / ( angSize * dms::DegToRad ) );

        if ( Options::zoomFactor() <= MINZOOM ) {
            Options::setZoomFactor( MINZOOM );
            actionCollection()->action("zoom_out")->setEnabled( false );
        } else {
            actionCollection()->action("zoom_out")->setEnabled( true );
        }

        if ( Options::zoomFactor() >= MAXZOOM ) {
            Options::setZoomFactor( MAXZOOM );
            actionCollection()->action("zoom_in")->setEnabled( false );
        } else {
            actionCollection()->action("zoom_in")->setEnabled( true );
        }

        map()->forceUpdate();
    }
}

void KStars::slotCoordSys() {
    if ( Options::useAltAz() ) {
        Options::setUseAltAz( false );
        if ( Options::useRefraction() ) {
            if ( map()->focusObject() ) //simply update focus to focusObject's position
                map()->setFocus( map()->focusObject() );
            else { //need to recompute focus for unrefracted position
                map()->setFocusAltAz( map()->refract( map()->focus()->alt(), false ).Degrees(),
                                      map()->focus()->az()->Degrees() );
                map()->focus()->HorizontalToEquatorial( data()->lst(), geo()->lat() );
            }
        }
        actionCollection()->action("coordsys")->setText( i18n("Equatorial &Coordinates") );
    } else {
        Options::setUseAltAz( true );
        if ( Options::useRefraction() ) {
            map()->setFocusAltAz( map()->refract( map()->focus()->alt(), true ).Degrees(),
                                  map()->focus()->az()->Degrees() );
        }
        actionCollection()->action("coordsys")->setText( i18n("Horizontal &Coordinates") );
    }
    map()->forceUpdate();
}

void KStars::slotMapProjection() {
    if ( sender() == actionCollection()->action("project_lambert") )
        Options::setProjection( SkyMap::Lambert );
    if ( sender() == actionCollection()->action("project_azequidistant") )
        Options::setProjection( SkyMap::AzimuthalEquidistant );
    if ( sender() == actionCollection()->action("project_orthographic") )
        Options::setProjection( SkyMap::Orthographic );
    if ( sender() == actionCollection()->action("project_equirectangular") )
        Options::setProjection( SkyMap::Equirectangular );
    if ( sender() == actionCollection()->action("project_stereographic") )
        Options::setProjection( SkyMap::Stereographic );
    if ( sender() == actionCollection()->action("project_gnomonic") )
        Options::setProjection( SkyMap::Gnomonic );

    //DEBUG
    kDebug() << i18n( "Projection system: %1", Options::projection() );

    skymap->forceUpdate();
}

//Settings Menu:
void KStars::slotColorScheme() {
    //use mid(3) to exclude the leading "cs_" prefix from the action name
    QString filename = QString( sender()->objectName() ).mid(3) + ".colors";
    loadColorScheme( filename );
}

void KStars::slotTargetSymbol() {
    QString symbolName( sender()->objectName() );

    FOV f( symbolName );

    Options::setFOVName( f.name() );
    Options::setFOVSize( f.size() );
    Options::setFOVShape( f.shape() );
    Options::setFOVColor( f.color() );
    data()->fovSymbol.setName( Options::fOVName() );
    data()->fovSymbol.setSize( Options::fOVSize() );
    data()->fovSymbol.setShape( Options::fOVShape() );
    data()->fovSymbol.setColor( Options::fOVColor() );

    map()->forceUpdate();
}

void KStars::slotFOVEdit() {
    FOVDialog fovdlg( this );
    if ( fovdlg.exec() == QDialog::Accepted ) {
        //replace existing fov.dat with data from the FOVDialog
        QFile f;
        f.setFileName( KStandardDirs::locateLocal( "appdata", "fov.dat" ) );

        //rebuild fov.dat if FOVList is empty
        if ( fovdlg.FOVList.isEmpty() ) {
            f.remove();
            initFOV();
        } else {
            if ( ! f.open( QIODevice::WriteOnly ) ) {
                kDebug() << i18n( "Could not open fov.dat for writing." );
            } else {
                QTextStream ostream(&f);

                foreach ( FOV *fov, fovdlg.FOVList )
                    ostream << fov->name() << ":" << fov->size()
                            << ":" << QString::number( fov->shape() ) 
                            << ":" << fov->color() << endl;

                f.close();
            }
        }

        //repopulate FOV menu  with items from new fov.dat
        fovActionMenu->menu()->clear();

        if ( f.open( QIODevice::ReadOnly ) ) {
            QTextStream stream( &f );
            while ( !stream.atEnd() ) {
                QString line = stream.readLine();
                QStringList fields = line.split( ":" );

                if ( fields.count() == 4 ) {
                    QString nm = fields[0].trimmed();
                    KToggleAction *kta = actionCollection()->add<KToggleAction>( nm.toUtf8() );
                    kta->setText( nm );
                    kta->setObjectName( nm.toUtf8() );
                    kta->setActionGroup( fovGroup );
                    connect( kta, SIGNAL( toggled(bool) ), this, SLOT( slotTargetSymbol() ) );
                    if ( nm == Options::fOVName() ) kta->setChecked( true );
                    fovActionMenu->addAction( kta );
                }
            }
        } else {
            kDebug() << i18n( "Could not open file: %1", f.fileName() );
        }

        fovActionMenu->menu()->addSeparator();
        QAction *ka = actionCollection()->addAction( "edit_fov" );
        ka->setText( i18n( "Edit FOV Symbols..." ) );
        connect( ka, SIGNAL( triggered() ), this, SLOT( slotFOVEdit() ) );
        fovActionMenu->addAction( ka );

        //set FOV to whatever was highlighted in FOV dialog
        if ( fovdlg.FOVList.count() > 0 ) {
            Options::setFOVName( fovdlg.FOVList.at( fovdlg.currentItem() )->name() );
            data()->fovSymbol.setName( Options::fOVName() );
            data()->fovSymbol.setSize( Options::fOVSize() );
            data()->fovSymbol.setShape( Options::fOVShape() );
            data()->fovSymbol.setColor( Options::fOVColor() );
        }

        //Careful!!  If the user selects a small FOV (like HST), this basically crashes kstars :(
        //		//set ZoomLevel to match the FOV symbol
        //		zoom( (double)(map()->width()) * 60.0 / ( 2.0 * dms::DegToRad * data()->fovSymbol.size() ) );
    }
}

void KStars::slotObsList() {
    obsList->show();
}

//Help Menu
void KStars::slotTipOfDay() {
    KTipDialog::showTip(this, "kstars/tips", true);
}

// Toggle to and from full screen mode
void KStars::slotFullScreen()
{
    if ( topLevelWidget()->isFullScreen() ) {
        topLevelWidget()->setWindowState( topLevelWidget()->windowState() & ~Qt::WindowFullScreen ); // reset
    } else {
        topLevelWidget()->setWindowState( topLevelWidget()->windowState() | Qt::WindowFullScreen ); // set
    }
}

void KStars::slotClearAllTrails() {
    //Exclude object with temporary trail
    SkyObject *exOb( NULL );
    if ( map()->focusObject() && map()->focusObject()->isSolarSystem() && data()->temporaryTrail ) {
        exOb = map()->focusObject();
    }

    data()->skyComposite()->clearTrailsExcept( exOb );

    map()->forceUpdate();
}

//toggle display of GUI Items on/off
void KStars::slotShowGUIItem( bool show ) {
    //Toolbars
    if ( sender() == actionCollection()->action( "show_mainToolBar" ) ) {
        Options::setShowMainToolBar( show );
        if ( show ) toolBar("kstarsToolBar")->show();
        else toolBar("kstarsToolBar")->hide();
    }

    if ( sender() == actionCollection()->action( "show_viewToolBar" ) ) {
        Options::setShowViewToolBar( show );
        if ( show ) toolBar( "viewToolBar" )->show();
        else toolBar( "viewToolBar" )->hide();
    }

    if ( sender() == actionCollection()->action( "show_statusBar" ) ) {
        Options::setShowStatusBar( show );
        if ( show ) statusBar()->show();
        else  statusBar()->hide();
    }

    if ( sender() == actionCollection()->action( "show_sbAzAlt" ) ) {
        Options::setShowAltAzField( show );
        if ( ! show ) { statusBar()->changeItem( QString(), 1 ); }
    }

    if ( sender() == actionCollection()->action( "show_sbRADec" ) ) {
        Options::setShowRADecField( show );
        if ( ! show ) { statusBar()->changeItem( QString(), 2 ); }
    }

    //InfoBoxes: we only change options here; these are also connected to slots in
    //InfoBoxes that actually toggle the display.
    if ( sender() == actionCollection()->action( "show_boxes" ) )
        Options::setShowInfoBoxes( show );
    if ( sender() == actionCollection()->action( "show_time_box" ) )
        Options::setShowTimeBox( show );
    if ( sender() == actionCollection()->action( "show_location_box" ) )
        Options::setShowGeoBox( show );
    if ( sender() == actionCollection()->action( "show_focus_box" ) )
        Options::setShowFocusBox( show );
}

void KStars::addColorMenuItem( const QString &name, const QString &actionName ) {
    KToggleAction *kta = actionCollection()->add<KToggleAction>( actionName );
    kta->setText( name );
    kta->setObjectName( actionName );
    kta->setActionGroup( cschemeGroup );
    connect( kta, SIGNAL( toggled( bool ) ), this, SLOT( slotColorScheme() ) );
    colorActionMenu->addAction( kta );
}

void KStars::removeColorMenuItem( const QString &actionName ) {
    kDebug() << "removing " << actionName;
    colorActionMenu->removeAction( actionCollection()->action( actionName ) );
}

void KStars::establishINDI()
{
    if (indimenu == NULL)
        indimenu = new INDIMenu(this);

    if (indidriver == NULL)
        indidriver = new INDIDriver(this);
}

void KStars::slotAboutToQuit()
{
    //store focus values in Options
    //If not trcking and using Alt/Az coords, stor the Alt/Az coordinates
    if( skymap && skymap->focus() && skymap->focus()->ra() ) {
        if ( Options::useAltAz && ! Options::isTracking() ) {
            Options::setFocusRA( skymap->focus()->az()->Degrees() );
            Options::setFocusDec( skymap->focus()->alt()->Degrees() );
        } else {
            Options::setFocusRA( skymap->focus()->ra()->Hours() );
            Options::setFocusDec( skymap->focus()->dec()->Degrees() );
        }
    }

    //Store Window geometry in Options object
    Options::setWindowWidth( width() );
    Options::setWindowHeight( height() );

    //explicitly save the colorscheme data to the config file
    data()->colorScheme()->saveToConfig();

    KConfigGroup cg = KGlobal::config()->group( "MainToolBar" );
    //explicitly save toolbar settings to config file
    toolBar("kstarsToolBar")->saveSettings( cg );
    cg = KGlobal::config()->group( "ViewToolBar" );
    toolBar( "viewToolBar" )->saveSettings( cg );

    //synch the config file with the Config object
    writeConfig();

    //Delete dialog window pointers
    clearCachedFindDialog();

    delete AAVSODialog;
    delete obsList;
    if ( findDialog ) delete findDialog;
    if ( avt ) delete avt;
    if ( sb ) delete sb;
    if ( pv ) delete pv;
    if ( jmt ) delete jmt;

    while ( ! m_ImageViewerList.isEmpty() )
        delete m_ImageViewerList.takeFirst();
}

