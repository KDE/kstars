/***************************************************************************
                          FITS Tab
                             -------------------
    copyright            : (C) 2012 by Jasem Mutlaq
    email                : mutlaqja@ikarustech.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef FITSTAB_H
#define FITSTAB_H

#include <QWidget>
#include <KUrl>

#include "fitscommon.h"

class QUndoStack;
class FITSImage;
class FITSHistogram;
class FITSHistogramCommand;
class FITSViewer;

class FITSTab : public QWidget
{
    Q_OBJECT
public:

   FITSTab(FITSViewer *parent);
   ~FITSTab();
   bool loadFITS(const KUrl *imageURL, FITSMode mode = FITS_NORMAL, FITSScale filter=FITS_NONE);
   int saveFITS(const QString &filename);

   inline QUndoStack *getUndoStack() { return undoStack; }
   inline KUrl * getCurrentURL() { return &currentURL; }
   inline FITSImage *getImage() { return image; }
   inline FITSHistogram *getHistogram() { return histogram; }
   inline FITSViewer *getViewer() { return viewer; }

   void saveFile();
   void saveFileAs();
   void copyFITS();
   void headerFITS();
   void histoFITS();
   void statFITS();

   void setUID(int newID) { uid = newID; }
   int getUID() { return uid; }

   void saveUnsaved();
   void tabPositionUpdated();
   void selectGuideStar();


public slots:
       void modifyFITSState(bool clean=true);
       void ZoomIn();
       void ZoomOut();
       void ZoomDefault();

protected:
   virtual void closeEvent(QCloseEvent *ev);

private:
    /** Ask user whether he wants to save changes and save if he do. */


    FITSImage *image;           /* FITS image object */
    FITSHistogram *histogram;   /* FITS Histogram */
    FITSViewer *viewer;

    QUndoStack *undoStack;        /* History for undo/redo */
    KUrl currentURL;            /* FITS File name and path */

    bool mDirty;

    int uid;

signals:
    void newStatus(const QString &msg, FITSBar id);
    void changeStatus(bool clean);

};

#endif // FITSTAB_H
