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
#include <linux/usb.h>
#include <linux/usbdevice_fs.h>
#include <sys/ioctl.h>

#include <errno.h>

#include "libfli-libfli.h"
#include "libfli-sys.h"
#include "libfli-usb.h"

/* Device descriptor */
typedef struct     
{
  __u8  bLength;
  __u8  bDescriptorType;
  __u16 bcdUSB;
  __u8  bDeviceClass;
  __u8  bDeviceSubClass;
  __u8  bDeviceProtocol;
  __u8  bMaxPacketSize0;
  __u16 idVendor;
  __u16 idProduct;
  __u16 bcdDevice;
  __u8  iManufacturer;
  __u8  iProduct;
  __u8  iSerialNumber;
  __u8  bNumConfigurations;
} usb_device_descriptor;

/* __attribute__ ((packed));*/

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
      if(usb_desc.idProduct != 0x0002)
      {
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

  io = DEVICE->io_data;
  bulk.ep = ep;
  bulk.len = *len;
  bulk.timeout = DEVICE->io_timeout;
  bulk.data = buf;

  /* Claim the interface */
  if (ioctl(io->fd, USBDEVFS_CLAIMINTERFACE, &iface))
    return -errno;

  /* This ioctl return the number of bytes transfered */
  *len = ioctl(io->fd, USBDEVFS_BULK, &bulk);

  /* Release the interface */
  if (ioctl(io->fd, USBDEVFS_RELEASEINTERFACE, &iface))
    return -errno;

  if (*len != bulk.len)
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
