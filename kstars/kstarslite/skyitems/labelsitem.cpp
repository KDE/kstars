/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Options.h"
#include <QSGNode>
#include "labelsitem.h"
#include "skylabeler.h"
#include "skynodes/labelnode.h"
#include "skynodes/guidelabelnode.h"

#include "cometsitem.h"
#include "staritem.h"
#include "rootnode.h"
#include "skymesh.h"
#include "skynodes/trixelnode.h"

LabelsItem::LabelsItem() : m_rootNode(0)
{
    LabelTypeNode *stars = new LabelTypeNode;
    appendChildNode(stars);

    //Add trixels to hold star labels
    int trixelNum = SkyMesh::Instance()->size();

    for (int i = 0; i < trixelNum; ++i)
    {
        TrixelNode *trixel = new TrixelNode(i);
        stars->appendChildNode(trixel);
    }

    m_labelsLists.insert(label_t::STAR_LABEL, stars);

    LabelTypeNode *catalog_stars = new LabelTypeNode;
    appendChildNode(catalog_stars);
    m_labelsLists.insert(label_t::CATALOG_STAR_LABEL, catalog_stars);

    LabelTypeNode *asteroids = new LabelTypeNode;
    appendChildNode(asteroids);
    m_labelsLists.insert(label_t::ASTEROID_LABEL, asteroids);

    LabelTypeNode *comets = new LabelTypeNode;
    appendChildNode(comets);
    m_labelsLists.insert(label_t::COMET_LABEL, comets);

    LabelTypeNode *planets = new LabelTypeNode;
    appendChildNode(planets);
    m_labelsLists.insert(label_t::PLANET_LABEL, planets);

    LabelTypeNode *jupiter_moons = new LabelTypeNode;
    appendChildNode(jupiter_moons);
    m_labelsLists.insert(label_t::JUPITER_MOON_LABEL, jupiter_moons);

    LabelTypeNode *saturn_moons = new LabelTypeNode;
    appendChildNode(saturn_moons);
    m_labelsLists.insert(label_t::SATURN_MOON_LABEL, saturn_moons);

    LabelTypeNode *deep_sky = new LabelTypeNode;
    appendChildNode(deep_sky);
    m_labelsLists.insert(label_t::DEEP_SKY_LABEL, deep_sky);

    LabelTypeNode *dso_messier = new LabelTypeNode;
    deep_sky->appendChildNode(dso_messier);
    m_labelsLists.insert(label_t::DSO_MESSIER_LABEL, dso_messier);

    LabelTypeNode *dso_other = new LabelTypeNode;
    deep_sky->appendChildNode(dso_other);
    m_labelsLists.insert(label_t::DSO_OTHER_LABEL, dso_other);

    LabelTypeNode *catalog_dso = new LabelTypeNode;
    appendChildNode(catalog_dso);
    m_labelsLists.insert(label_t::CATALOG_DSO_LABEL, catalog_dso);

    LabelTypeNode *constellation = new LabelTypeNode;
    appendChildNode(constellation);
    m_labelsLists.insert(label_t::CONSTEL_NAME_LABEL, constellation);

    LabelTypeNode *satellite = new LabelTypeNode;
    appendChildNode(satellite);
    m_labelsLists.insert(label_t::SATELLITE_LABEL, satellite);

    LabelTypeNode *rude = new LabelTypeNode;
    appendChildNode(rude);
    m_labelsLists.insert(label_t::RUDE_LABEL, rude);

    LabelTypeNode *num_label = new LabelTypeNode;
    appendChildNode(num_label);
    m_labelsLists.insert(label_t::NUM_LABEL_TYPES, num_label);

    LabelTypeNode *horizon_label = new LabelTypeNode;
    appendChildNode(horizon_label);
    m_labelsLists.insert(label_t::HORIZON_LABEL, horizon_label);

    LabelTypeNode *equator = new LabelTypeNode;
    appendChildNode(equator);
    m_labelsLists.insert(label_t::EQUATOR_LABEL, equator);

    LabelTypeNode *ecliptic = new LabelTypeNode;
    appendChildNode(ecliptic);
    m_labelsLists.insert(label_t::ECLIPTIC_LABEL, ecliptic);

    LabelTypeNode *telescopeSymbol = new LabelTypeNode;
    appendChildNode(telescopeSymbol);
    m_labelsLists.insert(label_t::TELESCOPE_SYMBOL, telescopeSymbol);

    skyLabeler = SkyLabeler::Instance();
    skyLabeler->reset();
}

LabelNode *LabelsItem::addLabel(SkyObject *skyObject, label_t labelType)
{
    LabelNode *label = new LabelNode(skyObject, labelType);
    m_labelsLists.value(labelType)->appendChildNode(label);
    return label;
}

LabelNode *LabelsItem::addLabel(SkyObject *skyObject, label_t labelType, Trixel trixel)
{
    Q_ASSERT(labelType == STAR_LABEL || labelType == DSO_MESSIER_LABEL || labelType == DSO_NGC_LABEL ||
             labelType == DSO_IC_LABEL || labelType == DSO_OTHER_LABEL);
    TrixelNode *triNode = static_cast<TrixelNode *>(m_labelsLists.value(labelType)->firstChild());
    LabelNode *label    = 0;

    while (triNode != 0)
    {
        if (triNode->trixelID() == trixel)
        {
            label = new LabelNode(skyObject, labelType);
            triNode->appendChildNode(label);
            break;
        }
        triNode = static_cast<TrixelNode *>(triNode->nextSibling());
    }
    return label;
}

LabelNode *LabelsItem::addLabel(QString name, label_t labelType)
{
    LabelNode *label = new LabelNode(name, labelType);
    m_labelsLists.value(labelType)->appendChildNode(label);
    return label;
}

GuideLabelNode *LabelsItem::addGuideLabel(QString name, label_t labelType)
{
    GuideLabelNode *label = new GuideLabelNode(name, labelType);
    m_labelsLists.value(labelType)->appendChildNode(label);
    return label;
}

TrixelNode *LabelsItem::addTrixel(label_t labelType, Trixel trixel)
{
    TrixelNode *triNode = new TrixelNode(trixel);
    getLabelNode(labelType)->appendChildNode(triNode);
    return triNode;
}

void LabelsItem::update()
{
    SkyLabeler *skyLabeler = SkyLabeler::Instance();
    skyLabeler->reset();

    updateChildLabels(label_t::TELESCOPE_SYMBOL);

    updateChildLabels(label_t::HORIZON_LABEL);

    updateChildLabels(label_t::EQUATOR_LABEL);
    updateChildLabels(label_t::ECLIPTIC_LABEL);

    updateChildLabels(label_t::PLANET_LABEL);

    updateChildLabels(label_t::JUPITER_MOON_LABEL);

    updateChildLabels(label_t::SATURN_MOON_LABEL);
    updateChildLabels(label_t::ASTEROID_LABEL);

    updateChildLabels(label_t::COMET_LABEL);

    updateChildLabels(label_t::CONSTEL_NAME_LABEL);

    updateChildLabels(label_t::DSO_MESSIER_LABEL);
    updateChildLabels(label_t::DSO_OTHER_LABEL);
    updateChildLabels(label_t::CATALOG_DSO_LABEL);

    updateChildLabels(label_t::STAR_LABEL);
    updateChildLabels(label_t::CATALOG_STAR_LABEL);
}

void LabelsItem::hideLabels(label_t labelType)
{
    if (labelType != NO_LABEL)
        m_labelsLists[labelType]->hide();
}

void LabelsItem::showLabels(label_t labelType)
{
    if (labelType != NO_LABEL)
        m_labelsLists[labelType]->show();
}

void LabelsItem::setRootNode(RootNode *rootNode)
{
    //Remove from previous parent if had any
    if (m_rootNode && parent() == m_rootNode)
        m_rootNode->removeChildNode(this);

    //Append to new parent if haven't already
    m_rootNode = rootNode;
    if (parent() != m_rootNode)
        m_rootNode->appendChildNode(this);
}

void LabelsItem::deleteLabels(label_t labelType)
{
    if (labelType == STAR_LABEL)
    {
        LabelTypeNode *node = m_labelsLists[labelType];
        QSGNode *trixel     = node->firstChild();
        while (trixel != 0)
        {
            while (QSGNode *label = trixel->firstChild())
            {
                trixel->removeChildNode(label);
                delete label;
            }
            trixel = trixel->nextSibling();
        }
    }
    else if (labelType != NO_LABEL)
    {
        LabelTypeNode *node = m_labelsLists[labelType];
        while (QSGNode *n = node->firstChild())
        {
            node->removeChildNode(n);
            delete n;
        }
    }
}

void LabelsItem::deleteLabel(LabelNode *label)
{
    if (label)
    {
        label_t type        = label->labelType();
        LabelTypeNode *node = m_labelsLists[type];

        if (type == STAR_LABEL || type == DSO_IC_LABEL || type == DSO_NGC_LABEL || type == DSO_MESSIER_LABEL ||
            type == DSO_OTHER_LABEL)
        {
            QSGNode *trixel = node->firstChild();
            bool found      = false;

            while (trixel != 0 && !found)
            {
                QSGNode *l = trixel->firstChild();
                while (l != 0)
                {
                    if (l == label)
                    {
                        trixel->removeChildNode(label);
                        delete label;

                        found = true;
                        break;
                    }
                    l = l->nextSibling();
                }
                trixel = trixel->nextSibling();
            }
        }
        else
        {
            QSGNode *n = node->firstChild();
            while (n != 0)
            {
                if (n == label)
                {
                    node->removeChildNode(label);
                    delete label;
                    break;
                }
                n = n->nextSibling();
            }
        }
    }
}

void LabelsItem::updateChildLabels(label_t labelType)
{
    LabelTypeNode *node = m_labelsLists[labelType];
    if (node->visible())
    {
        QSGNode *n = node->firstChild();
        while (n != 0)
        {
            if (labelType == STAR_LABEL || labelType == DSO_OTHER_LABEL)
            {
                TrixelNode *trixel = static_cast<TrixelNode *>(n);
                if (trixel->visible())
                {
                    QSGNode *l = trixel->firstChild();
                    while (l != 0)
                    {
                        LabelNode *label = static_cast<LabelNode *>(l);
                        l                = l->nextSibling();

                        if (skyLabeler->markText(label->labelPos, label->name()))
                        {
                            label->update();
                        }
                        else
                        {
                            label->hide();
                        }
                    }
                }
            }
            else
            {
                LabelNode *label = static_cast<LabelNode *>(n);

                if (label->visible())
                {
                    if (label->zoomFont())
                        skyLabeler->resetFont();
                    if (skyLabeler->markText(label->labelPos, label->name()))
                    {
                        label->update();
                    }
                    else
                    {
                        label->hide();
                    }
                    skyLabeler->useStdFont();
                }
            }
            n = n->nextSibling();
        }
    }
}
