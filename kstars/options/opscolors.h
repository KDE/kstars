/*
    SPDX-FileCopyrightText: 2004 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opscolors.h"

#include <qstringlist.h>

class KStars;

/**
 * @class OpsColors
 *
 * The Colors page allows the user to adjust all of the colors used to
 * display the night sky.  The colors are presented as a list of
 * colored rectangles and a description of its assignment in the map.
 * Clicking on any color opens a KColorDialog for selecting a new color.
 *
 * The user can also load preset color schemes, or create new schemes
 * from the current set of colors.
 *
 * @short The Colors page of the Options window.
 * @author Jason Harris
 * @author Jasem Mutlaq
 * @version 1.1
 */
class OpsColors : public QFrame, public Ui::OpsColors
{
    Q_OBJECT

  public:
    explicit OpsColors();
    virtual ~OpsColors() override = default;

  private slots:
    void newColor(QListWidgetItem *item);
    void slotPreset(int i);
    void slotAddPreset();
    void slotSavePreset();
    void slotRemovePreset();
    void slotStarColorMode(int);
    void slotStarColorIntensity(int);
    void slotChangeTheme(QListWidgetItem *item);

  private:
    bool setColors(const QString &filename);

    QStringList PresetFileList;
};
