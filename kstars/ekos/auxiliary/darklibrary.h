/*  Ekos Dark Library Handler
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "indi/indiccd.h"
#include "indi/indicap.h"

#include <QObject>

namespace Ekos
{
/**
 * @class DarkLibrary
 * @short Handles acquisition & loading of dark frames for cameras. If a suitable dark frame exists,
 * it is loaded from disk, otherwise it gets captured and saved for later use.
 *
 * @author Jasem Mutlaq
 * @version 1.0
 */
class DarkLibrary : public QObject
{
        Q_OBJECT

    public:
        static DarkLibrary *Instance();

        FITSData *getDarkFrame(ISD::CCDChip *targetChip, double duration);
        void subtract(FITSData *darkData, FITSView *lightImage, FITSScale filter, uint16_t offsetX, uint16_t offsetY);
        // Return false if canceled. True if dark capture proceeds
        void captureAndSubtract(ISD::CCDChip *targetChip, FITSView *targetImage, double duration, uint16_t offsetX,
                                uint16_t offsetY);
        void refreshFromDB();

        void setRemoteCap(ISD::GDInterface *remoteCap);
        void removeDevice(ISD::GDInterface *device);

    signals:
        void darkFrameCompleted(bool);
        void newLog(const QString &message);

    public slots:
        /**
         * @brief newFITS A new FITS blob is received by the CCD driver.
         * @param bp pointer to blob data
         */
        void newFITS(IBLOB *bp);

    private:
        explicit DarkLibrary(QObject *parent);
        ~DarkLibrary();

        static DarkLibrary *_DarkLibrary;

        bool loadDarkFile(const QString &filename);
        bool saveDarkFile(FITSData *darkData);

        template <typename T>
        void subtract(FITSData *darkData, FITSView *lightImage, FITSScale filter, uint16_t offsetX, uint16_t offsetY);

        QList<QVariantMap> darkFrames;
        QHash<QString, FITSData *> darkFiles;

        struct
        {
            ISD::CCDChip *targetChip { nullptr };
            double duration { 0 };
            uint16_t offsetX { 0 };
            uint16_t offsetY { 0 };
            FITSView *targetImage { nullptr };
            FITSScale filter;
        } subtractParams;

        bool m_TelescopeCovered { false };
        bool m_ConfirmationPending { false };


        QTimer captureSubtractTimer;
        ISD::DustCap *m_RemoteCap {nullptr};
};
}
