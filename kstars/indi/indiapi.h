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

/* Prototypes for functions in an INDI Device.
 *
 * The first set of functions are those which the Driver may call; they each
 * start with IC because they communicate with an INDI Client. The second set
 * are those functions which the Driver must define and are called by the
 * INDI system, implemented here in indimain.c; they each start with IS
 * because they must be defined by an INDI Server (or Driver).
 */

#define	INDIV	1.01			/* version */


/**********************************************************************
 **********************************************************************
 *
 * Common Constants and Data structures
 *
 **********************************************************************
 **********************************************************************/


/**********************************************************************
 * Manifest constants
 */

typedef enum {
    ISS_ON, ISS_OFF
} ISState;					/* switch state */

typedef enum {
    ILS_IDLE, ILS_OK, ILS_BUSY, ILS_ALERT
} ILState;					/* light and property state */

typedef enum {
    IP_RO, IP_WO, IP_RW
} IPerm;					/* text and number permission */

typedef enum {
    ISP_R, ISP_W
} ISPerm;					/* switch permission */

typedef enum {
    IR_1OFMANY, IR_NOFMANY
} IRule;					/* switch behavior rule */

typedef enum {					/* which range fields are set */
    INR_MIN=1, INR_MAX=2, INR_STEP=4		/* |-able bit masks */
} INRangeSet;


/**********************************************************************
 * Typedefs for each INDI Property type.
 * comments for fields that are not set when the ISNew dispatch functions
 * are called begin with *
 */

typedef struct {			/* text property descriptor */
    char *dev;				/* device name */
    char *name;				/* propert name */
    char *text;				/* text string */
    ILState s;				/* * property state */
    double tout;			/* * max time to change, secs */
    char *grouptag;			/* * group Propertiess within one Dev */
} IText;

typedef struct {			/* number property descriptor */
    char *dev;				/* device name */
    char *name;				/* propert name */
    char *nstr;				/* numeric value as a string */
    ILState s;				/* * property state */
    double tout;			/* * max time to change, secs */
    char *grouptag;			/* * group Propertiess within one Dev */
} INumber;

typedef struct {			/* specify legal range of an INumber */
    int rSet;				/* | of approproate INRangeSet */
    double min;				/* min value */
    double max;				/* max value */
    double step;			/* allowed increments */
} INRange;

typedef struct {			/* one switch descriptor */
    char *name;				/* this switch's label */
    ISState s;				/* this switch's state */
} ISwitch;

typedef struct {			/* switch property descriptor */
    char *dev;				/* device name */
    char *name;				/* propert name */
    ISwitch *sw;			/* array of each switch */
    int nsw;				/* * number in array sw */
    ILState s;				/* * property state */
    double tout;			/* * max time to change, secs */
    char *grouptag;			/* * group Propertiess within one Dev */
} ISwitches;

typedef struct {			/* one light descriptor */
    char *name;				/* this lights's label */
    ILState s;				/* this lights's state */
} ILight;

typedef struct {			/* Light property descriptor */
    char *dev;				/* device name */
    char *name;				/* propert name */
    ILight *l;				/* array of each light */
    int nl;				/* number in array l */
    ILState s;				/* property state */
    double tout;			/* max time to change, secs */
} ILights;


/**********************************************************************
 * Handy macro to find the number of elements in array a[].
 * N.B. must be used with actual array, not pointer.
 */

#define NARRAY(a)       (sizeof(a)/sizeof(a[0]))




/**********************************************************************
 **********************************************************************
 *
 * Functions the Driver may call to send commands to Clients.
 * all msg arguments function like printf in ANSI C.
 *
 **********************************************************************
 **********************************************************************/


/**********************************************************************
 * Functions Drivers call to define their Properties to Clients.
 */

#ifdef __cplusplus
extern "C" {
#endif

extern void ICDefText (IText *t, char *prompt, IPerm p);

extern void ICDefNumber (INumber *n, char *prompt, IPerm p, INRange *rp);

extern void ICDefSwitches (ISwitches *s, char *prompt, ISPerm p, IRule r);

extern void ICDefLights (ILights *l, char *prompt);



/**********************************************************************
 * Functions Drivers call to inform Clients of new values to existing
 * Properties. msg may be NULL.
 */

extern void ICSetText (IText *t, char *msg, ...);

extern void ICSetNumber (INumber *n, char *msg, ...);

extern void ICSetSwitch (ISwitches *s, char *msg, ...);

extern void ICSetLight (ILights *l, char *msg, ...);



/**********************************************************************
 * Function Drivers call to send a message to Clients. If dev is specified
 * the Client should associate the message with that device; if dev is NULL
 * the Client should treat the message as a general one from no specific Device.
 */

extern void ICMessage (char *dev, char *msg, ...);


/**********************************************************************
 * Function Drivers call to inform Clients a Property is no longer available,
 * or the entire device is if name is NULL. msg may be NULL.
 */

extern void ICDelete (char *dev, char *name, char *msg, ...);



/**********************************************************************
 * Function Drivers call to log a message locally; the message
 * is not sent to any Clients.
 */

extern void ICLog (char *msg, ...);


/**********************************************************************
 * Function Drivers call to ask that ISPoll() be called every ms
 * millisecs, or stop calling it if ms is 0.
 */

extern void ICPollMe (int ms);




/**********************************************************************
 **********************************************************************
 *
 * Functions defined by Drivers that get called to handle commands from
 * Clients. Drivers should not blindly trust Client commands but apply
 * their own sanity checks before proceeding with making any changes.
 * Drivers should return quietly if dev is not one of theirs.
 *
 **********************************************************************
 **********************************************************************/


/**********************************************************************
 * Function defined by Drivers that will be the first INDI function to be
 * called and which will be called exactly that one time and one time only.
 */

extern void ISInit (void);


/**********************************************************************
 * Function defined by Drivers that is called when a Client asks for the
 * definitions of all Properties this Driver supports for the given
 * device, or all its devices if dev is NULL. It is polite to call the
 * approprate defXXX functions soon after receiving this call.
 */

extern void ISGetProperties (char *dev);


/**********************************************************************
 * Functions defined by Drivers that are called when a Client wishes
 * to set a different value for the given Property.
 */

extern void ISNewText (IText *t);

extern void ISNewNumber (INumber *n);

extern void ISNewSwitch (ISwitches *s);	/* s->nsw will always be 1 */



/**********************************************************************
 * Function defined by Drivers that is called as specified in last
 * call to ICPollMe().
 */

extern void ISPoll (void);

#ifdef __cplusplus
}
#endif

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile$ $Date$ $Revision$ $Name:  $
 */
