/*
    SPDX-FileCopyrightText: 2025 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_platesolve.h"

#include "ekos/ekos.h"

#include <QFrame>
#include <QDateTime>
#include "ekos/auxiliary/solverutils.h"
#include "ekos/auxiliary/stellarsolverprofile.h"
#include "ekos/auxiliary/stellarsolverprofileeditor.h"
#include "kpagewidgetmodel.h"

class SkyPoint;

class PlateSolve: public QDialog, public Ui::PlateSolveUI
{
        Q_OBJECT

    public:
      PlateSolve(QWidget * parent = nullptr);

      void abort();
      void enableAuxButton(const QString &label, const QString &toolTip = "");
      void disableAuxButton();
      void setImageDisplay(const QImage &image);

      const FITSImage::Solution &solution() {
          return m_Solution;
      }
      void setPosition(const SkyPoint &p);
      void setUsePosition(bool yesNo);
      void setScale(double scale);
      void setScaleUnits(int units);
      void setUseScale(bool yesNo);
      void setLinear(bool yesNo);      

    public slots:
        void extractImage(const QSharedPointer<FITSData> &imageData);
        void solveImage(const QSharedPointer<FITSData> &imageData);
        void solveImage(const QString &filename);
        void plateSolveSub(const QSharedPointer<FITSData> &imageData, const double ra, const double dec,
                           const double pixScale, const int index, const int healpix,
                           const SSolver::ProcessType solveType);

    signals:
        void clicked();
        void extractorSuccess();
        void subExtractorSuccess(const double medianHFR, const int numStars);
        void solverSuccess();
        void subSolverSuccess();
        void extractorFailed();
        void subExtractorFailed();
        void solverFailed();
        void subSolverFailed();
        void auxClicked();

    private:
      void setup();
      void initSolverUI();
      void setupSolver(const QSharedPointer<FITSData> &imageData, bool extractOnly);
      void solverDone(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds);
      void subSolverDone(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds);
      void extractorDone(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds);
      void subExtractorDone(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds);
      void setupProfiles(int profileIndex);
      int getProfileIndex(int moduleIndex);
      void setProfileIndex(int moduleIndex, int profileIndex);
      void loadFileDone();

      QSharedPointer<SolverUtils> m_Solver;
      QSharedPointer<FITSData> m_imageData;
      QFutureWatcher<bool> m_Watcher;
      FITSImage::Solution m_Solution;

      // The StellarSolverProfileEditor is shared among all tabs of all FITS Viewers.
      // They all edit the same (align) profiles.
      static QPointer<Ekos::StellarSolverProfileEditor> m_ProfileEditor;
      static QPointer<KConfigDialog> m_EditorDialog;
      static QPointer<KPageWidgetItem> m_ProfileEditorPage;
};

