/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QString>

/**
 * @class DetalDialogLite
 * @short Backend for Object details dialog in QML
 * A backend of details dialog declared in QML. Members of this class are the properties that are used
 * in QML. Whenever user clicks on some object the properties are updated with the info about this object
 * and Details dialog in QML is updated automatically as we use property binding there.
 *
 * @author Artem Fedoskin, Jason Harris, Jasem Mutlaq
 * @version 1.0
 */
class DetailDialogLite : public QObject
{
    Q_OBJECT

    //General
    Q_PROPERTY(QString name MEMBER m_name NOTIFY nameChanged)
    Q_PROPERTY(QString magnitude MEMBER m_magnitude NOTIFY magnitudeChanged)
    Q_PROPERTY(QString distance MEMBER m_distance NOTIFY distanceChanged)
    Q_PROPERTY(QString BVindex MEMBER m_BVindex NOTIFY BVindexChanged)
    Q_PROPERTY(QString angSize MEMBER m_angSize NOTIFY angSizeChanged)
    Q_PROPERTY(QString illumination MEMBER m_illumination NOTIFY illuminationChanged)
    Q_PROPERTY(QString typeInConstellation MEMBER m_typeInConstellation NOTIFY typeInConstellationChanged)
    Q_PROPERTY(QString thumbnail MEMBER m_thumbnail NOTIFY thumbnailChanged)

    //General advanced
    Q_PROPERTY(QString perihelion MEMBER m_perihelion NOTIFY perihelionChanged)
    Q_PROPERTY(QString orbitID MEMBER m_orbitID NOTIFY orbitIDChanged)
    Q_PROPERTY(QString NEO MEMBER m_NEO NOTIFY NEOChanged)
    Q_PROPERTY(QString diameter MEMBER m_diameter NOTIFY diameterChanged)
    Q_PROPERTY(QString rotation MEMBER m_rotation NOTIFY rotationChanged)
    Q_PROPERTY(QString earthMOID MEMBER m_earthMOID NOTIFY earthMOIDChanged)
    Q_PROPERTY(QString orbitClass MEMBER m_orbitClass NOTIFY orbitClassChanged)
    Q_PROPERTY(QString albedo MEMBER m_albedo NOTIFY albedoChanged)
    Q_PROPERTY(QString dimensions MEMBER m_dimensions NOTIFY dimensionsChanged)
    Q_PROPERTY(QString period MEMBER m_period NOTIFY periodChanged)

    //Position
    Q_PROPERTY(QString decLabel MEMBER m_decLabel NOTIFY decLabelChanged)
    Q_PROPERTY(QString dec MEMBER m_dec NOTIFY decChanged)

    Q_PROPERTY(QString RALabel MEMBER m_RALabel NOTIFY RALabelChanged)
    Q_PROPERTY(QString RA MEMBER m_RA NOTIFY RAChanged)

    Q_PROPERTY(QString az MEMBER m_az NOTIFY azChanged)
    Q_PROPERTY(QString airmass MEMBER m_airmass NOTIFY airmassChanged)
    Q_PROPERTY(QString HA MEMBER m_HA NOTIFY HAChanged)
    Q_PROPERTY(QString alt MEMBER m_alt NOTIFY altChanged)
    Q_PROPERTY(QString RA0 MEMBER m_RA0 NOTIFY RA0Changed)
    Q_PROPERTY(QString dec0 MEMBER m_dec0 NOTIFY dec0Changed)

    Q_PROPERTY(QString timeRise MEMBER m_timeRise NOTIFY timeRiseChanged)
    Q_PROPERTY(QString timeTransit MEMBER m_timeTransit NOTIFY timeTransitChanged)
    Q_PROPERTY(QString timeSet MEMBER m_timeSet NOTIFY timeSetChanged)

    Q_PROPERTY(QString azRise MEMBER m_azRise NOTIFY azRiseChanged)
    Q_PROPERTY(QString altTransit MEMBER m_altTransit NOTIFY altTransitChanged)
    Q_PROPERTY(QString azSet MEMBER m_azSet NOTIFY azSetChanged)

    //Links
    Q_PROPERTY(QStringList infoTitleList MEMBER m_infoTitleList NOTIFY infoTitleListChanged)
    Q_PROPERTY(QStringList imageTitleList MEMBER m_imageTitleList NOTIFY imageTitleListChanged)
    Q_PROPERTY(bool isLinksOn MEMBER m_isLinksOn NOTIFY isLinksOnChanged)

    //Log
    Q_PROPERTY(bool isLogOn MEMBER m_isLogOn NOTIFY isLogOnChanged)
    Q_PROPERTY(QString userLog MEMBER m_userLog NOTIFY userLogChanged)

  public:
    DetailDialogLite();

    /** Connect SkyMapLite's signals to proper slots */
    void initialize();

    /** Set thumbnail to SkyMapLite::clickedObjectLite's thumbnail (if any) */
    void setupThumbnail();

    /**
     * @brief addLink adds new link to SkyObject
     * @param url URL of the link
     * @param desc description of the link
     * @param isImageLink true if it is a link to image. False if it is information link
     */
    Q_INVOKABLE void addLink(const QString &url, const QString &desc, bool isImageLink);

    /**
     * @short Remove link from user's database
     * @param itemIndex - index of a link
     * @param isImage - true if it is a link on image, false if it is an info link
     */
    Q_INVOKABLE void removeLink(int itemIndex, bool isImage);

    /**
     * @short Edit link's description and URL
     * @param itemIndex - index of a link
     * @param isImage - true if it is a link on image, false if it is an info link
     * @param desc - new description
     * @param url - new URL
     */
    void editLink(int itemIndex, bool isImage, const QString &desc, const QString &url);

    /**
     * Update the local info_url and image_url files
     * @param type The URL type. 0 for Info Links, 1 for Images.
     * @param search_line The line to be search for in the local URL files
     * @param replace_line The replacement line once search_line is found.
     * @note If replace_line is empty, the function will remove search_line from the file
     */
    void updateLocalDatabase(int type, const QString &search_line, const QString &replace_line = QString());

    //We don't need bindings to URLs so let's just have getters
    /**
     * @param index - URL's index in SkyObject::ImageList()
     * @return URL to user added information about object
     */
    Q_INVOKABLE QString getInfoURL(int index);

    /**
     * @param index - URL's index in SkyObject::ImageList()
     * @return URL to user added object image
     */
    Q_INVOKABLE QString getImageURL(int index);

  public slots:
    /** Update properties that are shown on "General" tab */
    void createGeneralTab();

    /** Update properties that are shown on "Position" tab */
    void createPositionTab();

    /** Update properties that are shown on "Log" tab */
    void createLogTab();

    /** Update properties that are shown on "Links" tab */
    void createLinksTab();

    /** Save the User's text in the Log Tab to the userlog.dat file. */
    void saveLogData(const QString &userLog);

  signals:
    //General
    void nameChanged(QString);
    void magnitudeChanged(QString);
    void distanceChanged(QString);
    void BVindexChanged(QString);
    void angSizeChanged(QString);
    void illuminationChanged(QString);
    void typeInConstellationChanged(QString);
    void thumbnailChanged(QString);

    //General advanced
    void perihelionChanged(QString);
    void orbitIDChanged(QString);
    void NEOChanged(QString);
    void diameterChanged(QString);
    void rotationChanged(QString);
    void earthMOIDChanged(QString);
    void orbitClassChanged(QString);
    void albedoChanged(QString);
    void dimensionsChanged(QString);
    void periodChanged(QString);

    //Position
    void decLabelChanged();
    void decChanged();

    void RALabelChanged();
    void RAChanged();

    void azChanged();
    void airmassChanged();
    void HAChanged();
    void altChanged();
    void RA0Changed();
    void dec0Changed();

    void timeRiseChanged();
    void timeTransitChanged();
    void timeSetChanged();

    void azRiseChanged();
    void altTransitChanged();
    void azSetChanged();

    //Links
    void infoTitleListChanged();
    void imageTitleListChanged();
    void isLinksOnChanged();

    //Log
    void isLogOnChanged();
    void userLogChanged();

  private:
    //General
    QString m_name;
    QString m_magnitude;
    QString m_distance;
    QString m_BVindex;
    QString m_angSize;
    QString m_illumination;
    QString m_typeInConstellation;
    QString m_thumbnail;

    //General advanced
    QString m_perihelion;
    QString m_orbitID;
    QString m_NEO;
    QString m_diameter;
    QString m_rotation;
    QString m_earthMOID;
    QString m_orbitClass;
    QString m_albedo;
    QString m_dimensions;
    QString m_period;

    //Position
    QString m_decLabel;
    QString m_dec;

    QString m_RALabel;
    QString m_RA;

    QString m_az;
    QString m_airmass;
    QString m_HA;
    QString m_alt;
    QString m_RA0;
    QString m_dec0;

    QString m_timeRise;
    QString m_timeTransit;
    QString m_timeSet;

    QString m_azRise;
    QString m_altTransit;
    QString m_azSet;

    //Links
    bool m_isLinksOn { false };
    QStringList m_infoTitleList;
    QStringList m_imageTitleList;

    //Log
    bool m_isLogOn { false };
    QString m_userLog;
};
