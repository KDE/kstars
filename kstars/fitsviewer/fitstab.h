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

#pragma once

#include "fitscommon.h"

#include <QUndoStack>
#include <QUrl>
#include <QWidget>

#include <memory>

class FITSHistogram;
class FITSView;
class FITSViewer;

/**
 * @brief The FITSTab class holds information on the current view (drawing area) in addition to the undo/redo stacks
 *  and status of current document (clean or dirty). It also creates the corresponding histogram associated with the
 *  image data that is stored in the FITSView class.
 * @author Jasem Mutlaq
 */
class FITSTab : public QWidget
{
    Q_OBJECT
  public:
    explicit FITSTab(FITSViewer *parent);
    virtual ~FITSTab();

    bool loadFITS(const QUrl *imageURL, FITSMode mode = FITS_NORMAL, FITSScale filter = FITS_NONE, bool silent = true);
    int saveFITS(const QString &filename);

    inline QUndoStack *getUndoStack() { return undoStack; }
    inline QUrl *getCurrentURL() { return &currentURL; }
    inline FITSView *getView() { return view.get(); }
    inline FITSHistogram *getHistogram() { return histogram; }
    inline FITSViewer *getViewer() { return viewer; }

    bool saveFile();
    bool saveFileAs();
    void copyFITS();
    void headerFITS();
    void histoFITS();
    void statFITS();

    void setUID(int newID) { uid = newID; }
    int getUID() { return uid; }

    void saveUnsaved();
    void tabPositionUpdated();
    void selectGuideStar();

    QString getPreviewText() const;
    void setPreviewText(const QString &value);

  public slots:
    void modifyFITSState(bool clean = true);
    void ZoomIn();
    void ZoomOut();
    void ZoomDefault();

  protected:
    virtual void closeEvent(QCloseEvent *ev);

  private:
    /** Ask user whether he wants to save changes and save if he do. */

    /// FITS image object
    std::unique_ptr<FITSView> view;
    /// FITS Histogram
    FITSHistogram *histogram { nullptr };
    FITSViewer *viewer { nullptr };

    /// History for undo/redo
    QUndoStack *undoStack { nullptr };
    /// FITS File name and path
    QUrl currentURL;

    bool mDirty { false };
    QString previewText;
    int uid { 0 };

  signals:
    void debayerToggled(bool);
    void newStatus(const QString &msg, FITSBar id);
    void changeStatus(bool clean);
};
