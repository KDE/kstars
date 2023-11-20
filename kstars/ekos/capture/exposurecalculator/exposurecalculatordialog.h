/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#ifndef EXPOSURECALCULATORDIALOG_H
#define EXPOSURECALCULATORDIALOG_H

#include <QDialog>
#include "optimalsubexposurecalculator.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
class ExposureCalculatorDialog;
}
QT_END_NAMESPACE

class ExposureCalculatorDialog : public QDialog
{
        Q_OBJECT

    public:
        // ExposureCalculatorDialog(QWidget *parent = nullptr);
        ExposureCalculatorDialog(QWidget *parent = nullptr,
                                 double aPreferredSkyQualityValue = 20.0,
                                 double aPreferredFocalRatioValue = 5.0,
                                 const QString &aPreferredCameraId = "");

        ~ExposureCalculatorDialog();

    public slots:
        void applyInitialInputs();  // This method is acting as a "fill-in" for initiating the calculator with data from KStars ekos/indi
        void handleUserAdjustment();  // Change to gain, does not change exposure envelope, but does require recalculation of shot
        void handleStackCalculation();

    private slots:
        void on_downloadCameraB_clicked();

    private:
        Ui::ExposureCalculatorDialog *ui;

        OptimalExposure::OptimalSubExposureCalculator *anOptimalSubExposureCalculator;
        OptimalExposure::ImagingCameraData *anImagingCameraData;

        void initializeSubExposureCalculator(double aNoiseTolerance,  double aSkyQualityValue, double aFocalRatioValue,
                                             double aFilterCompensationValue, QString aSelectedImagingCamera);
        void calculateSubExposure(double aNoiseTolerance, double aSkyQualityValue, double aFocalRatioValue,
                                  double aFilterCompensationValue, int aSelectedReadMode, int aSelectedGainValue);

        int getGainSelection(OptimalExposure::GainSelectionType aGainSelectionType);
        QColor makeASkyQualityColor(double anSQMValue);

        double aPreferredSkyQualityValue;
        double aPreferredFocalRatioValue;
        QString aPreferredCameraId;

        void refreshCameraSelector(Ui::ExposureCalculatorDialog *ui, QStringList availableCameraFileNames,
                                   const QString aPreferredCameraId);

        void hideGainSelectionWidgets();
        void showGainSelectionFixedWidgets();
        void showGainSelectionNormalWidgets();
        void showGainSelectionISODiscreteWidgets();


};
#endif // EXPOSURECALCULATORDIALOG_H
