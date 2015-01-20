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
#include <klineedit.h>

#include <cmath>
#ifndef __MINGW32__
  #include <unistd.h>
#endif
#include <cstdlib>
#include <string.h>
#include <errno.h>
#ifndef __MINGW32__
  #include <netinet/in.h>
#endif

#include "fitsimage.h"
#include "fitstab.h"
#include "fitsview.h"
#include "fitshistogram.h"
#include "ksutils.h"
#include "Options.h"

QStringList FITSViewer::filterTypes = QStringList() << I18N_NOOP("Auto Stretch") << I18N_NOOP("High Contrast")
                                                    << I18N_NOOP("Equalize") << I18N_NOOP("High Pass")
                                                    << I18N_NOOP("Rotate Right") << I18N_NOOP("Rotate Left")
                                                    << I18N_NOOP("Flip Horizontal") << I18N_NOOP("Flip Vertical");

FITSViewer::FITSViewer (QWidget *parent)
        : KXmlGuiWindow (parent)
{
    fitsTab   = new QTabWidget(this);
    undoGroup = new QUndoGroup(this);

    fitsID = 0;
    markStars = false;

    fitsTab->setTabsClosable(true);

    setCentralWidget(fitsTab);

    connect(fitsTab, SIGNAL(currentChanged(int)), this, SLOT(tabFocusUpdated(int)));
    connect(fitsTab, SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)));

    led.setColor(Qt::green);

    fitsPosition.setAlignment(Qt::AlignCenter);
    fitsValue.setAlignment(Qt::AlignCenter);

    gammaSlider.setMinimum(0);
    gammaSlider.setMaximum(100);
    gammaSlider.setValue(0);
    gammaSlider.setOrientation(Qt::Horizontal);
    gammaSlider.setFixedWidth(100);

    //fitsPosition.setFixedWidth(100);
    //fitsValue.setFixedWidth(100);
    fitsWCS.setVisible(false);

    connect(&gammaSlider, SIGNAL(valueChanged(int)), this, SLOT(setGamma(int)));

    statusBar()->insertPermanentWidget(FITS_WCS, &fitsWCS);
    statusBar()->insertPermanentWidget(FITS_VALUE, &fitsValue);    
    statusBar()->insertPermanentWidget(FITS_POSITION, &fitsPosition);
    statusBar()->insertPermanentWidget(FITS_ZOOM, &fitsZoom);
    statusBar()->insertPermanentWidget(FITS_RESOLUTION, &fitsResolution);
    statusBar()->insertPermanentWidget(FITS_GAMMA, &gammaSlider);
    statusBar()->insertPermanentWidget(FITS_LED, &led);

    QAction *action;

    action = actionCollection()->addAction("rotate_right", this, SLOT(rotateCW()));
    action->setText(xi18n("Rotate Right"));
    action->setIcon(QIcon::fromTheme("object-rotate-right"));
\
    action = actionCollection()->addAction("rotate_left", this, SLOT(rotateCCW()));
    action->setText(xi18n("Rotate Left"));
    action->setIcon(QIcon::fromTheme("object-rotate-left"));

    action = actionCollection()->addAction("flip_horizontal", this, SLOT(flipHorizontal()));
    action->setText(xi18n("Flip Horizontal"));
    action->setIcon(QIcon::fromTheme("object-flip-horizontal"));

    action = actionCollection()->addAction("flip_vertical", this, SLOT(flipVertical()));
    action->setText(xi18n("Flip Vertical"));
    action->setIcon(QIcon::fromTheme("object-flip-vertical"));

    QFile tempFile;

    action = actionCollection()->addAction("image_histogram");
    action->setText(xi18n("Histogram"));
    connect(action, SIGNAL(triggered(bool)), SLOT (histoFITS()));
    action->setShortcuts(QKeySequence::Replace);

    if (KSUtils::openDataFile( tempFile, "histogram.png" ) )
    {
        action->setIcon(QIcon::fromTheme(tempFile.fileName()));
        tempFile.close();
    }
    else
        action->setIcon(QIcon::fromTheme("tools-wizard"));

    KStandardAction::open(this,   SLOT(openFile()),   actionCollection());
    saveFileAction    = KStandardAction::save(this,   SLOT(saveFile()),   actionCollection());
    saveFileAsAction  = KStandardAction::saveAs(this, SLOT(saveFileAs()), actionCollection());

    action = actionCollection()->addAction("fits_header");
    action->setIcon(QIcon::fromTheme("document-properties"));
    action->setText(xi18n( "FITS Header"));
    connect(action, SIGNAL(triggered(bool) ), SLOT(headerFITS()));

    action = actionCollection()->addAction("image_stretch");
    action->setText(xi18n("Auto stretch"));
    connect(action, SIGNAL(triggered(bool)), SLOT (stretchFITS()));
    action->setShortcuts(QKeySequence::SelectAll);
    action->setIcon(QIcon::fromTheme("transform-move"));

    KStandardAction::close(this,  SLOT(slotClose()),  actionCollection());    

    KStandardAction::copy(this,   SLOT(copyFITS()),   actionCollection());

    KStandardAction::zoomIn(this,     SLOT(ZoomIn()),      actionCollection());
    KStandardAction::zoomOut(this,    SLOT(ZoomOut()),     actionCollection());  
    KStandardAction::actualSize(this, SLOT(ZoomDefault()), actionCollection());

    QAction *kundo = KStandardAction::undo(undoGroup, SLOT(undo()), actionCollection());
    QAction *kredo = KStandardAction::redo(undoGroup, SLOT(redo()), actionCollection());

    connect(undoGroup, SIGNAL(canUndoChanged(bool)), kundo, SLOT(setEnabled(bool)));
    connect(undoGroup, SIGNAL(canRedoChanged(bool)), kredo, SLOT(setEnabled(bool)));

    action = actionCollection()->addAction("image_stats");
    action->setIcon(QIcon::fromTheme("view-statistics"));
    action->setText(xi18n( "Statistics"));
    connect(action, SIGNAL(triggered(bool)), SLOT(statFITS()));

    action = actionCollection()->addAction("mark_stars");
    action->setText(xi18n( "Mark Stars"));
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
    createGUI("fitsviewer.rc");

    setWindowTitle(xi18n("KStars FITS Viewer"));

    /* initially resize in accord with KDE rules */
    resize(INITIAL_W, INITIAL_H);
}

FITSViewer::~FITSViewer()
{
    fitsTab->disconnect();

    qDeleteAll(fitsImages);
}

int FITSViewer::addFITS(const QUrl *imageName, FITSMode mode, FITSScale filter, bool preview)
{

    FITSTab *tab = new FITSTab(this);

    led.setColor(Qt::yellow);

    if (tab->loadFITS(imageName,mode, filter) == false)
    {
        led.setColor(Qt::red);
        if (fitsImages.size() == 0)
        {

            // Close FITS Viewer and let KStars know it is no longer needed in memory.
            close();
            return -2;
        }

        return -1;
    }

    switch (mode)
    {
      case FITS_NORMAL:
        fitsTab->addTab(tab, preview ? i18n("Preview") : imageName->fileName());
        break;

       case FITS_CALIBRATE:
        fitsTab->addTab(tab, xi18n("Calibrate"));
        break;

      case FITS_FOCUS:
        fitsTab->addTab(tab, xi18n("Focus"));
        break;

    case FITS_GUIDE:
      fitsTab->addTab(tab, xi18n("Guide"));
      break;

    default:
        break;

    }

    connect(tab, SIGNAL(newStatus(QString,FITSBar)), this, SLOT(updateStatusBar(QString,FITSBar)));
    connect(tab->getImage(), SIGNAL(actionUpdated(QString,bool)), this, SLOT(updateAction(QString,bool)));
    connect(tab, SIGNAL(changeStatus(bool)), this, SLOT(updateTabStatus(bool)));

    saveFileAction->setEnabled(true);
    saveFileAsAction->setEnabled(true);

    undoGroup->addStack(tab->getUndoStack());    

    fitsImages.push_back(tab);

    fitsMap[fitsID] = tab;

    fitsTab->setCurrentWidget(tab);

    tab->tabPositionUpdated();

    tab->setUID(fitsID);

    led.setColor(Qt::green);

    return (fitsID++);
}

bool FITSViewer::updateFITS(const QUrl *imageName, int fitsUID, FITSScale filter)
{
    FITSTab *tab = fitsMap.value(fitsUID);

    if (tab == NULL)
        return false;

    bool rc=false;

    if (tab->isVisible())
        led.setColor(Qt::yellow);

    if (tab)
    {
        rc = tab->loadFITS(imageName, tab->getImage()->getMode(), filter);

        int tabIndex = fitsTab->indexOf(tab);
        if (tabIndex != -1 && tab->getImage()->getMode() == FITS_NORMAL)
        {
            if (imageName->path().startsWith("/tmp") && Options::singlePreviewFITS())
                fitsTab->setTabText(tabIndex,xi18n("Preview"));
            else
                fitsTab->setTabText(tabIndex, imageName->fileName());
        }

        tab->getUndoStack()->clear();
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
    if (currentIndex < 0 || fitsImages.empty())
        return;

    fitsImages[currentIndex]->tabPositionUpdated();

    fitsImages[currentIndex]->getImage()->toggleStars(markStars);

    if (isVisible())
        fitsImages[currentIndex]->getImage()->updateFrame();

    if (markStars)
        updateStatusBar(xi18np("%1 star detected.", "%1 stars detected.",fitsImages[currentIndex]->getImage()->getImageData()->getDetectedStars()), FITS_MESSAGE);
    else
        updateStatusBar("", FITS_MESSAGE);

    updateStatusBar("", FITS_WCS);

}

void FITSViewer::slotClose()
{

    int rc=0;
    fitsTab->disconnect();

    if (undoGroup->isClean())
        close();
    else
    {
        for (int i=0; i < fitsImages.size(); i++)
            if ( (rc=saveUnsaved(i)) == 2)
                return;
    }
}

void FITSViewer::closeEvent(QCloseEvent *ev)
{

    int rc=0;
    fitsTab->disconnect();


   for (int i=0; i < fitsImages.size(); i++)
       if ( (rc=saveUnsaved(i)) == 2)
       {
           ev->ignore();
           return;
       }

    if( undoGroup->isClean() )
        ev->accept();
    else
        ev->ignore();
}

void FITSViewer::openFile()
{
    QUrl fileURL = QFileDialog::getOpenFileUrl(0, xi18n("Open FITS Image"), lastURL, "FITS (*.fits *.fit)");
    if (fileURL.isEmpty())
        return;

    lastURL = fileURL;
    QString fpath = fileURL.path();
    QString cpath;


    // Make sure we don't have it open already, if yes, switch to it
    foreach (FITSTab *tab, fitsImages)
    {
        cpath = tab->getCurrentURL()->path();
        if (fpath == cpath)
        {
            fitsTab->setCurrentWidget(tab);
            return;
        }
    }

    addFITS(&fileURL);

}

void FITSViewer::saveFile()
{
    fitsImages[fitsTab->currentIndex()]->saveFile();
}

void FITSViewer::saveFileAs()
{
    if (fitsImages.empty())
        return;

    if (fitsImages[fitsTab->currentIndex()]->saveFileAs() && fitsImages[fitsTab->currentIndex()]->getImage()->getMode() == FITS_NORMAL)
        fitsTab->setTabText(fitsTab->currentIndex(), fitsImages[fitsTab->currentIndex()]->getCurrentURL()->fileName());

}

void FITSViewer::copyFITS()
{
    if (fitsImages.empty())
        return;

   fitsImages[fitsTab->currentIndex()]->copyFITS();
}

void FITSViewer::histoFITS()
{
    if (fitsImages.empty())
        return;

    fitsImages[fitsTab->currentIndex()]->histoFITS();
}

void FITSViewer::statFITS()
{
    if (fitsImages.empty())
        return;

  fitsImages[fitsTab->currentIndex()]->statFITS();
}

void FITSViewer::stretchFITS()
{
    if (fitsImages.empty())
        return;

    fitsImages[fitsTab->currentIndex()]->getHistogram()->applyFilter(FITS_AUTO_STRETCH);
    fitsImages[fitsTab->currentIndex()]->getImage()->updateFrame();

}

void FITSViewer::rotateCW()
{
    if (fitsImages.empty())
        return;

    fitsImages[fitsTab->currentIndex()]->getHistogram()->applyFilter(FITS_ROTATE_CW);
    fitsImages[fitsTab->currentIndex()]->getImage()->updateFrame();

}

void FITSViewer::rotateCCW()
{
    if (fitsImages.empty())
        return;

    fitsImages[fitsTab->currentIndex()]->getHistogram()->applyFilter(FITS_ROTATE_CCW);
    fitsImages[fitsTab->currentIndex()]->getImage()->updateFrame();

}

void FITSViewer::flipHorizontal()
{
    if (fitsImages.empty())
        return;

    fitsImages[fitsTab->currentIndex()]->getHistogram()->applyFilter(FITS_FLIP_H);
    fitsImages[fitsTab->currentIndex()]->getImage()->updateFrame();
}

void FITSViewer::flipVertical()
{
    if (fitsImages.empty())
        return;

    fitsImages[fitsTab->currentIndex()]->getHistogram()->applyFilter(FITS_FLIP_V);
    fitsImages[fitsTab->currentIndex()]->getImage()->updateFrame();
}

void FITSViewer::headerFITS()
{
    if (fitsImages.empty())
        return;

  fitsImages[fitsTab->currentIndex()]->headerFITS();
}

int FITSViewer::saveUnsaved(int index)
{
    FITSTab *targetTab = NULL;

    if (index < 0 || index >= fitsImages.size())
        return -1;
    targetTab = fitsImages[index];

    if (targetTab->getImage()->getMode() != FITS_NORMAL)
        targetTab->getUndoStack()->clear();

    if (targetTab->getUndoStack()->isClean())
        return -1;

    QString caption = xi18n( "Save Changes to FITS?" );
    QString message = xi18n( "%1 has unsaved changes.  Would you like to save before closing it?", targetTab->getCurrentURL()->fileName());
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
            case FITS_GAMMA:
                gammaSlider.setValue(msg.toInt());
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
    if (fitsImages.empty())
        return;

  fitsImages[fitsTab->currentIndex()]->ZoomIn();
}

void FITSViewer::ZoomOut()
{
    if (fitsImages.empty())
        return;

  fitsImages[fitsTab->currentIndex()]->ZoomOut();
}

void FITSViewer::ZoomDefault()
{
    if (fitsImages.empty())
        return;

  fitsImages[fitsTab->currentIndex()]->ZoomDefault();
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
    if (fitsImages.empty() || (fitsTab->currentIndex() >= fitsImages.size()))
        return;

  if (fitsImages[fitsTab->currentIndex()]->getImage()->getMode() != FITS_NORMAL)
      return;

  //QString tabText = fitsImages[fitsTab->currentIndex()]->getCurrentURL()->fileName();

  QString tabText = fitsTab->tabText(fitsTab->currentIndex());

  fitsTab->setTabText(fitsTab->currentIndex(), clean ? tabText.remove('*') : tabText + '*');
}

void FITSViewer::closeTab(int index)
{
    if (fitsImages.empty())
        return;

    FITSTab *tab = fitsImages[index];

    if (tab->getImage()->getMode() != FITS_NORMAL)
        return;

    int rc = saveUnsaved(index);

    if (rc == 2)
        return;

    fitsMap.remove(tab->getUID());
    fitsImages.removeOne(tab);
    delete tab;

    if (fitsImages.empty())
    {
        saveFileAction->setEnabled(false);
        saveFileAsAction->setEnabled(false);
    }
}

void FITSViewer::toggleStars()
{


    if (markStars)
    {
        markStars = false;
        actionCollection()->action("mark_stars")->setText( xi18n( "Mark Stars" ) );
    }
    else
    {
        markStars = true;
        actionCollection()->action("mark_stars")->setText( xi18n( "Unmark Stars" ) );
    }

    foreach(FITSTab *tab, fitsImages)
    {
        tab->getImage()->toggleStars(markStars);
        tab->getImage()->updateFrame();
    }

}

void FITSViewer::applyFilter(int ftype)
{

    if (fitsImages.empty())
        return;

    fitsImages[fitsTab->currentIndex()]->getHistogram()->applyFilter((FITSScale) ftype);
    fitsImages[fitsTab->currentIndex()]->getImage()->updateFrame();


}

FITSView * FITSViewer::getImage(int fitsUID)
{
    FITSTab *tab = fitsMap.value(fitsUID);

    if (tab)
        return tab->getImage();
    else
        return NULL;
}

void FITSViewer::setGamma(int value)
{
    if (fitsTab->currentIndex() < 0 || fitsTab->currentIndex() > fitsImages.count())
        return;

        fitsImages[fitsTab->currentIndex()]->getImage()->setGammaValue(value);
}


