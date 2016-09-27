#include "syncedcatalogitem.h"
#include "syncedcatalogcomponent.h"

#include "skynodes/pointsourcenode.h"
#include "skynodes/deepskynode.h"
#include "skynodes/dsosymbolnode.h"

SyncedCatalogItem::SyncedCatalogItem(SyncedCatalogComponent *parent, RootNode *rootNode)
    :SkyItem(LabelsItem::label_t::NO_LABEL, rootNode), m_parent(parent), //It has NO_LABEL type because it handles two types of labels (CATALOG_STAR_LABEL and CATALOG_DSO_LABEL)
      stars(new QSGNode), dsoSymbols(new QSGNode), dsoNodes(new QSGNode)
{
    appendChildNode(stars);
    appendChildNode(dsoSymbols);
    appendChildNode(dsoNodes);
}

void SyncedCatalogItem::update() {
    if ( !m_parent->selected() ) {
        hide();
        return;
    }
    QColor catColor = m_parent->catColor();

    // If an object was added/deleted to catalog or we are initializing it recreate all nodes
    if(m_ObjectList != m_parent->objectList()) {
        //Update coordinates of newly added objects
        m_parent->update( 0 );

        QSGNode *n;
        //Clear all nodes
        while( (n = stars->firstChild()) ) { stars->removeChildNode(n); delete n; }
        while( (n = dsoSymbols->firstChild()) ) { dsoSymbols->removeChildNode(n); delete n; }
        while( (n = dsoNodes->firstChild()) ) { dsoNodes->removeChildNode(n); delete n; }

        m_ObjectList = m_parent->objectList();

        //Draw Custom Catalog objects
        // FIXME: Improve using HTM!
        foreach ( SkyObject *obj, m_ObjectList ) {
            if ( obj->type()==0 ) {
                StarObject *starObj = static_cast<StarObject*>(obj);
                PointSourceNode *point = new PointSourceNode(starObj, rootNode(), LabelsItem::label_t::CATALOG_STAR_LABEL, starObj->spchar(), starObj->mag());
                stars->appendChildNode(point);
            } else {
                DeepSkyObject *dsoObj = static_cast<DeepSkyObject*>(obj);
                DSOSymbolNode *dsoSymbol = new DSOSymbolNode(dsoObj, catColor);
                dsoSymbols->appendChildNode(dsoSymbol);

                DeepSkyNode *dsoNode = new DeepSkyNode(dsoObj, dsoSymbol, LabelsItem::label_t::CATALOG_DSO_LABEL);
                dsoNodes->appendChildNode(dsoNode);
            }
        }
    }

    // Check if the coordinates have been updated
    if( m_parent->getUpdateID() != KStarsData::Instance()->updateID() )
        m_parent->update( 0 );

    //Update stars
    QSGNode *n = stars->firstChild();
    while(n) {
        PointSourceNode *p = static_cast<PointSourceNode *>(n);
        p->update();
        n = n->nextSibling();
    }

    //Update DSOs
    n = dsoNodes->firstChild();
    while(n) {
        DeepSkyNode *p = static_cast<DeepSkyNode *>(n);
        p->update(true, false); //We don't draw labels for DSOs
        n = n->nextSibling();
    }
}
