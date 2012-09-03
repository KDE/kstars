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
#include <QPoint>
#include <QColor>

class QPaintDevice;
class QPointF;
class QSize;

class SkyQPainter;
class SkyMap;
class ColorScheme;

/**
 * \class Legend
 * \brief Legend class is used for painting legends on class inheriting QPaintDevice.
 * Its methods enable changing settings of legend such as legend type (scale only/full legend),
 * symbol size, sizes for symbol description's bounding rectangles, symbol spacing etc.
 * Typical use of this class would be to create instance of Legend class, set all properties
 * using appropriate methods and paint it by calling paintLegend() method, passing QPaintDevice
 * or QPainter subclass (useful eg. with QSvgGenerator class, which can't be painted by two QPainter
 * classes).
 * \author Rafał Kułaga
 */
class Legend
{
public:

    /**
      * \brief Legend type enumeration.
      */
    enum LEGEND_TYPE
    {
        LT_FULL,
        LT_SCALE_MAGNITUDES,
        LT_SCALE_ONLY,
        LT_MAGNITUDES_ONLY,
        LT_SYMBOLS_ONLY,
    };

    /**
      * \brief Legend orientation enumeration.
      */
    enum LEGEND_ORIENTATION
    {
        LO_HORIZONTAL,
        LO_VERTICAL
    };

    /**
      * \brief Legend position enumeration.
      */
    enum LEGEND_POSITION
    {
        LP_UPPER_LEFT,
        LP_UPPER_RIGHT,
        LP_LOWER_LEFT,
        LP_LOWER_RIGHT,
        LP_FLOATING
    };

    /**
      * \brief Constructor.
      * \param orientation Legend orientation.
      * \param pos Legend position.
     */
    Legend(LEGEND_ORIENTATION orientation = LO_HORIZONTAL, LEGEND_POSITION pos = LP_FLOATING);

    /**
      * \brief Destructor.
      */
    ~Legend();

    /**
      * \brief Get legend type.
      * \return Current legend type value.
      */
    inline LEGEND_TYPE getType() { return m_Type; }

    /**
      * \brief Get legend orientation.
      * \return Current legend orientation value.
      */
    inline LEGEND_ORIENTATION getOrientation() { return m_Orientation; }

    /**
      * \brief Get legend position.
      * \return Current legend position value.
      */
    inline LEGEND_POSITION getPosition() { return m_Position; }

    /**
      * \brief Get position of the floating legend.
      * \return Current position of the floating legend.
      */
    inline QPoint getFloatingPosition() { return m_PositionFloating; }

    /**
      * \brief Get symbol size.
      * \return Current symbol size value.
      */
    inline int getSymbolSize() { return m_SymbolSize; }

    /**
      * \brief Get symbol description's bounding rectangle width.
      * \return Current bounding rectangle width.
      */
    inline int getBRectWidth() { return m_BRectWidth; }

    /**
      * \brief Get symbol description's bounding rectangle height.
      * \return Current bounding rectangle height.
      */
    inline int getBRectHeight() { return m_BRectHeight; }

    /**
      * \brief Get maximal horizontal scale size in pixels.
      * \return Current maximal horizontal scale size in pixels.
      */
    inline int getMaxHScalePixels() { return m_MaxHScalePixels; }

    /**
      * \brief Get maximal vertical scale size in pixels.
      * \brief Current maximal vertical scale size in pixels.
      */
    inline int getMaxVScalePixels() { return m_MaxVScalePixels; }

    /**
      * \brief Get symbol image X spacing.
      * \return Current symbol image X spacing.
      */
    inline int getXSymbolSpacing() { return m_XSymbolSpacing; }

    /**
      * \brief Get symbol image Y spacing.
      * \return Current symbol image Y spacing.
      */
    inline int getYSymbolSpacing() { return m_YSymbolSpacing; }

    /**
      * \brief Get font.
      * \return Current font.
      */
    inline QFont getFont() { return m_Font; }

    /**
      * \brief Get background color.
      * \return Current background color.
      */
    inline QColor getBgColor() { return m_BgColor; }

    /**
      * \brief Check if frame around legend is drawn.
      * \return True if frame is drawn.
      */
    inline bool getDrawFrame() { return m_DrawFrame; }

    /**
      * \brief Set legend type.
      * \param type New legend type.
      */
    inline void setType(LEGEND_TYPE type) { m_Type = type; }

    /**
      * \brief Set legend orientation.
      * \param orientation New legend orientation.
      */
    inline void setOrientation(LEGEND_ORIENTATION orientation) { m_Orientation = orientation; }

    /**
      * \brief Set legend position.
      * \param pos New legend position enumeration value.
      */
    inline void setPosition(LEGEND_POSITION pos) { m_Position = pos; }

    /**
      * \brief Set floating legend position.
      * \param pos New floating legend position.
      */
    inline void setFloatingPosition(QPoint pos) { m_PositionFloating = pos; }

    /**
      * \brief Set symbol size.
      * \param size New symbol size.
      */
    inline void setSymbolSize(int size) { m_SymbolSize = size; }

    /**
      * \brief Set symbol description's bounding rectangle width.
      * \param width New bounding rectangle width.
      */
    inline void setBRectWidth(int width) { m_BRectWidth = width; }

    /**
      * \brief Set symbol description's bounding rectangle height.
      * \param height New bounding rectangle height.
      */
    inline void setBRectHeight(int height) { m_BRectHeight = height; }

    /**
      * \brief Set maximal horizontal scale size in pixels.
      * \param pixels New maximal horizontal scale size in pixels.
      */
    inline void setMaxHScalePixels(int pixels) { m_MaxHScalePixels = pixels; }

    /**
      * \brief Set maximal vertical scale size in pixels.
      * \param pixels New maximal vertical scale size in pixels.
      */
    inline void setMaxVScalePixels(int pixels) { m_MaxVScalePixels = pixels; }

    /**
      * \brief Set symbol X spacing in pixels.
      * \param spacing New symbol X spacing in pixels.
      */
    inline void setXSymbolSpacing(int spacing) { m_XSymbolSpacing = spacing; }

    /**
      * \brief Set symbol Y spacing in pixels.
      * \param spacing New symbol Y spacing in pixels.
      */
    inline void setYSymbolSpacing(int spacing) { m_YSymbolSpacing = spacing; }

    /**
      * \brief Set font.
      * \param font New font.
      */
    inline void setFont(const QFont &font) { m_Font = font; }

    /**
      * \brief Set background color.
      * \param color New background color.
      */
    inline void setBgColor(const QColor &color) { m_BgColor = color; }

    /**
      * \brief Set if frame is drawn.
      * \param draw True if frame should be drawn.
      */
    inline void setDrawFrame(bool draw) { m_DrawFrame = draw; }

    /**
      * \brief Calculates size of legend that will be painted using current settings.
      * \return Size of legend.
      */
    QSize calculateSize();

    /**
      * \brief Paint legend on passed QPaintDevice at selected position.
      * \param pd QPaintDevice on which legend will be painted.
      */
    void paintLegend(QPaintDevice *pd);

    /**
      * \brief Paint legend using passed SkyQPainter.
      * This method is used to enable painting on QPaintDevice subclasses that can't be
      * painted by multiple QPainter subclasses (e.g. QSvgGenerator).
      * \param painter that will be used to paint legend.
      * \note Passed SkyQPainter should be already set up to paint at specific QPaintDevice
      * subclass and should be initialized by its begin() method. After legend is painted, SkyQPainter
      * instance _will not_ be finished, so it's necessary to call end() method manually.
      */
    void paintLegend(SkyQPainter *painter);

    /**
      * \brief Paint legend on passed QPaintDevice at selected position.
      * \param pd QPaintDevice on which legend will be painted.
      * \param pos LEGEND_POSITION enum value.
      * \param scaleOnly should legend be painted scale-only?
      */
    void paintLegend(QPaintDevice *pd, LEGEND_TYPE type, LEGEND_POSITION pos);

    /**
      * \brief Paint legend using passed SkyQPainter.
      * This method is used to enable painting on QPaintDevice subclasses that can't be painted
      * by multiple QPainter subclasses (eg. QSvgGenerator).
      * \param painter that will be used to paint legend.
      * \param pos LEGEND_POSITION enum value.
      * \param scaleOnly should legend be painted scale-only?
      * \note Passed SkyQPainter should be already set up to paint at specific QPaintDevice
      * subclass and should be initialized by its begin() method. After legend is painted, SkyQPainter
      * instance _will not_ be finished, so it's necessary to call end() method manually.
      */
    void paintLegend(SkyQPainter *painter, LEGEND_TYPE type, LEGEND_POSITION pos);

private:
    /**
      * \brief Paint all symbols at passed position.
      * \param pos position at which symbols will be painted (upper left corner).
      * \note Orientation of the symbols group is determined by current legend orientation.
      */
    void paintSymbols(QPointF pos);

    /**
      * \brief Paint single symbol with specified parameters.
      * \param pos position at which symbol will be painted (center).
      * \param type symbol type (see SkyQPainter class for symbol types list).
      * \param e e parameter of symbol.
      * \param angle angle of symbol (in degrees).
      * \param label symbol label.
      */
    void paintSymbol(QPointF pos, int type, float e, float angle, QString label);

    /**
      * \brief Paint 'Star Magnitudes' group at passed position.
      * \param pos position at which 'Star Magnitudes' group will be painted (upper left corner).
      */
    void paintMagnitudes(QPointF pos);

    /**
      * \brief Paint chart scale bar at passed position.
      * \param pos position at which chart scale bar will be painted.
      * \note Orientation of chart scale bar is determined by current legend orientation. Maximal
      * bar size is determined by current values set by setMaxHScalePixels()/setMaxVScalePixels() method.
      * Exact size is adjusted to full deg/min/sec.
      */
    void paintScale(QPointF pos);

    /**
      * \brief Calculates legend position (upper left corner) based on LEGEND_POSITION enum value, paint device size and calculated legend size.
      * \param pos LEGEND_POSITION enum value.
      */
    QPoint positionToDeviceCoord(QPaintDevice *pd);

    SkyQPainter *m_Painter;
    SkyMap *m_SkyMap;
    bool m_DeletePainter;

    LEGEND_TYPE m_Type;
    LEGEND_ORIENTATION m_Orientation;
    LEGEND_POSITION m_Position;
    QPoint m_PositionFloating;

    ColorScheme *m_cScheme;
    QFont m_Font;
    QColor m_BgColor;
    bool m_DrawFrame;

    int m_SymbolSize;
    int m_BRectWidth;
    int m_BRectHeight;
    int m_MaxHScalePixels;
    int m_MaxVScalePixels;
    int m_XSymbolSpacing;
    int m_YSymbolSpacing;
};

#endif // LEGEND_H
