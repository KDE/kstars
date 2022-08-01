/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "../fitsviewer/fitsview.h"
#include "indicommon.h"

namespace ISD
{

class Camera;

/**
 * @class CameraChip
 * CameraChip class controls a particular chip in camera. While most amateur camera only have a single sensor, some
 * cameras (e.g. SBIG CCDs) have additional chips for guiding purposes.
 */
class CameraChip
{
    public:
        typedef enum { PRIMARY_CCD, GUIDE_CCD } ChipType;

        CameraChip(ISD::Camera *camera, ChipType type);

        FITSView *getImageView(FITSMode imageType);
        void setImageView(FITSView *image, FITSMode imageType);
        void setCaptureMode(FITSMode mode)
        {
            captureMode = mode;
        }
        void setCaptureFilter(FITSScale fType)
        {
            captureFilter = fType;
        }

        // Common commands
        bool getFrame(int *x, int *y, int *w, int *h);
        bool getFrameMinMax(int *minX, int *maxX, int *minY, int *maxY, int *minW, int *maxW, int *minH, int *maxH);
        bool setFrame(int x, int y, int w, int h, bool force = false);

        bool resetFrame();
        bool capture(double exposure);
        bool setFrameType(CCDFrameType fType);
        bool setFrameType(const QString &name);
        CCDFrameType getFrameType();
        bool setBinning(int bin_x, int bin_y);
        bool setBinning(CCDBinType binType);
        CCDBinType getBinning();
        bool getBinning(int *bin_x, int *bin_y);
        bool getMaxBin(int *max_xbin, int *max_ybin);
        ChipType getType() const
        {
            return m_Type;
        }
        ISD::Camera *getCCD()
        {
            return m_Camera;
        }

        // Set Image Info
        bool setImageInfo(uint16_t width, uint16_t height, double pixelX, double pixelY, uint8_t bitdepth);
        // Get Image Info
        bool getImageInfo(uint16_t &width, uint16_t &height, double &pixelX, double &pixelY, uint8_t &bitdepth);
        // Bayer Info
        bool getBayerInfo(uint16_t &offsetX, uint16_t &offsetY, QString &pattern);

        bool isCapturing();
        bool abortExposure();

        FITSMode getCaptureMode() const
        {
            return captureMode;
        }
        FITSScale getCaptureFilter() const
        {
            return captureFilter;
        }
        bool isBatchMode() const
        {
            return batchMode;
        }
        void setBatchMode(bool enable)
        {
            batchMode = enable;
        }
        QStringList getFrameTypes() const
        {
            return frameTypes;
        }
        void addFrameLabel(const QString &label)
        {
            frameTypes << label;
        }
        void clearFrameTypes()
        {
            frameTypes.clear();
        }

        bool canBin() const;
        void setCanBin(bool value);

        bool canSubframe() const;
        void setCanSubframe(bool value);

        bool canAbort() const;
        void setCanAbort(bool value);

        const QSharedPointer<FITSData> &getImageData() const;
        void setImageData(const QSharedPointer<FITSData> &data)
        {
            imageData = data;
        }

        int getISOIndex() const;
        bool getISOValue(QString &value) const;
        bool setISOIndex(int value);

        QStringList getISOList() const;

    private:
        QPointer<FITSView> normalImage, focusImage, guideImage, calibrationImage, alignImage;
        QSharedPointer<FITSData> imageData { nullptr };
        FITSMode captureMode { FITS_NORMAL };
        FITSScale captureFilter { FITS_NONE };
        bool batchMode { false };
        QStringList frameTypes;
        bool CanBin { false };
        bool CanSubframe { false };
        bool CanAbort { false };

        ISD::Camera *m_Camera { nullptr };
        ChipType m_Type;
};

}
