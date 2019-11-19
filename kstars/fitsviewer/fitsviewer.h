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

#pragma once

#include "fitscommon.h"

#include <KLed>
#include <KXmlGui/KXmlGuiWindow>

#include <QLabel>
#include <QList>
#include <QMap>
#include <QUrl>

#ifdef WIN32
// avoid compiler warning when windows.h is included after fitsio.h
#include <windows.h>
#endif

#include <fitsio.h>

class QCloseEvent;
class QUndoGroup;

class QTabWidget;

class FITSDebayer;
class FITSTab;
class FITSView;
class FITSData;

/**
 * @class FITSViewer
 * @short Primary window to view monochrome and color FITS images.
 * The FITSviewer can open multiple images each in a separate. It supports simple filters, histogram transforms, flip and rotation operations, and star detection.
 *
 * @author Jasem Mutlaq
 * @version 1.0
 */
class FITSViewer : public KXmlGuiWindow
{
        Q_OBJECT

    public:
        /** Constructor. */
        explicit FITSViewer(QWidget *parent);
        ~FITSViewer();

        void addFITS(const QUrl &imageName, FITSMode mode = FITS_NORMAL, FITSScale filter = FITS_NONE,
                     const QString &previewText = QString(), bool silent = true);

        bool addFITSFromData(FITSData *data, const QUrl &imageName, int *tab_uid,
			     FITSMode mode = FITS_NORMAL, FITSScale filter = FITS_NONE,
			     const QString &previewText = QString());

        void updateFITS(const QUrl &imageName, int fitsUID, FITSScale filter = FITS_NONE, bool silent = true);
        bool updateFITSFromData(FITSData *data, const QUrl &imageName, int fitsUID,
				int *tab_uid, FITSScale filter = FITS_NONE);
        bool removeFITS(int fitsUID);

        bool isStarsMarked()
        {
            return markStars;
        }

        bool empty() const
        {
            return fitsTabs.empty();
        }
        QList<FITSTab *> getTabs()
        {
            return fitsTabs;
        }
        FITSView *getView(int fitsUID);
        FITSView *getCurrentView();

        static QStringList filterTypes;

    protected:
        void closeEvent(QCloseEvent *) override;
        void hideEvent(QHideEvent *) override;
        void showEvent(QShowEvent *) override;

    public slots:
        void changeAlwaysOnTop(Qt::ApplicationState state);
        void openFile();
        void saveFile();
        void saveFileAs();
        void copyFITS();
        void statFITS();
        void headerFITS();
        void debayerFITS();
        void histoFITS();
        void tabFocusUpdated(int currentIndex);
        void updateStatusBar(const QString &msg, FITSBar id);
        void ZoomIn();
        void ZoomOut();
        void ZoomDefault();
        void ZoomToFit();
        void updateAction(const QString &name, bool enable);
        void updateTabStatus(bool clean);
        void closeTab(int index);
        void toggleStars();
        void toggleCrossHair();
        void toggleEQGrid();
        void toggleObjects();
        void togglePixelGrid();
        void toggle3DGraph();
        void starProfileButtonOff();
        void centerTelescope();
        void updateWCSFunctions();
        void applyFilter(int ftype);
        void toggleStretch();
        void rotateCW();
        void rotateCCW();
        void flipHorizontal();
        void flipVertical();
        void setDebayerAction(bool);
        void updateScopeButton();

    private:
        void updateButtonStatus(const QString &action, const QString &item, bool showing);
        // Shared utilites between the standard and "FromData" addFITS and updateFITS.
        bool addFITSCommon(FITSTab *tab, const QUrl &imageName,
                           FITSMode mode, const QString &previewText);
        bool updateFITSCommon(FITSTab *tab, const QUrl &imageName);
  
        QTabWidget *fitsTabWidget { nullptr };
        QUndoGroup *undoGroup { nullptr };
        FITSDebayer *debayerDialog { nullptr };
        KLed led;
        QLabel fitsPosition, fitsValue, fitsResolution, fitsZoom, fitsWCS;
        QAction *saveFileAction { nullptr };
        QAction *saveFileAsAction { nullptr };
        QList<FITSTab *> fitsTabs;
        int fitsID { 0 };
        bool markStars { false };
        QMap<int, FITSTab *> fitsMap;
        QUrl lastURL;

    signals:
        void trackingStarSelected(int x, int y);
        void loaded(int tabUID);
        void closed(int tabUID);
        void failed();
};
