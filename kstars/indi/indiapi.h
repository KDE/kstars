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
 * This is the interface to the reference INDI C API implementation. The API
 * and this file are divided into three main sections:
 *
 *   Constants and Data structure definitions.
 *
 *   Functions we define which the Driver calls
 *
 *     IDxxx functions to send messages to an INDI client.
 *     IExxx functions to implement the event driven model.
 *     IUxxx functions to perform handy utility functions.
 *
 *   Functions we call which the Driver must define.
 *
 *     ISxxx to respond to messages from a Client.
 */

/*******************************************************************************
 * INDI wire protocol version implemented by this API.
 * N.B. this is indepedent of the API itself.
 */

#define	INDIV	1.11


/*******************************************************************************
 *******************************************************************************
 *
 *   Constants and Data structure definitions.
 *
 *******************************************************************************
 *******************************************************************************
 */


/*******************************************************************************
 * Manifest constants
 */

typedef enum {
    ISS_ON, ISS_OFF
} ISState;				/* switch state */

typedef enum {
    IPS_IDLE, IPS_OK, IPS_BUSY, IPS_ALERT
} IPState;				/* property state */

typedef enum {
    ISR_1OFMANY, ISR_NOFMANY
} ISRule;				/* switch vector rule hint */

typedef enum {
    IP_RO, IP_WO, IP_RW
} IPerm;				/* permission hint */

/*******************************************************************************
 * Typedefs for each INDI Property type.
 *
 * INumber.format may be any printf-style appropriate for double
 * or style "m" to create sexigesimal using the form "%<w>.<f>m" where
 *   <w> is the total field width.
 *   <f> is the width of the fraction. valid values are:
 *      9  ->  :mm:ss.ss
 *      8  ->  :mm:ss.s
 *      6  ->  :mm:ss
 *      5  ->  :mm.m
 *      3  ->  :mm
 *
 * examples:
 *
 *   to produce     use
 *
 *    "-123:45"    %7.3m
 *  "  0:01:02"    %9.6m
 */

typedef struct {			/* one text descriptor */
    const char *name;			/* index name */
    const char *label;			/* short description */
    char *text;				/* current text string */
} IText;

typedef struct {			/* text vector property descriptor */
    const char *device;			/* device name */
    const char *name;			/* property name */
    const char *label;			/* short description */
    const char *grouptag;		/* GUI grouping hint, ignore if NULL */
    const IPerm p;			/* client accessibility hint */
    double timeout;			/* current max time to change, secs */
    IPState s;				/* current property state */
    IText *t;				/* texts comprising this vector */
    const int nt;			/* dimension of this vector */
} ITextVectorProperty;

typedef struct {			/* one number descriptor */
    const char *name;			/* index name */
    const char *label;			/* short description */
    const char *format;			/* GUI display format, see above */
    const double min, max;		/* range, ignore if min == max */
    const double step;			/* step size, ignore if step == 0 */
    double value;			/* current value */
} INumber;

typedef struct {			/* number vector property descriptor */
    const char *device;			/* device name */
    const char *name;			/* property name */
    const char *label;			/* short description */
    const char *grouptag;		/* GUI grouping hint, ignore if NULL */
    const IPerm p;			/* client accessibility hint */
    double timeout;			/* current max time to change, secs */
    IPState s;				/* current property state */
    INumber *n;				/* numbers comprising this vector */
    const int nn;			/* dimension of this vector */
} INumberVectorProperty;

typedef struct {			/* one switch descriptor */
    const char *name;			/* index name */
    const char *label;			/* this switch's label */
    ISState s;				/* this switch's state */
} ISwitch;

typedef struct {			/* switch vector property descriptor */
    const char *device;			/* device name */
    const char *name;			/* property name */
    const char *label;			/* short description */
    const char *grouptag;		/* GUI grouping hint, ignore if NULL */
    const IPerm p;			/* client accessibility hint */
    const ISRule r;			/* switch behavior hint */
    double timeout;			/* current max time to change, secs */
    IPState s;				/* current property state */
    ISwitch *sw;			/* switches comprising this vector */
    const int nsw;			/* dimension of this vector */
} ISwitchVectorProperty;

typedef struct {			/* one light descriptor */
    const char *name;			/* index name */
    const char *label;			/* this lights's label */
    IPState s;				/* this lights's state */
} ILight;

typedef struct {			/* light vector property descriptor */
    const char *device;			/* device name */
    const char *name;			/* property name */
    const char *label;			/* short description */
    const char *grouptag;		/* GUI grouping hint, ignore if NULL */
    IPState s;				/* current property state */
    ILight *l;				/* lights comprising this vector */
    const int nl;			/* dimension of this vector */
} ILightVectorProperty;

/*******************************************************************************
 * Handy macro to find the number of elements in array a[].
 * N.B. must be used with actual array, not pointer.
 */

#define NARRAY(a)       (sizeof(a)/sizeof(a[0]))


#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 *******************************************************************************
 *
 *   Functions we define which the Driver calls
 *
 *******************************************************************************
 *******************************************************************************
 */


/*******************************************************************************
 * Functions Drivers call to define their Properties to Clients.
 */

extern void IDDefText (const ITextVectorProperty *t);

extern void IDDefNumber (const INumberVectorProperty *n);

extern void IDDefSwitch (const ISwitchVectorProperty *s);

extern void IDDefLight (const ILightVectorProperty *l);



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

/* function to reliably save new text in a IText */
extern void IUSaveText (IText *tp, const char *newtext);



/*******************************************************************************
 *******************************************************************************
 *
 *   Functions we call which the Driver must define.
 *
 *******************************************************************************
 *******************************************************************************
 */

 extern int INumFormat (char *buf, INumber *ip);
 
/*******************************************************************************
 * Function defined by Drivers that is called when a Client asks for the
 * definitions of all Properties this Driver supports for the given
 * device, or all its devices if dev is NULL.
 */

extern void ISGetProperties (const char *dev);


/*******************************************************************************
 * Functions defined by Drivers that are called when a Client wishes to set
 * different values for the n named members of the given vector Property.
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
