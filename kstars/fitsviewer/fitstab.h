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

class FITSTab : public QWidget
{
    Q_OBJECT
public:

   FITSTab();
   bool loadFITS(const KUrl *imageURL);
   int saveFITS(const QString &filename);
   QUndoStack *getUndoStack() { return undoStack; }
   KUrl * getCurrentURL() { return &currentURL; }
   FITSImage *getImage() { return image; }
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

    QUndoStack *undoStack;        /* History for undo/redo */
    KUrl currentURL;            /* FITS File name and path */

    bool mDirty;

    int uid;

signals:
    void newStatus(const QString &msg, FITSBar id);
    void changeStatus(bool clean);

};

#endif // FITSTAB_H
