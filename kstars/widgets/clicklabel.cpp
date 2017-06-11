/***************************************************************************
                          clicklabel.cpp  -  description
                             -------------------
    begin                : Sat 03 Dec 2005
    copyright            : (C) 2005 by Jason Harris and Jasem Mutlaq
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "clicklabel.h"

ClickLabel::ClickLabel(QWidget *parent, const char *name) : QLabel(parent)
{
    setObjectName(name);
}
