#ifndef CCDCAMERA_H
#define CCDCAMERA_H

#include "oal.h"
#include "imager.h"

class CcdCamera : public Imager
{
public:
    CcdCamera(const QString &id, const QString &model, const QString &vendor, const QString &remarks, const int pixelsX,
              const int pixelsY, const double pixelXSize, const bool pixelXSizeDefined, const double pixelYSize,
              const bool pixelYSizeDefined, const double binning, const bool binningDefined)
                  : Imager(id, model, vendor, remarks)
    {
        setCamera(id, model, vendor, remarks, pixelsX, pixelsY, pixelXSize, pixelXSizeDefined, pixelYSize,
                  pixelYSizeDefined, binning, binningDefined);
    }

    int pixelsX() const { return m_PixelsX; }
    int pixelsY() const { return m_PixelsY; }
    double pixelXSize() const { return m_PixelXSize; }
    double pixelYSize() const { return m_PixelYSize; }
    double binning() const { return m_Binning; }

    void setCamera(const QString &id, const QString &model, const QString &vendor, const QString &remarks, const int pixelsX,
                   const int pixelsY, const double pixelXSize, const bool pixelXSizeDefined, const double pixelYSize,
                   const bool pixelYSizeDefined, const double binning, const bool binningDefined);

private:
    int m_PixelsX;
    int m_PixelsY;
    double m_PixelXSize;
    double m_PixelYSize;
    double m_Binning;

    bool m_PixelXSizeDefined;
    bool m_PixelYSizeDefined;
    bool m_BinningDefined;
};

#endif // CCDCAMERA_H
