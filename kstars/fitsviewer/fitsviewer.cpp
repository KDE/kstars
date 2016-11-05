/***************************************************************************
                          FITSViewer.cpp  -  A FITSViewer for KStars
                             -------------------
    begin                : Thu Jan 22 2004
    copyright            : (C) 2004 by Jasem Mutlaq
    email                : mutlaqja@ikarustech.com

 2006-03-03	Using CFITSIO, Porting to Qt4
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "fitsviewer.h"

#include <QFileDialog>
#include <QDebug>
#include <QTabWidget>
#include <QAction>
#include <QTemporaryFile>
#include <QFile>
#include <QCursor>
#include <QRadioButton>
#include <QClipboard>
#include <QImage>
#include <QRegExp>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QTreeWidget>
#include <QHeaderView>
#include <QApplication>
#include <QUndoStack>
#include <QUndoGroup>
#include <QSignalMapper>
#include <QStatusBar>
#include <QMenuBar>
#include <QKeySequence>

#include <KActionCollection>
#include <KLed>
#include <KMessageBox>
#include <KStandardAction>
#include <KToolBar>
#include <KLocalizedString>

#include "kstars.h"
#include "fitsdata.h"
#include "fitstab.h"
#include "fitsview.h"
#include "fitsdebayer.h"
#include "fitshistogram.h"
#include "ksutils.h"
#include "Options.h"

#ifdef HAVE_INDI
#include "indi/indilistener.h"
#endif

#define INITIAL_W	785
#define INITIAL_H	650

QStringList FITSViewer::filterTypes = QStringList() << I18N_NOOP("Auto Stretch") << I18N_NOOP("High Contrast")
                                                    << I18N_NOOP("Equalize") << I18N_NOOP("High Pass") << I18N_NOOP("Median")
                                                    << I18N_NOOP("Rotate Right") << I18N_NOOP("Rotate Left")
                                                    << I18N_NOOP("Flip Horizontal") << I18N_NOOP("Flip Vertical");

FITSViewer::FITSViewer (QWidget *parent)
        : KXmlGuiWindow (parent)
{
#ifdef Q_OS_OSX
    if(Options::independentWindowFITS())
         setWindowFlags(Qt::Window);
     else{
        setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
        connect(QApplication::instance(), SIGNAL(applicationStateChanged(Qt::ApplicationState)), this, SLOT(changeAlwaysOnTop(Qt::ApplicationState)));
     }
#endif

    fitsTab   = new QTabWidget(this);
    undoGroup = new QUndoGroup(this);

    fitsID = 0;
    debayerDialog= NULL;
    markStars = false;

    lastURL = QUrl(QDir::homePath());

    fitsTab->setTabsClosable(true);

    setWindowIcon(QIcon::fromTheme("kstars_fitsviewer", QIcon(":/icons/breeze/default/kstars_fitsviewer.svg")));

    setCentralWidget(fitsTab);

    connect(fitsTab, SIGNAL(currentChanged(int)), this, SLOT(tabFocusUpdated(int)));
    connect(fitsTab, SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)));

    //These two connections will enable or disable the scope button if a scope is available or not.
    //Of course this is also dependent on the presence of WCS data in the image.

    #ifdef HAVE_INDI
    connect(INDIListener::Instance(), SIGNAL(newTelescope(ISD::GDInterface *)), this, SLOT(updateWCSFunctions()));
    connect(INDIListener::Instance(), SIGNAL(deviceRemoved(ISD::GDInterface *)), this, SLOT(updateWCSFunctions()));
    #endif

    led.setColor(Qt::green);

    fitsPosition.setAlignment(Qt::AlignCenter);
    fitsValue.setAlignment(Qt::AlignCenter);

    //fitsPosition.setFixedWidth(100);
    //fitsValue.setFixedWidth(100);
    fitsWCS.setVisible(false);

    statusBar()->insertPermanentWidget(FITS_WCS, &fitsWCS);
    statusBar()->insertPermanentWidget(FITS_VALUE, &fitsValue);
    statusBar()->insertPermanentWidget(FITS_POSITION, &fitsPosition);
    statusBar()->insertPermanentWidget(FITS_ZOOM, &fitsZoom);
    statusBar()->insertPermanentWidget(FITS_RESOLUTION, &fitsResolution);
    statusBar()->insertPermanentWidget(FITS_LED, &led);

    QAction *action;

    action = actionCollection()->addAction("rotate_right", this, SLOT(rotateCW()));
    action->setText(i18n("Rotate Right"));
    action->setIcon(QIcon::fromTheme("object-rotate-right", QIcon(":/icons/breeze/default/object-rotate-right.svg")));

    action = actionCollection()->addAction("rotate_left", this, SLOT(rotateCCW()));
    action->setText(i18n("Rotate Left"));
    action->setIcon(QIcon::fromTheme("object-rotate-left", QIcon(":/icons/breeze/default/object-rotate-left.svg")));

    action = actionCollection()->addAction("flip_horizontal", this, SLOT(flipHorizontal()));
    action->setText(i18n("Flip Horizontal"));
    action->setIcon(QIcon::fromTheme("object-flip-horizontal", QIcon(":/icons/breeze/default/object-flip-horizontal.svg")));

    action = actionCollection()->addAction("flip_vertical", this, SLOT(flipVertical()));
    action->setText(i18n("Flip Vertical"));
    action->setIcon(QIcon::fromTheme("object-flip-vertical", QIcon(":/icons/breeze/default/object-flip-vertical.svg")));

    action = actionCollection()->addAction("image_histogram");
    action->setText(i18n("Histogram"));
    connect(action, SIGNAL(triggered(bool)), SLOT (histoFITS()));
    actionCollection()->setDefaultShortcut(action, QKeySequence::Replace);

    action->setIcon(QIcon(":/icons/histogram.png"));

    action = KStandardAction::open(this,   SLOT(openFile()),   actionCollection());
    action->setIcon(QIcon::fromTheme("document-open", QIcon(":/icons/breeze/default/document-open.svg")));
    
    saveFileAction    = KStandardAction::save(this,   SLOT(saveFile()),   actionCollection());
    saveFileAction->setIcon(QIcon::fromTheme("document-save", QIcon(":/icons/breeze/default/document-save.svg")));

    action=saveFileAsAction  = KStandardAction::saveAs(this, SLOT(saveFileAs()), actionCollection());
    saveFileAsAction->setIcon(QIcon::fromTheme("document-save_as", QIcon(":/icons/breeze/default/document-save-as.svg")));

    action = actionCollection()->addAction("fits_header");
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL+Qt::Key_H));
    action->setIcon(QIcon::fromTheme("document-properties", QIcon(":/icons/breeze/default/document-properties.svg")));
    action->setText(i18n( "FITS Header"));
    connect(action, SIGNAL(triggered(bool) ), SLOT(headerFITS()));

    action = actionCollection()->addAction("fits_debayer");
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL+Qt::Key_D));
    action->setIcon(QIcon::fromTheme("view-preview", QIcon(":/icons/breeze/default/view-preview.svg")));
    action->setText(i18n( "Debayer..."));
    connect(action, SIGNAL(triggered(bool) ), SLOT(debayerFITS()));

    action = actionCollection()->addAction("image_stretch");
    action->setText(i18n("Auto stretch"));
    connect(action, SIGNAL(triggered(bool)), SLOT (stretchFITS()));
    actionCollection()->setDefaultShortcut(action, QKeySequence::SelectAll);
    action->setIcon(QIcon::fromTheme("transform-move", QIcon(":/icons/breeze/default/transform-move.svg")));

    action = KStandardAction::close(this,  SLOT(close()),  actionCollection());
    action->setIcon(QIcon::fromTheme("window-close", QIcon(":/icons/breeze/default/window-close.svg")));
    
    action = KStandardAction::copy(this,   SLOT(copyFITS()),   actionCollection());
    action->setIcon(QIcon::fromTheme("edit-copy", QIcon(":/icons/breeze/default/edit-copy.svg")));
    
    action=KStandardAction::zoomIn(this,     SLOT(ZoomIn()),      actionCollection());
    action->setIcon(QIcon::fromTheme("zoom-in", QIcon(":/icons/breeze/default/zoom-in.svg")));
    
    action=KStandardAction::zoomOut(this,    SLOT(ZoomOut()),     actionCollection());
    action->setIcon(QIcon::fromTheme("zoom-out", QIcon(":/icons/breeze/default/zoom-out.svg")));
    
    action=KStandardAction::actualSize(this, SLOT(ZoomDefault()), actionCollection());
    action->setIcon(QIcon::fromTheme("zoom-fit-best", QIcon(":/icons/breeze/default/zoom-fit-best.svg")));

    QAction *kundo = KStandardAction::undo(undoGroup, SLOT(undo()), actionCollection());
    kundo->setIcon(QIcon::fromTheme("edit-undo", QIcon(":/icons/breeze/default/edit-undo.svg")));
    
    QAction *kredo = KStandardAction::redo(undoGroup, SLOT(redo()), actionCollection());
    kredo->setIcon(QIcon::fromTheme("edit-redo", QIcon(":/icons/breeze/default/edit-redo.svg")));

    connect(undoGroup, SIGNAL(canUndoChanged(bool)), kundo, SLOT(setEnabled(bool)));
    connect(undoGroup, SIGNAL(canRedoChanged(bool)), kredo, SLOT(setEnabled(bool)));

    action = actionCollection()->addAction("image_stats");
    action->setIcon(QIcon::fromTheme("view-statistics", QIcon(":/icons/breeze/default/view-statistics.svg")));
    action->setText(i18n( "Statistics"));
    connect(action, SIGNAL(triggered(bool)), SLOT(statFITS()));

    action = actionCollection()->addAction("view_crosshair");
    action->setIcon(QIcon::fromTheme("crosshairs", QIcon(":/icons/breeze/default/crosshairs.svg")));
    action->setText(i18n( "Show Cross Hairs"));
    action->setCheckable(true);
    connect(action, SIGNAL(triggered(bool)), SLOT(toggleCrossHair()));

    action = actionCollection()->addAction("view_pixel_grid");
    action->setIcon(QIcon::fromTheme("map-flat", QIcon(":/icons/breeze/default/map-flat.svg")));
    action->setText(i18n( "Show Pixel Gridlines"));
    action->setCheckable(true);
    connect(action, SIGNAL(triggered(bool)), SLOT(togglePixelGrid()));

    action = actionCollection()->addAction("view_eq_grid");
    action->setIcon(QIcon::fromTheme("kstars_grid", QIcon(":/icons/breeze/default/kstars_grid.svg")));
    action->setText(i18n( "Show Equatorial Gridlines"));
    action->setCheckable(true);
    action->setDisabled(true);
    connect(action, SIGNAL(triggered(bool)), SLOT(toggleEQGrid()));

    action = actionCollection()->addAction("view_objects");
    action->setIcon(QIcon::fromTheme("help-hint", QIcon(":/icons/breeze/default/help-hint.svg")));
    action->setText(i18n( "Show Objects in Image"));
    action->setCheckable(true);
    action->setDisabled(true);
    connect(action, SIGNAL(triggered(bool)), SLOT(toggleObjects()));

    action = actionCollection()->addAction("center_telescope");
    action->setIcon(QIcon(":/icons/center_telescope.svg"));
    action->setText(i18n( "Center Telescope\n*No Telescopes Detected*"));
    action->setDisabled(true);
    action->setCheckable(true);
    connect(action, SIGNAL(triggered(bool)), SLOT(centerTelescope()));

    action = actionCollection()->addAction("view_zoom_fit");
    action->setIcon(QIcon::fromTheme("zoom-fit-width", QIcon(":/icons/breeze/default/zoom-fit-width.svg")));
    action->setText(i18n( "Zoom To Fit"));
    connect(action, SIGNAL(triggered(bool)), SLOT(ZoomToFit()));

    action = actionCollection()->addAction("mark_stars");
    action->setText(i18n( "Mark Stars"));
    connect(action, SIGNAL(triggered(bool)), SLOT(toggleStars()));

    QSignalMapper *filterMapper = new QSignalMapper(this);

    int filterCounter=1;

    foreach(QString filter, FITSViewer::filterTypes)
    {

        action = actionCollection()->addAction(QString("filter%1").arg(filterCounter));
        action->setText(i18n(filter.toUtf8().constData()));
        filterMapper->setMapping(action, filterCounter++);
        connect(action, SIGNAL(triggered()), filterMapper, SLOT(map()));


    }

    connect(filterMapper, SIGNAL(mapped(int)), this, SLOT(applyFilter(int)));

    /* Create GUI */
    createGUI("fitsviewerui.rc");

    setWindowTitle(i18n("KStars FITS Viewer"));

    /* initially resize in accord with KDE rules */
    resize(INITIAL_W, INITIAL_H);
}

void FITSViewer::changeAlwaysOnTop(Qt::ApplicationState state)
{
    if(isVisible()){
        if (state == Qt::ApplicationActive)
            setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
        else
            setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
        show();
    }
}

FITSViewer::~FITSViewer()
{
    fitsTab->disconnect();

    qDeleteAll(fitsTabs);
}


void FITSViewer::closeEvent(QCloseEvent * /*event*/)
{
    KStars *ks = KStars::Instance();

    if (ks)
    {
        QAction *a = KStars::Instance()->actionCollection()->action( "show_fits_viewer" );
        QList<FITSViewer *> viewers = KStars::Instance()->findChildren<FITSViewer *>();

        if (a && viewers.count() == 1)
        {
            a->setEnabled(false);
            a->setChecked(false);
        }
    }
}

void FITSViewer::hideEvent(QHideEvent * /*event*/)
{
    KStars *ks = KStars::Instance();

    if (ks)
    {
        QAction *a = KStars::Instance()->actionCollection()->action( "show_fits_viewer" );
        if (a)
        {
            QList<FITSViewer *> viewers = KStars::Instance()->findChildren<FITSViewer *>();

            if (viewers.count() == 1)
                a->setChecked(false);
        }
    }
}

void FITSViewer::showEvent(QShowEvent * /*event*/)
{
    QAction *a = KStars::Instance()->actionCollection()->action( "show_fits_viewer" );
    if (a)
    {
        a->setEnabled(true);
        a->setChecked(true);
    }
}

int FITSViewer::addFITS(const QUrl *imageName, FITSMode mode, FITSScale filter, const QString &previewText, bool silent)
{
    FITSTab *tab = new FITSTab(this);

    led.setColor(Qt::yellow);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    if (tab->loadFITS(imageName,mode, filter, silent) == false)
    {
        QApplication::restoreOverrideCursor();
        led.setColor(Qt::red);
        if (fitsTabs.size() == 0)
        {

            // Close FITS Viewer and let KStars know it is no longer needed in memory.
            close();
            return -2;
        }

        return -1;
    }

    lastURL = QUrl(imageName->url(QUrl::RemoveFilename));

    QApplication::restoreOverrideCursor();
    tab->setPreviewText(previewText);

    switch (mode)
    {
      case FITS_NORMAL:
        fitsTab->addTab(tab, previewText.isEmpty() ? imageName->fileName() : previewText);
        break;

       case FITS_CALIBRATE:
        fitsTab->addTab(tab, i18n("Calibrate"));
        break;

      case FITS_FOCUS:
        fitsTab->addTab(tab, i18n("Focus"));
        break;

    case FITS_GUIDE:
      fitsTab->addTab(tab, i18n("Guide"));
      break;

    case FITS_ALIGN:
      fitsTab->addTab(tab, i18n("Align"));
      break;

    default:
        break;

    }

    connect(tab, SIGNAL(newStatus(QString,FITSBar)), this, SLOT(updateStatusBar(QString,FITSBar)));
    connect(tab->getView(), SIGNAL(actionUpdated(QString,bool)), this, SLOT(updateAction(QString,bool)));
    connect(tab, SIGNAL(changeStatus(bool)), this, SLOT(updateTabStatus(bool)));
    connect(tab, SIGNAL(debayerToggled(bool)), this, SLOT(setDebayerAction(bool)));
    connect(tab->getView(), SIGNAL(wcsToggled(bool)), this, SLOT(updateWCSFunctions()));

    saveFileAction->setEnabled(true);
    saveFileAsAction->setEnabled(true);

    undoGroup->addStack(tab->getUndoStack());

    fitsTabs.push_back(tab);

    fitsMap[fitsID] = tab;

    fitsTab->setCurrentWidget(tab);

    actionCollection()->action("fits_debayer")->setEnabled(tab->getView()->getImageData()->hasDebayer());

    tab->tabPositionUpdated();

    tab->setUID(fitsID);

    led.setColor(Qt::green);

    updateStatusBar(i18n("Ready."), FITS_MESSAGE);

    updateWCSFunctions();
    tab->getView()->setMouseMode(FITSView::dragMouse);

    return (fitsID++);
}

bool FITSViewer::removeFITS(int fitsUID)
{
    FITSTab *tab = fitsMap.value(fitsUID);

    if (tab == NULL)
    {
        qDebug() << "Cannot find tab with UID " << fitsUID << " in the FITS Viewer";
        return false;
    }

    int index = fitsTabs.indexOf(tab);

    if (index >= 0)
    {
        closeTab(index);
        return true;
    }

    return false;

}

bool FITSViewer::updateFITS(const QUrl *imageName, int fitsUID, FITSScale filter, bool silent)
{
    FITSTab *tab = fitsMap.value(fitsUID);

    if (tab == NULL)
    {
        qDebug() << "Cannot find tab with UID " << fitsUID << " in the FITS Viewer";
        return false;
    }

    bool rc=false;

    if (tab->isVisible())
        led.setColor(Qt::yellow);

    if (tab)
    {
        rc = tab->loadFITS(imageName, tab->getView()->getMode(), filter, silent);

        if (rc)
        {
            int tabIndex = fitsTab->indexOf(tab);
            if (tabIndex != -1 && tab->getView()->getMode() == FITS_NORMAL)
            {
                if ( (imageName->path().startsWith("/tmp") || imageName->path().contains("/Temp")) && Options::singlePreviewFITS())
                    fitsTab->setTabText(tabIndex, tab->getPreviewText().isEmpty()? i18n("Preview") : tab->getPreviewText());
                else
                    fitsTab->setTabText(tabIndex, imageName->fileName());
            }

            tab->getUndoStack()->clear();
        }
    }

    if (tab->isVisible())
    {
        if (rc)
            led.setColor(Qt::green);
        else
            led.setColor(Qt::red);
    }

    return rc;
}

void FITSViewer::tabFocusUpdated(int currentIndex)
{
    if (currentIndex < 0 || fitsTabs.empty())
        return;

    fitsTabs[currentIndex]->tabPositionUpdated();

    FITSView *view = fitsTabs[currentIndex]->getView();

    view->toggleStars(markStars);

    if (isVisible())
        view->updateFrame();

    if (markStars)
        updateStatusBar(i18np("%1 star detected.", "%1 stars detected.",view->getImageData()->getDetectedStars()), FITS_MESSAGE);
    else
        updateStatusBar("", FITS_MESSAGE);

    if (view->getImageData()->hasDebayer())
    {
        actionCollection()->action("fits_debayer")->setEnabled(true);

        if (debayerDialog)
        {
            BayerParams param;
            view->getImageData()->getBayerParams(&param);
            debayerDialog->setBayerParams(&param);
        }
    }
    else
        actionCollection()->action("fits_debayer")->setEnabled(false);

    updateStatusBar("", FITS_WCS);
    updateButtonStatus("view_crosshair", "Cross Hairs", getCurrentView()->isCrosshairShown());
    updateButtonStatus("view_eq_grid", "Equatorial Gridines", getCurrentView()->isEQGridShown());
    updateButtonStatus("view_objects", "Objects in Image", getCurrentView()->areObjectsShown());
    updateButtonStatus("view_pixel_grid", "Pixel Gridines", getCurrentView()->isPixelGridShown());
    updateScopeButton();
    updateWCSFunctions();


}

// No need to warn users about unsaved changes in a "viewer".
/*void FITSViewer::slotClose()
{
    int rc=0;
    fitsTab->disconnect();

    if (undoGroup->isClean())
        close();
    else
    {
        for (int i=0; i < fitsTabs.size(); i++)
            if ( (rc=saveUnsaved(i)) == 2)
                return;
    }
}

void FITSViewer::closeEvent(QCloseEvent *ev)
{

    int rc=0;
    fitsTab->disconnect();


   for (int i=0; i < fitsTabs.size(); i++)
       if ( (rc=saveUnsaved(i)) == 2)
       {
           ev->ignore();
           return;
       }

    if( undoGroup->isClean() )
        ev->accept();
    else
        ev->ignore();
}*/

void FITSViewer::openFile()
{
    QUrl fileURL = QFileDialog::getOpenFileUrl(KStars::Instance(), i18n("Open FITS Image"), lastURL, "FITS (*.fits *.fit)");
    if (fileURL.isEmpty())
        return;

    lastURL = QUrl(fileURL.url(QUrl::RemoveFilename));
    QString fpath = fileURL.toLocalFile();
    QString cpath;


    // Make sure we don't have it open already, if yes, switch to it
    foreach (FITSTab *tab, fitsTabs)
    {
        cpath = tab->getCurrentURL()->path();
        if (fpath == cpath)
        {
            fitsTab->setCurrentWidget(tab);
            return;
        }
    }

    addFITS(&fileURL, FITS_NORMAL, FITS_NONE, QString(), false);

}

void FITSViewer::saveFile()
{
    fitsTabs[fitsTab->currentIndex()]->saveFile();
}

void FITSViewer::saveFileAs()
{
    if (fitsTabs.empty())
        return;

    if (fitsTabs[fitsTab->currentIndex()]->saveFileAs() && fitsTabs[fitsTab->currentIndex()]->getView()->getMode() == FITS_NORMAL)
        fitsTab->setTabText(fitsTab->currentIndex(), fitsTabs[fitsTab->currentIndex()]->getCurrentURL()->fileName());

}

void FITSViewer::copyFITS()
{
    if (fitsTabs.empty())
        return;

   fitsTabs[fitsTab->currentIndex()]->copyFITS();
}

void FITSViewer::histoFITS()
{
    if (fitsTabs.empty())
        return;

    fitsTabs[fitsTab->currentIndex()]->histoFITS();
}

void FITSViewer::statFITS()
{
    if (fitsTabs.empty())
        return;

  fitsTabs[fitsTab->currentIndex()]->statFITS();
}

void FITSViewer::stretchFITS()
{
    applyFilter(FITS_AUTO_STRETCH);
}

void FITSViewer::rotateCW()
{
    applyFilter(FITS_ROTATE_CW);
}

void FITSViewer::rotateCCW()
{
    applyFilter(FITS_ROTATE_CCW);
}

void FITSViewer::flipHorizontal()
{
    applyFilter(FITS_FLIP_H);
}

void FITSViewer::flipVertical()
{
    applyFilter(FITS_FLIP_V);
}

void FITSViewer::headerFITS()
{
    if (fitsTabs.empty())
        return;

  fitsTabs[fitsTab->currentIndex()]->headerFITS();
}

void FITSViewer::debayerFITS()
{
    if (debayerDialog == NULL)
    {
        debayerDialog = new FITSDebayer(this);
    }

    FITSView *view = getCurrentView();

    if (view == NULL)
        return;

    BayerParams param;
    view->getImageData()->getBayerParams(&param);
    debayerDialog->setBayerParams(&param);

    debayerDialog->show();

}

int FITSViewer::saveUnsaved(int index)
{
    FITSTab *targetTab = NULL;

    if (index < 0 || index >= fitsTabs.size())
        return -1;
    targetTab = fitsTabs[index];

    if (targetTab->getView()->getMode() != FITS_NORMAL)
        targetTab->getUndoStack()->clear();

    if (targetTab->getUndoStack()->isClean())
        return -1;

    QString caption = i18n( "Save Changes to FITS?" );
    QString message = i18n( "%1 has unsaved changes.  Would you like to save before closing it?", targetTab->getCurrentURL()->fileName());
    int ans = KMessageBox::warningYesNoCancel( 0, message, caption, KStandardGuiItem::save(), KStandardGuiItem::discard() );
    if( ans == KMessageBox::Yes )
    {
        targetTab->saveFile();
        return 0;
    }
    else if( ans == KMessageBox::No )
    {
       targetTab->getUndoStack()->clear();
       return 1;
    }
    else if ( ans == KMessageBox::Cancel)
    {
        return 2;
    }


    return -1;
}

void FITSViewer::updateStatusBar(const QString &msg, FITSBar id)
{
    switch (id)
    {
            case FITS_POSITION:
                fitsPosition.setText(msg);
                break;
            case FITS_RESOLUTION:
                fitsResolution.setText(msg);
                break;
             case FITS_ZOOM:
                fitsZoom.setText(msg);
                break;
             case FITS_WCS:
                fitsWCS.setVisible(true);
                fitsWCS.setText(msg);
                break;
             case FITS_VALUE:
                fitsValue.setText(msg);
                break;
             case FITS_MESSAGE:
                statusBar()->showMessage(msg);
                break;

             default:
                break;

    }
}

void FITSViewer::ZoomIn()
{
    if (fitsTabs.empty())
        return;

  fitsTabs[fitsTab->currentIndex()]->ZoomIn();
}

void FITSViewer::ZoomOut()
{
    if (fitsTabs.empty())
        return;

  fitsTabs[fitsTab->currentIndex()]->ZoomOut();
}

void FITSViewer::ZoomDefault()
{
    if (fitsTabs.empty())
        return;

  fitsTabs[fitsTab->currentIndex()]->ZoomDefault();
}

void FITSViewer::ZoomToFit()
{
    if (fitsTabs.empty())
        return;

  getCurrentView()->ZoomToFit();
}

void FITSViewer::updateAction(const QString &name, bool enable)
{
    QAction *toolAction = NULL;

    toolAction = actionCollection()->action(name);
    if (toolAction != NULL)
        toolAction->setEnabled (enable);
}

void FITSViewer::updateTabStatus(bool clean)
{
    if (fitsTabs.empty() || (fitsTab->currentIndex() >= fitsTabs.size()))
        return;

  if (fitsTabs[fitsTab->currentIndex()]->getView()->getMode() != FITS_NORMAL)
      return;

  //QString tabText = fitsImages[fitsTab->currentIndex()]->getCurrentURL()->fileName();

  QString tabText = fitsTab->tabText(fitsTab->currentIndex());

  fitsTab->setTabText(fitsTab->currentIndex(), clean ? tabText.remove('*') : tabText + '*');
}

void FITSViewer::closeTab(int index)
{
    if (fitsTabs.empty())
        return;

    FITSTab *tab = fitsTabs[index];

    // N.B. We will allow closing of all tabs
    //if (tab->getView()->getMode() != FITS_NORMAL)
    //   return;

    /* Disabling user confirmation for saving edited FITS
       Since in most cases the modifications are done to enhance the view and not to change the data
       This is _intentional_, it's a feature, not a bug! */
    /*int rc = saveUnsaved(index);

    if (rc == 2)
        return;*/

    fitsMap.remove(tab->getUID());
    fitsTabs.removeOne(tab);
    delete tab;

    if (fitsTabs.empty())
    {
        saveFileAction->setEnabled(false);
        saveFileAsAction->setEnabled(false);
    }
}

/**
 This is helper function to make it really easy to make the update the state of toggle buttons
 that either show or hide information in the Current view.  This method would get called both
 when one of them gets pushed and also when tabs are switched.
 */

void FITSViewer::updateButtonStatus(QString action, QString item, bool showing){
    QAction *a=actionCollection()->action(action);
    if (showing)
    {
        a->setText( "Hide " + item );
        a->setChecked(true);
    }
    else
    {
        a->setText( "Show " + item );
        a->setChecked(false);
    }
}

/**
This is a method that either enables or disables the WCS based features in the Current View.
 */

void FITSViewer::updateWCSFunctions()
{
    if (getCurrentView() == NULL)
        return;

    if(getCurrentView()->imageHasWCS()){
        actionCollection()->action("view_eq_grid")->setDisabled(false);
        actionCollection()->action("view_eq_grid")->setText( i18n( "Show Equatorial Gridlines" ));
        actionCollection()->action("view_objects")->setDisabled(false);
        actionCollection()->action("view_objects")->setText( i18n( "Show Objects in Image" ));
        if(getCurrentView()->isTelescopeActive()){
            actionCollection()->action("center_telescope")->setDisabled(false);

            actionCollection()->action("center_telescope")->setText( i18n( "Center Telescope\n*Ready*" ));
        }
        else{
            actionCollection()->action("center_telescope")->setDisabled(true);
            actionCollection()->action("center_telescope")->setText( i18n( "Center Telescope\n*No Telescopes Detected*" ));
        }
    }
    else{
        actionCollection()->action("view_eq_grid")->setDisabled(true);
        actionCollection()->action("view_eq_grid")->setText( i18n( "Show Equatorial Gridlines\n*No WCS Info*" ));
        actionCollection()->action("center_telescope")->setDisabled(true);
        actionCollection()->action("center_telescope")->setText( i18n( "Center Telescope\n*No WCS Info*" ));
        actionCollection()->action("view_objects")->setDisabled(true);
        actionCollection()->action("view_objects")->setText( i18n( "Show Objects in Image\n*No WCS Info*" ));
    }

}

void FITSViewer::updateScopeButton(){
    if(getCurrentView()->getMouseMode()==FITSView::scopeMouse){
        actionCollection()->action("center_telescope")->setChecked(true);
    } else{
        actionCollection()->action("center_telescope")->setChecked(false);
    }
}

/**
 This methood either enables or disables the scope mouse mode so you can slew your scope to coordinates
 just by clicking the mouse on a spot in the image.
 */

void FITSViewer::centerTelescope(){
    if(getCurrentView()->getMouseMode()==FITSView::scopeMouse){
        getCurrentView()->setMouseMode(FITSView::dragMouse);
    } else{
        getCurrentView()->setMouseMode(FITSView::scopeMouse);
    }
    updateScopeButton();
}



void FITSViewer::toggleCrossHair()
{
    getCurrentView()->toggleCrosshair();
    updateButtonStatus("view_crosshair", "Cross Hairs", getCurrentView()->isCrosshairShown());
}
void FITSViewer::toggleEQGrid()
{
    getCurrentView()->toggleEQGrid();
    updateButtonStatus("view_eq_grid", "Equatorial Gridines", getCurrentView()->isEQGridShown());
}

void FITSViewer::toggleObjects()
{
    getCurrentView()->toggleObjects();
    updateButtonStatus("view_objects", "Objects in Image", getCurrentView()->areObjectsShown());
}

void FITSViewer::togglePixelGrid()
{
    getCurrentView()->togglePixelGrid();
    updateButtonStatus("view_pixel_grid", "Pixel Gridines", getCurrentView()->isPixelGridShown());
}

void FITSViewer::toggleStars()
{
    if (markStars)
    {
        markStars = false;
        actionCollection()->action("mark_stars")->setText( i18n( "Mark Stars" ) );
    }
    else
    {
        markStars = true;
        actionCollection()->action("mark_stars")->setText( i18n( "Unmark Stars" ) );
    }

    foreach(FITSTab *tab, fitsTabs)
    {
        tab->getView()->toggleStars(markStars);
        tab->getView()->updateFrame();
    }

}

void FITSViewer::applyFilter(int ftype)
{

    if (fitsTabs.empty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    updateStatusBar(i18n("Processing %1...", filterTypes[ftype-1]), FITS_MESSAGE);
    qApp->processEvents();
    fitsTabs[fitsTab->currentIndex()]->getHistogram()->applyFilter((FITSScale) ftype);
    qApp->processEvents();
    fitsTabs[fitsTab->currentIndex()]->getView()->updateFrame();
    QApplication::restoreOverrideCursor();
    updateStatusBar(i18n("Ready."), FITS_MESSAGE);
}

FITSView * FITSViewer::getView(int fitsUID)
{
    FITSTab *tab = fitsMap.value(fitsUID);

    if (tab)
        return tab->getView();
    else
        return NULL;
}

FITSView *FITSViewer::getCurrentView()
{
    if (fitsTabs.empty() || fitsTab->currentIndex() >= fitsTabs.count() )
        return NULL;

     return fitsTabs[fitsTab->currentIndex()]->getView();
}

void FITSViewer::setDebayerAction(bool enable)
{
    actionCollection()->addAction("fits_debayer")->setEnabled(enable);
}

