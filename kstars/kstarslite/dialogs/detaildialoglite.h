/***************************************************************************
                          detaildialoglite.h  -  K Desktop Planetarium
                             -------------------
    begin                : Aug Jul 13 2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef DETAILDIALOGLITE_H_
#define DETAILDIALOGLITE_H_

#include "skyobjects/skyobject.h"
#include <QImage>

class QTimer;
class QStringListModel;
class QSortFilterProxyModel;
class SkyObjectListModel;

/** @class DetalDialogLite
 * A backend of details dialog declared in QML.
 *
 * @short Backend for Object details dialog in QML
 * @author Artem Fedoskin, Jason Harris, Jasem Mutlaq
 * @version 1.0
 */
class DetailDialogLite : public QObject {
    Q_OBJECT
    //General
    Q_PROPERTY(QString name MEMBER m_name NOTIFY nameChanged )
    //Q_PROPERTY(QString otherName MEMBER m_otherName NOTIFY otherNameChanged)
    Q_PROPERTY(QString magnitude MEMBER m_magnitude NOTIFY magnitudeChanged)
    Q_PROPERTY(QString distance MEMBER m_distance NOTIFY distanceChanged)
    Q_PROPERTY(QString BVindex MEMBER m_BVindex NOTIFY BVindexChanged)
    //Q_PROPERTY(QString angSizeLabel MEMBER m_angSizeLabel NOTIFY angSizeLabelChanged)
    Q_PROPERTY(QString angSize MEMBER m_angSize NOTIFY angSizeChanged)
    Q_PROPERTY(QString illumination MEMBER m_illumination NOTIFY illuminationChanged)
    Q_PROPERTY(QString typeInConstellation MEMBER m_typeInConstellation NOTIFY typeInConstellationChanged)
    Q_PROPERTY(QString thumbnail MEMBER m_thumbnail NOTIFY thumbnailChanged)
public:
    explicit DetailDialogLite();
    /**
     * @short initialize connects SkyMapLite's signals to proper slots
     */
    void initialize();

    void setupThumbnail();

public slots:
    void createGeneralTab();
signals:
    void nameChanged(QString);
    //void otherNameChanged(QString);
    void magnitudeChanged(QString);
    void distanceChanged(QString);
    void BVindexChanged(QString);
    void angSizeChanged(QString);
    //void angSizeLabelChanged(QString);
    void illuminationChanged(QString);
    void typeInConstellationChanged(QString);
    void thumbnailChanged(QString);

private:
    //General
    QString m_name;
    //QString m_otherName;
    QString m_magnitude;
    QString m_distance;
    QString m_BVindex;
    QString m_angSize;
    //QString m_angSizeLabel;
    QString m_illumination;
    QString m_typeInConstellation;
    QString  m_thumbnail;
};

#endif
