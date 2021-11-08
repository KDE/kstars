/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indi/indiccd.h"
#include "indi/indicap.h"
#include "darkview.h"
#include "defectmap.h"
#include "ekos/ekos.h"

#include <QFutureWatcher>

class TestDefects;
class TestSubtraction;

namespace Ekos
{

/**
 * @brief The DarkProcessor class
 *
 * Denoises a light image by either subtracting a dark frame or applying a defect map.
 *
 * The primary denoise function searches first for defect maps that matches the criteria of the passed parameters.
 * These include sensor binning, size, temperature, and date. If a defect map is found, then it is loaded from disk and all the bad
 * pixels are treated with a 3x3 median filter. If no defect map is found, it searches for suitable dark frames and if any is found then
 * a simple subtraction is applied.
 *
 * @author Jasem Mutlaq
 * @version 1.0
 */
class DarkProcessor : public QObject
{
        Q_OBJECT
    public:
        explicit DarkProcessor(QObject *parent = nullptr);

        // Perform defect correction or dark subtraction
        void denoise(ISD::CCDChip *targetChip, const QSharedPointer<FITSData> &targetData, double duration,
                     uint16_t offsetX, uint16_t offsetY);


    private:

        // Denoise Internal
        bool denoiseInternal();
        void processDenoiseResult();

        ////////////////////////////////////////////////////////////////////////////////////////////////
        /// Subtraction Functions
        ////////////////////////////////////////////////////////////////////////////////////////////////

        /**
        * @brief subtractHelper Calls tempelated subtract function
        * @param darkData passes dark frame data to templerated subtract function.
        * @param lightData passes list frame data to templerated subtract function.
        * @param offsetX passes offsetX to templerated subtract function.
        * @param offsetY passes offsetY to templerated subtract function.
        */
        void subtractDarkData(const QSharedPointer<FITSData> &darkData, const QSharedPointer<FITSData> &lightData,
                              uint16_t offsetX, uint16_t offsetY);

        /**
        * @brief subtract Subtracts dark pixels from light pixels given the supplied parameters
        * @param darkData Dark frame data.
        * @param lightData Light frame data. The light frame data is modified in this process.
        * @param offsetX Only apply subtraction beyond offsetX in X-axis.
        * @param offsetY Only apply subtraction beyond offsetY in Y-axis.
        */
        template <typename T>
        void subtractInternal(const QSharedPointer<FITSData> &darkData, const QSharedPointer<FITSData> &lightData,
                              uint16_t offsetX, uint16_t offsetY);

        ////////////////////////////////////////////////////////////////////////////////////////////////
        /// Defect Map Functions
        ////////////////////////////////////////////////////////////////////////////////////////////////

        /**
        * @brief normalizeDefects Remove defects from LIGHT image by replacing bad pixels with a 3x3 median filter around
        * them.
        * @param defectMap Defect Map containing a list of hot and cold pixels.
        * @param lightData Target light data to remove noise from.
        * @param offsetX Only apply filtering beyond offsetX in X-axis.
        * @param offsetY Only apply filtering beyond offsetX in Y-axis.
        */
        void normalizeDefects(const QSharedPointer<DefectMap> &defectMap, const QSharedPointer<FITSData> &lightData,
                              uint16_t offsetX, uint16_t offsetY);

        template <typename T>
        void normalizeDefectsInternal(const QSharedPointer<DefectMap> &defectMap, const QSharedPointer<FITSData> &lightData,
                                      uint16_t offsetX, uint16_t offsetY);

        template <typename T>
        T median3x3Filter(uint16_t x, uint16_t y, uint32_t width, T *buffer);

    signals:
        void darkFrameCompleted(bool);
        void newLog(const QString &message);

    private:
        QFutureWatcher<bool> m_Watcher;
        struct
        {
            ISD::CCDChip *targetChip;
            QSharedPointer<FITSData> targetData;
            double duration;
            uint16_t offsetX;
            uint16_t offsetY;
        } info;


        // Testing
        friend class ::TestDefects;
        friend class ::TestSubtraction;

};

}

