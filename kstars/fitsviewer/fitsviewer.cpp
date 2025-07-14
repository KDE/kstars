/*
    SPDX-FileCopyrightText: 2004 Jasem Mutlaq <mutlaqja@ikarustech.com>

    2006-03-03	Using CFITSIO, Porting to Qt4

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fitsviewer.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtGui/QUndoGroup>
#endif

#include "QtWidgets/qmenu.h"
#include "QtWidgets/qstatusbar.h"
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
#include <QFileDialog>
#include <KMessageBox>
#include <KToolBar>
#include <knotification.h>

#include <fits_debug.h>

#define INITIAL_W 785
#define INITIAL_H 640

bool FITSViewer::m_BlinkBusy = false;

QList<KLocalizedString> FITSViewer::filterTypes = {ki18n("Auto Stretch"), ki18n("High Contrast"), ki18n("Equalize"),
                                                   ki18n("High Pass"), ki18n("Median"), ki18n("Gaussian blur"), ki18n("Rotate Right"), ki18n("Rotate Left"), ki18n("Flip Horizontal"),
                                                   ki18n("Flip Vertical")
                                                  };

FITSViewer::FITSViewer(QWidget *parent) : KXmlGuiWindow(parent)
{
#ifdef Q_OS_MACOS
    if (Options::independentWindowFITS())
        setWindowFlags(Qt::Window);
    else
    {
        setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
        connect(QApplication::instance(), SIGNAL(applicationStateChanged(Qt::ApplicationState)), this,
                SLOT(changeAlwaysOnTop(Qt::ApplicationState)));
    }
#endif

    // Since QSharedPointer is managing it, do not delete automatically.
    setAttribute(Qt::WA_DeleteOnClose, false);

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

    QAction *action = actionCollection()->addAction("rotate_right", this, &FITSViewer::rotateCW);

    action->setText(i18n("Rotate Right"));
    action->setIcon(QIcon::fromTheme("object-rotate-right"));

    action = actionCollection()->addAction("rotate_left", this, &FITSViewer::rotateCCW);
    action->setText(i18n("Rotate Left"));
    action->setIcon(QIcon::fromTheme("object-rotate-left"));

    action = actionCollection()->addAction("flip_horizontal", this, &FITSViewer::flipHorizontal);
    action->setText(i18n("Flip Horizontal"));
    action->setIcon(
        QIcon::fromTheme("object-flip-horizontal"));

    action = actionCollection()->addAction("flip_vertical", this, &FITSViewer::flipVertical);
    action->setText(i18n("Flip Vertical"));
    action->setIcon(QIcon::fromTheme("object-flip-vertical"));

    action = actionCollection()->addAction("image_histogram");
    action->setText(i18n("Histogram"));
    connect(action, &QAction::triggered, this, &FITSViewer::histoFITS);
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL | Qt::Key_T));

    action->setIcon(QIcon(":/icons/histogram.png"));

    action = KStandardAction::open(this, &FITSViewer::openFile, actionCollection());
    action->setIcon(QIcon::fromTheme("document-open"));

    action = actionCollection()->addAction("blink");
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL | Qt::Key_O | Qt::AltModifier));
    action->setText(i18n("Open/Blink Directory"));
    connect(action, &QAction::triggered, this, &FITSViewer::blink);

    saveFileAction = KStandardAction::save(this, &FITSViewer::saveFile, actionCollection());
    saveFileAction->setIcon(QIcon::fromTheme("document-save"));

    saveFileAsAction = KStandardAction::saveAs(this, &FITSViewer::saveFileAs, actionCollection());
    saveFileAsAction->setIcon(
        QIcon::fromTheme("document-save_as"));

    action = actionCollection()->addAction("fits_header");
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL | Qt::Key_H));
    action->setIcon(QIcon::fromTheme("document-properties"));
    action->setText(i18n("FITS Header"));
    connect(action, &QAction::triggered, this, &FITSViewer::headerFITS);

    action = actionCollection()->addAction("fits_debayer");
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL | Qt::Key_D));
    action->setIcon(QIcon::fromTheme("view-preview"));
    action->setText(i18n("Debayer..."));
    connect(action, &QAction::triggered, this, &FITSViewer::debayerFITS);

    action = KStandardAction::close(this, &FITSViewer::close, actionCollection());
    action->setIcon(QIcon::fromTheme("window-close"));

    action = KStandardAction::copy(this, &FITSViewer::copyFITS, actionCollection());
    action->setIcon(QIcon::fromTheme("edit-copy"));

    action = KStandardAction::zoomIn(this, &FITSViewer::ZoomIn, actionCollection());
    action->setIcon(QIcon::fromTheme("zoom-in"));

    action = KStandardAction::zoomOut(this, &FITSViewer::ZoomOut, actionCollection());
    action->setIcon(QIcon::fromTheme("zoom-out"));

    action = KStandardAction::actualSize(this, &FITSViewer::ZoomDefault, actionCollection());
    action->setIcon(QIcon::fromTheme("zoom-fit-best"));

    QAction *kundo = KStandardAction::undo(undoGroup, &QUndoGroup::undo, actionCollection());
    kundo->setIcon(QIcon::fromTheme("edit-undo"));

    QAction *kredo = KStandardAction::redo(undoGroup, &QUndoGroup::redo, actionCollection());
    kredo->setIcon(QIcon::fromTheme("edit-redo"));

    connect(undoGroup, &QUndoGroup::canUndoChanged, kundo, &QAction::setEnabled);
    connect(undoGroup, &QUndoGroup::canRedoChanged, kredo, &QAction::setEnabled);

    action = actionCollection()->addAction("image_stats");
    action->setIcon(QIcon::fromTheme("view-statistics"));
    action->setText(i18n("Statistics"));
    connect(action, &QAction::triggered, this, &FITSViewer::statFITS);

    action = actionCollection()->addAction("image_roi_stats");

    roiActionMenu = new KActionMenu(QIcon(":/icons/select_stat"), "Selection Statistics", action );
    roiActionMenu->setText(i18n("&Selection Statistics"));
    roiActionMenu->setPopupMode(QToolButton::InstantPopup);
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
    connect(action, &QAction::triggered, this, &FITSViewer::toggleCrossHair);

    action = actionCollection()->addAction("view_clipping");
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL | Qt::Key_L));
    action->setIcon(QIcon::fromTheme("media-record"));
    action->setText(i18n("Show Clipping"));
    action->setCheckable(true);
    connect(action, &QAction::triggered, this, &FITSViewer::toggleClipping);

    action = actionCollection()->addAction("view_pixel_grid");
    action->setIcon(QIcon::fromTheme("map-flat"));
    action->setText(i18n("Show Pixel Gridlines"));
    action->setCheckable(true);
    connect(action, &QAction::triggered, this, &FITSViewer::togglePixelGrid);

    action = actionCollection()->addAction("view_eq_grid");
    action->setIcon(QIcon::fromTheme("kstars_grid"));
    action->setText(i18n("Show Equatorial Gridlines"));
    action->setCheckable(true);
    action->setDisabled(true);
    connect(action, &QAction::triggered, this, &FITSViewer::toggleEQGrid);

    action = actionCollection()->addAction("view_objects");
    action->setIcon(QIcon::fromTheme("help-hint"));
    action->setText(i18n("Show Objects in Image"));
    action->setCheckable(true);
    action->setDisabled(true);
    connect(action, &QAction::triggered, this, &FITSViewer::toggleObjects);

    action = actionCollection()->addAction("view_hips_overlay");
    action->setIcon(QIcon::fromTheme("pixelate"));
    action->setText(i18n("Show HiPS Overlay"));
    action->setCheckable(true);
    action->setDisabled(true);
    connect(action, &QAction::triggered, this, &FITSViewer::toggleHiPSOverlay);

    action = actionCollection()->addAction("center_telescope");
    action->setIcon(QIcon(":/icons/center_telescope.svg"));
    action->setText(i18n("Center Telescope\n*No Telescopes Detected*"));
    action->setDisabled(true);
    action->setCheckable(true);
    connect(action, &QAction::triggered, this, &FITSViewer::centerTelescope);

    action = actionCollection()->addAction("view_zoom_fit");
    action->setIcon(QIcon::fromTheme("zoom-fit-width"));
    action->setText(i18n("Zoom To Fit"));
    connect(action, &QAction::triggered, this, &FITSViewer::ZoomToFit);

    action = actionCollection()->addAction("view_fit_page");
    action->setIcon(QIcon::fromTheme("zoom-original"));
    action->setText(i18n("Fit Page to Zoom"));
    connect(action, &QAction::triggered, this, &FITSViewer::FitToZoom);

    action = actionCollection()->addAction("next_tab");
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL | Qt::Key_Tab));
    action->setText(i18n("Next Tab"));
    connect(action, &QAction::triggered, this, &FITSViewer::nextTab);

    action = actionCollection()->addAction("previous_tab");
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL | Qt::Key_Tab | Qt::ShiftModifier));
    action->setText(i18n("Previous Tab"));
    connect(action, &QAction::triggered, this, &FITSViewer::previousTab);

    action = actionCollection()->addAction("next_blink");
    actionCollection()->setDefaultShortcut(action, QKeySequence(QKeySequence::SelectNextWord));
    action->setText(i18n("Next Blink Image"));
    connect(action, &QAction::triggered, this, &FITSViewer::nextBlink);

    action = actionCollection()->addAction("previous_blink");
    actionCollection()->setDefaultShortcut(action, QKeySequence(QKeySequence::SelectPreviousWord));
    action->setText(i18n("Previous Blink Image"));
    connect(action, &QAction::triggered, this, &FITSViewer::previousBlink);

    action = actionCollection()->addAction("zoom_all_in");
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL | Qt::Key_Plus | Qt::AltModifier));
    action->setText(i18n("Zoom all tabs in"));
    connect(action, &QAction::triggered, this, &FITSViewer::ZoomAllIn);

    action = actionCollection()->addAction("zoom_all_out");
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL | Qt::Key_Minus | Qt::AltModifier));
    action->setText(i18n("Zoom all tabs out"));
    connect(action, &QAction::triggered, this, &FITSViewer::ZoomAllOut);

    action = actionCollection()->addAction("mark_stars");
    action->setIcon(QIcon::fromTheme("glstarbase", QIcon(":/icons/glstarbase.png")));
    action->setText(i18n("Mark Stars"));
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL | Qt::Key_A));
    action->setCheckable(true);
    connect(action, &QAction::triggered, this, &FITSViewer::toggleStars);

#ifdef HAVE_DATAVISUALIZATION
    action = actionCollection()->addAction("toggle_3D_graph");
    action->setIcon(QIcon::fromTheme("star_profile", QIcon(":/icons/star_profile.svg")));
    action->setText(i18n("View 3D Graph"));
    action->setCheckable(true);
    connect(action, &QAction::triggered, this, &FITSViewer::toggle3DGraph);
#endif


    int filterCounter = 1;

    for (auto &filter : FITSViewer::filterTypes)
    {
        action = actionCollection()->addAction(QString("filter%1").arg(filterCounter));
        action->setText(i18n(filter.toString().toUtf8().constData()));
        connect(action, &QAction::triggered, this, [this, filterCounter] { applyFilter(filterCounter);});
        filterCounter++;
    }

    this->setAttribute(Qt::WA_AlwaysShowToolTips);
    /* Create GUI */
    createGUI("fitsviewerui.rc");

    setWindowTitle(i18nc("@title:window", "KStars FITS Viewer"));

    /* initially resize in accord with KDE rules */
    show();
    if (qgetenv("KDE_FULL_SESSION") != "true")
    {
        if (!Options::fITSWindowGeometry().isEmpty())
            restoreGeometry(QByteArray::fromBase64(Options::fITSWindowGeometry().toLatin1()));
        else
            resize(INITIAL_W, INITIAL_H);
    }
    else
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
}

void FITSViewer::closeEvent(QCloseEvent * /*event*/)
{
    KStars *ks = KStars::Instance();

    if (qgetenv("KDE_FULL_SESSION") != "true")
    {
        // Admittedly, this isn't as obvious as KStars/INDI/Ekos geometry, as there
        // could be multiple FITS Windows. However, guessing that it's better to save the
        // geometry that not.
        Options::setFITSWindowGeometry(QString::fromLatin1(saveGeometry().toBase64()));
    }

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

    emit terminated();
}

void FITSViewer::hideEvent(QHideEvent * /*event*/)
{
    if (qgetenv("KDE_FULL_SESSION") != "true")
        Options::setFITSWindowGeometry(QString::fromLatin1(saveGeometry().toBase64()));

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

bool FITSViewer::addFITSCommon(const QPointer<FITSTab> &tab, const QUrl &imageName,
                               FITSMode mode, const QString &previewText)
{
    int tabIndex = fitsTabWidget->indexOf(tab);
    if (tabIndex != -1)
        return false;

    if (!imageName.isValid())
        lastURL = QUrl(imageName.url(QUrl::RemoveFilename));

    if (mode != FITS_LIVESTACKING)
        QApplication::restoreOverrideCursor();
    tab->setPreviewText(previewText);

    // Connect tab signals
    tab->disconnect(this);
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
        case FITS_LIVESTACKING:
        case FITS_CALIBRATE:
            fitsTabWidget->addTab(tab, previewText.isEmpty() ? imageName.fileName() : previewText);
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

    actionCollection()->action("next_blink")->setEnabled(tab->blinkFilenames().size() > 1);
    actionCollection()->action("previous_blink")->setEnabled(tab->blinkFilenames().size() > 1);

    updateWCSFunctions();

    return true;
}

void FITSViewer::loadFiles()
{
    if (m_urls.size() == 0)
        return;

    const QUrl imageName = m_urls[0];
    m_urls.pop_front();

    // Make sure we don't have it open already, if yes, switch to it
    QString fpath = imageName.toLocalFile();
    for (auto tab : m_Tabs)
    {
        const QString cpath = tab->getCurrentURL()->path();
        if (fpath == cpath)
        {
            fitsTabWidget->setCurrentWidget(tab);
            if (m_urls.size() > 0)
                loadFiles();
            return;
        }
    }

    led.setColor(Qt::yellow);
    QApplication::setOverrideCursor(Qt::WaitCursor);

    QPointer<FITSTab> tab(new FITSTab(this));

    m_Tabs.push_back(tab);

    connect(tab, &FITSTab::failed, this, [ this ](const QString & errorMessage)
    {
        QApplication::restoreOverrideCursor();
        led.setColor(Qt::red);
        m_Tabs.removeLast();
        emit failed(errorMessage);
        if (m_Tabs.size() == 0)
        {
            // Close FITS Viewer and let KStars know it is no longer needed in memory.
            close();
        }

        if (m_urls.size() > 0)
            loadFiles();
    });

    connect(tab, &FITSTab::loaded, this, [ = ]()
    {
        if (addFITSCommon(m_Tabs.last(), imageName, FITS_NORMAL, ""))
            emit loaded(fitsID++);
        else
            m_Tabs.removeLast();

        if (m_urls.size() > 0)
            loadFiles();
    });

    tab->loadFile(imageName, FITS_NORMAL, FITS_NONE);
}

int FITSViewer::loadFile(const QUrl &imageName, FITSMode mode, FITSScale filter, const QString &previewText)
{
    led.setColor(Qt::yellow);
    QApplication::setOverrideCursor(Qt::WaitCursor);

    QPointer<FITSTab> tab(new FITSTab(this));

    m_Tabs.push_back(tab);
    const int id = fitsID;

    connect(tab, &FITSTab::failed, this, [ this ](const QString & errorMessage)
    {
        QApplication::restoreOverrideCursor();
        led.setColor(Qt::red);
        m_Tabs.removeLast();
        emit failed(errorMessage);
        if (m_Tabs.size() == 0)
        {
            // Close FITS Viewer and let KStars know it is no longer needed in memory.
            close();
        }
    });

    connect(tab, &FITSTab::loaded, this, [ = ]()
    {
        if (addFITSCommon(m_Tabs.last(), imageName, mode, previewText))
            emit loaded(fitsID++);
        else
            m_Tabs.removeLast();
    });

    tab->loadFile(imageName, mode, filter);
    return id;
}

bool FITSViewer::loadData(const QSharedPointer<FITSData> &data, const QUrl &imageName, int *tab_uid, FITSMode mode,
                          FITSScale filter, const QString &previewText)
{
    led.setColor(Qt::yellow);
    QApplication::setOverrideCursor(Qt::WaitCursor);

    QPointer<FITSTab> tab(new FITSTab(this));

    m_Tabs.push_back(tab);

    if (!tab->loadData(data, mode, filter))
    {
        auto errorMessage = tab->getView()->imageData()->getLastError();
        QApplication::restoreOverrideCursor();
        led.setColor(Qt::red);
        m_Tabs.removeLast();
        emit failed(errorMessage);
        if (m_Tabs.size() == 0)
        {
            // Close FITS Viewer and let KStars know it is no longer needed in memory.
            close();
        }
        return false;
    }

    if (!addFITSCommon(tab, imageName, mode, previewText))
    {
        m_Tabs.removeLast();
        return false;
    }

    *tab_uid = fitsID++;
    return true;
}

bool FITSViewer::removeFITS(int fitsUID)
{
    auto tab = fitsMap.value(fitsUID);

    if (tab.isNull())
    {
        qCWarning(KSTARS_FITS) << "Cannot find tab with UID " << fitsUID << " in the FITS Viewer";
        return false;
    }

    int index = m_Tabs.indexOf(tab);

    if (index >= 0)
    {
        closeTab(index);
        return true;
    }

    return false;
}

void FITSViewer::updateFile(const QUrl &imageName, int fitsUID, FITSScale filter)
{
    static bool updateBusy = false;
    if (updateBusy)
        return;
    updateBusy = true;

    auto tab = fitsMap.value(fitsUID);

    if (tab.isNull())
    {
        QString message = i18n("Cannot find tab with UID %1 in the FITS Viewer", fitsUID);
        emit failed(message);
        updateBusy = false;
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
            updateBusy = false;
        }
    });

    auto conn2 = std::make_shared<QMetaObject::Connection>();
    *conn2 = connect(tab, &FITSTab::failed, this, [ = ](const QString & errorMessage)
    {
        Q_UNUSED(errorMessage);
        QObject::disconnect(*conn2);
        updateBusy = false;
    });

    tab->loadFile(imageName, tab->getView()->getMode(), filter);
}

bool FITSViewer::updateFITSCommon(const QPointer<FITSTab> &tab, const QUrl &imageName, const QString tabTitle)
{
    // On tab load success
    int tabIndex = fitsTabWidget->indexOf(tab);
    if (tabIndex == -1)
        return false;

    if (tabTitle != "")
        fitsTabWidget->setTabText(tabIndex, tabTitle);
    else if (tab->getView()->getMode() == FITS_NORMAL)
    {
        if ((imageName.fileName() == "Preview" ||
                imageName.path().startsWith(QLatin1String("/tmp")) ||
                imageName.path().contains("/Temp")) &&
                Options::singlePreviewFITS())
            fitsTabWidget->setTabText(tabIndex,
                                      tab->getPreviewText().isEmpty() ? i18n("Preview") : tab->getPreviewText());
        else if (tab->getPreviewText() != i18n("Preview") || imageName.fileName().size() > 0)
            fitsTabWidget->setTabText(tabIndex, imageName.fileName());
    }
    else if (imageName.isValid())
        fitsTabWidget->setTabText(tabIndex, imageName.fileName());

    tab->getUndoStack()->clear();

    if (tab->isVisible())
        led.setColor(Qt::green);

    if (tab->shouldComputeHFR())
        updateStatusBar(HFRStatusString(tab->getView()->imageData()), FITS_HFR);
    else
        updateStatusBar("", FITS_HFR);

    updateStatusBar(HFRClipString(tab->getView().get()), FITS_CLIP);

    actionCollection()->action("next_blink")->setEnabled(tab->blinkFilenames().size() > 1);
    actionCollection()->action("previous_blink")->setEnabled(tab->blinkFilenames().size() > 1);

    return true;
}

bool FITSViewer::updateData(const QSharedPointer<FITSData> &data, const QUrl &imageName, int fitsUID, int *tab_uid,
                            FITSMode mode, FITSScale filter, const QString &tabTitle)
{
    auto tab = fitsMap.value(fitsUID);

    if (tab.isNull())
        return false;

    if (mode != FITS_UNKNOWN)
        tab->getView()->updateMode(mode);

    if (tab->isVisible())
        led.setColor(Qt::yellow);

    if (!tab->loadData(data, tab->getView()->getMode(), filter))
        return false;

    if (!updateFITSCommon(tab, imageName, tabTitle))
        return false;

    *tab_uid = tab->getUID();
    return true;
}

void FITSViewer::tabFocusUpdated(int currentIndex)
{
    if (currentIndex < 0 || currentIndex >= m_Tabs.size())
        return;

    m_Tabs[currentIndex]->tabPositionUpdated();

    auto view = m_Tabs[currentIndex]->getView();

    view->toggleStars(markStars);

    if (isVisible())
        view->updateFrame();

    if (m_Tabs[currentIndex]->shouldComputeHFR())
        updateStatusBar(HFRStatusString(view->imageData()), FITS_HFR);
    else
        updateStatusBar("", FITS_HFR);

    updateStatusBar(HFRClipString(m_Tabs[currentIndex]->getView().get()), FITS_CLIP);

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
        updateButtonStatus("view_crosshair", i18n("Cross Hairs"), currentView->isCrosshairShown());
        updateButtonStatus("view_clipping", i18n("Clipping"), currentView->isClippingShown());
        updateButtonStatus("view_eq_grid", i18n("Equatorial Gridlines"), currentView->isEQGridShown());
        updateButtonStatus("view_objects", i18n("Objects in Image"), currentView->areObjectsShown());
        updateButtonStatus("view_pixel_grid", i18n("Pixel Gridlines"), currentView->isPixelGridShown());
        updateButtonStatus("view_hips_overlay", i18n("HiPS Overlay"), currentView->isHiPSOverlayShown());
    }

    actionCollection()->action("next_blink")->setEnabled(m_Tabs[currentIndex]->blinkFilenames().size() > 1);
    actionCollection()->action("previous_blink")->setEnabled(m_Tabs[currentIndex]->blinkFilenames().size() > 1);

    updateScopeButton();
    updateWCSFunctions();
}

void FITSViewer::starProfileButtonOff()
{
    updateButtonStatus("toggle_3D_graph", i18n("View 3D Graph"), false);
}


QList<QString> findAllImagesBelowDir(const QDir &topDir)
{
    QList<QString> result;
    QList<QString> nameFilter = { "*" };
    QDir::Filters filter = QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Files | QDir::NoSymLinks;

    QList<QDir> dirs;
    dirs.push_back(topDir);

    QRegularExpression re(".*(fits|fits.fz|fit|fts|xisf|jpg|jpeg|png|gif|bmp|cr2|cr3|crw|nef|raf|dng|arw|orf)$");
    while (!dirs.empty())
    {
        auto dir = dirs.back();
        dirs.removeLast();
        auto list = dir.entryInfoList( nameFilter, filter );
        foreach( const QFileInfo &entry,  list)
        {
            if( entry.isDir() )
                dirs.push_back(entry.filePath());
            else
            {
                const QString suffix = entry.completeSuffix();
                QRegularExpressionMatch match = re.match(suffix);
                if (match.hasMatch())
                    result.append(entry.absoluteFilePath());
            }
        }
    }
    return result;
}

void FITSViewer::blink()
{
    if (m_BlinkBusy)
        return;
    m_BlinkBusy = true;
    QFileDialog dialog(KStars::Instance(), i18nc("@title:window", "Blink Top Directory"));
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setDirectoryUrl(lastURL);

    if (!dialog.exec())
    {
        m_BlinkBusy = false;
        return;
    }
    QStringList selected = dialog.selectedFiles();
    if (selected.size() < 1)
    {
        m_BlinkBusy = false;
        return;
    }
    QString topDir = selected[0];

    auto allImages = findAllImagesBelowDir(QDir(topDir));
    if (allImages.size() == 0)
    {
        m_BlinkBusy = false;
        return;
    }

    const QUrl imageName(QUrl::fromLocalFile(allImages[0]));

    led.setColor(Qt::yellow);
    QApplication::setOverrideCursor(Qt::WaitCursor);

    QPointer<FITSTab> tab(new FITSTab(this));

    int tabIndex = m_Tabs.size();
    if (allImages.size() > 1)
    {
        m_Tabs.push_back(tab);
        tab->initBlink(allImages);
        tab->setBlinkUpto(1);
    }
    QString tabName = QString("%1/%2 %3")
                      .arg(1).arg(allImages.size()).arg(QFileInfo(allImages[0]).fileName());
    connect(tab, &FITSTab::failed, this, [ this, tab ](const QString & errorMessage)
    {
        Q_UNUSED(errorMessage);
        if (tab)
            tab->disconnect(this);
        QApplication::restoreOverrideCursor();
        led.setColor(Qt::red);
        m_BlinkBusy = false;
    });

    connect(tab, &FITSTab::loaded, this, [ this, tab, imageName, tabIndex, tabName ]()
    {
        if (tab)
        {
            tab->disconnect(this);
            addFITSCommon(m_Tabs.last(), imageName, FITS_NORMAL, "");
            //fitsTabWidget->tabBar()->setTabTextColor(tabIndex, Qt::red);
            fitsTabWidget->setTabText(tabIndex, tabName);
        }
        m_BlinkBusy = false;
    });

    actionCollection()->action("next_blink")->setEnabled(allImages.size() > 1);
    actionCollection()->action("previous_blink")->setEnabled(allImages.size() > 1);

    tab->loadFile(imageName, FITS_NORMAL, FITS_NONE);
}


void FITSViewer::changeBlink(bool increment)
{
    if (m_Tabs.empty() || m_BlinkBusy)
        return;

    m_BlinkBusy = true;
    const int tabIndex = fitsTabWidget->currentIndex();
    if (tabIndex >= m_Tabs.count() || tabIndex < 0)
    {
        m_BlinkBusy = false;
        return;
    }
    auto tab = m_Tabs[tabIndex];
    const QList<QString> &filenames = tab->blinkFilenames();
    if (filenames.size() <= 1)
    {
        m_BlinkBusy = false;
        return;
    }

    int blinkIndex = tab->blinkUpto() + (increment ? 1 : -1);
    if (blinkIndex >= filenames.size())
        blinkIndex = 0;
    else if (blinkIndex < 0)
        blinkIndex = filenames.size() - 1;

    QString nextFilename = filenames[blinkIndex];
    QString tabName = QString("%1/%2 %3")
                      .arg(blinkIndex + 1).arg(filenames.size()).arg(QFileInfo(nextFilename).fileName());
    tab->disconnect(this);
    connect(tab, &FITSTab::failed, this, [ this, tab, nextFilename ](const QString & errorMessage)
    {
        Q_UNUSED(errorMessage);
        if (tab)
            tab->disconnect(this);
        QApplication::restoreOverrideCursor();
        led.setColor(Qt::red);
        m_BlinkBusy = false;
    });

    connect(tab, &FITSTab::loaded, this, [ this, tab, nextFilename, tabIndex, tabName ]()
    {
        if (tab)
        {
            tab->disconnect(this);
            updateFITSCommon(tab, QUrl::fromLocalFile(nextFilename));
            fitsTabWidget->setTabText(tabIndex, tabName);
        }
        m_BlinkBusy = false;
    });

    tab->setBlinkUpto(blinkIndex);
    tab->loadFile(QUrl::fromLocalFile(nextFilename), FITS_NORMAL, FITS_NONE);
}

void FITSViewer::nextBlink()
{
    changeBlink(true);
}

void FITSViewer::previousBlink()
{
    changeBlink(false);
}

void FITSViewer::openFile()
{
    QFileDialog dialog(KStars::Instance(), i18nc("@title:window", "Open Image"));
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setDirectoryUrl(lastURL);
    dialog.setNameFilter("Images (*.fits *.fits.fz *.fit *.fts *.xisf "
                         "*.jpg *.jpeg *.png *.gif *.bmp "
                         "*.cr2 *.cr3 *.crw *.nef *.raf *.dng *.arw *.orf)");
    if (!dialog.exec())
        return;
    m_urls = dialog.selectedUrls();
    if (m_urls.size() < 1)
        return;
    // Protect against, e.g. opening 1000 tabs. Not sure what the right number is.
    constexpr int MAX_NUM_OPENS = 40;
    if (m_urls.size() > MAX_NUM_OPENS)
        return;

    lastURL = QUrl(m_urls[0].url(QUrl::RemoveFilename));
    loadFiles();
}

// Launch the Live Stacking functionality...
void FITSViewer::stack()
{
    if (m_StackBusy)
        return;
    m_StackBusy = true;

    QString topDir = QDir::homePath();
    QString filePath = lastURL.path();
    if (filePath.isEmpty())
        filePath = lastURL.toString();
    QFileInfo fileInfo(filePath);
    if (fileInfo.isDir())
        topDir = fileInfo.absoluteFilePath();
    else if (fileInfo.isFile())
        topDir = fileInfo.absolutePath();
    const QUrl imageName;

    led.setColor(Qt::yellow);

    QPointer<FITSTab> tab(new FITSTab(this));

    m_Tabs.push_back(tab);
    QString tabName = QString("Stack");
    connect(tab, &FITSTab::failed, this, [ this, tab ](const QString & errorMessage)
    {
        Q_UNUSED(errorMessage);
        if (tab)
            tab->disconnect(this);
        led.setColor(Qt::red);
        m_StackBusy = false;
    });

    connect(tab, &FITSTab::loaded, this, [ this, tab, imageName, tabName ]()
    {
        if (tab)
        {
            tab->disconnect(this);
            addFITSCommon(tab, imageName, FITS_LIVESTACKING, tabName);
            fitsID++;
        }
        m_StackBusy = false;
    });

    tab->initStack(topDir, FITS_LIVESTACKING, FITS_NONE);
}

// Called when a stacking operation is in motion...
void FITSViewer::restack(const QString dir, const int tabUID)
{
    auto tab = fitsMap.value(tabUID);
    const QUrl imageName;

    led.setColor(Qt::yellow);
    updateStatusBar(i18n("Stacking..."), FITS_MESSAGE);
    QString tabName = i18n("Watching %1", dir);
    connect(tab, &FITSTab::failed, this, [ this, tab ](const QString & errorMessage)
    {
        Q_UNUSED(errorMessage);
        if (tab)
        {
            tab->disconnect(this);
            led.setColor(Qt::red);
            updateStatusBar(i18n("Stacking Failed"), FITS_MESSAGE);
        }
    });

    connect(tab, &FITSTab::loaded, this, [ this, tab, imageName, tabName ]()
    {
        // There doesn't seem to be a way in a lambda to just disconnect the loaded signal which if not
        // disconnected results in fitsviewer crashing. So disconnect all and reset the other signals
        if (tab)
        {
            tab->disconnect(this);
            connect(tab, &FITSTab::newStatus, this, &FITSViewer::updateStatusBar);
            connect(tab, &FITSTab::changeStatus, this, &FITSViewer::updateTabStatus);
            connect(tab, &FITSTab::debayerToggled, this, &FITSViewer::setDebayerAction);
            connect(tab->getView().get(), &FITSView::actionUpdated, this, &FITSViewer::updateAction);
            connect(tab->getView().get(), &FITSView::wcsToggled, this, &FITSViewer::updateWCSFunctions);
            connect(tab->getView().get(), &FITSView::starProfileWindowClosed, this, &FITSViewer::starProfileButtonOff);
            updateFITSCommon(tab, imageName, tabName);
            updateStatusBar(i18n("Stacking Complete"), FITS_MESSAGE);
        }
    });
}

void FITSViewer::saveFile()
{
    m_Tabs[fitsTabWidget->currentIndex()]->saveFile();
}

void FITSViewer::saveFileAs()
{
    if (m_Tabs.empty())
        return;

    if (m_Tabs[fitsTabWidget->currentIndex()]->saveFileAs() &&
            m_Tabs[fitsTabWidget->currentIndex()]->getView()->getMode() == FITS_NORMAL)
        fitsTabWidget->setTabText(fitsTabWidget->currentIndex(),
                                  m_Tabs[fitsTabWidget->currentIndex()]->getCurrentURL()->fileName());
}

void FITSViewer::copyFITS()
{
    if (m_Tabs.empty())
        return;

    m_Tabs[fitsTabWidget->currentIndex()]->copyFITS();
}

void FITSViewer::histoFITS()
{
    if (m_Tabs.empty())
        return;

    m_Tabs[fitsTabWidget->currentIndex()]->histoFITS();
}

void FITSViewer::statFITS()
{
    if (m_Tabs.empty())
        return;

    m_Tabs[fitsTabWidget->currentIndex()]->statFITS();
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
    if (m_Tabs.empty())
        return;

    m_Tabs[fitsTabWidget->currentIndex()]->headerFITS();
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

void FITSViewer::ZoomAllIn()
{
    if (m_Tabs.empty())
        return;

    // Could add code to not call View::updateFrame for these
    for (int i = 0; i < fitsTabWidget->count(); ++i)
        if (i != fitsTabWidget->currentIndex())
            m_Tabs[i]->ZoomIn();

    m_Tabs[fitsTabWidget->currentIndex()]->ZoomIn();
}

void FITSViewer::ZoomAllOut()
{
    if (m_Tabs.empty())
        return;

    // Could add code to not call View::updateFrame for these
    for (int i = 0; i < fitsTabWidget->count(); ++i)
        if (i != fitsTabWidget->currentIndex())
            m_Tabs[i]->ZoomOut();

    m_Tabs[fitsTabWidget->currentIndex()]->ZoomOut();
}

void FITSViewer::ZoomIn()
{
    if (m_Tabs.empty())
        return;

    m_Tabs[fitsTabWidget->currentIndex()]->ZoomIn();
}

void FITSViewer::ZoomOut()
{
    if (m_Tabs.empty())
        return;

    m_Tabs[fitsTabWidget->currentIndex()]->ZoomOut();
}

void FITSViewer::ZoomDefault()
{
    if (m_Tabs.empty())
        return;

    m_Tabs[fitsTabWidget->currentIndex()]->ZoomDefault();
}

void FITSViewer::ZoomToFit()
{
    if (m_Tabs.empty())
        return;

    QSharedPointer<FITSView> currentView;
    if (getCurrentView(currentView))
        currentView->ZoomToFit();
}

// Adjust the (top-level) window size to fit the current zoom, so that there
// are no scroll bars and minimal empty space in the view. Note, if the zoom
// is such that it would be larger than the display size than this can't be
// accomplished and the windowing will do its best.
void FITSViewer::FitToZoom()
{
    if (m_Tabs.empty())
        return;

    QSharedPointer<FITSView> currentView;
    if (!getCurrentView(currentView))
        return;

    const double zoom = currentView->getCurrentZoom() * .01;
    const int w = zoom * currentView->imageData()->width();
    const int h = zoom * currentView->imageData()->height();
    const int extraW = width() - currentView->width();
    const int extraH = height() - currentView->height();
    // These 4s seem to be needed to make sure we don't wind up with scroll bars.
    // Not sure why, would be great to figure out how to remove them.
    const int wNeeded = w + extraW + 4;
    const int hNeeded = h + extraH + 4;
    resize(wNeeded, hNeeded);
}

void FITSViewer::updateAction(const QString &name, bool enable)
{
    QAction *toolAction = actionCollection()->action(name);

    if (toolAction != nullptr)
        toolAction->setEnabled(enable);
}

void FITSViewer::updateTabStatus(bool clean, const QUrl &imageURL)
{
    if (m_Tabs.empty() || (fitsTabWidget->currentIndex() >= m_Tabs.size()))
        return;

    if (m_Tabs[fitsTabWidget->currentIndex()]->getView()->getMode() != FITS_NORMAL)
        return;

    //QString tabText = fitsImages[fitsTab->currentIndex()]->getCurrentURL()->fileName();

    QString tabText = imageURL.isEmpty() ? fitsTabWidget->tabText(fitsTabWidget->currentIndex()) : imageURL.fileName();

    fitsTabWidget->setTabText(fitsTabWidget->currentIndex(), clean ? tabText.remove('*') : tabText + '*');
}

void FITSViewer::closeTab(int index)
{
    if (index < 0 || index >= m_Tabs.size())
        return;

    auto tab = m_Tabs[index];
    if (!tab)
        return;

    tab->disconnect(this);

    int UID = tab->getUID();

    fitsMap.remove(UID);
    m_Tabs.removeOne(tab);

    delete tab;

    if (m_Tabs.empty())
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
        actionCollection()->action("view_hips_overlay")->setDisabled(false);
    }
    else
    {
        actionCollection()->action("view_eq_grid")->setDisabled(true);
        actionCollection()->action("view_eq_grid")->setText(i18n("Show Equatorial Gridlines\n*No WCS Info*"));
        actionCollection()->action("center_telescope")->setDisabled(true);
        actionCollection()->action("center_telescope")->setText(i18n("Center Telescope\n*No WCS Info*"));
        actionCollection()->action("view_objects")->setDisabled(true);
        actionCollection()->action("view_objects")->setText(i18n("Show Objects in Image\n*No WCS Info*"));
        actionCollection()->action("view_hips_overlay")->setDisabled(true);
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
    if (m_Tabs.empty())
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
    if(m_Tabs.empty())
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
    if (m_Tabs.empty())
        return;

    QSharedPointer<FITSView> currentView;
    if (!getCurrentView(currentView))
        return;

    currentView->toggleCrosshair();
    updateButtonStatus("view_crosshair", i18n("Cross Hairs"), currentView->isCrosshairShown());
}

void FITSViewer::toggleClipping()
{
    if (m_Tabs.empty())
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
    if (m_Tabs.empty())
        return;

    QSharedPointer<FITSView> currentView;
    if (!getCurrentView(currentView))
        return;

    currentView->toggleEQGrid();
    updateButtonStatus("view_eq_grid", i18n("Equatorial Gridlines"), currentView->isEQGridShown());
}

void FITSViewer::toggleHiPSOverlay()
{
    if (m_Tabs.empty())
        return;

    QSharedPointer<FITSView> currentView;
    if (!getCurrentView(currentView))
        return;

    currentView->toggleHiPSOverlay();
    updateButtonStatus("view_hips_overlay", i18n("HiPS Overlay"), currentView->isHiPSOverlayShown());
}

void FITSViewer::toggleSelectionMode()
{
    if (m_Tabs.empty())
        return;

    QSharedPointer<FITSView> currentView;
    if (!getCurrentView(currentView))
        return;

    currentView->toggleSelectionMode();
    updateButtonStatus("image_roi_stats", i18n("Selection Rectangle"), currentView->isSelectionRectShown());
}

void FITSViewer::toggleObjects()
{
    if (m_Tabs.empty())
        return;

    QSharedPointer<FITSView> currentView;
    if (!getCurrentView(currentView))
        return;

    currentView->toggleObjects();
    updateButtonStatus("view_objects", i18n("Objects in Image"), currentView->areObjectsShown());
}

void FITSViewer::togglePixelGrid()
{
    if (m_Tabs.empty())
        return;

    QSharedPointer<FITSView> currentView;
    if (!getCurrentView(currentView))
        return;

    currentView->togglePixelGrid();
    updateButtonStatus("view_pixel_grid", i18n("Pixel Gridlines"), currentView->isPixelGridShown());
}

void FITSViewer::toggle3DGraph()
{
    if (m_Tabs.empty())
        return;

    QSharedPointer<FITSView> currentView;
    if (!getCurrentView(currentView))
        return;

    currentView->toggleStarProfile();
    updateButtonStatus("toggle_3D_graph", i18n("View 3D Graph"), currentView->isStarProfileShown());
}

void FITSViewer::nextTab()
{
    if (m_Tabs.empty())
        return;

    int index = fitsTabWidget->currentIndex() + 1;
    if (index >= m_Tabs.count() || index < 0)
        index = 0;
    fitsTabWidget->setCurrentIndex(index);

    actionCollection()->action("next_blink")->setEnabled(m_Tabs[index]->blinkFilenames().size() > 1);
    actionCollection()->action("previous_blink")->setEnabled(m_Tabs[index]->blinkFilenames().size() > 1);
}

void FITSViewer::previousTab()
{
    if (m_Tabs.empty())
        return;

    int index = fitsTabWidget->currentIndex() - 1;
    if (index >= m_Tabs.count() || index < 0)
        index = m_Tabs.count() - 1;
    fitsTabWidget->setCurrentIndex(index);

    actionCollection()->action("next_blink")->setEnabled(m_Tabs[index]->blinkFilenames().size() > 1);
    actionCollection()->action("previous_blink")->setEnabled(m_Tabs[index]->blinkFilenames().size() > 1);

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

    for (auto tab : m_Tabs)
    {
        tab->getView()->toggleStars(markStars);
        tab->getView()->updateFrame();
    }
}

void FITSViewer::applyFilter(int ftype)
{
    if (m_Tabs.empty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    updateStatusBar(i18n("Processing %1...", filterTypes[ftype - 1]), FITS_MESSAGE);
    qApp->processEvents();
    m_Tabs[fitsTabWidget->currentIndex()]->getHistogram()->applyFilter(static_cast<FITSScale>(ftype));
    qApp->processEvents();
    m_Tabs[fitsTabWidget->currentIndex()]->getView()->updateFrame();
    QApplication::restoreOverrideCursor();
    updateStatusBar(i18n("Ready."), FITS_MESSAGE);
}

bool FITSViewer::getView(int fitsUID, QSharedPointer<FITSView> &view)
{
    auto tab = fitsMap.value(fitsUID);
    if (tab)
    {
        view = tab->getView();
        return true;
    }
    return false;

}

bool FITSViewer::getCurrentView(QSharedPointer<FITSView> &view)
{
    if (m_Tabs.empty() || fitsTabWidget->currentIndex() >= m_Tabs.count())
        return false;

    view = m_Tabs[fitsTabWidget->currentIndex()]->getView();
    return true;
}

void FITSViewer::setDebayerAction(bool enable)
{
    actionCollection()->addAction("fits_debayer")->setEnabled(enable);
}

bool FITSViewer::tabExists(int fitsUID)
{
    auto tab = fitsMap.value(fitsUID);
    return (!tab.isNull() && tab.data() != nullptr);
}
