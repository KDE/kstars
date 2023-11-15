/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <Q3DSurface>
#include <QCustom3DLabel>

#include "curvefit.h"
#include "ui_aberrationinspector.h"
#include "aberrationinspectorutils.h"

// The AberrationInspector class manages the Aberration Inspector dialog.
// Settings are managed in a global way, rather than per Optical Train which would be overkill. The approach is the same as Focus
// using loadSettings, connectSettings & syncSettings.
//
// Aberration Inspector uses focus position of different parts of the sensor to examine backfocus and tilt. A single Autofocus run can
// be used as the basis for the analysis.
//
// Note, Aberration Inspector assumes all focus differences between different tiles on the sensor are due to Backfocus and sensor tilt.
// In reality many other aberrations (e.g. collimation, coma, etc.) could contribute to focus differences but are assumed to be negligible.
// If other aberrations are significant then the analysis output of Aberration Inspector is likely to be invalid.
//
// Aberration Inspector can have 2 use cases:
// 1. Analysis mode. Run the inspector and look at the output.
// 2. Use the tool to help with adjustment of Backfocus and / or tilt with a device such as a PhotonCage or Octopi. In this mode, use of the
//    tool will be iterative. Run the tool, look at the output, make an adjustment for Backfocus and / or tilt, rerun the tool and compare
//    the new output. Make a further adjustment and repeat until happy with the output. For this reason, each time Aberration Inspector is
//    run, a new Aberration Inspector Dialog (with incrementing Run number) is launched allowing comparision of results.
//
// To invoke Aberration Inspector:
// 1. Setup focus to give the consistently good focus results. Point to a part of the sky with lots of stars.
// 2. Set the Mosaic Mask on and set it up so that there are sufficient stars in each tile. This is important as each tile
//    will be focused solved individually so there needs to be enough stars in each tile. Increase the tile size to get each tile to cover
//    more of the sensor and therefore contain more stars; but don't overdo it as the bigger the tile the less accurate the results.
// 3. Run Autofocus manually by pressing the Auto Focus button. Note, Aberration Inspector is not run when Focus is run in a sequence.
// 4. Autofocus will run normally but will collect data for Aberration Inspector for each datapoint. If the Focus run isn't good then
//    discard and retry. If basic Autofocus isn't good then Aberration Inspector will not be good.
// 5. When Autofocus completes, the Aberration Inspector dialog is launched.
// 6. To run Aberration Inspector again, simply rerun Autofocus.
//
// The Aberration Inspector dialog has 4 components:
// 1. The v-curves. A curve is drawn for each tile dependent on the user setting of TileSelection. So either 5 or 9 curves are drawn.
//    Like Focus, the v-curve shows measure (e.g. HFR) on the y-axis versus focuser position on the x-axis
// 2. A table of results from the v-curves. A row is displayed per tile showing v-curve solution and delta from central tile
// 3. Analysis of results table. Here the deltas between the solve position for the central tile is compared with other tiles and used
//    to produce numbers for:
//    Backfocus - The idea is that if backfocus is perfect then the average of tile deltas from the centre will be zero.
//    Tilt      - Differences in tile deltas when backfocus is compensated for, are due to tilt. The analyse works on 2 axes of tilt,
//                Left-to-Right and Top-to-Bottom.
// 4. 3D Graphic. This helps to orient the user and explain the results. 2 Surfaces can be displayed:
//    Sensor          - The sensor is drawn as a 3D plane to scale, with the tilt shown on the z-axis. Top, bottom, left and right are labelled.
//    Petzval Surface - This is light surface that comes out of the telescope and hits the sensor. The surface is drawn as a simple,
//                      circularly symmetrical paraboloid. In reality, the light surface coming out of the field flattener may be much more
//                      complex.
//    With the 3D graphic, it is possible to enter simulation mode, and adjust the Backfocus and tilt and see the effect on the Sensor and
//    Petzval surface.
//

using namespace QtDataVisualization;

namespace Ekos
{

class SensorGraphic;
class AberrationInspectorPlot;

class AberrationInspector : public QDialog, public Ui::aberrationInspectorDialog
{
        Q_OBJECT

    public:

        typedef enum { TILES_ALL, TILES_OUTER_CORNERS, TILES_INNER_DIAMOND } TileSelection;
        typedef struct
        {
            int run;
            CurveFitting::CurveFit curveFit;
            bool useWeights;
            CurveFitting::OptimisationDirection optDir;
            int sensorWidth;
            int sensorHeight;
            double pixelSize;
            int tileWidth;
            double focuserStepMicrons;
            QString yAxisLabel;
            double starUnits;
            double cfzSteps;
            bool isPositionBased;
        } abInsData;

        /**
         * @brief create an AberrationInspector with the associated data
         * @param data is a structure describing the curve fitting methods
         * @param positions datapoints
         * @param measures datapoints for each tile
         * @param weights datapoints for each tile
         */
        AberrationInspector(const abInsData &data, const QVector<int> &positions, const QVector<QVector<double>> &measures,
                            const QVector<QVector<double>> &weights, const QVector<QVector<int>> &numStars,
                            const QVector<QPoint> &tileCenterOffset);
        ~AberrationInspector();

    private slots:
        /**
         * @brief mouse moved into table event. Used to show sensor graphic widget
         * @param row
         * @param column
         */
        void newMousePos(int row, int column);

        /**
         * @brief mouse left the table. Used to hide the sensor graphic widget
         */
        void leaveTableEvent();

        /**
         * @brief checkbox state changed event
         * @param new state
         */
        void onStateChanged(int state);

        /**
         * @brief table cell changed
         * @param new state
         */
        void onCellChanged(int row, int column);

    private:
        /**
         * @brief setup elements of the GUI
         */
        void setupGUI();

        /**
         * @brief connect settings in order to persist changes
         */
        void connectSettings();

        /**
         * @brief load persisted settings
         */
        void loadSettings();

        /**
         * @brief persist settings
         */
        void syncSettings();

        /**
         * @brief initialise Aberration Inspector
         */
        void initAberrationInspector();

        /**
         * @brief fit v-curves for each tile and update other widgets with results
         */
        void fitCurves();

        /**
         * @brief initialise the 3D graphic
         */
        void initGraphic();

        /**
         * @brief setTileSelection combobox
         * @param tileSelection
         */
        void setTileSelection(TileSelection tileSelection);

        /**
         * @brief setup an array of tiles to use / don't use
         * @param tileSelection
         */
        void setupTiles(TileSelection tileSelection);

        /**
         * @brief update table widget as per user selection
         */
        void updateTable();

        /**
         * @brief analyse and display the results for backfocus and tilt
         */
        void analyseResults();

        /**
         * @brief 3D graphic sim mode toggled by user
         * @param sim mode on / off
         */
        void simModeToggled(bool setting);

        /**
         * @brief update 3D graphic based on user selection
         * @param tileSelection
         */
        void updateGraphic(TileSelection tileSelection);

        /**
         * @brief draw Sensor on 3D graphic
         * @return success
         */
        bool processSensor();

        /**
         * @brief draw Sensor Labels on 3D graphic
         */
        void processSensorLabels();

        /**
         * @brief draw Petzval surface (light cone from flattener) on 3D graphic
         * @param tileSelection
         * @return success
         */
        bool processPetzval(TileSelection tileSelection);

        /**
         * @brief show / hide v-curve solution labels
         * @param show / hide setting
         */
        void setShowLabels(bool setting);

        /**
         * @brief show / hide CFZ
         * @param show / hide setting
         */
        void setShowCFZ(bool setting);

        /**
         * @brief Optimise tile centres based on weighted star position
         * @param setting
         */
        void setOptCentres(bool setting);

        /**
         * @brief resize table based on contents
         */
        void tableResize();

        /**
         * @brief calculate backfocus based on user selection
         * @param tileSelection
         * @param calculated backfocusDelta
         * @return success = true
         */
        bool calcBackfocusDelta(TileSelection tileSelection, double &backfocusDelta);

        /**
         * @brief calculate tilt based on user selection
         * @return success = true
         */
        bool calcTilt();

        /**
         * @brief calculates average of 3 tile values
         * @param tiles to average
         * @param retured tile average
         * @return success = true
         */
        bool avTiles(int tiles[3], double &average);

        /**
         * @brief set exclude tiles vector
         * @param row
         * @param checked
         * @param tile selection
         */
        void setExcludeTile(int row, bool checked, TileSelection tileSelection);

        /**
         * @brief get tile from table row
         * @param tileSelection
         * @param row
         * @return tile
         */
        int getTileFromRow(TileSelection tileSelection, int row);

        /**
         * @brief get the X,Y centre of the tile
         * @param tile
         * @return 2D vector of tile centre
         */
        QVector2D getXYTileCentre(tileID tile);

        /**
         * @brief get the label position for the passed in tile
         * @param tile
         * @return 3D vector of label position
         */
        QVector3D getLabelCentre(tileID tile);

        /**
         * @brief get the sensor vertex nearest the passed in tile
         * @param tile
         * @return 3D vector of sensor vertex
         */
        QVector3D getSensorVertex(tileID tile);

        /**
         * @brief get backspace adapted delta the passed in tile
         * @param tile
         * @return backspace adapted delta
         */
        double getBSDelta(tileID tile);
        QVector3D rotatePoint(QVector3D point);

        /**
         * @brief simulation of backfocus changed
         * @param value of slider
         */
        void simBackfocusChanged(int value);

        /**
         * @brief simulation of L-R tilt changed
         * @param value of slider
         */
        void simLRTiltChanged(int value);

        /**
         * @brief simulation of T-B tilt changed
         * @param value of slider
         */
        void simTBTiltChanged(int value);

        abInsData m_data;
        QVector<int> m_positions;
        QVector<QVector<double>> m_measures;
        QVector<QVector<double>> m_weights;
        QVector<QVector<int>> m_numStars;
        QVector<QPoint> m_tileOffsets;

        // Which tiles to use
        bool m_useTile[NUM_TILES] = { false, false, false, false, false, false, false, false, false };
        bool m_excludeTile[NUM_TILES] = { false, false, false, false, false, false, false, false, false };

        // Table
        SensorGraphic *sensorGraphic { nullptr };
        int m_HighlightedRow { -1 };

        // Curve fitting
        std::unique_ptr<CurveFitting> curveFitting;
        QVector<int> m_minimum;
        QVector<double> m_minMeasure;
        QVector<bool> m_fit;
        QVector<double> m_R2;

        // Analysis - the folowing members are in microns
        double m_backfocus = 0.0;
        QVector<double> m_deltas;
        double m_LRMicrons = 0.0;
        double m_TBMicrons = 0.0;
        double m_diagonalMicrons = 0.0;
        // Tilts are in % slope
        double m_LRTilt = 0.0;
        double m_TBTilt = 0.0;
        double m_diagonalTilt = 0.0;
        bool m_resultsOK = false;

        // Graphic simulation variables
        bool m_simMode { false };
        double m_simBackfocus { 0 };
        double m_simLRTilt { 0 };
        double m_simTBTilt { 0 };
        float m_maxX { 0.0 };
        float m_maxY { 0.0 };
        float m_minZ { 0.0 };
        float m_maxZ { 0.0 };

        // Plot widget
        AberrationInspectorPlot *m_plot;

        // Graphic
        Q3DSurface *m_graphic = nullptr;
        QSurface3DSeries *m_sensor = nullptr;
        QSurface3DSeries *m_petzval = nullptr;
        QSurfaceDataProxy *m_sensorProxy = nullptr;
        QSurfaceDataProxy *m_petzvalProxy = nullptr;
        QCustom3DLabel *m_label[NUM_TILES] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
        bool m_graphicLabels { true };
        bool m_graphicSensor { true };
        bool m_graphicPetzvalWire { true };
        bool m_graphicPetzvalSurface { false };
};

}
