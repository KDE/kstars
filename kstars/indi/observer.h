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

/**
 * \defgroup observerFunctions Observer Functions: Inter-driver communication functions.
 
    <p>INDI Library employes the <a href="http://en.wikipedia.org/wiki/Observer_pattern">observer pattern</a> to enable communication among devices using a publisher/subscriber mechanism. All drivers are by default publishers. The observer pattern permits a driver to monitor a property in another driver. The driver that monitors the value and status of a property in a different driver is called the \e subscriber driver, that is, it subscribes to the property of the other driver (the \e publisher). When the subscriber driver chooses to minotor a property, it can choose how it gets notified when the monitored property is updated. The \e subscriber can opt to receives notifications only when the monitored property \e status changes, or only when its \e value changes, or both. Furthermore, the \e subscriber provides its own callback function to process any updates from the monitored property. The callback function prototype <b>must</b> match the observed property type. That is, if the subscriber is watching a switch property, its callback function prototype must be that of a switch property.</p>

    <p>In addition to monitoring changes in status or value, the subscriber gets updated when the monitored property is defined, deleted, or dead. A property is dead when the \e publisher  driver crashes or severs communication with INDI server.</p>

    <p>Refer to the inter-driver communication tutorial under the <code>examples</code> directory of INDI for a typical implementation involving a dome driver that monitors the status of a rain collector. The dome driver subscribes to a light property called rain collector in the rain driver. When the property changes its status to alert, the dome driver promptly closes down the dome.</p>

 */
/*@{*/

/** \typedef IDState
    \brief Property state in observer pattern.*/
typedef enum {
   IDS_DEFINED /** Property is defined */,
   IDS_UPDATED /** Property is updated */,
   IDS_DELETED /** Property is deleted */,
   IDS_DIED /** Property is dead */
} IDState;				/* Property state in observer pattern.*/

/** \typedef IDType
    \brief Subscription notification type.*/
typedef enum {
   IDT_VALUE /** Only report changes in property value */,
   IDT_STATE /** Only report changes in property state */,
   IDT_ALL /** Report changes when property value OR state changes */
} IDType;				/* brief Subscription notification type.*/	

/** \typedef IPType
    \brief Monitored property type.*/
typedef enum {
 IPT_SWITCH /** Swtich Property */,
 IPT_TEXT /** Text Property */,
 IPT_NUMBER /** Number Property */,
 IPT_LIGHT /** Light Property */,
 IPT_BLOB /** Blob Property */ 
} IPType;				/* Monitored property type.*/

typedef void (*fpt)();

/** \typedef CBSP
    \brief Switch Property Callback. */
typedef void (CBSP) (const char *dev, const char *name, IDState driver_state, ISState *states, char *names[], int n);

/** \typedef CBTP
    \brief Text Property Callback. */
typedef void (CBTP) (const char *dev, const char *name, IDState driver_state, char *texts[], char *names[], int n);

/** \typedef CBNP
    \brief Number Property Callback.*/
typedef void (CBNP) (const char *dev, const char *name, IDState driver_state, double values[], char *names[], int n);

/** \typedef CBLP
    \brief Light Property Callback.*/
typedef void (CBLP) (const char *dev, const char *name, IDState driver_state, ISState *states, char *names[], int n);

/** \typedef CBBP
    \brief Blob Property Callback.*/
typedef void (CBBP) (const char *dev, const char *name, IDState driver_state, int sizes[], char *blobs[], char *formats[], char *names[], int n);

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Subscribe to monitor changes in a property in another device.
    \param dev Name of the device to monitor.
    \param name Name of the property in \e dev to monitor.
    \param property_type Type of property to monitor.
    \param notification_type determines what changes in the monitered property get reported to the subscriber driver.
    \param fp a pointer to a callback function. The callback function prototype <b>must</b> match the property_type.
*/
void IOSubscribeProperty(const char *dev, const char *name, IPType property_type, IDType notification_type, fpt fp);

/** \brief Unsubscribe from a property.
    \param dev Name of the device
    \param name Name of property in \e dev to stop monitoring.
**/ 
void IOUnsubscribeProperty(const char *dev, const char *name);

/*@}*/

int processObservers(XMLEle *root);
const char * idtypeStr(IDType type);
int crackObserverState(char *stateStr);
int crackPropertyState(char *pstateStr);

#ifdef __cplusplus
}
#endif

#endif
