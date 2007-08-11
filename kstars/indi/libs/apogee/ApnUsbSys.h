/*  Apogee Control Library

Copyright (C) 2001-2006 Dave Mills  (rfactory@theriver.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
// ApnUsbSys.h    
//
//
// Defines common data structure(s) for sharing between application
// layer and the ApUSB.sys device driver.
//

#if !defined(_APNUSBSYS_H__INCLUDED_)
#define _APNUSBSYS_H__INCLUDED_

#define VND_ANCHOR_LOAD_INTERNAL		0xA0

#define VND_APOGEE_CMD_BASE				0xC0
#define VND_APOGEE_STATUS				( VND_APOGEE_CMD_BASE + 0x0 )
#define VND_APOGEE_CAMCON_REG			( VND_APOGEE_CMD_BASE + 0x2 )
#define VND_APOGEE_BUFCON_REG			( VND_APOGEE_CMD_BASE + 0x3 )
#define VND_APOGEE_SET_SERIAL			( VND_APOGEE_CMD_BASE + 0x4 )
#define VND_APOGEE_SERIAL				( VND_APOGEE_CMD_BASE + 0x5 )
#define VND_APOGEE_EEPROM				( VND_APOGEE_CMD_BASE + 0x6 )
#define VND_APOGEE_SOFT_RESET			( VND_APOGEE_CMD_BASE + 0x8 )
#define VND_APOGEE_GET_IMAGE			( VND_APOGEE_CMD_BASE + 0x9 )
#define VND_APOGEE_STOP_IMAGE			( VND_APOGEE_CMD_BASE + 0xA )


#define	REQUEST_IN	0x1
#define REQUEST_OUT	0x0


typedef struct _APN_USB_REQUEST
{
	unsigned char	Request;
	unsigned char	Direction;
	unsigned short	Value;
	unsigned short	Index;
} APN_USB_REQUEST, *PAPN_USB_REQUEST;



#endif  // !defined(_APNUSBSYS_H__INCLUDED_)
