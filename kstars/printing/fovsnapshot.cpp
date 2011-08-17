#include "fovsnapshot.h"

FovSnapshot::FovSnapshot(const QPixmap &pixmap, QString description, FOV *fov, const SkyPoint &center) :
        m_Pixmap(pixmap), m_Description(description), m_Fov(fov), m_CentralPoint(center)
{}

FovSnapshot::~FovSnapshot()
{}




