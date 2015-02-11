/*  INDI Group
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
    JM Changelog
    2004-16-1:	Start
   
 */

#ifndef INDIGROUP_H_
#define INDIGROUP_H_

#include <indiproperty.h>

#include <QString>
#include <QList>


class INDI_P;
class INDI_D;

class QFrame;
class QVBoxLayout;
class QSpacerItem;
class QScrollArea;

/**
 * @class INDI_G represents a collection of INDI properties that share a common group. The group is usually represented in the GUI as a separate tab with the device tab.
 *
 * @author Jasem Mutlaq
 */
class INDI_G
{
public:
    INDI_G(INDI_D *idv, const QString &inName);
    ~INDI_G();


    bool addProperty(INDI::Property *prob);

    bool removeProperty(const QString &propName);

    INDI_P * getProperty(const QString & propName);
    QFrame *getContainer() { return propertyContainer;}
    QScrollArea *getScrollArea() { return scrollArea; }
    const QString & getName() { return name; }

    INDI_D *getDevice() { return dp;}

    QList<INDI_P*> getProperties() { return propList; }

    int size() { return propList.count(); }

private:
    QString       name;			/* Group name */
    INDI_D 	  *dp;			/* Parent device */
    QFrame        *propertyContainer;	/* Properties container */
    QVBoxLayout   *propertyLayout;        /* Properties layout */
    QSpacerItem   *VerticalSpacer;	/* Vertical spacer */
    QScrollArea   *scrollArea;

    QList<INDI_P*> propList;			/* malloced list of pointers to properties */

};

#endif
