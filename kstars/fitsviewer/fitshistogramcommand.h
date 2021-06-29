/*  FITS Histogram Command
    Copyright (C) 2021 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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
