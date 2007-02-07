#if 0
    INDI
    Copyright (C) 2006 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

   Changelog:

   2006-09-23: Start (JM)

#endif

#ifndef OBSERVER_H
#define OBSERVER_H

#include "lilxml.h"

typedef enum {
   IDS_DEFINED, IDS_UPDATED, IDS_DELETED, IDS_DIED
} IDState;				/* Property state in observer pattern */

typedef enum {
   IDT_VALUE, IDT_STATE, IDT_ALL
} IDType;				/* Notification type */

typedef enum {
 IPT_SWITCH, IPT_TEXT, IPT_NUMBER, IPT_LIGHT, IPT_BLOB
} IPType;				/* Property type */

/* Generic function pointer */
typedef void (*fpt)();
/* Switch Property Callback */
typedef void (CBSP) (const char *dev, const char *name, IDState driver_state, ISState *states, char *names[], int n);
/* Text Property Callback */
typedef void (CBTP) (const char *dev, const char *name, IDState driver_state, char *texts[], char *names[], int n);
/* Number Property Callback */
typedef void (CBNP) (const char *dev, const char *name, IDState driver_state, double values[], char *names[], int n);
/* Light Property Callback */
typedef void (CBLP) (const char *dev, const char *name, IDState driver_state, ISState *states, char *names[], int n);
/* Blob Property Callback */
typedef void (CBBP) (const char *dev, const char *name, IDState driver_state, int sizes[], char *blobs[], char *formats[], char *names[], int n);

#ifdef __cplusplus
extern "C" {
#endif

void IOSubscribeProperty(const char *dev, const char *name, IPType property_type, IDType notification_type, fpt fp);
void IOUnsubscribeProperty(const char *dev, const char *name);

int processObservers(XMLEle *root);
const char * idtypeStr(IDType type);
int crackObserverState(char *stateStr);
int crackPropertyState(char *pstateStr);

#ifdef __cplusplus
}
#endif

#endif
