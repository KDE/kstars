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

#include <QDialog>
#include <QUrl>
#include <QLabel>
#include <QSlider>
#include <kxmlguiwindow.h>
#include <KLed>

#ifdef WIN32
// avoid compiler warning when windows.h is included after fitsio.h
#include <windows.h>
#endif

#include <fitsio.h>

#include "fitscommon.h"

class QCloseEvent;
class QUndoGroup;

class QTabWidget;
class QUrl;

class FITSView;
class FITSTab;
class FITSDebayer;


/**
 *@class FITSViewer
 *@short Primary window to view monochrome and color FITS images.
 * The FITSviewer can open multiple images each in a separate. It supports simple filters, histogram transforms, flip and roration operations, and star detection.
 *@author Jasem Mutlaq
 *@version 1.0
 */
class FITSViewer : public KXmlGuiWindow
{
    Q_OBJECT

public:

    /**Constructor. */
    FITSViewer (QWidget *parent);
    ~FITSViewer();

    int addFITS(const QUrl *imageName, FITSMode mode=FITS_NORMAL, FITSScale filter=FITS_NONE, const QString &previewText = QString(), bool silent=true);

    bool updateFITS(const QUrl *imageName, int fitsUID, FITSScale filter=FITS_NONE, bool silent=true);
    bool removeFITS(int fitsUID);

    void toggleMarkStars(bool enable) { markStars = enable; }
    bool isStarsMarked() { return markStars; }

    QList<FITSTab*> getTabs() { return fitsTabs; }    
    FITSView *getView(int fitsUID);
    FITSView *getCurrentView();

    static QStringList filterTypes;


protected:
    void closeEvent(QCloseEvent *);
    void hideEvent(QHideEvent *);
    void showEvent(QShowEvent *);

public slots:

    void openFile();
    void saveFile();
    void saveFileAs();
    void copyFITS();
    void statFITS();
    void headerFITS();
    void debayerFITS();
    //void slotClose();
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
    void rotateCW();
    void rotateCCW();
    void flipHorizontal();
    void flipVertical();
    void setGamma(int value);
    void setDebayerAction(bool);

private:

    QTabWidget *fitsTab;
    QUndoGroup *undoGroup;
    FITSDebayer *debayerDialog;
    KLed led;
    QLabel fitsPosition, fitsValue, fitsResolution, fitsZoom, fitsWCS, fitsMessage;
    QSlider gammaSlider;
    QAction *saveFileAction, *saveFileAsAction;
    QList<FITSTab*> fitsTabs;
    int fitsID;
    bool markStars;
    QMap<int, FITSTab*> fitsMap;
    QUrl lastURL;

signals:
    void trackingStarSelected(int x, int y);
};

#endif
