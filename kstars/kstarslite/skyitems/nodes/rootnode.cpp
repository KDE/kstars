#include "rootnode.h"
#include "skymaplite.h"
#include "projections/projector.h"

#include <QSGTexture>
#include <QQuickWindow>

RootNode::RootNode()
    :m_polyNode(new QSGGeometryNode), m_skyMapLite(SkyMapLite::Instance())
{
    genCachedTextures();
    updateClipPoly();
    setIsRectangular(false);
}

void RootNode::genCachedTextures() {
    QVector<QVector<QPixmap*>> images = m_skyMapLite->getImageCache();

    QQuickWindow *win = m_skyMapLite->window();

    m_textureCache = QVector<QVector<QSGTexture*>> (images.length());

    for(int i = 0; i < m_textureCache.length(); ++i) {
        int length = images[i].length();
        m_textureCache[i] = QVector<QSGTexture *>(length);
        for(int c = 1; c < length; ++c) {
            m_textureCache[i][c] = win->createTextureFromImage(images[i][c]->toImage());
        }
    }
}

QSGTexture* RootNode::getCachedTexture(int size, char spType) {
    return m_textureCache[SkyMapLite::Instance()->harvardToIndex(spType)][size];
}

void RootNode::appendSkyNode(QSGNode * skyNode) {
    m_skyNodes.append(skyNode);
    appendChildNode(skyNode);
}

void RootNode::updateClipPoly() {
    QPolygonF newClip = m_skyMapLite->projector()->clipPoly();
    if(m_clipPoly != newClip) {
        m_clipPoly = newClip;
        QVector<QPointF> triangles;

        for(int i = 1; i < m_clipPoly.size() - 1; ++i) {
            triangles.append(m_clipPoly[0]);
            triangles.append(m_clipPoly[i]);
            triangles.append(m_clipPoly[i+1]);
        }

        const int size = triangles.size();
        if(!m_polyGeometry) {
            m_polyGeometry = new QSGGeometry (QSGGeometry::defaultAttributes_Point2D (),
                                              size);
            m_polyGeometry->setDrawingMode (GL_TRIANGLES);
            setGeometry(m_polyGeometry);
        } else {
            m_polyGeometry->allocate(size);
        }

        QSGGeometry::Point2D * vertex = m_polyGeometry->vertexDataAsPoint2D ();
        for (int idx = 0; idx < size; idx++) {
            vertex [idx].x = triangles[idx].x ();
            vertex [idx].y = triangles[idx].y ();
        }
    }
}
