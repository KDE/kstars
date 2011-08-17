#ifndef FOVSNAPSHOT_H
#define FOVSNAPSHOT_H

#include "fov.h"
#include "skypoint.h"
#include "QPixmap"
#include "QString"

class FOV;

class FovSnapshot
{
public:
    FovSnapshot(const QPixmap &pixmap, const QString description, FOV *fov, const SkyPoint &center);
    ~FovSnapshot();

    QPixmap getPixmap() { return m_Pixmap; }
    QString getDescription() { return m_Description; }
    FOV* getFov() { return m_Fov; }
    SkyPoint getCentralPoint() { return m_CentralPoint; }

    void setPixmap(const QPixmap &pixmap) { m_Pixmap = pixmap; }
    void setDescription(const QString &description) { m_Description = description; }
    void setFov(FOV *fov) { m_Fov = fov; }
    void setCentralPoint(const SkyPoint &point) { m_CentralPoint = point; }

private:
    QPixmap m_Pixmap;
    QString m_Description;
    FOV *m_Fov;
    SkyPoint m_CentralPoint;
};

#endif // FOVSNAPSHOT_H
