/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skyitem.h"

#include <QSGOpacityNode>

class ConstellationNamesComponent;

/**
 * @short The ConstellationName struct. It holds SkyObject (with coordinates of label) and 2 labels
 * (latin and secondary names)
 */
struct ConstellationName
{
    explicit ConstellationName(SkyObject *skyObj);

    SkyObject *obj { nullptr };
    LabelNode *latin { nullptr };
    LabelNode *secondary { nullptr };

    /** @short hides both latin and secondary labels */
    void hide();
};

/**
 * @class ConstellationNamesItem
 *
 * This class represents ConstellationNamesComponent in SkyMapLite.
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class ConstellationNamesItem : public SkyItem
{
  public:
    /**
     * Constructor.
     * @param constComp a pointer to ConstellationNamesComponent that this object has to represent in
     * SkyMapLite
     * @param rootNode a pointer to SkyItem's parent node
     */
    explicit ConstellationNamesItem(ConstellationNamesComponent *constComp, RootNode *rootNode = nullptr);

    /**
     * Update positions ConstellationName labels based on user settings and labels visibility.
     * If the user chose latin names then label with secondary name won't be created until
     * settings are not changed.
     *
     * @short updates positions of labels based on their visibility and user settings.
     */
    virtual void update();

    /** @short Delete all labels and recreate m_names according to object list of m_constelNamesComp */
    void recreateList();

  private:
    ConstellationNamesComponent *m_constelNamesComp { nullptr };
    QVector<ConstellationName *> m_names;
};
