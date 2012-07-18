/***************************************************************************
                          FITSViewer.cpp  -  A FITSViewer for KStars
                             -------------------
    begin                : Thu Jan 22 2004
    copyright            : (C) 2004 by Jasem Mutlaq
    email                : mutlaqja@ikarustech.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Some code fragments were adapted from Peter Kirchgessner's FITS plugin*
 *   See http://members.aol.com/pkirchg for more details.                  *
 ***************************************************************************/

#ifndef FITSViewer_H_
#define FITSViewer_H_

#include <QList>

#include <KDialog>
#include <kxmlguiwindow.h>

#ifdef WIN32
// avoid compiler warning when windows.h is included after fitsio.h
#include <windows.h>
#endif

#include <fitsio.h>

#include "fitscommon.h"

class QCloseEvent;
class QUndoGroup;

class KUndoStack;
class KTabWidget;
class KUrl;

class FITSImage;
class FITSHistogram;
class FITSTab;


class FITSViewer : public KXmlGuiWindow
{
    Q_OBJECT

public:

    /**Constructor. */
    FITSViewer (QWidget *parent);
    ~FITSViewer();
    bool addFITS(const KUrl *imageName, FITSMode mode=FITS_NORMAL);


protected:

    virtual void closeEvent(QCloseEvent *ev);

public slots:

    void openFile();
    void saveFile();
    void saveFileAs();
    void copyFITS();
    void statFITS();
    void headerFITS();
    void slotClose();
    void histoFITS();
    void tabFocusUpdated(int currentIndex);
    void updateStatusBar(const QString &msg, FITSBar id);
    void ZoomIn();
    void ZoomOut();
    void ZoomDefault();
    void updateAction(const QString &name, bool enable);
    void updateTabStatus(bool clean);
    int saveUnsaved(int index=-1);
    void closeTab(int index);

private:

    KTabWidget *fitsTab;
    QUndoGroup *undoGroup;

    QList<FITSTab*> fitsImages;
};

#endif
