/*
    SPDX-FileCopyrightText: 2004 Jasem Mutlaq <mutlaqja@ikarustech.com>

    2006-03-03	Using CFITSIO, Porting to Qt4

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fitsviewer.h"

#include "config-kstars.h"

#include "fitsdata.h"
#include "fitsdebayer.h"
#include "fitstab.h"
#include "fitsview.h"
#include "kstars.h"
#include "ksutils.h"
#include "Options.h"
#ifdef HAVE_INDI
#include "indi/indilistener.h"
#endif

#include <KActionCollection>
#include <KMessageBox>
#include <KToolBar>
#include <KNotifications/KStatusNotifierItem>

#ifndef KSTARS_LITE
#include "fitshistogrameditor.h"
#endif

#include <fits_debug.h>

#define INITIAL_W 785
#define INITIAL_H 640

QStringList FITSViewer::filterTypes =
    QStringList() << I18N_NOOP("Auto Stretch") << I18N_NOOP("High Contrast") << I18N_NOOP("Equalize")
    << I18N_NOOP("High Pass") << I18N_NOOP("Median") << I18N_NOOP("Gaussian blur")
    << I18N_NOOP("Rotate Right") << I18N_NOOP("Rotate Left") << I18N_NOOP("Flip Horizontal")
    << I18N_NOOP("Flip Vertical");

FITSViewer::FITSViewer(QWidget *parent) : KXmlGuiWindow(parent)
{
#ifdef Q_OS_OSX
    if (Options::independentWindowFITS())
        setWindowFlags(Qt::Window);
    else
    {
        setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
        connect(QApplication::instance(), SIGNAL(applicationStateChanged(Qt::ApplicationState)), this,
                SLOT(changeAlwaysOnTop(Qt::ApplicationState)));
    }
#endif

    fitsTabWidget   = new QTabWidget(this);
    undoGroup = new QUndoGroup(this);

    lastURL = QUrl(QDir::homePath());

    fitsTabWidget->setTabsClosable(true);

    setWindowIcon(QIcon::fromTheme("kstars_fitsviewer"));

    setCentralWidget(fitsTabWidget);

    connect(fitsTabWidget, &QTabWidget::currentChanged, this, &FITSViewer::tabFocusUpdated);
    connect(fitsTabWidget, &QTabWidget::tabCloseRequested, this, &FITSViewer::closeTab);

    //These two connections will enable or disable the scope button if a scope is available or not.
    //Of course this is also dependent on the presence of WCS data in the image.

#ifdef HAVE_INDI
    connect(INDIListener::Instance(), &INDIListener::newDevice, this, &FITSViewer::updateWCSFunctions);
    connect(INDIListener::Instance(), &INDIListener::newDevice, this, &FITSViewer::updateWCSFunctions);
#endif

    led.setColor(Qt::green);

    fitsPosition.setAlignment(Qt::AlignCenter);
    fitsPosition.setMinimumWidth(100);
    fitsValue.setAlignment(Qt::AlignCenter);
    fitsValue.setMinimumWidth(40);

    fitsWCS.setVisible(false);

    statusBar()->insertPermanentWidget(FITS_CLIP, &fitsClip);
    statusBar()->insertPermanentWidget(FITS_HFR, &fitsHFR);
    statusBar()->insertPermanentWidget(FITS_WCS, &fitsWCS);
    statusBar()->insertPermanentWidget(FITS_VALUE, &fitsValue);
    statusBar()->insertPermanentWidget(FITS_POSITION, &fitsPosition);
    statusBar()->insertPermanentWidget(FITS_ZOOM, &fitsZoom);
    statusBar()->insertPermanentWidget(FITS_RESOLUTION, &fitsResolution);
    statusBar()->insertPermanentWidget(FITS_LED, &led);

    QAction *action = actionCollection()->addAction("rotate_right", this, SLOT(rotateCW()));

    action->setText(i18n("Rotate Right"));
    action->setIcon(QIcon::fromTheme("object-rotate-right"));

    action = actionCollection()->addAction("rotate_left", this, SLOT(rotateCCW()));
    action->setText(i18n("Rotate Left"));
    action->setIcon(QIcon::fromTheme("object-rotate-left"));

    action = actionCollection()->addAction("flip_horizontal", this, SLOT(flipHorizontal()));
    action->setText(i18n("Flip Horizontal"));
    action->setIcon(
        QIcon::fromTheme("object-flip-horizontal"));

    action = actionCollection()->addAction("flip_vertical", this, SLOT(flipVertical()));
    action->setText(i18n("Flip Vertical"));
    action->setIcon(QIcon::fromTheme("object-flip-vertical"));

    action = actionCollection()->addAction("image_histogram");
    action->setText(i18n("Histogram"));
    connect(action, SIGNAL(triggered(bool)), SLOT(histoFITS()));
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL + Qt::Key_T));

    action->setIcon(QIcon(":/icons/histogram.png"));

    action = KStandardAction::open(this, SLOT(openFile()), actionCollection());
    action->setIcon(QIcon::fromTheme("document-open"));

    saveFileAction = KStandardAction::save(this, SLOT(saveFile()), actionCollection());
    saveFileAction->setIcon(QIcon::fromTheme("document-save"));

    saveFileAsAction = KStandardAction::saveAs(this, SLOT(saveFileAs()), actionCollection());
    saveFileAsAction->setIcon(
        QIcon::fromTheme("document-save_as"));

    action = actionCollection()->addAction("fits_header");
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL + Qt::Key_H));
    action->setIcon(QIcon::fromTheme("document-properties"));
    action->setText(i18n("FITS Header"));
    connect(action, SIGNAL(triggered(bool)), SLOT(headerFITS()));

    action = actionCollection()->addAction("fits_debayer");
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL + Qt::Key_D));
    action->setIcon(QIcon::fromTheme("view-preview"));
    action->setText(i18n("Debayer..."));
    connect(action, SIGNAL(triggered(bool)), SLOT(debayerFITS()));

    action = KStandardAction::close(this, SLOT(close()), actionCollection());
    action->setIcon(QIcon::fromTheme("window-close"));

    action = KStandardAction::copy(this, SLOT(copyFITS()), actionCollection());
    action->setIcon(QIcon::fromTheme("edit-copy"));

    action = KStandardAction::zoomIn(this, SLOT(ZoomIn()), actionCollection());
    action->setIcon(QIcon::fromTheme("zoom-in"));

    action = KStandardAction::zoomOut(this, SLOT(ZoomOut()), actionCollection());
    action->setIcon(QIcon::fromTheme("zoom-out"));

    action = KStandardAction::actualSize(this, SLOT(ZoomDefault()), actionCollection());
    action->setIcon(QIcon::fromTheme("zoom-fit-best"));

    QAction *kundo = KStandardAction::undo(undoGroup, SLOT(undo()), actionCollection());
    kundo->setIcon(QIcon::fromTheme("edit-undo"));

    QAction *kredo = KStandardAction::redo(undoGroup, SLOT(redo()), actionCollection());
    kredo->setIcon(QIcon::fromTheme("edit-redo"));

    connect(undoGroup, SIGNAL(canUndoChanged(bool)), kundo, SLOT(setEnabled(bool)));
    connect(undoGroup, SIGNAL(canRedoChanged(bool)), kredo, SLOT(setEnabled(bool)));

    action = actionCollection()->addAction("image_stats");
    action->setIcon(QIcon::fromTheme("view-statistics"));
    action->setText(i18n("Statistics"));
    connect(action, SIGNAL(triggered(bool)), SLOT(statFITS()));

    action = actionCollection()->addAction("image_roi_stats");

    roiActionMenu = new KActionMenu(QIcon(":/icons/select_stat"), "Selection Statistics", action );
    roiActionMenu->setText(i18n("&Selection Statistics"));
    roiActionMenu->setDelayed(false);
    roiActionMenu->addSeparator();
    connect(roiActionMenu, &QAction::triggered, this, &FITSViewer::toggleSelectionMode);

    KToggleAction *ksa = actionCollection()->add<KToggleAction>("100x100");
    ksa->setText("100x100");
    ksa->setCheckable(false);
    roiActionMenu->addAction(ksa);
    ksa = actionCollection()->add<KToggleAction>("50x50");
    ksa->setText("50x50");
    ksa->setCheckable(false);
    roiActionMenu->addAction(ksa);
    ksa = actionCollection()->add<KToggleAction>("25x25");
    ksa->setText("25x25");
    ksa->setCheckable(false);
    roiActionMenu->addAction(ksa);
    ksa = actionCollection()->add<KToggleAction>("CustomRoi");
    ksa->setText("Custom");
    ksa->setCheckable(false);
    roiActionMenu->addAction(ksa);

    action->setMenu(roiActionMenu->menu());
    action->setIcon(QIcon(":/icons/select_stat"));
    action->setCheckable(true);

    connect(roiActionMenu->menu()->actions().at(1), &QAction::triggered, this, [this] { ROIFixedSize(100); });
    connect(roiActionMenu->menu()->actions().at(2), &QAction::triggered, this, [this] { ROIFixedSize(50); });
    connect(roiActionMenu->menu()->actions().at(3), &QAction::triggered, this, [this] { ROIFixedSize(25); });
    connect(roiActionMenu->menu()->actions().at(4), &QAction::triggered, this, [this] { customROIInputWindow();});
    connect(action, &QAction::triggered, this, &FITSViewer::toggleSelectionMode);

    action = actionCollection()->addAction("view_crosshair");
    action->setIcon(QIcon::fromTheme("crosshairs"));
    action->setText(i18n("Show Cross Hairs"));
    action->setCheckable(true);
    connect(action, SIGNAL(triggered(bool)), SLOT(toggleCrossHair()));

    action = actionCollection()->addAction("view_clipping");
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL + Qt::Key_L));
    action->setIcon(QIcon::fromTheme("media-record"));
    action->setText(i18n("Show Clipping"));
    action->setCheckable(true);
    connect(action, SIGNAL(triggered(bool)), SLOT(toggleClipping()));

    action = actionCollection()->addAction("view_pixel_grid");
    action->setIcon(QIcon::fromTheme("map-flat"));
    action->setText(i18n("Show Pixel Gridlines"));
    action->setCheckable(true);
    connect(action, SIGNAL(triggered(bool)), SLOT(togglePixelGrid()));

    action = actionCollection()->addAction("view_eq_grid");
    action->setIcon(QIcon::fromTheme("kstars_grid"));
    action->setText(i18n("Show Equatorial Gridlines"));
    action->setCheckable(true);
    action->setDisabled(true);
    connect(action, SIGNAL(triggered(bool)), SLOT(toggleEQGrid()));

    action = actionCollection()->addAction("view_objects");
    action->setIcon(QIcon::fromTheme("help-hint"));
    action->setText(i18n("Show Objects in Image"));
    action->setCheckable(true);
    action->setDisabled(true);
    connect(action, SIGNAL(triggered(bool)), SLOT(toggleObjects()));

    action = actionCollection()->addAction("center_telescope");
    action->setIcon(QIcon(":/icons/center_telescope.svg"));
    action->setText(i18n("Center Telescope\n*No Telescopes Detected*"));
    action->setDisabled(true);
    action->setCheckable(true);
    connect(action, SIGNAL(triggered(bool)), SLOT(centerTelescope()));

    action = actionCollection()->addAction("view_zoom_fit");
    action->setIcon(QIcon::fromTheme("zoom-fit-width"));
    action->setText(i18n("Zoom To Fit"));
    connect(action, SIGNAL(triggered(bool)), SLOT(ZoomToFit()));

    action = actionCollection()->addAction("mark_stars");
    action->setIcon(QIcon::fromTheme("glstarbase", QIcon(":/icons/glstarbase.png")));
    action->setText(i18n("Mark Stars"));
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL + Qt::Key_A));
    action->setCheckable(true);
    connect(action, SIGNAL(triggered(bool)), SLOT(toggleStars()));

#ifdef HAVE_DATAVISUALIZATION
    action = actionCollection()->addAction("toggle_3D_graph");
    action->setIcon(QIcon::fromTheme("star_profile", QIcon(":/icons/star_profile.svg")));
    action->setText(i18n("View 3D Graph"));
    action->setCheckable(true);
    connect(action, SIGNAL(triggered(bool)), SLOT(toggle3DGraph()));
#endif


    int filterCounter = 1;

    for (auto &filter : FITSViewer::filterTypes)
    {
        action = actionCollection()->addAction(QString("filter%1").arg(filterCounter));
        action->setText(i18n(filter.toUtf8().constData()));
        connect(action, &QAction::triggered, this, [this, filterCounter] { applyFilter(filterCounter);});
        filterCounter++;
    }

    this->setAttribute(Qt::WA_AlwaysShowToolTips);
    /* Create GUI */
    createGUI("fitsviewerui.rc");

    setWindowTitle(i18nc("@title:window", "KStars FITS Viewer"));

    /* initially resize in accord with KDE rules */
    show();
    resize(INITIAL_W, INITIAL_H);
}

void FITSViewer::changeAlwaysOnTop(Qt::ApplicationState state)
{
    if (isVisible())
    {
        if (state == Qt::ApplicationActive)
            setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
        else
            setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
        show();
    }
}

FITSViewer::~FITSViewer()
{
    //    if (KStars::Instance())
    //    {
    //        for (QPointer<FITSViewer> fv : KStars::Instance()->getFITSViewersList())
    //        {
    //            if (fv.data() == this)
    //            {
    //                KStars::Instance()->getFITSViewersList().removeOne(this);
    //                break;
    //            }
    //        }
    //    }

    fitsTabWidget->disconnect();

    qDeleteAll(fitsTabs);
    fitsTabs.clear();
}

void FITSViewer::closeEvent(QCloseEvent * /*event*/)
{
    KStars *ks = KStars::Instance();

    if (ks)
    {
        QAction *a                  = KStars::Instance()->actionCollection()->action("show_fits_viewer");
        QList<FITSViewer *> viewers = KStars::Instance()->findChildren<FITSViewer *>();

        if (a && viewers.count() == 1)
        {
            a->setEnabled(false);
            a->setChecked(false);
        }
    }

    //emit terminated();
}

void FITSViewer::hideEvent(QHideEvent * /*event*/)
{
    KStars *ks = KStars::Instance();

    if (ks)
    {
        QAction *a = KStars::Instance()->actionCollection()->action("show_fits_viewer");
        if (a)
        {
            QList<FITSViewer *> viewers = KStars::Instance()->findChildren<FITSViewer *>();

            if (viewers.count() <= 1)
                a->setChecked(false);
        }
    }
}

void FITSViewer::showEvent(QShowEvent * /*event*/)
{
    QAction *a = KStars::Instance()->actionCollection()->action("show_fits_viewer");
    if (a)
    {
        a->setEnabled(true);
        a->setChecked(true);
    }
}


namespace
{
QString HFRStatusString(const QSharedPointer<FITSData> &data)
{
    const double hfrValue = data->getHFR();
    if (hfrValue <= 0.0) return QString("");
    if (data->getSkyBackground().starsDetected > 0)
        return
            i18np("HFR:%2 Ecc:%3 %1 star.", "HFR:%2 Ecc:%3 %1 stars.",
                  data->getSkyBackground().starsDetected,
                  QString::number(hfrValue, 'f', 2),
                  QString::number(data->getEccentricity(), 'f', 2));
    else
        return
            i18np("HFR:%2, %1 star.", "HFR:%2, %1 stars.",
                  data->getDetectedStars(),
                  QString::number(hfrValue, 'f', 2));
}

QString HFRClipString(FITSView* view)
{
    if (view->isClippingShown())
    {
        const int numClipped = view->getNumClipped();
        if (numClipped < 0)
            return QString("Clip:failed");
        else
            return QString("Clip:%1").arg(view->getNumClipped());
    }
    return "";
}
}  // namespace

bool FITSViewer::addFITSCommon(FITSTab *tab, const QUrl &imageName,
                               FITSMode mode, const QString &previewText)
{
    int tabIndex = fitsTabWidget->indexOf(tab);
    if (tabIndex != -1)
        return false;

    if (!imageName.isValid())
        lastURL = QUrl(imageName.url(QUrl::RemoveFilename));

    QApplication::restoreOverrideCursor();
    tab->setPreviewText(previewText);

    // Connect tab signals
    connect(tab, &FITSTab::newStatus, this, &FITSViewer::updateStatusBar);
    connect(tab, &FITSTab::changeStatus, this, &FITSViewer::updateTabStatus);
    connect(tab, &FITSTab::debayerToggled, this, &FITSViewer::setDebayerAction);
    // Connect tab view signals
    connect(tab->getView().get(), &FITSView::actionUpdated, this, &FITSViewer::updateAction);
    connect(tab->getView().get(), &FITSView::wcsToggled, this, &FITSViewer::updateWCSFunctions);
    connect(tab->getView().get(), &FITSView::starProfileWindowClosed, this, &FITSViewer::starProfileButtonOff);

    switch (mode)
    {
        case FITS_NORMAL:
            fitsTabWidget->addTab(tab, previewText.isEmpty() ? imageName.fileName() : previewText);
            break;

        case FITS_CALIBRATE:
            fitsTabWidget->addTab(tab, i18n("Calibrate"));
            break;

        case FITS_FOCUS:
            fitsTabWidget->addTab(tab, i18n("Focus"));
            break;

        case FITS_GUIDE:
            fitsTabWidget->addTab(tab, i18n("Guide"));
            break;

        case FITS_ALIGN:
            fitsTabWidget->addTab(tab, i18n("Align"));
            break;

        case FITS_UNKNOWN:
            break;
    }

    saveFileAction->setEnabled(true);
    saveFileAsAction->setEnabled(true);

    undoGroup->addStack(tab->getUndoStack());

    fitsTabs.push_back(tab);

    fitsMap[fitsID] = tab;

    fitsTabWidget->setCurrentWidget(tab);

    actionCollection()->action("fits_debayer")->setEnabled(tab->getView()->imageData()->hasDebayer());

    tab->tabPositionUpdated();

    tab->setUID(fitsID);

    led.setColor(Qt::green);

    if (tab->shouldComputeHFR())
        updateStatusBar(HFRStatusString(tab->getView()->imageData()), FITS_HFR);
    else
        updateStatusBar("", FITS_HFR);
    updateStatusBar(i18n("Ready."), FITS_MESSAGE);

    updateStatusBar(HFRClipString(tab->getView().get()), FITS_CLIP);

    tab->getView()->setCursorMode(FITSView::dragCursor);

    updateWCSFunctions();

    return true;
}

void FITSViewer::loadFile(const QUrl &imageName, FITSMode mode, FITSScale filter, const QString &previewText)
{
    led.setColor(Qt::yellow);
    QApplication::setOverrideCursor(Qt::WaitCursor);

    FITSTab *tab = new FITSTab(this);

    connect(tab, &FITSTab::failed, this, [&](const QString & errorMessage)
    {
        QApplication::restoreOverrideCursor();
        led.setColor(Qt::red);
        if (fitsTabs.size() == 0)
        {
            // Close FITS Viewer and let KStars know it is no longer needed in memory.
            close();
        }

        emit failed(errorMessage);
    });

    connect(tab, &FITSTab::loaded, this, [ = ]()
    {
        if (addFITSCommon(tab, imageName, mode, previewText))
            emit loaded(fitsID++);
    });

    tab->loadFile(imageName, mode, filter);
}

bool FITSViewer::loadData(const QSharedPointer<FITSData> &data, const QUrl &imageName, int *tab_uid, FITSMode mode,
                          FITSScale filter, const QString &previewText)
{
    led.setColor(Qt::yellow);
    QApplication::setOverrideCursor(Qt::WaitCursor);

    FITSTab *tab = new FITSTab(this);

    if (!tab->loadData(data, mode, filter))
    {
        auto errorMessage = tab->getView()->imageData()->getLastError();
        QApplication::restoreOverrideCursor();
        led.setColor(Qt::red);
        if (fitsTabs.size() == 0)
        {
            // Close FITS Viewer and let KStars know it is no longer needed in memory.
            close();
        }
        emit failed(errorMessage);
        return false;
    }

    if (!addFITSCommon(tab, imageName, mode, previewText))
        return false;

    *tab_uid = fitsID++;
    return true;
}

bool FITSViewer::removeFITS(int fitsUID)
{
    FITSTab *tab = fitsMap.value(fitsUID);

    if (tab == nullptr)
    {
        qCWarning(KSTARS_FITS) << "Cannot find tab with UID " << fitsUID << " in the FITS Viewer";
        return false;
    }

    int index = fitsTabs.indexOf(tab);

    if (index >= 0)
    {
        closeTab(index);
        return true;
    }

    return false;
}

void FITSViewer::updateFile(const QUrl &imageName, int fitsUID, FITSScale filter)
{
    FITSTab *tab = fitsMap.value(fitsUID);

    if (tab == nullptr)
    {
        QString message = i18n("Cannot find tab with UID %1 in the FITS Viewer", fitsUID);
        emit failed(message);
        return;
    }

    if (tab->isVisible())
        led.setColor(Qt::yellow);

    // On tab load success
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(tab, &FITSTab::loaded, this, [ = ]()
    {
        if (updateFITSCommon(tab, imageName))
        {
            QObject::disconnect(*conn);
            emit loaded(tab->getUID());
        }
    });

    tab->loadFile(imageName, tab->getView()->getMode(), filter);
}

bool FITSViewer::updateFITSCommon(FITSTab *tab, const QUrl &imageName)
{
    // On tab load success
    int tabIndex = fitsTabWidget->indexOf(tab);
    if (tabIndex == -1)
        return false;

    if (tab->getView()->getMode() == FITS_NORMAL)
    {
        if ((imageName.path().startsWith(QLatin1String("/tmp")) ||
                imageName.path().contains("/Temp")) &&
                Options::singlePreviewFITS())
            fitsTabWidget->setTabText(tabIndex,
                                      tab->getPreviewText().isEmpty() ? i18n("Preview") : tab->getPreviewText());
        else
            fitsTabWidget->setTabText(tabIndex, imageName.fileName());
    }

    tab->getUndoStack()->clear();

    if (tab->isVisible())
        led.setColor(Qt::green);

    if (tab->shouldComputeHFR())
        updateStatusBar(HFRStatusString(tab->getView()->imageData()), FITS_HFR);
    else
        updateStatusBar("", FITS_HFR);

    updateStatusBar(HFRClipString(tab->getView().get()), FITS_CLIP);

    return true;
}

bool FITSViewer::updateData(const QSharedPointer<FITSData> &data, const QUrl &imageName, int fitsUID, int *tab_uid,
                            FITSScale filter, FITSMode mode)
{
    FITSTab *tab = fitsMap.value(fitsUID);

    if (mode != FITS_UNKNOWN)
        tab->getView()->updateMode(mode);

    if (tab == nullptr)
        return false;

    if (tab->isVisible())
        led.setColor(Qt::yellow);

    if (!tab->loadData(data, tab->getView()->getMode(), filter))
        return false;

    if (!updateFITSCommon(tab, imageName))
        return false;

    *tab_uid = tab->getUID();
    return true;
}

void FITSViewer::tabFocusUpdated(int currentIndex)
{
    if (currentIndex < 0 || fitsTabs.empty())
        return;

    fitsTabs[currentIndex]->tabPositionUpdated();

    auto view = fitsTabs[currentIndex]->getView();

    view->toggleStars(markStars);

    if (isVisible())
        view->updateFrame();

    if (fitsTabs[currentIndex]->shouldComputeHFR())
        updateStatusBar(HFRStatusString(view->imageData()), FITS_HFR);
    else
        updateStatusBar("", FITS_HFR);

    updateStatusBar(HFRClipString(fitsTabs[currentIndex]->getView().get()), FITS_CLIP);

    if (view->imageData()->hasDebayer())
    {
        actionCollection()->action("fits_debayer")->setEnabled(true);

        if (debayerDialog)
        {
            BayerParams param;
            view->imageData()->getBayerParams(&param);
            debayerDialog->setBayerParams(&param);
        }
    }
    else
        actionCollection()->action("fits_debayer")->setEnabled(false);

    updateStatusBar("", FITS_WCS);
    connect(view.get(), &FITSView::starProfileWindowClosed, this, &FITSViewer::starProfileButtonOff);
    QSharedPointer<FITSView> currentView;
    if (getCurrentView(currentView))
    {
        updateButtonStatus("toggle_3D_graph", i18n("currentView 3D Graph"), currentView->isStarProfileShown());
        updateButtonStatus("currentView_crosshair", i18n("Cross Hairs"), currentView->isCrosshairShown());
        updateButtonStatus("currentView_clipping", i18n("Clipping"), currentView->isClippingShown());
        updateButtonStatus("currentView_eq_grid", i18n("Equatorial Gridlines"), currentView->isEQGridShown());
        updateButtonStatus("currentView_objects", i18n("Objects in Image"), currentView->areObjectsShown());
        updateButtonStatus("currentView_pixel_grid", i18n("Pixel Gridlines"), currentView->isPixelGridShown());
    }

    updateScopeButton();
    updateWCSFunctions();
}

void FITSViewer::starProfileButtonOff()
{
    updateButtonStatus("toggle_3D_graph", i18n("View 3D Graph"), false);
}

void FITSViewer::openFile()
{
    QUrl fileURL = QFileDialog::getOpenFileUrl(KStars::Instance(), i18nc("@title:window", "Open Image"), lastURL,
                   "Images (*.fits *.fits.fz *.fit *.fts *.xisf "
                   "*.jpg *.jpeg *.png *.gif *.bmp "
                   "*.cr2 *.cr3 *.crw *.nef *.raf *.dng *.arw *.orf)");

    if (fileURL.isEmpty())
        return;

    lastURL = QUrl(fileURL.url(QUrl::RemoveFilename));
    QString fpath = fileURL.toLocalFile();
    QString cpath;

    // Make sure we don't have it open already, if yes, switch to it
    for (auto tab : fitsTabs)
    {
        cpath = tab->getCurrentURL()->path();
        if (fpath == cpath)
        {
            fitsTabWidget->setCurrentWidget(tab);
            return;
        }
    }

    loadFile(fileURL, FITS_NORMAL, FITS_NONE, QString());
}

void FITSViewer::saveFile()
{
    fitsTabs[fitsTabWidget->currentIndex()]->saveFile();
}

void FITSViewer::saveFileAs()
{
    if (fitsTabs.empty())
        return;

    if (fitsTabs[fitsTabWidget->currentIndex()]->saveFileAs() &&
            fitsTabs[fitsTabWidget->currentIndex()]->getView()->getMode() == FITS_NORMAL)
        fitsTabWidget->setTabText(fitsTabWidget->currentIndex(),
                                  fitsTabs[fitsTabWidget->currentIndex()]->getCurrentURL()->fileName());
}

void FITSViewer::copyFITS()
{
    if (fitsTabs.empty())
        return;

    fitsTabs[fitsTabWidget->currentIndex()]->copyFITS();
}

void FITSViewer::histoFITS()
{
    if (fitsTabs.empty())
        return;

    fitsTabs[fitsTabWidget->currentIndex()]->histoFITS();
}

void FITSViewer::statFITS()
{
    if (fitsTabs.empty())
        return;

    fitsTabs[fitsTabWidget->currentIndex()]->statFITS();
}

void FITSViewer::rotateCW()
{
    applyFilter(FITS_ROTATE_CW);
}

void FITSViewer::rotateCCW()
{
    applyFilter(FITS_ROTATE_CCW);
}

void FITSViewer::flipHorizontal()
{
    applyFilter(FITS_MOUNT_FLIP_H);
}

void FITSViewer::flipVertical()
{
    applyFilter(FITS_MOUNT_FLIP_V);
}

void FITSViewer::headerFITS()
{
    if (fitsTabs.empty())
        return;

    fitsTabs[fitsTabWidget->currentIndex()]->headerFITS();
}

void FITSViewer::debayerFITS()
{
    if (debayerDialog == nullptr)
    {
        debayerDialog = new FITSDebayer(this);
    }

    QSharedPointer<FITSView> view;
    if (getCurrentView(view))
    {
        BayerParams param;
        view->imageData()->getBayerParams(&param);
        debayerDialog->setBayerParams(&param);
        debayerDialog->show();
    }
}

void FITSViewer::updateStatusBar(const QString &msg, FITSBar id)
{
    switch (id)
    {
        case FITS_POSITION:
            fitsPosition.setText(msg);
            break;
        case FITS_RESOLUTION:
            fitsResolution.setText(msg);
            break;
        case FITS_ZOOM:
            fitsZoom.setText(msg);
            break;
        case FITS_WCS:
            fitsWCS.setVisible(true);
            fitsWCS.setText(msg);
            break;
        case FITS_VALUE:
            fitsValue.setText(msg);
            break;
        case FITS_HFR:
            fitsHFR.setText(msg);
            break;
        case FITS_CLIP:
            fitsClip.setText(msg);
            break;
        case FITS_MESSAGE:
            statusBar()->showMessage(msg);
            break;

        default:
            break;
    }
}

void FITSViewer::ZoomIn()
{
    if (fitsTabs.empty())
        return;

    fitsTabs[fitsTabWidget->currentIndex()]->ZoomIn();
}

void FITSViewer::ZoomOut()
{
    if (fitsTabs.empty())
        return;

    fitsTabs[fitsTabWidget->currentIndex()]->ZoomOut();
}

void FITSViewer::ZoomDefault()
{
    if (fitsTabs.empty())
        return;

    fitsTabs[fitsTabWidget->currentIndex()]->ZoomDefault();
}

void FITSViewer::ZoomToFit()
{
    if (fitsTabs.empty())
        return;

    QSharedPointer<FITSView> currentView;
    if (getCurrentView(currentView))
        currentView->ZoomToFit();
}

void FITSViewer::updateAction(const QString &name, bool enable)
{
    QAction *toolAction = actionCollection()->action(name);

    if (toolAction != nullptr)
        toolAction->setEnabled(enable);
}

void FITSViewer::updateTabStatus(bool clean, const QUrl &imageURL)
{
    if (fitsTabs.empty() || (fitsTabWidget->currentIndex() >= fitsTabs.size()))
        return;

    if (fitsTabs[fitsTabWidget->currentIndex()]->getView()->getMode() != FITS_NORMAL)
        return;

    //QString tabText = fitsImages[fitsTab->currentIndex()]->getCurrentURL()->fileName();

    QString tabText = imageURL.isEmpty() ? fitsTabWidget->tabText(fitsTabWidget->currentIndex()) : imageURL.fileName();

    fitsTabWidget->setTabText(fitsTabWidget->currentIndex(), clean ? tabText.remove('*') : tabText + '*');
}

void FITSViewer::closeTab(int index)
{
    if (fitsTabs.empty())
        return;

    FITSTab *tab = fitsTabs[index];

    int UID = tab->getUID();

    fitsMap.remove(UID);
    fitsTabs.removeOne(tab);
    delete tab;

    if (fitsTabs.empty())
    {
        saveFileAction->setEnabled(false);
        saveFileAsAction->setEnabled(false);
    }

    emit closed(UID);
}

/**
 This is helper function to make it really easy to make the update the state of toggle buttons
 that either show or hide information in the Current view.  This method would get called both
 when one of them gets pushed and also when tabs are switched.
 */

void FITSViewer::updateButtonStatus(const QString &action, const QString &item, bool showing)
{
    QAction *a = actionCollection()->action(action);
    if (a == nullptr)
        return;

    if (showing)
    {
        a->setText(i18n("Hide %1", item));
        a->setChecked(true);
    }
    else
    {
        a->setText(i18n("Show %1", item));
        a->setChecked(false);
    }
}

/**
This is a method that either enables or disables the WCS based features in the Current View.
 */

void FITSViewer::updateWCSFunctions()
{
    QSharedPointer<FITSView> currentView;
    if (!getCurrentView(currentView))
        return;

    if (currentView->imageHasWCS())
    {
        actionCollection()->action("view_eq_grid")->setDisabled(false);
        actionCollection()->action("view_eq_grid")->setText(i18n("Show Equatorial Gridlines"));
        actionCollection()->action("view_objects")->setDisabled(false);
        actionCollection()->action("view_objects")->setText(i18n("Show Objects in Image"));
        if (currentView->isTelescopeActive())
        {
            actionCollection()->action("center_telescope")->setDisabled(false);
            actionCollection()->action("center_telescope")->setText(i18n("Center Telescope\n*Ready*"));
        }
        else
        {
            actionCollection()->action("center_telescope")->setDisabled(true);
            actionCollection()->action("center_telescope")->setText(i18n("Center Telescope\n*No Telescopes Detected*"));
        }
    }
    else
    {
        actionCollection()->action("view_eq_grid")->setDisabled(true);
        actionCollection()->action("view_eq_grid")->setText(i18n("Show Equatorial Gridlines\n*No WCS Info*"));
        actionCollection()->action("center_telescope")->setDisabled(true);
        actionCollection()->action("center_telescope")->setText(i18n("Center Telescope\n*No WCS Info*"));
        actionCollection()->action("view_objects")->setDisabled(true);
        actionCollection()->action("view_objects")->setText(i18n("Show Objects in Image\n*No WCS Info*"));
    }
}

void FITSViewer::updateScopeButton()
{
    QSharedPointer<FITSView> currentView;
    if (!getCurrentView(currentView))
        return;

    if (currentView->getCursorMode() == FITSView::scopeCursor)
    {
        actionCollection()->action("center_telescope")->setChecked(true);
    }
    else
    {
        actionCollection()->action("center_telescope")->setChecked(false);
    }
}

void FITSViewer::ROIFixedSize(int s)
{
    if (fitsTabs.empty())
        return;

    QSharedPointer<FITSView> currentView;
    if (getCurrentView(currentView))
    {
        if(!currentView->isSelectionRectShown())
        {
            toggleSelectionMode();
            updateButtonStatus("image_roi_stats", i18n("Selection Rectangle"), currentView->isSelectionRectShown());
        }
        currentView->processRectangleFixed(s);
    }
}

void FITSViewer::customROIInputWindow()
{
    if(fitsTabs.empty())
        return;

    QSharedPointer<FITSView> currentView;
    if (getCurrentView(currentView))
    {
        if(!currentView->isSelectionRectShown())
            return;

        int mh = currentView->imageData()->height();
        int mw = currentView->imageData()->width();

        if(mh % 2)
            mh++;
        if(mw % 2)
            mw++;

        QDialog customRoiDialog;
        QFormLayout form(&customRoiDialog);
        QDialogButtonBox buttonBox(QDialogButtonBox:: Ok | QDialogButtonBox:: Cancel, Qt::Horizontal, &customRoiDialog);

        form.addRow(new QLabel(i18n("Size")));

        QLineEdit wle(&customRoiDialog);
        QLineEdit hle(&customRoiDialog);

        wle.setValidator(new QIntValidator(1, mw, &wle));
        hle.setValidator(new QIntValidator(1, mh, &hle));

        form.addRow(i18n("Width"), &wle);
        form.addRow(i18n("Height"), &hle);
        form.addRow(&buttonBox);

        connect(&buttonBox, &QDialogButtonBox::accepted, &customRoiDialog, &QDialog::accept);
        connect(&buttonBox, &QDialogButtonBox::rejected, &customRoiDialog, &QDialog::reject);

        if(customRoiDialog.exec() == QDialog::Accepted)
        {
            QPoint resetCenter = currentView->getSelectionRegion().center();
            int newheight = hle.text().toInt();
            int newwidth = wle.text().toInt();

            newheight = qMin(newheight, mh) ;
            newheight = qMax(newheight, 1) ;
            newwidth = qMin(newwidth, mw);
            newwidth = qMax(newwidth, 1);

            QPoint topLeft = resetCenter;
            QPoint botRight = resetCenter;

            topLeft.setX((topLeft.x() - newwidth / 2));
            topLeft.setY((topLeft.y() - newheight / 2));
            botRight.setX((botRight.x() + newwidth / 2));
            botRight.setY((botRight.y() + newheight / 2));

            emit currentView->setRubberBand(QRect(topLeft, botRight));
            currentView->processRectangle(topLeft, botRight, true);
        }
    }
}
/**
 This method either enables or disables the scope mouse mode so you can slew your scope to coordinates
 just by clicking the mouse on a spot in the image.
 */

void FITSViewer::centerTelescope()
{
    QSharedPointer<FITSView> currentView;
    if (!getCurrentView(currentView))
        return;

    currentView->setScopeButton(actionCollection()->action("center_telescope"));
    if (currentView->getCursorMode() == FITSView::scopeCursor)
    {
        currentView->setCursorMode(currentView->lastMouseMode);
    }
    else
    {
        currentView->lastMouseMode = currentView->getCursorMode();
        currentView->setCursorMode(FITSView::scopeCursor);
    }
    updateScopeButton();
}

void FITSViewer::toggleCrossHair()
{
    if (fitsTabs.empty())
        return;

    QSharedPointer<FITSView> currentView;
    if (!getCurrentView(currentView))
        return;

    currentView->toggleCrosshair();
    updateButtonStatus("view_crosshair", i18n("Cross Hairs"), currentView->isCrosshairShown());
}

void FITSViewer::toggleClipping()
{
    if (fitsTabs.empty())
        return;

    QSharedPointer<FITSView> currentView;
    if (!getCurrentView(currentView))
        return;
    currentView->toggleClipping();
    if (!currentView->isClippingShown())
        fitsClip.clear();
    updateButtonStatus("view_clipping", i18n("Clipping"), currentView->isClippingShown());
}

void FITSViewer::toggleEQGrid()
{
    if (fitsTabs.empty())
        return;

    QSharedPointer<FITSView> currentView;
    if (!getCurrentView(currentView))
        return;

    currentView->toggleEQGrid();
    updateButtonStatus("view_eq_grid", i18n("Equatorial Gridlines"), currentView->isEQGridShown());
}

void FITSViewer::toggleSelectionMode()
{
    if (fitsTabs.empty())
        return;

    QSharedPointer<FITSView> currentView;
    if (!getCurrentView(currentView))
        return;

    currentView->toggleSelectionMode();
    updateButtonStatus("image_roi_stats", i18n("Selection Rectangle"), currentView->isSelectionRectShown());
}

void FITSViewer::toggleObjects()
{
    if (fitsTabs.empty())
        return;

    QSharedPointer<FITSView> currentView;
    if (!getCurrentView(currentView))
        return;

    currentView->toggleObjects();
    updateButtonStatus("view_objects", i18n("Objects in Image"), currentView->areObjectsShown());
}

void FITSViewer::togglePixelGrid()
{
    if (fitsTabs.empty())
        return;

    QSharedPointer<FITSView> currentView;
    if (!getCurrentView(currentView))
        return;

    currentView->togglePixelGrid();
    updateButtonStatus("view_pixel_grid", i18n("Pixel Gridlines"), currentView->isPixelGridShown());
}

void FITSViewer::toggle3DGraph()
{
    if (fitsTabs.empty())
        return;

    QSharedPointer<FITSView> currentView;
    if (!getCurrentView(currentView))
        return;

    currentView->toggleStarProfile();
    updateButtonStatus("toggle_3D_graph", i18n("View 3D Graph"), currentView->isStarProfileShown());
}

void FITSViewer::toggleStars()
{
    if (markStars)
    {
        markStars = false;
        actionCollection()->action("mark_stars")->setText(i18n("Mark Stars"));
    }
    else
    {
        markStars = true;
        actionCollection()->action("mark_stars")->setText(i18n("Unmark Stars"));
    }

    foreach (FITSTab *tab, fitsTabs)
    {
        tab->getView()->toggleStars(markStars);
        tab->getView()->updateFrame();
    }
}

void FITSViewer::applyFilter(int ftype)
{
    if (fitsTabs.empty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    updateStatusBar(i18n("Processing %1...", filterTypes[ftype - 1]), FITS_MESSAGE);
    qApp->processEvents();
    fitsTabs[fitsTabWidget->currentIndex()]->getHistogram()->applyFilter(static_cast<FITSScale>(ftype));
    qApp->processEvents();
    fitsTabs[fitsTabWidget->currentIndex()]->getView()->updateFrame();
    QApplication::restoreOverrideCursor();
    updateStatusBar(i18n("Ready."), FITS_MESSAGE);
}

bool FITSViewer::getView(int fitsUID, QSharedPointer<FITSView> &view)
{
    FITSTab *tab = fitsMap.value(fitsUID);
    if (tab)
    {
        view = tab->getView();
        return true;
    }
    return false;

}

bool FITSViewer::getCurrentView(QSharedPointer<FITSView> &view)
{
    if (fitsTabs.empty() || fitsTabWidget->currentIndex() >= fitsTabs.count())
        return false;

    view = fitsTabs[fitsTabWidget->currentIndex()]->getView();
    return true;
}

void FITSViewer::setDebayerAction(bool enable)
{
    actionCollection()->addAction("fits_debayer")->setEnabled(enable);
}
