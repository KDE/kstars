#include <QSGTexture>
#include <QQuickWindow>

#include "linesrootnode.h"
#include "skymaplite.h"
#include "projections/projector.h"
#include "../trixelnode.h"
#include <QSGOpacityNode>

LinesRootNode::LinesRootNode()
{

}

void LinesRootNode::addLinesComponent(LineListIndex * linesComp, QString color, int width, Qt::PenStyle style) {
    QSGOpacityNode *node = new QSGOpacityNode;
    appendChildNode(node);

    m_lineIndexes.insert(node, linesComp);
    m_colors.append(color);
    m_widths.append(width);
    m_styles.append(style);


    LineListHash *trixels = linesComp->lineIndex();

    QHash< Trixel, LineListList *>::const_iterator i = trixels->begin();
    while( i != trixels->end()) {

        TrixelNode *trixel = new TrixelNode(i.key(), i.value());
        node->appendChildNode(trixel);
        //QString color = //m_colors[m_colors.size()-1];
        //int width = m_widths[m_widths.size()-1]; // TODO do something with this
        trixel->setStyle(color, width);
        ++i;
    }
}

void LinesRootNode::update() {
    QMap< QSGOpacityNode *, LineListIndex *>::const_iterator i = m_lineIndexes.begin();
    while( i != m_lineIndexes.end()) {
        QVector<Trixel> visTrixels;
        SkyMesh * mesh = SkyMesh::Instance();
        SkyMapLite *map = SkyMapLite::Instance();
        double radius = map->projector()->fov();
        if ( radius > 180.0 )
            radius = 180.0;
        if(mesh) {
            //mesh->aperture(map->focus(), radius);
        }

        MeshIterator region (mesh,DRAW_BUF);
        while ( region.hasNext() ) {
            visTrixels.append(region.next());
        }

        qDebug() << visTrixels.size();

        QSGOpacityNode * node = i.key();
        if(i.value()->selected()) {
            node->setOpacity(1);
            for(int c = 0; c < node->childCount(); ++c) {
                TrixelNode * trixel = static_cast<TrixelNode *>(node->childAtIndex(c));
                //if(visTrixels.contains(c)) {
                    trixel->update();
               /* } else {
                    trixel->hide();
                }*/
            }
        } else {
            node->setOpacity(0);
        }
        node->markDirty(QSGNode::DirtyOpacity);
        ++i;
    }
}
