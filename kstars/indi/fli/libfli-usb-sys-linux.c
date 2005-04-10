/*

  Copyright (c) 2002 Finger Lakes Instrumentation (FLI), L.L.C.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

        Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

        Redistributions in binary form must reproduce the above
        copyright notice, this list of conditions and the following
        disclaimer in the documentation and/or other materials
        provided with the distribution.

        Neither the name of Finger Lakes Instrumentation (FLI), LLC
        nor the names of its contributors may be used to endorse or
        promote products derived from this software without specific
        prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

  ======================================================================

  Finger Lakes Instrumentation, L.L.C. (FLI)
  web: http://www.fli-cam.com
  email: support@fli-cam.com

*/

#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdio.h>

#include <errno.h>

#include "libfli-libfli.h"
#include "libfli-sys.h"
#include "libfli-usb.h"

#define USB_DIR_OUT			0		/* to device */
#define USB_DIR_IN			0x80		/* to host */
#define USBDEVFS_CLAIMINTERFACE    _IOR('U', 15, unsigned int)
#define USBDEVFS_BULK              _IOWR('U', 2, struct usbdevfs_bulktransfer)
#define USBDEVFS_RELEASEINTERFACE  _IOR('U', 16, unsigned int)
#define IOCTL_USB_RESET		   _IO('U', 20)

/* Device descriptor */
typedef struct
{
  u_int8_t  bLength;
  u_int8_t  bDescriptorType;
  u_int16_t bcdUSB;
  u_int8_t  bDeviceClass;
  u_int8_t  bDeviceSubClass;
  u_int8_t  bDeviceProtocol;
  u_int8_t  bMaxPacketSize0;
  u_int16_t idVendor;
  u_int16_t idProduct;
  u_int16_t bcdDevice;
  u_int8_t  iManufacturer;
  u_int8_t  iProduct;
  u_int8_t  iSerialNumber;
  u_int8_t  bNumConfigurations;
}  usb_device_descriptor __attribute__ ((packed));

struct usbdevfs_bulktransfer {
  unsigned int ep;
  unsigned int len;
  unsigned int timeout; /* in milliseconds */
  void *data;
};

long linux_usb_reset(flidev_t dev);

long unix_usbverifydescriptor(flidev_t dev, fli_unixio_t *io)
{
  usb_device_descriptor usb_desc;
  int r;

  if ((r = read(io->fd, &usb_desc, sizeof(usb_device_descriptor))) !=
      sizeof(usb_device_descriptor))
  {
    debug(FLIDEBUG_FAIL, "linux_usbverifydescriptor(): Could not read descriptor.");
    return -EIO;
  }
  else
  {
    debug(FLIDEBUG_INFO, "USB device descriptor:");
    if(usb_desc.idVendor != 0x0f18)
    {
      debug(FLIDEBUG_FAIL, "linux_usbverifydescriptor(): Not a FLI device!");
      return -ENODEV;
    }

    switch(DEVICE->domain)
    {
			case FLIDOMAIN_USB:
				if( (usb_desc.idProduct != 0x0002) &&
						(usb_desc.idProduct != 0x0006) &&
						(usb_desc.idProduct != 0x0007) ) {
					return -ENODEV;
				}
      break;

			default:
				return -EINVAL;
				break;
    }

    DEVICE->devinfo.fwrev = usb_desc.bcdDevice;
  }

  return 0;
}

static long linux_bulktransfer(flidev_t dev, int ep, void *buf, long *len)
{
  fli_unixio_t *io;
  unsigned int iface = 0;
  struct usbdevfs_bulktransfer bulk;
  unsigned int tbytes = 0;
  long bytes;

/* This section of code has been modified since the Linux kernel has (had)
   a 4096 byte limit (kernel page size) on the IOCTL for data transfer.
   We ran into a problem when the CCD camera became large and the data
   transfer requirements grew. */

  io = DEVICE->io_data;

  /* Claim the interface */
  if (ioctl(io->fd, USBDEVFS_CLAIMINTERFACE, &iface))
    return -errno;

/* #define _DEBUG */

#ifdef _DEBUG

	if ((ep & 0xf0) == 0) {
		char buffer[1024];
		int i;

		sprintf(buffer, "OUT %6ld: ", *len);
		for (i = 0; i < ((*len > 16)?16:*len); i++) {
			sprintf(buffer + strlen(buffer), "%02x ", ((unsigned char *) buf)[i]);
		}

		debug(FLIDEBUG_INFO, buffer);
	}

#endif /* _DEBUG */

	while (tbytes < (unsigned) *len) {
		bulk.ep = ep;
		bulk.len = ((*len - tbytes) > 4096)?4096:(*len - tbytes);
		bulk.timeout = DEVICE->io_timeout;
		bulk.data = buf + tbytes;

		/* This ioctl return the number of bytes transfered */
		if((bytes = ioctl(io->fd, USBDEVFS_BULK, &bulk)) != (long) bulk.len)
			break;

		tbytes += bytes;
	}

#ifdef _DEBUG

	if ((ep & 0xf0) != 0) {
		char buffer[1024];
		int i;

		sprintf(buffer, " IN %6ld: ", *len);
		for (i = 0; i < ((*len > 16)?16:*len); i++) {
			sprintf(buffer + strlen(buffer), "%02x ", ((unsigned char *) buf)[i]);
		}

		debug(FLIDEBUG_INFO, buffer);
	}

#endif /* _DEBUG */

  /* Release the interface */
/*  if (ioctl(io->fd, USBDEVFS_RELEASEINTERFACE, &iface))
    return -errno; */

  if ((unsigned) *len != tbytes)
    return -errno;
  else
    return 0;
}

long linux_bulkwrite(flidev_t dev, void *buf, long *wlen)
{
  return linux_bulktransfer(dev, FLI_CMD_ENDPOINT | USB_DIR_OUT, buf, wlen);
}

long linux_bulkread(flidev_t dev, void *buf, long *rlen)
{
  return linux_bulktransfer(dev, FLI_CMD_ENDPOINT | USB_DIR_IN, buf, rlen);
}

long linux_usb_reset(flidev_t dev)
{
	fli_unixio_t *io;
	
	io = DEVICE->io_data;
	
	return (ioctl(io->fd, IOCTL_USB_RESET, NULL));

}
