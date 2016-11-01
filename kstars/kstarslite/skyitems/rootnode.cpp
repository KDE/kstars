#include <QSGTexture>
#include <QQuickWindow>

#include "rootnode.h"
#include "skymaplite.h"
#include "projections/projector.h"
#include "skymapcomposite.h"
#include "solarsystemcomposite.h"
#include "Options.h"

#include "constellationboundarylines.h"
#include "constellationlines.h"
#include "starcomponent.h"

#include "horizontalcoordinategrid.h"
#include "equatorialcoordinategrid.h"

#include "ecliptic.h"
#include "equator.h"

//SkyItems
#include "kstarslite/skyitems/staritem.h"
#include "kstarslite/skyitems/deepskyitem.h"
#include "kstarslite/skyitems/planetsitem.h"
#include "kstarslite/skyitems/asteroidsitem.h"
#include "kstarslite/skyitems/cometsitem.h"
#include "kstarslite/skyitems/horizonitem.h"
#include "kstarslite/skyitems/lines/linesitem.h"
#include "kstarslite/skyitems/labelsitem.h"
#include "kstarslite/skyitems/constellationnamesitem.h"
#include "kstarslite/skyitems/constellationartitem.h"
#include "kstarslite/skyitems/satellitesitem.h"
#include "kstarslite/skyitems/supernovaeitem.h"
#include "kstarslite/skyitems/syncedcatalogitem.h"

//Lines
#include "kstarslite/skyitems/lines/equatoritem.h"
#include "kstarslite/skyitems/lines/eclipticitem.h"
#include "kstarslite/skyitems/lines/milkywayitem.h"

//Symbols
#include "kstarslite/skyitems/telescopesymbolsitem.h"

#include "kstarslite/skyitems/fovitem.h"

#include <QSGFlatColorMaterial>

RootNode::RootNode()
    :m_skyMapLite(SkyMapLite::Instance()), m_clipGeometry(0)
{
    SkyMapLite::setRootNode(this);
    genCachedTextures();

    m_skyComposite = KStarsData::Instance()->skyComposite();

    // LabelsItem needs to be created first so that other items could insert their labels in labelsList
    m_labelsItem = new LabelsItem();

    m_MWItem = new MilkyWayItem(m_skyComposite->milkyWay(), this);

    m_linesItem = new LinesItem(this);

    m_linesItem->addLinesComponent( m_skyComposite->equatorialCoordGrid(), "EquatorialGridColor", 1, Qt::DotLine );
    m_linesItem->addLinesComponent( m_skyComposite->horizontalCoordGrid(), "HorizontalGridColor", 2, Qt::DotLine );

    if ( m_skyComposite->currentCulture() == "Western" )
    {
        m_linesItem->addLinesComponent( m_skyComposite->constellationBoundary(), "CBoundColor", 1, Qt::SolidLine );
    }

    m_artItem = new ConstellationArtItem(m_skyComposite->constellationArt(), this);

    m_linesItem->addLinesComponent( m_skyComposite->constellationLines(), "CLineColor", 1, Qt::SolidLine );

    m_equator = new EquatorItem(m_skyComposite->equator(),this);
    m_ecliptic = new EclipticItem(m_skyComposite->ecliptic(),this);

    m_dsoItem = new DeepSkyItem( m_skyComposite->deepSkyComponent(), this );
    m_starItem = new StarItem(StarComponent::Instance(), this);

    m_solarSystem = m_skyComposite->solarSystemComposite();

    m_planetsItem = new PlanetsItem(m_solarSystem->planets(), m_solarSystem->planetMoonsComponent(), this);
    m_asteroidsItem = new AsteroidsItem(m_solarSystem->asteroids(), this);
    m_cometsItem = new CometsItem(m_solarSystem->comets(), this);

    m_constelNamesItem = new ConstellationNamesItem(m_skyComposite->constellationNamesComponent(), this);

    m_satItem = new SatellitesItem( m_skyComposite->satellites(), this);
    m_snovaItem = new SupernovaeItem(m_skyComposite->supernovaeComponent(), this);

    m_horizonItem = new HorizonItem(m_skyComposite->horizon(), this);

    m_FOVItem = new FOVItem(this);

    m_internedResolvedCatalogItem = new SyncedCatalogItem(m_skyComposite->internetResolvedComponent(), this);
    m_manualAdditionsCatalogItem = new SyncedCatalogItem(m_skyComposite->manualAdditionsComponent(), this);

    m_telescopeSymbols = new TelescopeSymbolsItem(this);

    setIsRectangular(false);
    updateClipPoly();

    m_labelsItem->setRootNode(this);
}

void RootNode::testLeakAdd() {
    /*    m_linesItem = new LinesItem(this);

    m_linesItem->addLinesComponent( m_skyComposite->equatorialCoordGrid(), "EquatorialGridColor", 1, Qt::DotLine );
    m_linesItem->addLinesComponent( m_skyComposite->horizontalCoordGrid(), "HorizontalGridColor", 2, Qt::DotLine );

    m_linesItem->addLinesComponent( m_skyComposite->constellationBoundary(), "CBoundColor", 1, Qt::SolidLine );
    m_linesItem->addLinesComponent( m_skyComposite->constellationLines(), "CLineColor", 1, Qt::SolidLine );

    m_starItem = new StarItem(m_skyComposite->starComponent(), this);

    m_solarSystem = m_skyComposite->solarSystemComposite();

    m_equator = new EquatorItem(m_skyComposite->equator(),this);
    m_ecliptic = new EclipticItem(m_skyComposite->ecliptic(),this);

    m_planetsItem = new PlanetsItem(m_solarSystem->planets(), m_solarSystem->planetMoonsComponent(), this);
    m_asteroidsItem = new AsteroidsItem(m_solarSystem->asteroids(), this);
    m_cometsItem = new CometsItem(m_solarSystem->comets(), this);

    m_constelNamesItem = new ConstellationNamesItem(m_skyComposite->constellationNamesComponent(), this);
    m_horizonItem = new HorizonItem(m_skyComposite->horizon(), this);*/
}

void RootNode::testLeakDelete() {
    /*removeChildNode(m_linesItem);
    delete m_linesItem;

    removeChildNode(m_starItem);
    delete m_starItem;

    removeChildNode(m_equator);
    delete m_equator;
    removeChildNode(m_ecliptic);
    delete m_ecliptic;

    removeChildNode(m_planetsItem);
    delete m_planetsItem;
    removeChildNode(m_asteroidsItem);
    delete m_asteroidsItem;
    removeChildNode(m_cometsItem);
    delete m_cometsItem;

    removeChildNode(m_constelNamesItem);
    delete m_constelNamesItem;

    removeChildNode(m_horizonItem);
    delete m_horizonItem;*/
}

RootNode::~RootNode() {
    for(int i = 0; i < m_textureCache.length(); ++i) {
        for(int c = 0; c < m_textureCache[i].size(); ++c) {
            delete m_textureCache[i][c];
        }
    }
}

void RootNode::genCachedTextures() {
    QVector<QVector<QPixmap*>> images = m_skyMapLite->getImageCache();

    QQuickWindow *win = m_skyMapLite->window();

    m_textureCache = QVector<QVector<QSGTexture*>> (images.length());

    for(int i = 0; i < m_textureCache.length(); ++i) {
        int length = images[i].length();
        m_textureCache[i] = QVector<QSGTexture *>(length);
        for(int c = 1; c < length; ++c) {
            m_textureCache[i][c] = win->createTextureFromImage(images[i][c]->toImage(), QQuickWindow::TextureCanUseAtlas);
        }
    }
}

QSGTexture* RootNode::getCachedTexture(int size, char spType) {
    return m_textureCache[SkyMapLite::Instance()->harvardToIndex(spType)][size];
}

void RootNode::updateClipPoly() {
    QPolygonF newClip = m_skyMapLite->projector()->clipPoly();
    m_clipPoly = newClip;

    const int size = m_clipPoly.size();
    if(!m_clipGeometry) {
        m_clipGeometry = new QSGGeometry (QSGGeometry::defaultAttributes_Point2D (),
                                          size);
        m_clipGeometry->setDrawingMode(GL_TRIANGLE_FAN);
        setGeometry(m_clipGeometry);
        setFlag(QSGNode::OwnsGeometry);

    }
    m_clipGeometry->allocate(size);

    QSGGeometry::Point2D * vertex = m_clipGeometry->vertexDataAsPoint2D ();
    for (int i = 0; i < size; i++) {
        vertex[i].x = m_clipPoly[i].x();
        vertex[i].y = m_clipPoly[i].y();
    }
    m_clipGeometry->markIndexDataDirty();
    m_clipGeometry->markVertexDataDirty();
    markDirty(QSGNode::DirtyGeometry);
}

void RootNode::update(bool clearTextures) {
    updateClipPoly();
    if(clearTextures) {
        //First we need to create new textures and only after all PointNodes changed their textures we
        //can delete old textures
        genCachedTextures();
    }

    m_MWItem->update();

    m_artItem->update();

    m_linesItem->update();

    m_equator->update();
    m_ecliptic->update();

    m_dsoItem->update();
    m_starItem->update();

    //TODO: Move this check somewhere else (create a separate function)
    if(Options::showSolarSystem()) {
        m_planetsItem->update();
        if (!Options::showAsteroids() ) {
            if (m_asteroidsItem) delete m_asteroidsItem;
        } else {
            if(!m_asteroidsItem) m_asteroidsItem = new AsteroidsItem(m_solarSystem->asteroids(), this);
            m_asteroidsItem->update();
        }

        if (!Options::showComets() ) {
            if (m_cometsItem) delete m_cometsItem;
        } else {
            if(!m_cometsItem) m_cometsItem = new CometsItem(m_solarSystem->comets(), this);
            m_cometsItem->update();
        }
    } else {
        m_planetsItem->hide();
        if(m_asteroidsItem) {
            delete m_asteroidsItem;
            m_asteroidsItem = 0;
        }
        if(m_cometsItem) {
            delete m_cometsItem;
            m_cometsItem = 0;
        }
    }

    m_constelNamesItem->update();

    m_satItem->update();
    m_snovaItem->update();
    m_horizonItem->update();

    m_internedResolvedCatalogItem->update();
    m_manualAdditionsCatalogItem->update();

    m_telescopeSymbols->update();
    m_labelsItem->update();

    m_FOVItem->update();

    if(clearTextures) {
        //Delete old textures
        if(m_oldTextureCache.length()) {
            foreach(QVector<QSGTexture *> textures, m_oldTextureCache) {
                qDeleteAll(textures.begin(), textures.end());
            }
        }
    }
}

