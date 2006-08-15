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

#if !defined(_APOGEEUSB_H__INCLUDED_)
#define _APOGEEUSB_H__INCLUDED_

#ifndef APN_USB_TYPE
#define APN_USB_TYPE unsigned short
#endif

#define APN_USB_MAXCAMERAS	255


typedef struct _APN_USB_CAMINFO {
	unsigned short CamNumber;
	unsigned short CamModel;
} APN_USB_CAMINFO;



#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif


APN_USB_TYPE ApnUsbOpen( unsigned short DeviceNumber );


APN_USB_TYPE ApnUsbClose( void );


APN_USB_TYPE ApnUsbDiscovery( unsigned short *UsbCamCount, 
							  APN_USB_CAMINFO UsbCamInfo[] );


APN_USB_TYPE ApnUsbReadReg( unsigned short FpgaReg, 
						    unsigned short *FpgaData );


APN_USB_TYPE ApnUsbWriteReg( unsigned short FpgaReg, 
							 unsigned short FpgaData );


APN_USB_TYPE ApnUsbWriteRegMulti( unsigned short FpgaReg,
								  unsigned short FpgaData[],
								  unsigned short RegCount );


APN_USB_TYPE ApnUsbWriteRegMultiMRMD( unsigned short FpgaReg[],
									  unsigned short FpgaData[],
									  unsigned short RegCount );


APN_USB_TYPE ApnUsbReadStatusRegs( unsigned short *StatusReg,
			 					   unsigned short *HeatsinkTempReg,
								   unsigned short *CcdTempReg,
								   unsigned short *CoolerDriveReg,
								   unsigned short *VoltageReg,
								   unsigned short *TdiCounter,
								   unsigned short *SequenceCounter );


APN_USB_TYPE ApnUsbStartExp( unsigned short ImageWidth,
							 unsigned short ImageHeight );


APN_USB_TYPE ApnUsbStopExp( bool DigitizeData );


APN_USB_TYPE ApnUsbGetImage( unsigned short *pMem );


APN_USB_TYPE ApnUsbReset();


#endif  // !defined(_APOGEEUSB_H__INCLUDED_)
