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

/* public interface to eventloop mechanism */

/* signature of a callback, workproc and timer function */
typedef void (CBF) (int fd, void *);
typedef void (WPF) (void *);
typedef void (TCF) (void *);

#ifdef __cplusplus
extern "C" {
#endif

/* main calls this when ready to hand over control */
extern void eventLoop(void);

/* functions to add and remove callbacks, workprocs and timers */
extern int addCallback (int fd, CBF *fp, void *ud);
extern void rmCallback (int cid);
extern int addWorkProc (WPF *fp, void *ud);
extern void rmWorkProc (int wid);
extern int addTimer (int ms, TCF *fp, void *ud);
extern void rmTimer (int tid);

#ifdef __cplusplus
}
#endif

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile$ $Date$ $Revision$ $Name:  $
 */
