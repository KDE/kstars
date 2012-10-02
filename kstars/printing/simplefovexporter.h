/***************************************************************************
                          simplefovexporter.h  -  K Desktop Planetarium
                             -------------------
    begin                : Thu Jul 7 2011
    copyright            : (C) 2011 by Rafał Kułaga
    email                : rl.kulaga@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

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

/**
  * \class SimpleFovExporter
  * \brief SimpleFovExporter class is used for FOV representation exporting.
  * Central point is defined by passed pointer to SkyPoint instance and field-of-view parameters
  * are defined by FOV class instance. Fragment of sky is painted on passed QPaintDevice subclass.
  * SimpleFovExporter class can be used for export of FOV representations in user-interactive mode as well as
  * for export of multiple FOVs at once, without user interaction.
  * \note Please note that SimpleFovExporter class instances may pause simulation clock if they're configured
  * to do so (via setClockStopping() method).
  * \note FOV representation's shape can be overridden (i.e. FOV image will be always rectangular) using
  * setFovShapeOverriden() method.
  */
class SimpleFovExporter
{
public:
    /**
      * \brief Constructor
      */
    SimpleFovExporter();

    /**
      * \brief Paint FOV representation on passed QPaintDevice subclass.
      * \param point central point of the exported FOV.
      * \param fov represented field-of-view.
      * \param pd paint device on which the representation of the FOV will be painted.
      */
    void exportFov(SkyPoint *point, FOV *fov, QPaintDevice *pd);

    /**
      * \brief Paint FOV representation on passed QPaintDevice subclass.
      * \param fov represented field-of-view.
      * \param pd paint device on which the representation of the FOV will be painted.
      */
    void exportFov(FOV *fov, QPaintDevice *pd);

    /**
      * \brief Paint FOV representation on passed QPaintDevice subclass.
      * \param pd paint device on which the representation of the FOV will be painted.
      */
    void exportFov(QPaintDevice *pd);

    /**
      * \brief Export multiple FOV representations.
      * \param points list of central points.
      * \param fovs list of fields-of-view.
      * \param pds list of paint devices on which the representation of the FOV will be painted.
      */
    void exportFov(const QList<SkyPoint*> &points, const QList<FOV*> &fovs, const QList<QPaintDevice*> &pds);

    /**
      * \brief Export multiple FOV representations.
      * \param points list of central points.
      * \param fovs list of fields-of-view.
      * \param pds list of paint devices on which the representation of the FOV will be painted.
      */
    void exportFov(const QList<SkyPoint*> &points, FOV* fov, const QList<QPaintDevice*> &pds);

    /**
      * \brief Check if FOV export will cause simulation clock to be stopped.
      * \return true if clock will be stopped for FOV export.
      * \note If changed, previous clock state will be restored after FOV export is done.
      */
    inline bool isClockStopping() { return m_StopClock; }

    /**
      * \brief Check if FOV representation will be always rectangular.
      * \return true if FOV shape is overridden.
      */
    inline bool isFovShapeOverriden() { return m_OverrideFovShape; }

    /**
      * \brief Check if FOV symbol will be drawn.
      * \return true if FOV symbol will be drawn.
      */
    inline bool isFovSymbolDrawn() { return m_DrawFovSymbol; }

    /**
      * \brief Enable or disable stopping simulation for FOV export.
      * \param stopping should be true if stopping is to be enabled.
      */
    inline void setClockStopping(bool stopping) { m_StopClock = stopping; }

    /**
      * \brief Enable or disable FOV shape overriding.
      * \param override should be true if FOV representation is to be always rectangular.
      */
    inline void setFovShapeOverriden(bool override) { m_OverrideFovShape = override; }

    /**
      * \brief Enable or disable FOV symbol drawing.
      * \param draw should be true if FOV symbol is to be drawn.
      */
    inline void setFovSymbolDrawn(bool draw) { m_DrawFovSymbol = draw; }

    /**
      * \brief Calculate zoom level at which given angular length will occupy given length in pixels.
      * \param pixelSize size in pixels.
      * \param degrees angular length.
      * \return zoom level.
      */
    static inline double calculateZoomLevel(int pixelSize, float degrees) { return (pixelSize * 57.3 * 60) / degrees; }

    /**
      * \brief Calculate pixel size of given angular length at given zoom level.
      * \param degrees angular length.
      * \param zoomLevel zoom level.
      * \return size in pixels.
      */
    static inline double calculatePixelSize(float degrees, double zoomLevel) { return degrees * zoomLevel / (57.3 * 60.0); }

private:
    /**
      * \brief Save SkyMap state.
      * \param savePos should be true if current position is to be saved.
      */
    void saveState(bool savePos);

    /**
      * \brief Restore saved SkyMap state.
      * \param restorePos should be true if saved position is to be restored.
      */
    void restoreState(bool restorePos);

    /**
      * \brief Private FOV export method.
      * \param point central point of the exported FOV.
      * \param fov represented field-of-view.
      * \param pd paint device on which the representation of the FOV will be painted.
      * \note Call to this method will not change SkyMap's state.
      */
    void pExportFov(SkyPoint *point, FOV* fov, QPaintDevice *pd);

    KStarsData *m_KSData;
    SkyMap *m_Map;

    bool m_StopClock;
    bool m_OverrideFovShape;
    bool m_DrawFovSymbol;

    bool m_PrevClockState;
    bool m_PrevSlewing;
    SkyPoint *m_PrevPoint;
    double m_PrevZoom;
};

#endif // SIMPLEFOVEXPORTER_H
