/*
    SPDX-FileCopyrightText: 2024 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platesolve.h"
#include "ui_platesolve.h"

#include "auxiliary/kspaths.h"
#include "Options.h"
#include <KConfigDialog>
#include "fitsdata.h"
#include "skymap.h"
#include <fits_debug.h>

QPointer<Ekos::StellarSolverProfileEditor> PlateSolve::m_ProfileEditor;
QPointer<KConfigDialog> PlateSolve::m_EditorDialog;
QPointer<KPageWidgetItem> PlateSolve::m_ProfileEditorPage;

namespace
{
const QList<SSolver::Parameters> getSSolverParametersList(Ekos::ProfileGroup module)
{
    QString savedProfiles;
    switch(module)
    {
        case Ekos::AlignProfiles:
        default:
            savedProfiles = QDir(KSPaths::writableLocation(
                                     QStandardPaths::AppLocalDataLocation)).filePath("SavedAlignProfiles.ini");
            return QFile(savedProfiles).exists() ?
                   StellarSolver::loadSavedOptionsProfiles(savedProfiles) :
                   Ekos::getDefaultAlignOptionsProfiles();
            break;
        case Ekos::FocusProfiles:
            savedProfiles = QDir(KSPaths::writableLocation(
                                     QStandardPaths::AppLocalDataLocation)).filePath("SavedFocusProfiles.ini");
            return QFile(savedProfiles).exists() ?
                   StellarSolver::loadSavedOptionsProfiles(savedProfiles) :
                   Ekos::getDefaultFocusOptionsProfiles();
            break;
        case Ekos::GuideProfiles:
            savedProfiles = QDir(KSPaths::writableLocation(
                                     QStandardPaths::AppLocalDataLocation)).filePath("SavedGuideProfiles.ini");
            return QFile(savedProfiles).exists() ?
                   StellarSolver::loadSavedOptionsProfiles(savedProfiles) :
                   Ekos::getDefaultGuideOptionsProfiles();
            break;
        case Ekos::HFRProfiles:
            savedProfiles = QDir(KSPaths::writableLocation(
                                     QStandardPaths::AppLocalDataLocation)).filePath("SavedHFRProfiles.ini");
            return QFile(savedProfiles).exists() ?
                   StellarSolver::loadSavedOptionsProfiles(savedProfiles) :
                   Ekos::getDefaultHFROptionsProfiles();
            break;
    }
}
} // namespace


PlateSolve::PlateSolve(QWidget * parent) : QDialog(parent)
{
    setup();
}

void PlateSolve::setup()
{
    setupUi(this);
    editProfile->setIcon(QIcon::fromTheme("document-edit"));
    editProfile->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    const QString EditorID = "FITSSolverProfileEditor";
    if (!m_EditorDialog)
    {
        // These are static, shared by all FITS Viewer tabs.
        m_EditorDialog = new KConfigDialog(nullptr, EditorID, Options::self());
        m_ProfileEditor = new Ekos::StellarSolverProfileEditor(nullptr, Ekos::AlignProfiles, m_EditorDialog.data());
        m_ProfileEditorPage = m_EditorDialog->addPage(m_ProfileEditor.data(),
                              i18n("FITS Viewer Solver Profiles Editor"));
    }

    connect(editProfile, &QAbstractButton::clicked, this, [this, EditorID]
    {
        m_ProfileEditor->loadProfile(kcfg_FitsSolverProfile->currentText());
        KConfigDialog * d = KConfigDialog::exists(EditorID);
        if(d)
        {
            d->setCurrentPage(m_ProfileEditorPage);
            d->show();
        }
    });
    connect(SolveButton, &QPushButton::clicked, this, [this]()
    {
        if (m_Solver.get() && m_Solver->isRunning())
        {
            SolveButton->setText(i18n("Aborting..."));
            m_Solver->abort();
            emit solverFailed();
            return;
        }

        emit clicked();
    });
    initSolverUI();
}

void PlateSolve::enableAuxButton(const QString &label, const QString &toolTip)
{
    auxButton->setText(label);
    auxButton->setToolTip(toolTip);
    auxButton->setVisible(true);
    disconnect(auxButton);
    connect(auxButton, &QPushButton::clicked, this, [this]()
    {
        emit auxClicked();
    });
}

void PlateSolve::disableAuxButton()
{
    auxButton->setVisible(false);
    disconnect(auxButton);
}

void PlateSolve::abort()
{
    disconnect(&m_Watcher);
    if (m_Solver.get())
    {
        disconnect(m_Solver.get());
        m_Solver->abort();
    }
}

void PlateSolve::setupSolver(const QSharedPointer<FITSData> &imageData, bool extractOnly)
{
    auto parameters = getSSolverParametersList(static_cast<Ekos::ProfileGroup>(Options::fitsSolverModule())).at(
                          kcfg_FitsSolverProfile->currentIndex());
    parameters.search_radius = kcfg_FitsSolverRadius->value();
    if (extractOnly)
    {
        if (!kcfg_FitsSolverLinear->isChecked())
        {
            // If image is non-linear seed the threshold offset with the background using median pixel value. Note
            // that there is a bug in the median calculation due to an issue compiling on Mac that means that not
            // all datatypes are supported by the median calculation. If median is zero use the mean instead.
            double offset = imageData->getAverageMedian();
            if (offset <= 0.0)
                offset = imageData->getAverageMean();
            parameters.threshold_offset = offset;
        }

        m_Solver.reset(new SolverUtils(parameters, parameters.solverTimeLimit, SSolver::EXTRACT),  &QObject::deleteLater);
        connect(m_Solver.get(), &SolverUtils::done, this, &PlateSolve::extractorDone, Qt::UniqueConnection);
    }
    else
    {
        // If image is non-linear then set the offset to the average background in the image
        // which was found in the first solver (extract only) run.
        if (m_Solver && !kcfg_FitsSolverLinear->isChecked())
            parameters.threshold_offset = m_Solver->getBackground().global;

        m_Solver.reset(new SolverUtils(parameters, parameters.solverTimeLimit, SSolver::SOLVE),  &QObject::deleteLater);
        connect(m_Solver.get(), &SolverUtils::done, this, &PlateSolve::solverDone, Qt::UniqueConnection);
    }

    const int imageWidth = imageData->width();
    const int imageHeight = imageData->height();
    if (kcfg_FitsSolverUseScale->isChecked() && imageWidth != 0 && imageHeight != 0)
    {
        const double scale = kcfg_FitsSolverScale->value();
        double lowScale = scale * 0.8;
        double highScale = scale * 1.2;

        // solver utils uses arcsecs per pixel only
        const int units = kcfg_FitsSolverImageScaleUnits->currentIndex();
        if (units == SSolver::DEG_WIDTH)
        {
            lowScale = (lowScale * 3600) / std::max(imageWidth, imageHeight);
            highScale = (highScale * 3600) / std::min(imageWidth, imageHeight);
        }
        else if (units == SSolver::ARCMIN_WIDTH)
        {
            lowScale = (lowScale * 60) / std::max(imageWidth, imageHeight);
            highScale = (highScale * 60) / std::min(imageWidth, imageHeight);
        }

        m_Solver->useScale(kcfg_FitsSolverUseScale->isChecked(), lowScale, highScale);
    }
    else m_Solver->useScale(false, 0, 0);

    if (kcfg_FitsSolverUsePosition->isChecked())
    {
        bool ok;
        const dms ra = FitsSolverEstRA->createDms(&ok);
        bool ok2;
        const dms dec = FitsSolverEstDec->createDms(&ok2);
        if (ok && ok2)
            m_Solver->usePosition(true, ra.Degrees(), dec.Degrees());
        else
            m_Solver->usePosition(false, 0, 0);
    }
    else m_Solver->usePosition(false, 0, 0);
}

// If it is currently solving an image, then cancel the solve.
// Otherwise start solving.
void PlateSolve::extractImage(const QSharedPointer<FITSData> &imageData)
{
    m_imageData = imageData;
    if (m_Solver.get() && m_Solver->isRunning())
    {
        SolveButton->setText(i18n("Aborting..."));
        m_Solver->abort();
        return;
    }
    SolveButton->setText(i18n("Cancel"));

    setupSolver(imageData, true);

    FitsSolverAngle->setText("");
    FitsSolverIndexfile->setText("");
    Solution1->setText(i18n("Extracting..."));
    Solution2->setText("");

    m_Solver->runSolver(imageData);
}

void PlateSolve::solveImage(const QString &filename)
{
    connect(&m_Watcher, &QFutureWatcher<bool>::finished, this, &PlateSolve::loadFileDone, Qt::UniqueConnection);

    m_imageData.reset(new FITSData(), &QObject::deleteLater);
    QFuture<bool> response = m_imageData->loadFromFile(filename);
    m_Watcher.setFuture(response);
}

void PlateSolve::loadFileDone()
{
    solveImage(m_imageData);
    disconnect(&m_Watcher);
}

void PlateSolve::solveImage(const QSharedPointer<FITSData> &imageData)
{
    m_imageData = imageData;
    if (m_Solver.get() && m_Solver->isRunning())
    {
        SolveButton->setText(i18n("Aborting..."));
        m_Solver->abort();
        return;
    }
    SolveButton->setText(i18n("Cancel"));

    setupSolver(imageData, false);

    Solution2->setText(i18n("Solving..."));

    m_Solver->runSolver(imageData);
}

// Plate solve an image sub as part of Live Stacking
// In this case we are not using the FitsTab Plate Solving UI except to use
// the currently selected profile for solving.
void PlateSolve::plateSolveSub(const QSharedPointer<FITSData> &imageData, const double ra, const double dec,
                               const double pixScale, const int index, const int healpix,
                               const SSolver::ProcessType solveType)
{
    m_imageData = imageData;
    if (m_Solver.get() && m_Solver->isRunning())
        m_Solver->abort();

    auto parameters = getSSolverParametersList(static_cast<Ekos::ProfileGroup>(Options::fitsSolverModule())).at(
        kcfg_FitsSolverProfile->currentIndex());

    double lowerPixScale, upperPixScale;
    if (index == -1)
    {
        // First solve so use wider criteria...
        parameters.search_radius = kcfg_FitsSolverRadius->value();
        lowerPixScale = pixScale * 0.8;
        upperPixScale = pixScale * 1.2;
    }
    else
    {
        // Tighten the search radius and pixscale...
        parameters.search_radius = 1;
        lowerPixScale = pixScale * 0.95;
        upperPixScale = pixScale * 1.05;
    }

    m_Solver.reset(new SolverUtils(parameters, parameters.solverTimeLimit, solveType), &QObject::deleteLater);

    if (solveType == SSolver::EXTRACT || solveType == SSolver::EXTRACT_WITH_HFR)
        // We need star details for later calculations so firstly extract stars
        connect(m_Solver.get(), &SolverUtils::done, this, &PlateSolve::subExtractorDone, Qt::UniqueConnection);
    else
        // No star details required (or we just extracted them) so now plate solve
        connect(m_Solver.get(), &SolverUtils::done, this, &PlateSolve::subSolverDone, Qt::UniqueConnection);

    m_Solver->useScale(true, lowerPixScale, upperPixScale);
    m_Solver->usePosition(true, ra, dec);
    m_Solver->setHealpix(index, healpix);
    m_Solver->runSolver(imageData, true);
}

void PlateSolve::extractorDone(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds)
{
    Q_UNUSED(solution);
    disconnect(m_Solver.get(), &SolverUtils::done, this, &PlateSolve::extractorDone);
    Solution2->setText("");

    if (timedOut)
    {
        const QString result = i18n("Extractor timed out: %1s", QString("%L1").arg(elapsedSeconds, 0, 'f', 1));
        Solution1->setText(result);

        // Can't run the solver. Just reset.
        SolveButton->setText("Solve");
        emit extractorFailed();
        return;
    }
    else if (!success)
    {
        const QString result = i18n("Extractor failed: %1s", QString("%L1").arg(elapsedSeconds, 0, 'f', 1));
        Solution1->setText(result);

        // Can't run the solver. Just reset.
        SolveButton->setText(i18n("Solve"));
        emit extractorFailed();
        return;
    }
    else
    {
        const QString starStr = i18n("Extracted %1 stars (%2 unfiltered) in %3s",
                                     m_Solver->getNumStarsFound(),
                                     m_Solver->getBackground().num_stars_detected,
                                     QString("%1").arg(elapsedSeconds, 0, 'f', 1));
        Solution1->setText(starStr);

        // Set the stars in the FITSData object so the user can view them.
        const QList<FITSImage::Star> &starList = m_Solver->getStarList();
        QList<Edge*> starCenters;
        starCenters.reserve(starList.size());
        for (int i = 0; i < starList.size(); i++)
        {
            const auto &star = starList[i];
            Edge *oneEdge = new Edge();
            oneEdge->x = star.x;
            oneEdge->y = star.y;
            oneEdge->val = star.peak;
            oneEdge->sum = star.flux;
            oneEdge->HFR = star.HFR;
            oneEdge->width = star.a;
            oneEdge->numPixels = star.numPixels;
            if (star.a > 0)
                // See page 63 to find the ellipticity equation for SEP.
                // http://astroa.physics.metu.edu.tr/MANUALS/sextractor/Guide2source_extractor.pdf
                oneEdge->ellipticity = 1 - star.b / star.a;
            else
                oneEdge->ellipticity = 0;

            starCenters.append(oneEdge);
        }
        m_imageData->setStarCenters(starCenters);
        emit extractorSuccess();
    }
}

void PlateSolve::subExtractorDone(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds)
{
    Q_UNUSED(solution);
    disconnect(m_Solver.get(), &SolverUtils::done, this, &PlateSolve::subExtractorDone);

    if (timedOut)
    {
        qCDebug(KSTARS_FITS) << QString("Extractor timed out: %1s").arg(elapsedSeconds, 0, 'f', 1);
        emit subExtractorFailed();
        return;
    }
    else if (!success)
    {
        qCDebug(KSTARS_FITS) << QString("Extractor failed: %1s").arg(elapsedSeconds, 0, 'f', 1);
        emit subExtractorFailed();
        return;
    }

    const QList<FITSImage::Star> &starList = m_Solver->getStarList();

    // Get the median HFR
    double medianHFR = 0.0;
    if (starList.size() > 0)
    {
        std::vector<FITSImage::Star> stars(starList.constBegin(), starList.constEnd());
        // Use nth_element to get the median HFR
        std::nth_element(stars.begin(), stars.begin() + stars.size() / 2, stars.end(),
                         [](const FITSImage::Star &a, const FITSImage::Star &b) { return a.HFR < b.HFR; });
        FITSImage::Star medianStar = stars[stars.size() / 2];
        medianHFR = medianStar.HFR;
    }
    // Set the stars in the FITSData object so the user can view them.
    emit subExtractorSuccess(medianHFR, starList.size());
}

void PlateSolve::solverDone(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds)
{
    m_Solution = FITSImage::Solution();
    disconnect(m_Solver.get(), &SolverUtils::done, this, &PlateSolve::solverDone);
    SolveButton->setText("Solve");

    if (m_Solver->isRunning())
        qCDebug(KSTARS_FITS) << "solverDone called, but it is still running.";

    if (timedOut)
    {
        const QString result = i18n("Solver timed out: %1s", QString("%L1").arg(elapsedSeconds, 0, 'f', 1));
        Solution2->setText(result);
        emit solverFailed();
    }
    else if (!success)
    {
        const QString result = i18n("Solver failed: %1s", QString("%L1").arg(elapsedSeconds, 0, 'f', 1));
        Solution2->setText(result);
        emit solverFailed();
    }
    else
    {
        m_Solution = solution;
        const bool eastToTheRight = solution.parity == FITSImage::POSITIVE ? false : true;
        m_imageData->injectWCS(solution.orientation, solution.ra, solution.dec, solution.pixscale, eastToTheRight);
        m_imageData->loadWCS();

        const QString result = QString("Solved in %1s").arg(elapsedSeconds, 0, 'f', 1);
        const double solverPA = KSUtils::rotationToPositionAngle(solution.orientation);
        FitsSolverAngle->setText(QString("%1º").arg(solverPA, 0, 'f', 2));

        int indexUsed = -1, healpixUsed = -1;
        m_Solver->getSolutionHealpix(&indexUsed, &healpixUsed);
        if (indexUsed < 0)
            FitsSolverIndexfile->setText("");
        else
            FitsSolverIndexfile->setText(
                QString("%1%2")
                .arg(indexUsed)
                .arg(healpixUsed >= 0 ? QString("-%1").arg(healpixUsed) : QString("")));;

        // Set the scale widget to the current result
        const int imageWidth = m_imageData->width();
        const int units = kcfg_FitsSolverImageScaleUnits->currentIndex();
        if (units == SSolver::DEG_WIDTH)
            kcfg_FitsSolverScale->setValue(solution.pixscale * imageWidth / 3600.0);
        else if (units == SSolver::ARCMIN_WIDTH)
            kcfg_FitsSolverScale->setValue(solution.pixscale * imageWidth / 60.0);
        else
            kcfg_FitsSolverScale->setValue(solution.pixscale);

        // Set the ra and dec widgets to the current result
        FitsSolverEstRA->show(dms(solution.ra));
        FitsSolverEstDec->show(dms(solution.dec));

        Solution2->setText(result);
        emit solverSuccess();
    }
}

void PlateSolve::subSolverDone(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds)
{
    disconnect(m_Solver.get(), &SolverUtils::done, this, &PlateSolve::subSolverDone);

    if (m_Solver->isRunning())
    {
        qCDebug(KSTARS_FITS) << "subSolverDone called, but it is still running. Waiting to finish...";
        QTimer::singleShot(1000, this, [ &, timedOut, success, solution, elapsedSeconds]()
        {
            subSolverDone(timedOut, success, solution, elapsedSeconds);
        });
        return;
    }

    if (timedOut)
    {
        qCDebug(KSTARS_FITS) << QString("subSolver timed out: %1s").arg(elapsedSeconds, 0, 'f', 1);
        emit subSolverFailed();
        return;
    }
    if (!success)
    {
        qCDebug(KSTARS_FITS) << QString("subSolver failed: %1s").arg(elapsedSeconds, 0, 'f', 1);
        emit subSolverFailed();
        return;
    }

#if !defined (KSTARS_LITE) && defined (HAVE_WCSLIB) && defined (HAVE_OPENCV)
    int indexUsed = -1, healpixUsed = -1;
    m_Solver->getSolutionHealpix(&indexUsed, &healpixUsed);
    m_imageData->setStackSubSolution(solution.ra, solution.dec, solution.pixscale, indexUsed, healpixUsed);
    const bool eastToTheRight = solution.parity == FITSImage::POSITIVE ? false : true;
    m_imageData->injectStackWCS(solution.orientation, solution.ra, solution.dec, solution.pixscale, eastToTheRight);
    m_imageData->stackLoadWCS();
    emit subSolverSuccess();
#endif // !KSTARS_LITE, HAVE_WCSLIB, HAVE_OPENCV
}

// Each module can default to its own profile index. These two methods retrieves and saves
// the values in a JSON string using an Options variable.
int PlateSolve::getProfileIndex(int moduleIndex)
{
    if (moduleIndex < 0 || moduleIndex >= Ekos::ProfileGroupNames.size())
        return 0;
    const QString moduleName = Ekos::ProfileGroupNames[moduleIndex].toString();
    const QString str = Options::fitsSolverProfileIndeces();
    const QJsonDocument doc = QJsonDocument::fromJson(str.toUtf8());
    if (doc.isNull() || !doc.isObject())
        return 0;
    const QJsonObject indeces = doc.object();
    return indeces[moduleName].toString().toInt();
}

void PlateSolve::setProfileIndex(int moduleIndex, int profileIndex)
{
    if (moduleIndex < 0 || moduleIndex >= Ekos::ProfileGroupNames.size())
        return;
    QString str = Options::fitsSolverProfileIndeces();
    QJsonDocument doc = QJsonDocument::fromJson(str.toUtf8());
    if (doc.isNull() || !doc.isObject())
    {
        QJsonObject initialIndeces;
        for (int i = 0; i < Ekos::ProfileGroupNames.size(); i++)
        {
            QString name = Ekos::ProfileGroupNames[i].toString();
            if (name == "Align")
                initialIndeces[name] = QString::number(Options::solveOptionsProfile());
            else if (name == "Guide")
                initialIndeces[name] = QString::number(Options::guideOptionsProfile());
            else if (name == "HFR")
                initialIndeces[name] = QString::number(Options::hFROptionsProfile());
            else // Focus has a weird setting, just default to 0
                initialIndeces[name] = "0";
        }
        doc = QJsonDocument(initialIndeces);
    }

    QJsonObject indeces = doc.object();
    indeces[Ekos::ProfileGroupNames[moduleIndex].toString()] = QString::number(profileIndex);
    doc = QJsonDocument(indeces);
    Options::setFitsSolverProfileIndeces(QString(doc.toJson()));
}

void PlateSolve::setupProfiles(int moduleIndex)
{
    if (moduleIndex < 0 || moduleIndex >= Ekos::ProfileGroupNames.size())
        return;
    Ekos::ProfileGroup profileGroup = static_cast<Ekos::ProfileGroup>(moduleIndex);
    Options::setFitsSolverModule(moduleIndex);

    // Set up the profiles' menu.
    const QList<SSolver::Parameters> optionsList = getSSolverParametersList(profileGroup);
    kcfg_FitsSolverProfile->clear();
    for(auto &param : optionsList)
        kcfg_FitsSolverProfile->addItem(param.listName);

    m_ProfileEditor->setProfileGroup(profileGroup, false);

    // Restore the stored options.
    kcfg_FitsSolverProfile->setCurrentIndex(getProfileIndex(Options::fitsSolverModule()));

    m_ProfileEditorPage->setHeader(QString("FITS Viewer Solver %1 Profiles Editor")
                                   .arg(Ekos::ProfileGroupNames[moduleIndex].toString()));
}

void PlateSolve::setPosition(const SkyPoint &p)
{
    FitsSolverEstRA->show(p.ra());
    FitsSolverEstDec->show(p.dec());
}
void PlateSolve::setUsePosition(bool yesNo)
{
    kcfg_FitsSolverUsePosition->setChecked(yesNo);
}
void PlateSolve::setScale(double scale)
{
    kcfg_FitsSolverScale->setValue(scale);
}
void PlateSolve::setScaleUnits(int units)
{
    kcfg_FitsSolverImageScaleUnits->setCurrentIndex(units);
}
void PlateSolve::setUseScale(bool yesNo)
{
    kcfg_FitsSolverUseScale->setChecked(yesNo);
}
void PlateSolve::setLinear(bool yesNo)
{
    kcfg_FitsSolverLinear->setChecked(yesNo);
}

void PlateSolve::initSolverUI()
{
    // Init the modules combo box.
    kcfg_FitsSolverModule->clear();
    for (int i = 0; i < Ekos::ProfileGroupNames.size(); i++)
        kcfg_FitsSolverModule->addItem(Ekos::ProfileGroupNames[i].toString());
    kcfg_FitsSolverModule->setCurrentIndex(Options::fitsSolverModule());

    setupProfiles(Options::fitsSolverModule());

    // Change the profiles combo box whenever the modules combo changes
    connect(kcfg_FitsSolverModule, QOverload<int>::of(&QComboBox::activated), this, &PlateSolve::setupProfiles,
            Qt::UniqueConnection);

    kcfg_FitsSolverUseScale->setChecked(Options::fitsSolverUseScale());
    kcfg_FitsSolverScale->setValue(Options::fitsSolverScale());
    kcfg_FitsSolverImageScaleUnits->setCurrentIndex(Options::fitsSolverImageScaleUnits());

    kcfg_FitsSolverUsePosition->setChecked(Options::fitsSolverUsePosition());
    kcfg_FitsSolverRadius->setValue(Options::fitsSolverRadius());

    FitsSolverEstRA->setUnits(dmsBox::HOURS);
    FitsSolverEstDec->setUnits(dmsBox::DEGREES);

    // Save the values of user controls when the user changes them.
    connect(kcfg_FitsSolverProfile, QOverload<int>::of(&QComboBox::activated), [this](int index)
    {
        setProfileIndex(kcfg_FitsSolverModule->currentIndex(), index);
    });

    connect(kcfg_FitsSolverUseScale, &QCheckBox::stateChanged, this, [](int state)
    {
        Options::setFitsSolverUseScale(state);
    });
    connect(kcfg_FitsSolverScale, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [](double value)
    {
        Options::setFitsSolverScale(value);
    });
    connect(kcfg_FitsSolverImageScaleUnits, QOverload<int>::of(&QComboBox::activated), [](int index)
    {
        Options::setFitsSolverImageScaleUnits(index);
    });

    connect(kcfg_FitsSolverUsePosition, &QCheckBox::stateChanged, this, [](int state)
    {
        Options::setFitsSolverUsePosition(state);
    });

    connect(kcfg_FitsSolverRadius, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [](double value)
    {
        Options::setFitsSolverRadius(value);
    });
    connect(UpdatePosition, &QPushButton::clicked, this, [&]()
    {
        const auto center = SkyMap::Instance()->getCenterPoint();
        FitsSolverEstRA->show(center.ra());
        FitsSolverEstDec->show(center.dec());
    });

    // Warn if the user is not using the internal StellarSolver solver.
    const SSolver::SolverType type = static_cast<SSolver::SolverType>(Options::solverType());
    if(type != SSolver::SOLVER_STELLARSOLVER)
    {
        Solution2->setText(i18n("Warning! This tool only supports the internal StellarSolver solver."));
        Solution1->setText(i18n("Change to that in the Ekos Align options menu."));
    }
}

void PlateSolve::setImageDisplay(const QImage &image)
{
    QImage scaled = image.scaledToHeight(300);
    plateSolveImage->setVisible(true);
    plateSolveImage->setPixmap(QPixmap::fromImage(scaled));
}
