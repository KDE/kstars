/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef PLANETNODE_H_
#define PLANETNODE_H_
#include "skynode.h"
#include "../skyopacitynode.h"

class QSGSimpleTextureNode;
class QImage;
class KSPlanetBase;
class RootNode;
class PointNode;
class LabelNode;
#include "../labelsitem.h"

/** @class PlanetNode
 *
 * A SkyNode derived class used as a container for holding two other nodes: PointNode
 * and QSGSimpleTextureNode(displays object as image) that are displayed depending according to the
 * conditions (zoom level, user options)
 *
 *@short A container for PointNode and QSGSimpleTextureNode used for displaying some solar system objects
 *@author Artem Fedoskin
 *@version 1.0
 */

class PlanetNode : public SkyNode
{
  public:
    /**
         * @brief Constructor
         * @param pb used in PlanetItem to update position of PlanetNode
         * @param parentNode used by PointNode to get textures from cache
         * @param labelType type of label. Pluto has different from planets label type
         */
    PlanetNode(KSPlanetBase *pb, RootNode *parentNode,
               LabelsItem::label_t labelType = LabelsItem::label_t::PLANET_LABEL);

    /**
         * @short updates the size of m_point
         * @param size new size of m_point
         */
    void setPointSize(float size);
    /**
         * @short updates the size of m_planetPic
         * @param size new size of m_planetPic
         */
    void setPlanetPicSize(float size);
    /**
         * @short hides m_planetPic and shows m_point
         */
    void showPoint();
    /**
         * @short hides m_point and shows m_planetPic
         */
    void showPlanetPic();
    /**
         * @short changePos changes the position m_point and m_planetPic
         * @param pos new position
         */
    virtual void changePos(QPointF pos) override;

    /**
         * @note similar to SolarSystemSingleComponent::draw()
         */
    virtual void update() override;
    virtual void hide() override;

  private:
    PointNode *m_point;

    // This opacity node is used to hide m_planetPic. m_point is subclass of QSGOpacityNode so it needs
    // no explicit opacity node here.
    QSGSimpleTextureNode *m_planetPic;
    SkyOpacityNode *m_planetOpacity;
    LabelNode *m_label;
};

#endif
