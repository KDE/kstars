#include <QSGSimpleTextureNode>

#include "planetnode.h"
#include "pointsourcenode.h"
#include "planetmoonsnode.h"
#include "planetmoons.h"
#include "ksplanetbase.h"
#include "skylabeler.h"

#include "Options.h"

PlanetMoonsNode::PlanetMoonsNode(KSPlanetBase* planet, RootNode* parentNode)
    :SkyNode(planet), m_rootNode(parentNode), pmoons(0),
      m_planetNode(new PlanetNode(planet, parentNode))
{
    appendChildNode(m_planetNode);
}

void PlanetMoonsNode::update() {
    if(pmoons) updateMoons();
    m_planetNode->update();
}

void PlanetMoonsNode::hide() {
    m_planetNode->hide();

    foreach(SkyNode *moon, m_moonNodes) {
        moon->hide();
    }
}

//TODO updateMoons and destructor
void PlanetMoonsNode::updateMoons() {
    //In order to get the z-order right for the moons and the planet,
    //we need to first append the planet (both m_point and m_planetPic) then append all nodes for moons
    //that are nearer than the planet and then prepend nodes for moons that are further than the planet
    int nmoons = pmoons->nMoons();

    if(!m_moonNodes.length()) { //Initialize PointSourceNodes used for drawing moons
        for ( int i=0; i<nmoons; ++i ) {
            m_moonNodes.append(new PointSourceNode(pmoons->moon(i), m_rootNode,LabelsItem::label_t::JUPITER_MOON_LABEL));
        }
    }

    removeAllChildNodes(); // Clear all child nodes so that we can render moons according to z-order

    // We need to reappend node that draws the planet
    appendChildNode(m_planetNode);

    bool drawLabel = true;

    if ( ! (Options::showPlanetNames() && Options::zoomFactor() > 50.*MINZOOM) ) {
        drawLabel = false;
    }

    for ( int i=0; i<nmoons; ++i ) {
        if ( pmoons->z(i) < 0.0 ) { //Moon is nearer than the planet
            appendChildNode(m_moonNodes[i]);
            m_moonNodes[i]->update(drawLabel);
        } else {
            //Draw Moons that are further than the planet
            //skyp->drawPointSource( pmoons->moon(i), pmoons->moon(i)->mag() );
            prependChildNode(m_moonNodes[i]);
            m_moonNodes[i]->update(drawLabel);
        }
    }

/*  //Draw Moon name labels if at high zoom
     return;
    for ( int i=0; i<nmoons; ++i ) {
        /*
        if (planet ==KSPlanetBase::SATURN)
            SkyLabeler::AddLabel( pmoons->moon(i), SkyLabeler::SATURN_MOON_LABEL );
        else

        SkyLabeler::AddLabel( pmoons->moon(i), SkyLabeler::JUPITER_MOON_LABEL );
    }*/
}
