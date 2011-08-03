/***************************************************************************
                          legend.h  -  K Desktop Planetarium
                             -------------------
    begin                : Thu May 26 2011
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

#ifndef LEGEND_H
#define LEGEND_H

#include <QObject>
#include <QFont>
#include "QPoint"

class QPaintDevice;
class QPoint;
class QPointF;
class QSize;
class QString;

class SkyQPainter;
class KStars;
class ColorScheme;

/**@class Legend
 * Legend class is used for painting legends on class inheriting QPaintDevice.
 * Its methods enable changing settings of legend such as legend type (scale only/full legend),
 * symbol size, sizes for symbol description's bounding rectangles, symbol spacing etc.
 * Typical use of this class would be to create instance of Legend class, set all properties
 * using appropriate methods and paint it by calling paintLegend() method, passing QPaintDevice
 * or QPainter subclass (useful eg. with QSvgGenerator class, which can't be painted by two QPainter
 * classes).
 * @author Rafał Kułaga
 */

class Legend
{
public:

    enum LEGEND_ORIENTATION
    {
        LO_HORIZONTAL,
        LO_VERTICAL
    };

    enum LEGEND_POSITION
    {
        LP_UPPER_LEFT,
        LP_UPPER_RIGHT,
        LP_LOWER_LEFT,
        LP_LOWER_RIGHT,
        LP_FLOATING
    };

    /**@short Default constructor.
      *@param kstars pointer to KStars instance.
      *@param orientation legend orientation.
     */
    Legend(LEGEND_ORIENTATION orientation = LO_HORIZONTAL, LEGEND_POSITION pos = LP_FLOATING);

    /**@short Default destructor.*/
    ~Legend();

    /* 'GETTERS' */
    LEGEND_ORIENTATION getOrientation() { return m_Orientation; }

    inline LEGEND_POSITION getPosition() { return m_Position; }

    inline QPoint getFloatingPosition() { return m_PositionFloating; }

    inline bool getScaleOnly() { return m_ScaleOnly; }

    inline int getSymbolSize() { return m_SymbolSize; }

    inline int getBRectWidth() { return m_BRectWidth; }

    inline int getBRectHeight() { return m_BRectHeight; }

    inline qreal getMaxHScalePixels() { return m_MaxHScalePixels; }

    inline qreal getMaxVScalePixels() { return m_MaxVScalePixels; }

    inline int getXSymbolSpacing() { return m_XSymbolSpacing; }

    inline int getYSymbolSpacing() { return m_YSymbolSpacing; }

    inline QFont getFont() { return m_Font; }

    /* 'SETTERS' */
    inline void setOrientation(LEGEND_ORIENTATION orientation) { m_Orientation = orientation; }

    inline void setPosition(LEGEND_POSITION pos) { m_Position = pos; }

    inline void setFloatingPosition(QPoint pos) { m_PositionFloating = pos; }

    inline void setScaleOnly(bool scaleOnly) { m_ScaleOnly = scaleOnly; }

    inline void setSymbolSize(int size) { m_SymbolSize = size; }

    inline void setBRectWidth(int width) { m_BRectWidth = width; }

    inline void setBRectHeight(int height) { m_BRectHeight = height; }

    inline void setMaxHScalePixels(qreal pixels) { m_MaxHScalePixels = pixels; }

    inline void setMaxVScalePixels(qreal pixels) { m_MaxVScalePixels = pixels; }

    inline void setXSymbolSpacing(int spacing) { m_XSymbolSpacing = spacing; }

    inline void setYSymbolSpacing(int spacing) { m_YSymbolSpacing = spacing; }

    inline void setFont(const QFont &font) { m_Font = font; }

    /**@short Calculates size of legend that will be painted using current
      settings.
      *@return size of legend.
      */
    QSize calculateSize();

    /**@short Paint legend on passed QPaintDevice at selected position.
      *@param pd QPaintDevice on which legend will be painted.
      */
    void paintLegend(QPaintDevice *pd);

    /**@short Paint legend using passed SkyQPainter. This method is used to enable
      painting on QPaintDevice subclasses that can't be painted by multiple QPainter
      subclasses (e.g. QSvgGenerator).
      *@param painter that will be used to paint legend.
      *@note Passed SkyQPainter should be already set up to paint at specific QPaintDevice
      subclass and should be initialized by its begin() method. After legend is painted, SkyQPainter
      instance _will not_ be finished, so it's necessary to call end() method manually.
      */
    void paintLegend(SkyQPainter *painter);

    /**@short Paint legend on passed QPaintDevice at selected position.
      *@param pd QPaintDevice on which legend will be painted.
      *@param pos LEGEND_POSITION enum value.
      *@param scaleOnly should legend be painted scale-only?
      */
    void paintLegend(QPaintDevice *pd, LEGEND_POSITION pos, bool scaleOnly);

    /**@short Paint legend using passed SkyQPainter. This method is used to enable
      painting on QPaintDevice subclasses that can't be painted by multiple QPainter
      subclasses (eg. QSvgGenerator).
      *@param painter that will be used to paint legend.
      *@param pos LEGEND_POSITION enum value.
      *@param scaleOnly should legend be painted scale-only?
      *@note Passed SkyQPainter should be already set up to paint at specific QPaintDevice
      subclass and should be initialized by its begin() method. After legend is painted, SkyQPainter
      instance _will not_ be finished, so it's necessary to call end() method manually.
      */
    void paintLegend(SkyQPainter *painter, LEGEND_POSITION pos, bool scaleOnly);

private:
    /**@short Paint all symbols at passed position.
      *@param pos position at which symbols will be painted (upper left corner).
      *@note Orientation of the symbols group is determined by current legend orientation.
      */
    void paintSymbols(QPointF pos);

    /**@short Paint single symbol with specified parameters.
      *@param pos position at which symbol will be painted (center).
      *@param type symbol type (see SkyQPainter class for symbol types list).
      *@param e e parameter of symbol.
      *@param angle angle of symbol (in degrees).
      *@param label symbol label.
      */
    void paintSymbol(QPointF pos, int type, float e, float angle, QString label);

    /**@short Paint 'Star Magnitudes' group at passed position.
      *@param pos position at which 'Star Magnitudes' group will be painted (upper left corner).
      */
    void paintMagnitudes(QPointF pos);

    /**@short Paint chart scale bar at passed position.
      *@param pos position at which chart scale bar will be painted.
      *@note Orientation of chart scale bar is determined by current legend orientation. Maximal
      bar size is determined by current values set by setMaxHScalePixels()/setMaxVScalePixels() method.
      Exact size is adjusted to full deg/min/sec.
      */
    void paintScale(QPointF pos);

    /**@short Calculates legend position (upper left corner) based on LEGEND_POSITION enum value, paint
      device size and calculated legend size.
      *@param pos LEGEND_POSITION enum value.
      *@param pd paint device.
      *@param legendSize legend size (returned by Legend::calculateSize() method).
      */
    QPoint positionToDeviceCoord(QPaintDevice *pd);

    SkyQPainter *m_Painter;
    KStars *m_KStars;
    bool m_DeletePainter;

    LEGEND_ORIENTATION m_Orientation;
    LEGEND_POSITION m_Position;
    QPoint m_PositionFloating;
    bool m_ScaleOnly;

    ColorScheme *m_cScheme;


    QFont m_Font;

    int m_SymbolSize;
    int m_BRectWidth;
    int m_BRectHeight;
    qreal m_MaxHScalePixels;
    qreal m_MaxVScalePixels;
    int m_XSymbolSpacing;
    int m_YSymbolSpacing;
};

#endif // LEGEND_H
