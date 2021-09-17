/*
    SPDX-FileCopyrightText: 2015 M.S.Adityan <msadityan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skycomponent.h"

class ConstellationsArt;
class CultureList;

/**
 * @class ConstellationArtComponent
 * Represents the ConstellationsArt objects.
 * For each skyculture there is a separate table in skycultures.sqlite.
 * @author M.S.Adityan
 * @version 0.1
 */
class ConstellationArtComponent : public SkyComponent
{
  public:
    /** Constructor */
    explicit ConstellationArtComponent(SkyComposite *, CultureList *cultures);

    /** Destructor */
    ~ConstellationArtComponent() override;

    /**
     * @short Read the skycultures.sqlite database file.
     * Parse all the data from the skycultures database.Construct a ConstellationsArt object
     * from the data, and add it to a QList.
     * @return true if data file is successfully read.
     */
    void loadData();

    /**
     * @short deletes all created ConstellationsArt objects.
     * Used in KStars Lite to reduce memory consumption if Constellation Art is switched off
     */
    void deleteData();

    /** @short Shows the details of the constellations selected skyculture */
    void showList();

    void draw(SkyPainter *skyp) override;

    QList<ConstellationsArt *> m_ConstList;

  private:
    QString cultureName;
    int records { 0 };
};
