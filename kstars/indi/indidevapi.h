#if 0
    INDI
    Copyright (C) 2003 Elwood C. Downey

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#endif

/*******************************************************************************
 * This is the interface to the reference INDI C API device implementation on
 * the Device Driver side. This file are divided into two main sections:
 *
 *   Functions the INDI device driver framework defines which the Driver may
 *   call:
 *
 *     IDxxx functions to send messages to an INDI client.
 *     IExxx functions to implement the event driven model.
 *     IUxxx functions to perform handy utility functions.
 *
 *   Functions the INDI device driver framework calls which the Driver must
 *   define:
 *
 *     ISxxx to respond to messages from a Client.
 */

/*******************************************************************************
 * get the data structures
 */

#include "indiapi.h"

/*******************************************************************************
 *******************************************************************************
 *
 *   Functions the INDI device driver framework defines which the Driver calls
 *
 *******************************************************************************
 *******************************************************************************
 */

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Functions Drivers call to define their Properties to Clients.
 */
 
 extern void IDDefText (const ITextVectorProperty *t, const char *msg, ...);

extern void IDDefNumber (const INumberVectorProperty *n, const char *msg, ...);

extern void IDDefSwitch (const ISwitchVectorProperty *s, const char *msg, ...);

extern void IDDefLight (const ILightVectorProperty *l, const char *msg, ...);



/*******************************************************************************
 * Functions Drivers call to tell Clients of new values for existing Properties.
 * msg argument functions like printf in ANSI C; may be NULL for no message.
 */

extern void IDSetText (const ITextVectorProperty *t, const char *msg, ...);

extern void IDSetNumber (const INumberVectorProperty *n, const char *msg, ...);

extern void IDSetSwitch (const ISwitchVectorProperty *s, const char *msg, ...);

extern void IDSetLight (const ILightVectorProperty *l, const char *msg, ...);


/*******************************************************************************
 * Function Drivers call to send log messages to Clients. If dev is specified
 * the Client shall associate the message with that device; if dev is NULL the
 * Client shall treat the message as generic from no specific Device.
 */

extern void IDMessage (const char *dev, const char *msg, ...);


/*******************************************************************************
 * Function Drivers call to inform Clients a Property is no longer
 * available, or the entire device is gone if name is NULL.
 * msg argument functions like printf in ANSI C; may be NULL for no message.
 */

extern void IDDelete (const char *dev, const char *name, const char *msg, ...);


/*******************************************************************************
 * Function Drivers call to log a message locally; the message
 * is not sent to any Clients.
 * msg argument functions like printf in ANSI C; may be NULL for no message.
 */

extern void IDLog (const char *msg, ...);


/*******************************************************************************
 * Functions Drivers call to register with the INDI event utilities.
 *
 *   Callbacks are called when a read on a file descriptor will not block.
 *   Timers are called once after a specified interval.
 *   Workprocs are called when there is nothing else to do.
 *
 * The "Add" functions return a unique id for use with their corresponding "Rm"
 * removal function. An arbitrary pointer may be specified when a function is
 * registered which will be stored and forwarded unchanged when the function
 * is later involked.
 */

/* signature of a callback, timout caller and work procedure function */

typedef void (IE_CBF) (int readfiledes, void *userpointer);
typedef void (IE_TCF) (void *userpointer);
typedef void (IE_WPF) (void *userpointer);

/* functions to add and remove callbacks, timers and work procedures */

extern int  IEAddCallback (int readfiledes, IE_CBF *fp, void *userpointer);
extern void IERmCallback (int callbackid);

extern int  IEAddTimer (int millisecs, IE_TCF *fp, void *userpointer);
extern void IERmTimer (int timerid);

extern int  IEAddWorkProc (IE_WPF *fp, void *userpointer);
extern void IERmWorkProc (int workprocid);


/*******************************************************************************
 * Functions Drivers call to perform handy utility functions.
 * these do not communicate with the Client in any way.
 */

/* functions to find a specific member of a vector property */

extern IText   *IUFindText  (const ITextVectorProperty *tp, const char *name);
extern INumber *IUFindNumber(const INumberVectorProperty *tp, const char *name);
extern ISwitch *IUFindSwitch(const ISwitchVectorProperty *tp, const char *name);
extern ISwitch *IUFindOnSwitch (const ISwitchVectorProperty *tp);

/* function to set all property switches off */
extern void IUResetSwitches(const ISwitchVectorProperty *svp);

extern int IUUpdateSwitches(const ISwitchVectorProperty *svp, ISState *states, char *names[], int n);

/* function to reliably save new text in a IText */
extern void IUSaveText (IText *tp, const char *newtext);

/*******************************************************************************
 *******************************************************************************
 *
 *   Functions the INDI Device Driver framework calls which the Driver must
 *   define.
 *
 *******************************************************************************
 *******************************************************************************
 */


/*******************************************************************************
 * Function defined by Drivers that is called when a Client asks for the
 * definitions of all Properties this Driver supports for the given
 * device, or all its devices if dev is NULL.
 */

extern void ISGetProperties (const char *dev);


/*******************************************************************************
 * Functions defined by Drivers that are called when a Client wishes to set
 * different values named members of the given vector Property. The values and
 * their names are parallel arrays of n elements each.
 */

extern void ISNewText (const char *dev, const char *name, char *texts[],
    char *names[], int n); 

extern void ISNewNumber (const char *dev, const char *name, double *doubles,
    char *names[], int n); 

extern void ISNewSwitch (const char *dev, const char *name, ISState *states,
    char *names[], int n); 
  
#ifdef __cplusplus
}
#endif

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile$ $Date$ $Revision$ $Name:  $
 */
