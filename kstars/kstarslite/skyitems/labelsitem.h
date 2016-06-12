/** *************************************************************************
                          asteroidsitem.h  -  K Desktop Planetarium
                             -------------------
    begin                : 16/05/2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef LABELSITEM_H_
#define LABELSITEM_H_

#include "skylabeler.h"

#include <QSGOpacityNode>

typedef QSGOpacityNode LabelTypeNode;

class KSAsteroid;
class LineListIndex;
class LabelNode;
class GuideLabelNode;
class SkyObject;
class RootNode;

class LabelsItem : public QSGOpacityNode {

public:
    LabelsItem(RootNode *rootNode);

    enum label_t {
        STAR_LABEL,
        ASTEROID_LABEL,
        COMET_LABEL,
        PLANET_LABEL,
        JUPITER_MOON_LABEL,
        SATURN_MOON_LABEL,
        DEEP_SKY_LABEL,
        CONSTEL_NAME_LABEL,
        SATELLITE_LABEL,
        RUDE_LABEL, ///Rude labels block other labels FIXME: find a better solution
        NUM_LABEL_TYPES,
        HORIZON_LABEL,
        EQUATOR_LABEL,
        ECLIPTIC_LABEL
    };

    LabelNode *addLabel(SkyObject *skyObject, label_t type);
    LabelNode *addLabel(QString name, label_t type);

    GuideLabelNode *addGuideLabel(QString name, label_t type);
    void update();
    void updateChildLabels(label_t type);
    QSGOpacityNode *getLabelNode(label_t type) { return labelsLists.value(type); }

    void deleteLabels(label_t type);

    void hideLabels(label_t type);

    RootNode *rootNode() { return m_rootNode; }

private:
    QMap<label_t, LabelTypeNode *> labelsLists;
    RootNode *m_rootNode;
};
#endif

