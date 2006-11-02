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
 
#ifndef APOGEELINUX_H
#define APOGEELINUX_H

#define APISA_READ_USHORT	_IOR('a', 0x01, unsigned int)
#define APISA_READ_LINE 	_IOR('a', 0x02, unsigned int)
#define APISA_WRITE_USHORT	_IOW('a', 0x03, unsigned int)

#define APPPI_READ_USHORT	_IOR('a', 0x01, unsigned int)
#define APPPI_READ_LINE 	_IOR('a', 0x02, unsigned int)
#define APPPI_WRITE_USHORT	_IOW('a', 0x03, unsigned int)

#define APPCI_READ_USHORT	_IOR('a', 0x01, unsigned int)
#define APPCI_READ_LINE 	_IOR('a', 0x02, unsigned int)
#define APPCI_WRITE_USHORT	_IOW('a', 0x03, unsigned int)

#define APUSB_READ_USHORT	_IOR('a', 0x01, unsigned int)
#define APUSB_WRITE_USHORT	_IOW('a', 0x02, unsigned int)
#define APUSB_USB_STATUS 	_IOR('a', 0x03, unsigned int)
#define APUSB_PRIME_USB_DOWNLOAD	_IOR('a', 0x04, unsigned int)
#define APUSB_STOP_USB_IMAGE	_IOR('a', 0x05, unsigned int)
#define APUSB_READ_USB_IMAGE	_IOR('a', 0x06, unsigned int)
#define APUSB_USB_RESET		_IOR('a', 0x07, unsigned int)
#define APUSB_READ_USB_SERIAL	_IOR('a', 0x08, unsigned int)
#define APUSB_WRITE_USB_SERIAL	_IOR('a', 0x09, unsigned int)
#define APUSB_USB_SET_SERIAL	_IOR('a', 0x0A, unsigned int)
#define APUSB_USB_REQUEST	_IOR('a', 0x0B, unsigned int)
#define APUSB_READ_STATUS	_IOR('a', 0x0C, unsigned int)

#define appci_major_number 60
#define apppi_major_number 61
#define apisa_major_number 62

struct apIOparam                  // IOCTL data
  {
  unsigned int reg;
  unsigned long param1, param2;
  };

#define APOGEE_PCI_DEVICE "/dev/appci"
#define APOGEE_PPI_DEVICE "/dev/apppi"
#define APOGEE_ISA_DEVICE "/dev/apisa"
#define APOGEE_USB_DEVICE "/dev/usb/alta"

const int NUM_POSITIONS = 6;
const int NUM_STEPS_PER_FILTER = 48;
const int STEP_DELAY = 10;

const unsigned char Steps[] = { 0x10, 0x30, 0x20, 0x60, 0x40, 0xc0, 0x80, 0x90 };
const int NUM_STEPS = sizeof ( Steps );

#endif
