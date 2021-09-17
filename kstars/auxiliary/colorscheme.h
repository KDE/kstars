/*
    SPDX-FileCopyrightText: 2002 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QColor>
#include <QMap>
#include <QStringList>

/**
 * @class ColorScheme
 * This class stores all of the adjustable colors in KStars, in
 * a QMap object keyed by the names of the colors.  It also stores
 * information on how stars are to be rendered in the map
 * (with realistic colors, or as solid red/whit/black circles).
 * In addition to the brief "Key names" used to index the colors in
 * the QMap, each color has a "long name" description that is a bit
 * more verbose, and suitable for UI display.
 *
 * @author Jason Harris
 * @version 1.0
 */
class ColorScheme
{
  public:
    /**
     * Constructor. Enter all adjustable colors and their default
     * values into the QMap.  Also assign the corresponding long names.
     */
    ColorScheme();

    /** @return true if the Palette contains the given key name */
    bool hasColorNamed(const QString &name) const { return (Palette.contains(name)); }

    /**
     * @short Retrieve a color by name.
     * @p name the key name of the color to be retrieved.
     * @return the requested color, or Qt::white if color name not found.
     */
    QColor colorNamed(const QString &name) const;

    /**
     * @p i the index of the color to retrieve
     * @return a color by its index in the QMap
     */
    QColor colorAt(int i) const;

    /**
     * @p i the index of the long name to retrieve
     * @return the name of the color at index i
     */
    QString nameAt(int i) const;

    /**
     * @p i the index of the key name to retrieve
     * @return the key name of the color at index i
     */
    QString keyAt(int i) const;

    /**
     * @return the long name of the color whose key name is given
     * @p key the key name identifying the color.
     */
    QString nameFromKey(const QString &key) const;

    /**
     * Change the color with the given key to the given value
     * @p key the key-name of the color to be changed
     * @p color the new color value
     */
    void setColor(const QString &key, const QString &color);

    /**
     * Load a color scheme from a *.colors file
     * @p filename the filename of the color scheme to be loaded.
     * @return true if the scheme was successfully loaded
     */
    bool load(const QString &filename);

    /**
     * Save the current color scheme to a *.colors file.
     * @p name the filename to create
     * @return true if the color scheme is successfully writen to a file
     */
    bool save(const QString &name);

    /** @return the Filename associated with the color scheme. */
    QString fileName() const { return FileName; }

    /** Read color-scheme data from the Config object. */
    void loadFromConfig();

    /** Save color-scheme data to the Config object. */
    void saveToConfig();

    /** @return the number of colors in the color scheme. */
    unsigned int numberOfColors() const { return (int)Palette.size(); }

    /** @return the star color mode used by the color scheme */
    int starColorMode() const { return StarColorMode; }

    /** @return True if dark palette colors are used by the color scheme */
    bool useDarkPalette() const { return DarkPalette == 1; }

    /** @return the star color intensity value used by the color scheme */
    int starColorIntensity() const { return StarColorIntensity; }

    /**
     * Set the star color mode used by the color scheme
     * @p mode the star color mode to use
     */
    void setStarColorMode(int mode);

    /**
     * Set the star color intensity value used by the color scheme
     * @p intens The star color intensity value
     */
    void setStarColorIntensity(int intens);

    /**
     * Set the star color mode and intensity value used by the color scheme
     * @p mode the star color mode to use
     * @p intens The star color intensity value
     */
    void setStarColorModeIntensity(int mode, int intens);

    /**
     * @brief setDarkPalette Set whether the color schemes uses dark palette
     * @param enable True to use dark palette. False to use application default palette
     */
    void setDarkPalette(bool enable);

  private:
    /** Append items to all string lists. */
    void appendItem(const QString &key, const QString &name, const QString &def);

    int StarColorMode { 0 };
    int StarColorIntensity { 0 };
    int DarkPalette { 0 };
    QString FileName;
    QStringList KeyName, Name, Default;
    QMap<QString, QString> Palette;
};
