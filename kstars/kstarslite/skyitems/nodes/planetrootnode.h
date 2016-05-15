/** *************************************************************************
                          planetrootnode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 05/05/2016
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
#ifndef PLANETROOTNODE_H_
#define PLANETROOTNODE_H_
#include "rootnode.h"
class PlanetNode;

/** @class PlanetRootNode
 *@short A RootNode derived class used as a container for holding PlanetNodes.
 *@author Artem Fedoskin
 *@version 1.0
 */

class PlanetRootNode : public RootNode {
public:
    PlanetRootNode();
private:
};
#endif
