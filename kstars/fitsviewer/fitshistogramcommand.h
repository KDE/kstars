/*
    SPDX-FileCopyrightText: 2021 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QUndoCommand>
#include "fitsdata.h"

class FITSData;
class FITSHistogramEditor;

class FITSHistogramCommand : public QUndoCommand
{
    public:
        FITSHistogramCommand(const QSharedPointer<FITSData> &data, FITSHistogramEditor * inHisto, FITSScale newType,
                             const QVector<double> &lmin,
                             const QVector<double> &lmax);
        virtual ~FITSHistogramCommand();

        virtual void redo() override;
        virtual void undo() override;
        virtual QString text() const;

    private:
        bool calculateDelta(const uint8_t * buffer);
        bool reverseDelta();

        FITSImage::Statistic stats;

        QSharedPointer<FITSData> m_ImageData;
        FITSHistogramEditor * histogram { nullptr };
        FITSScale type;
        QVector<double> min, max;

        unsigned char * delta { nullptr };
        unsigned long compressedBytes { 0 };

};
