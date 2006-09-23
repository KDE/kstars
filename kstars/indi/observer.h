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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   Changelog:

   2006-09-23: Start (JM)

#endif

#ifndef OBSERVER_H
#define OBSERVER_H

typedef enum {
   IDS_OK, IDS_DISCONNECTED, IDS_UNAVAILABLE, IDS_DIED
} IDState;				/* Driver state in observer pattern */

typedef enum {
   IDT_VALUE, IDT_STATE
} IDType;				/* Driver state in observer pattern */

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

void IOSubscribeSwitch(const char *dev, const char *name, IDType data_type, CBSP *fp);
void IOUnsubscribeSwitch(const char *dev, const char *name);





const char * idtypeStr(IDType type);


#ifdef __cplusplus
}
#endif

#endif
