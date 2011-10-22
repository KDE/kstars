#include "ccdcamera.h"

void CcdCamera::setCamera(const QString &id, const QString &model, const QString &vendor, const QString &remarks, int pixelsX,
                          int pixelsY, double pixelXSize, bool pixelXSizeDefined, double pixelYSize, bool pixelYSizeDefined,
                          double binning, bool binningDefined)
{
    Imager::setImager(id, model, vendor, remarks);

    m_PixelsX = pixelsX;
    m_PixelsY = pixelsY;
    m_PixelXSize = pixelXSize;
    m_PixelXSizeDefined = pixelXSizeDefined;
    m_PixelYSize = pixelYSize;
    m_PixelYSizeDefined = pixelYSizeDefined;
    m_Binning = binning;
    m_BinningDefined = binningDefined;
}
