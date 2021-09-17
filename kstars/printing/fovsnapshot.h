/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef FOVSNAPSHOT_H
#define FOVSNAPSHOT_H

#include "fov.h"
#include "skypoint.h"
#include "QPixmap"
#include "QString"

class FOV;

/**
  * \class FovSnapshot
  * \brief Class that represents single field of view snapshot.
  * FovSnapshot class stores data of single FOV snapshot: image, description, FOV symbol at which it was
  * captured and its central point.
  * \author Rafał Kułaga
  */
class FovSnapshot
{
  public:
    /**
          * \brief Constructor.
          * \param pixmap Snapshot image.
          * \param description Snapshot description.
          * \param fov FOV symbol at which snapshot was captured.
          * \param center Central point of the snapshot.
          */
    FovSnapshot(const QPixmap &pixmap, const QString description, FOV *fov, const SkyPoint &center);

    /**
          * \brief Get snapshot image.
          * \return Image of the snapshot.
          */
    QPixmap getPixmap() { return m_Pixmap; }

    /**
          * \brief Get snapshot description.
          * \return Description of the snapshot.
          */
    QString getDescription() { return m_Description; }

    /**
          * \brief Get FOV symbol at which snapshot was captured.
          * \return FOV of the snapshot.
          */
    FOV *getFov() { return m_Fov; }

    /**
          * \brief Get central point of the snapshot.
          * \return Central point of the snapshot.
          */
    SkyPoint getCentralPoint() { return m_CentralPoint; }

    /**
          * \brief Set snapshot image.
          * \param pixmap Snapshot image.
          */
    void setPixmap(const QPixmap &pixmap) { m_Pixmap = pixmap; }

    /**
          * \brief Set snapshot description.
          * \param description Snapshot description.
          */
    void setDescription(const QString &description) { m_Description = description; }

    /**
          * \brief Set snapshot FOV symbol.
          * \param fov FOV symbol of the snapshot.
          */
    void setFov(FOV *fov) { m_Fov = fov; }

    /**
          * \brief Set central point of the snapshot.
          * \param point Central point of the snapshot.
          */
    void setCentralPoint(const SkyPoint &point) { m_CentralPoint = point; }

  private:
    QPixmap m_Pixmap;
    QString m_Description;
    FOV *m_Fov;
    SkyPoint m_CentralPoint;
};

#endif // FOVSNAPSHOT_H
