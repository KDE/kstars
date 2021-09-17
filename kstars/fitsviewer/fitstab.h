/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "fitscommon.h"

#include <QUndoStack>
#include <QSplitter>
#include <QToolBox>
#include <QUrl>
#include <QWidget>
#include "ui_fitsheaderdialog.h"
#include "ui_statform.h"
#include <QFuture>
#include <QPointer>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QSlider>

#include <memory>

class FITSHistogramEditor;
class FITSView;
class FITSViewer;
class FITSData;

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
        virtual ~FITSTab() override;

        enum
        {
            STAT_WIDTH,
            STAT_HEIGHT,
            STAT_BITPIX,
            STAT_HFR,
            STAT_MIN,
            STAT_MAX,
            STAT_MEAN,
            STAT_MEDIAN,
            STAT_STDDEV
        };

        void clearRecentFITS();
        void selectRecentFITS(int i);
        void loadFile(const QUrl &imageURL, FITSMode mode = FITS_NORMAL, FITSScale filter = FITS_NONE, bool silent = true);
        bool loadData(const QSharedPointer<FITSData> &data, FITSMode mode = FITS_NORMAL, FITSScale filter = FITS_NONE);

        bool saveImage(const QString &filename);

        inline QUndoStack *getUndoStack()
        {
            return undoStack;
        }
        inline QUrl *getCurrentURL()
        {
            return &currentURL;
        }
        inline FITSView *getView()
        {
            return m_View.get();
        }
        inline QPointer<FITSHistogramEditor> getHistogram()
        {
            return m_HistogramEditor;
        }
        inline QPointer<FITSViewer> getViewer()
        {
            return viewer;
        }

        bool saveFile();
        bool saveFileAs();
        void copyFITS();
        void loadFITSHeader();
        void headerFITS();
        void histoFITS();
        void evaluateStats();
        void statFITS();

        void setUID(int newID)
        {
            uid = newID;
        }
        int getUID()
        {
            return uid;
        }

        void saveUnsaved();
        void tabPositionUpdated();
        void selectGuideStar();

        QString getPreviewText() const;
        void setPreviewText(const QString &value);
        bool shouldComputeHFR() const;

    public slots:
        void modifyFITSState(bool clean = true, const QUrl &imageURL = QUrl());
        void ZoomIn();
        void ZoomOut();
        void ZoomDefault();

    protected:
        virtual void closeEvent(QCloseEvent *ev) override;

    private:
        bool setupView(FITSMode mode, FITSScale filter);

        QHBoxLayout* setupStretchBar();
        void setStretchUIValues(bool adjustSliders);
        void rescaleShadows();
        void rescaleMidtones();

        void processData();

        /** Ask user whether he wants to save changes and save if he do. */

        /// The FITSTools Toolbox
        QPointer<QToolBox> fitsTools;
        /// The Splitter for th FITSTools Toolbox
        QPointer<QSplitter> fitsSplitter;
        /// The FITS Header Panel
        QPointer<QDialog> fitsHeaderDialog;
        Ui::fitsHeaderDialog header;
        /// The Statistics Panel
        QPointer<QDialog> statWidget;
        Ui::statForm stat;
        /// FITS Histogram
        QPointer<FITSHistogramEditor> m_HistogramEditor;
        QPointer<FITSViewer> viewer;

        QPointer<QListWidget> recentImages;

        /// FITS image object
        std::unique_ptr<FITSView> m_View;

        /// History for undo/redo
        QUndoStack *undoStack { nullptr };
        /// FITS File name and path
        QUrl currentURL;

        bool mDirty { false };
        QString previewText;
        int uid { 0 };

        // Stretch bar widgets
        std::unique_ptr<QLabel> shadowsLabel, midtonesLabel, highlightsLabel;
        std::unique_ptr<QLabel> shadowsVal, midtonesVal, highlightsVal;
        std::unique_ptr<QSlider> shadowsSlider, midtonesSlider, highlightsSlider;
        std::unique_ptr<QPushButton> stretchButton, autoButton;
        float maxShadows {0.5}, maxMidtones {0.5}, maxHighlights {1.0};

        //QFuture<void> histogramFuture;

    signals:
        void debayerToggled(bool);
        void newStatus(const QString &msg, FITSBar id);
        void changeStatus(bool clean, const QUrl &imageUrl);
        void loaded();
        void failed();
};
