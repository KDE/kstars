/*
    SPDX-FileCopyrightText: 2004 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Some code fragments were adapted from Peter Kirchgessner's FITS plugin
    SPDX-FileCopyrightText: Peter Kirchgessner <http://members.aol.com/pkirchg>
*/

#pragma once

#include "fitscommon.h"
#include "fitsviewer/stretch.h"

#include <KLed>
#include <kxmlguiwindow.h>
#include <KActionMenu>

#include <QLabel>
#include <QList>
#include <QMap>
#include <QUrl>

#ifndef KSTARS_LITE
#include "fitshistogrameditor.h"
#endif

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
        enum class Mode
        {
            Full,
            LiveStacking
        };

        explicit FITSViewer(QWidget *parent = nullptr, Mode mode = Mode::Full);
        /** Constructor. */
        explicit FITSViewer(QWidget *parent);
        ~FITSViewer() override;

        // Returns the tab id for the tab to be loaded (could be used in updateFile/Data).
        // It may be that the id is not used if the load fails.
        int loadFile(const QUrl &imageName, FITSMode mode = FITS_NORMAL, FITSScale filter = FITS_NONE,
                      const QString &previewText = QString());

        bool loadData(const QSharedPointer<FITSData> &data, const QUrl &imageName, int *tab_uid,
                      FITSMode mode = FITS_NORMAL, FITSScale filter = FITS_NONE,
                      const QString &previewText = QString());

        void updateFile(const QUrl &imageName, int fitsUID, FITSScale filter = FITS_NONE);
        bool updateData(const QSharedPointer<FITSData> &data, const QUrl &imageName, int fitsUID, int *tab_uid,
                        FITSMode mode = FITS_UNKNOWN, FITSScale filter = FITS_NONE, const QString &tabTitle = QString());
        bool removeFITS(int fitsUID);

        bool isStarsMarked()
        {
            return markStars;
        }

        bool empty() const
        {
            return m_Tabs.empty();
        }
        const QList<QPointer<FITSTab>> tabs() const
        {
            return m_Tabs;
        }
        bool getView(int fitsUID, QSharedPointer<FITSView> &view);
        bool getCurrentView(QSharedPointer<FITSView> &view);

        // Checks if the tab is in the fitsMap;
        bool tabExists(int tab_uid);

        static QList<KLocalizedString> filterTypes;

    protected:
        void closeEvent(QCloseEvent *) override;
        void hideEvent(QHideEvent *) override;
        void showEvent(QShowEvent *) override;
        bool eventFilter(QObject *watched, QEvent *event) override;


    public slots:
        void changeAlwaysOnTop(Qt::ApplicationState state);
        void openFile();
        void blink();
        void nextBlink();
        void previousBlink();
        void stack();
        void restack(const QString dir, const int tabUID);
        void saveFile();
        void saveFileAs();
        void copyFITS();
        void statFITS();
        void toggleSelectionMode();
        void headerFITS();
        void debayerFITS();
        void histoFITS();
        void tabFocusUpdated(int currentIndex);
        void updateStatusBar(const QString &msg, FITSBar id);
        void ZoomIn();
        void ZoomOut();
        void ZoomAllIn();
        void ZoomAllOut();
        void ZoomDefault();
        void ZoomToFit();
        void FitToZoom();
        void updateAction(const QString &name, bool enable);
        void updateTabStatus(bool clean, const QUrl &imageURL);
        void closeTab(int index);
        void toggleStars();
        void nextTab();
        void previousTab();
        void toggleCrossHair();
        void toggleClipping();
        void toggleEQGrid();
        void toggleObjects();
        void togglePixelGrid();
        void toggle3DGraph();
        void toggleHiPSOverlay();
        void starProfileButtonOff();
        void centerTelescope();
        void updateWCSFunctions();
        void applyFilter(int ftype);
        void rotateCW();
        void rotateCCW();
        void flipHorizontal();
        void flipVertical();
        void setDebayerAction(bool);
        void updateScopeButton();
        void ROIFixedSize(int s);
        void customROIInputWindow();


    private:
        void updateButtonStatus(const QString &action, const QString &item, bool showing);
        // Shared utilites between the standard and "FromData" addFITS and updateFITS.
        bool addFITSCommon(const QPointer<FITSTab> &tab, const QUrl &imageName,
                           FITSMode mode, const QString &previewText);
        bool updateFITSCommon(const QPointer<FITSTab> &tab, const QUrl &imageName, const QString tabTitle = "");

        QTabWidget *fitsTabWidget { nullptr };
        QUndoGroup *undoGroup { nullptr };
        FITSDebayer *debayerDialog { nullptr };
        KLed led;
        QLabel fitsPosition, fitsValue, fitsResolution, fitsZoom, fitsWCS, fitsHFR, fitsClip;
        QAction *saveFileAction { nullptr };
        QAction *saveFileAsAction { nullptr };
        QList<QPointer<FITSTab>> m_Tabs;
        int fitsID { 0 };
        bool markStars { false };
        QMap<int, QPointer<FITSTab>> fitsMap;
        QUrl lastURL;
        KActionMenu *roiActionMenu { nullptr };
        KActionMenu* roiMenu { nullptr };

        void loadFiles();
        QList<QUrl> m_urls;
        void changeBlink(bool increment);
        static bool m_BlinkBusy;

        // Live Stacking
        Mode m_Mode { Mode::Full };
        bool m_StackBusy { false };
        void createLiveStackingOnly();

    signals:
        void trackingStarSelected(int x, int y);
        void loaded(int tabUID);
        void closed(int tabUID);
        void failed(const QString &errorMessage);
        void terminated();

};
