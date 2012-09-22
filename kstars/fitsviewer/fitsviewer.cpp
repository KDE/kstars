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

#include <klocale.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kaction.h>

#include <kstandardaction.h>

#include <kdebug.h>
#include <ktoolbar.h>
#include <ktemporaryfile.h>
#include <kmenubar.h>
#include <kstatusbar.h>
#include <klineedit.h>
#include <kicon.h>

#include <KUndoStack>
#include <KTabWidget>
#include <KAction>
#include <KActionCollection>

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

#include <math.h>
#ifndef __MINGW32__
  #include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifndef __MINGW32__
  #include <netinet/in.h>
#endif

#include "fitstab.h"
#include "fitsimage.h"
#include "fitshistogram.h"
#include "ksutils.h"
#include "Options.h"

FITSViewer::FITSViewer (QWidget *parent)
        : KXmlGuiWindow (parent)
{
    fitsTab   = new KTabWidget(this);
    undoGroup = new QUndoGroup(this);

    fitsID = 0;
    markStars = false;

    fitsTab->setTabsClosable(true);

    setCentralWidget(fitsTab);

    connect(fitsTab, SIGNAL(currentChanged(int)), this, SLOT(tabFocusUpdated(int)));
    connect(fitsTab, SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)));

    statusBar()->insertItem(QString(), FITS_POSITION);
    statusBar()->setItemFixed(FITS_POSITION, 100);
    statusBar()->insertItem(QString(), FITS_VALUE);
    statusBar()->setItemFixed(FITS_VALUE, 100);
    statusBar()->insertItem(QString(), FITS_RESOLUTION);
    statusBar()->setItemFixed(FITS_RESOLUTION, 100);
    statusBar()->insertItem(QString(), FITS_ZOOM);
    statusBar()->setItemFixed(FITS_ZOOM, 50);
    statusBar()->insertPermanentItem(i18n("Welcome to KStars FITS Viewer"), FITS_MESSAGE, 1);
    statusBar()->setItemAlignment(FITS_MESSAGE , Qt::AlignLeft);

    KAction *action;
    QFile tempFile;

    action = actionCollection()->addAction("image_histogram");
    action->setText(i18n("Histogram"));
    connect(action, SIGNAL(triggered(bool)), SLOT (histoFITS()));
    action->setShortcuts(KShortcut( Qt::CTRL+Qt::Key_H ));

    if (KSUtils::openDataFile( tempFile, "histogram.png" ) )
    {
        action->setIcon(KIcon(tempFile.fileName()));
        tempFile.close();
    }
    else
        action->setIcon(KIcon("tools-wizard"));

    KStandardAction::open(this,   SLOT(openFile()),   actionCollection());
    saveFileAction    = KStandardAction::save(this,   SLOT(saveFile()),   actionCollection());
    saveFileAsAction  = KStandardAction::saveAs(this, SLOT(saveFileAs()), actionCollection());

    action = actionCollection()->addAction("fits_header");
    action->setIcon(KIcon("document-properties"));
    action->setText(i18n( "FITS Header"));
    connect(action, SIGNAL(triggered(bool) ), SLOT(headerFITS()));

    KStandardAction::close(this,  SLOT(slotClose()),  actionCollection());    

    KStandardAction::copy(this,   SLOT(copyFITS()),   actionCollection());

    KStandardAction::zoomIn(this,     SLOT(ZoomIn()),      actionCollection());
    KStandardAction::zoomOut(this,    SLOT(ZoomOut()),     actionCollection());  
    KStandardAction::actualSize(this, SLOT(ZoomDefault()), actionCollection());

    KAction *kundo = KStandardAction::undo(undoGroup, SLOT(undo()), actionCollection());
    KAction *kredo = KStandardAction::redo(undoGroup, SLOT(redo()), actionCollection());

    connect(undoGroup, SIGNAL(canUndoChanged(bool)), kundo, SLOT(setEnabled(bool)));
    connect(undoGroup, SIGNAL(canRedoChanged(bool)), kredo, SLOT(setEnabled(bool)));

    action = actionCollection()->addAction("image_stats");
    action->setIcon(KIcon("view-statistics"));
    action->setText(i18n( "Statistics"));
    connect(action, SIGNAL(triggered(bool)), SLOT(statFITS()));

    action = actionCollection()->addAction("mark_stars");
    action->setText(i18n( "Mark Stars"));
    connect(action, SIGNAL(triggered(bool)), SLOT(toggleStars()));

    action = actionCollection()->addAction("low_pass_filter");
    action->setText(i18n( "Low Pass Filter"));
    connect(action, SIGNAL(triggered(bool)), SLOT(lowPassFilter()));

    action = actionCollection()->addAction("equalize");
    action->setText(i18n( "Equalize"));
    connect(action, SIGNAL(triggered(bool)), SLOT(equalize()));

    
    /* Create GUI */
    createGUI("fitsviewer.rc");

    setWindowTitle(i18n("KStars FITS Viewer"));

    /* initially resize in accord with KDE rules */
    resize(INITIAL_W, INITIAL_H);
}

FITSViewer::~FITSViewer()
{
    qDeleteAll(fitsImages);
}

int FITSViewer::addFITS(const KUrl *imageName, FITSMode mode, FITSScale filter)
{

    FITSTab *tab = new FITSTab();

    if (tab->loadFITS(imageName,mode, filter) == false)
    {
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
        fitsTab->addTab(tab, imageName->fileName());
        break;

      case FITS_FOCUS:
        fitsTab->addTab(tab, i18n("Focus"));
        break;

    case FITS_GUIDE:
      fitsTab->addTab(tab, i18n("Guide"));
      break;

    }

    connect(tab, SIGNAL(newStatus(QString,FITSBar)), this, SLOT(updateStatusBar(QString,FITSBar)));
    connect(tab->getImage(), SIGNAL(actionUpdated(QString,bool)), this, SLOT(updateAction(QString,bool)));
    connect(tab, SIGNAL(changeStatus(bool)), this, SLOT(updateTabStatus(bool)));

    saveFileAction->setEnabled(true);
    saveFileAsAction->setEnabled(true);

    undoGroup->addStack(tab->getUndoStack());
    tab->tabPositionUpdated();

    fitsImages.push_back(tab);

    fitsTab->setCurrentWidget(tab);

    tab->setUID(fitsID);

    return (fitsID++);
}

bool FITSViewer::updateFITS(const KUrl *imageName, int fitsUID, FITSScale filter)
{
    foreach (FITSTab *tab, fitsImages)
    {
        if (tab->getUID() == fitsUID)
            return tab->loadFITS(imageName, tab->getImage()->getMode(), filter);
    }

    return false;
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
        updateStatusBar(i18np("%1 star detected.", "%1 stars detected.",fitsImages[currentIndex]->getImage()->getDetectedStars(),
                              fitsImages[currentIndex]->getImage()->getDetectedStars()), FITS_MESSAGE);
    else
        updateStatusBar("", FITS_MESSAGE);

}

void FITSViewer::slotClose()
{

    fitsTab->disconnect();

    if (undoGroup->isClean())
        close();
    else
    {
        for (int i=0; i < fitsImages.size(); i++)
            saveUnsaved(i);
    }
}

void FITSViewer::closeEvent(QCloseEvent *ev)
{

    fitsTab->disconnect();


   for (int i=0; i < fitsImages.size(); i++)
         saveUnsaved(i);

    if( undoGroup->isClean() )
        ev->accept();
    else
        ev->ignore();
}

void FITSViewer::openFile()
{
    KUrl fileURL = KFileDialog::getOpenUrl( QDir::homePath(), "*.fits *.fit *.fts|Flexible Image Transport System");
    if (fileURL.isEmpty())
        return;

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
    //currentURL.clear();

    if (fitsImages.empty())
        return;

    fitsImages[fitsTab->currentIndex()]->saveFileAs();

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

    if (targetTab->getUndoStack()->isClean())
        return -1;

    QString caption = i18n( "Save Changes to FITS?" );
    QString message = i18n( "%1 has unsaved changes.  Would you like to save before closing it?" ).arg(targetTab->getCurrentURL()->fileName());
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

    return -1;
}

void FITSViewer::updateStatusBar(const QString &msg, FITSBar id)
{
   statusBar()->changeItem(msg, id);
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

  QString tabText = fitsImages[fitsTab->currentIndex()]->getCurrentURL()->fileName();

  fitsTab->setTabText(fitsTab->currentIndex(), clean ? tabText : tabText + "*");
}

void FITSViewer::closeTab(int index)
{
    if (fitsImages.empty())
        return;

    FITSTab *tab = fitsImages[index];

    if (tab->getImage()->getMode() != FITS_NORMAL)
        return;

    saveUnsaved(index);

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
        actionCollection()->action("mark_stars")->setText( i18n( "Mark Stars" ) );
    }
    else
    {
        markStars = true;
        actionCollection()->action("mark_stars")->setText( i18n( "Unmark Stars" ) );

        updateStatusBar(i18np("%1 star detected.", "%1 stars detected.", fitsImages[fitsTab->currentIndex()]->getImage()->getDetectedStars()), FITS_MESSAGE);

    }

    foreach(FITSTab *tab, fitsImages)
    {
        tab->getImage()->toggleStars(markStars);
        tab->getImage()->updateFrame();
    }

}

void FITSViewer::lowPassFilter()
{
    if (fitsImages.empty())
        return;

  fitsImages[fitsTab->currentIndex()]->lowPassFilter();

}

void FITSViewer::equalize()
{
    if (fitsImages.empty())
        return;

    fitsImages[fitsTab->currentIndex()]->equalize();

}

FITSImage * FITSViewer::getImage(int fitsUID)
{
    if (fitsUID < 0 || fitsUID >= fitsImages.size())
        return NULL;

    return fitsImages[fitsUID]->getImage();

}

#include "fitsviewer.moc"
