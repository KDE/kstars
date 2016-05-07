#include "planetitemnode.h"
#include "skymaplite.h"

#include <QSGTexture>
#include <QQuickWindow>

PlanetItemNode::PlanetItemNode() {
    genCachedTextures();
}

void PlanetItemNode::genCachedTextures() {
    SkyMapLite* skyMap = SkyMapLite::Instance();
    QVector<QVector<QPixmap*>> images = skyMap->getImageCache();

    QQuickWindow *win = skyMap->window();

    textureCache = QVector<QVector<QSGTexture*>> (images.length());

    for(int i = 0; i < textureCache.length(); ++i) {
        int length = images[i].length();
        textureCache[i] = QVector<QSGTexture *>(length);
        for(int c = 1; c < length; ++c) {
            textureCache[i][c] = win->createTextureFromImage(images[i][c]->toImage());
        }
    }
}

QSGTexture* PlanetItemNode::getCachedTexture(int size, char spType) {
    return textureCache[SkyMapLite::Instance()->harvardToIndex(spType)][size];
}
