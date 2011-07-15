#ifndef SIMPLEFOVEXPORTER_H
#define SIMPLEFOVEXPORTER_H

#include "QList"

class SkyPoint;
class SkyQPainter;
class FOV;
class KStarsData;
class SkyMap;
class QPainter;
class QPaintDevice;

class SimpleFovExporter
{
public:
    SimpleFovExporter();
    ~SimpleFovExporter();

    void exportFov(SkyPoint *point, FOV* fov, QPaintDevice *pd);
    void exportFov(const QList<SkyPoint*> &points, const QList<FOV*> &fovs, const QList<QPaintDevice*> &pds);
    void exportFov(const QList<SkyPoint*> &points, FOV* fov, const QList<QPaintDevice*> &pds);

    // getters
    inline bool isClockStopping() { return m_StopClock; }
    inline bool isFovShapeOverriden() { return m_OverrideFovShape; }
    inline bool isFovSymbolDrawn() { return m_DrawFovSymbol; }

    // setters
    inline void setClockStopping(bool stopping) { m_StopClock = stopping; }
    inline void setFovShapeOverriden(bool override) { m_OverrideFovShape = override; }
    inline void setFovSymbolDrawn(bool draw) { m_DrawFovSymbol = draw; }

private:
    inline double calculateZoomLevel(int pixelSize, float degrees) { return (pixelSize * 57.3 * 60) / degrees; }
    inline double calculatePixelSize(float degrees, double zoomLevel) { return degrees * zoomLevel / 57.3 / 60.0; }

    KStarsData *m_KSData;
    SkyMap *m_Map;

    bool m_StopClock;
    bool m_OverrideFovShape;
    bool m_DrawFovSymbol;
};

#endif // SIMPLEFOVEXPORTER_H
