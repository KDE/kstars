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
#include <QMap>

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
class KAction;
class KLed;

class FITSView;
class FITSHistogram;
class FITSTab;


class FITSViewer : public KXmlGuiWindow
{
    Q_OBJECT

public:

    /**Constructor. */
    FITSViewer (QWidget *parent);
    ~FITSViewer();
    int addFITS(const KUrl *imageName, FITSMode mode=FITS_NORMAL, FITSScale filter=FITS_NONE, bool preview=false);

    bool updateFITS(const KUrl *imageName, int fitsUID, FITSScale filter=FITS_NONE);

    void toggleMarkStars(bool enable) { markStars = enable; }
    bool isStarsMarked() { return markStars; }

    QList<FITSTab*> getImages() { return fitsImages; }

    FITSView *getImage(int fitsUID);

    static QStringList filterTypes;

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
    void stretchFITS();
    void tabFocusUpdated(int currentIndex);
    void updateStatusBar(const QString &msg, FITSBar id);
    void ZoomIn();
    void ZoomOut();
    void ZoomDefault();
    void updateAction(const QString &name, bool enable);
    void updateTabStatus(bool clean);
    int saveUnsaved(int index);
    void closeTab(int index);
    void toggleStars();
    void applyFilter(int ftype);

private:

    KTabWidget *fitsTab;
    QUndoGroup *undoGroup;

    KLed *led;
    KAction *saveFileAction, *saveFileAsAction;
    QList<FITSTab*> fitsImages;
    int fitsID;
    bool markStars;
    QMap<int, FITSTab*> fitsMap;


signals:
    void guideStarSelected(int x, int y);
};

#endif
