/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kstarsdocument.h"

class QString;
class QRectF;
class QImage;

class LoggingForm;
class DetailsTable;
class KStarsDateTime;
class GeoLocation;

/**
  * @class FinderChart
  * @brief Class that represents finder chart document.
  * FinderChart class is a subclass of KStarsDocument class, representing finder chart.
  * Finder chart can contain following elements: title, subtitle, description, logging forms,
  * images and details tables.
  *
  * @author Rafał Kułaga
  */
class FinderChart : public KStarsDocument
{
  public:
    /** Constructor */
    FinderChart();

    /**
     * @brief Insert title and subtitle to the finder chart.
     * @param title Title.
     * @param subtitle Subtitle.
     */
    void insertTitleSubtitle(const QString &title, const QString &subtitle);

    /**
     * @brief Insert description to the finder chart.
     * @param description Description text.
     */
    void insertDescription(const QString &description);

    /**
     * @brief Insert details about date&time and geographic location.
     * @param ut Date and time.
     * @param geo Geographic location.
     */
    void insertGeoTimeInfo(const KStarsDateTime &ut, GeoLocation *geo);

    /**
     * @brief Insert logging form to the finder chart.
     * @param log Logging form to be inserted.
     */
    void insertLoggingForm(LoggingForm *log);

    /**
     * @brief Insert image to the finder chart.
     * @param img Image to be inserted.
     * @param description Description of the image.s
     * @param descriptionBelow True if description should be placed below image.
     */
    void insertImage(const QImage &img, const QString &description, bool descriptionBelow = true);

    /**
     * @brief Insert details table to the finder chart.
     * @param table Details table to be inserted.
     */
    void insertDetailsTable(DetailsTable *table);

    /**
     * @brief Insert section title to the finder chart.
     * @param title Section title.
     */
    void insertSectionTitle(const QString &title);
};
