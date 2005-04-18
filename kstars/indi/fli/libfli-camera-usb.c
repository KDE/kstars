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

#ifdef WIN32
#include <winsock.h>
#else
#include <sys/param.h>
#include <netinet/in.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include "libfli-libfli.h"
#include "libfli-debug.h"
#include "libfli-mem.h"
#include "libfli-camera.h"
#include "libfli-camera-usb.h"

double dconvert(void *buf)
{
  unsigned char *fnum = (unsigned char *) buf;
  double sign, exponent, mantissa, result;
  
  sign = (double) ((fnum[3] & 0x80)?(-1):(1));
  exponent = (double) ((fnum[3] & 0x7f) << 1 | ((fnum[2] & 0x80)?1:0));
  
  mantissa = 1.0 + 
    ((double) ((fnum[2] & 0x7f) << 16 | fnum[1] << 8 | fnum[0]) /
     pow(2, 23));
  
  result = sign * (double) pow(2, (exponent - 127.0)) * mantissa;
  
  return result;
}

long fli_camera_usb_open(flidev_t dev)
{
  flicamdata_t *cam;
  long rlen, wlen;
  unsigned short buf[32];

  cam = DEVICE->device_data;

  if ((cam->gbuf = xmalloc(USB_READ_SIZ_MAX)) == NULL)
    return -ENOMEM;

  buf[0] = htons(FLI_USBCAM_HARDWAREREV);
  rlen = 2; wlen = 2;
  IO(dev, buf, &wlen, &rlen);
  DEVICE->devinfo.hwrev = ntohs(buf[0]);

  buf[0] = htons(FLI_USBCAM_DEVICEID);
  rlen = 2; wlen = 2;
  IO(dev, buf, &wlen, &rlen);
  DEVICE->devinfo.devid = ntohs(buf[0]);

  buf[0] = htons(FLI_USBCAM_SERIALNUM);
  rlen = 2; wlen = 2;
  IO(dev, buf, &wlen, &rlen);
  DEVICE->devinfo.serno = ntohs(buf[0]);

  debug(FLIDEBUG_INFO, "DeviceID %d", DEVICE->devinfo.devid);
  debug(FLIDEBUG_INFO, "SerialNum %d", DEVICE->devinfo.serno);
  debug(FLIDEBUG_INFO, "HWRev %d", DEVICE->devinfo.hwrev);
  debug(FLIDEBUG_INFO, "FWRev %d", DEVICE->devinfo.fwrev);

  if (DEVICE->devinfo.fwrev < 0x0201)
  {
    int id;

    for (id = 0; knowndev[id].index != 0; id++)
      if (knowndev[id].index == DEVICE->devinfo.devid)
				break;

    if (knowndev[id].index == 0)
      return -ENODEV;

    cam->ccd.pixelwidth = knowndev[id].pixelwidth;
    cam->ccd.pixelheight = knowndev[id].pixelheight;

    wlen = sizeof(unsigned short) * 7;
    rlen = 0;
    buf[0] = htons(FLI_USBCAM_DEVINIT);
    buf[1] = htons((unsigned short)knowndev[id].array_area.lr.x);
    buf[2] = htons((unsigned short)knowndev[id].array_area.lr.y);
    buf[3] = htons((unsigned short)(knowndev[id].visible_area.lr.x -
				    knowndev[id].visible_area.ul.x));
    buf[4] = htons((unsigned short)(knowndev[id].visible_area.lr.y -
				    knowndev[id].visible_area.ul.y));
    buf[5] = htons((unsigned short)knowndev[id].visible_area.ul.x);
    buf[6] = htons((unsigned short)knowndev[id].visible_area.ul.y);
    IO(dev, buf, &wlen, &rlen);
    if ((DEVICE->devinfo.model =
	 (char *)xmalloc(strlen(knowndev[id].model) + 1)) == NULL)
      return -ENOMEM;
    strcpy(DEVICE->devinfo.model, knowndev[id].model);
    
    switch(DEVICE->devinfo.fwrev & 0xff00)
    {
     case 0x0100:
      cam->tempslope = (70.0 / 215.75);
      cam->tempintercept = (-52.5681);
      break;
      
     case 0x0200:
      cam->tempslope = (100.0 / 201.1);
      cam->tempintercept = (-61.613);
      break;
      
     default:
      cam->tempslope = 1e-12;
      cam->tempintercept = 0;
    }
  }
  else if (DEVICE->devinfo.fwrev >= 0x0201) /* Here, all the parameters are stored on the camera */
  {
    rlen = 64; wlen = 2;
    buf[0] = htons(FLI_USBCAM_READPARAMBLOCK);
    IO(dev, buf, &wlen, &rlen);
    
    cam->ccd.pixelwidth = dconvert((char *) ((void *)buf) + 31);
    cam->ccd.pixelheight = dconvert((char*) ((void *)buf) + 35);
    cam->tempslope = dconvert((char *) ((void *)buf) + 23);
    cam->tempintercept = dconvert((char *) ((void *)buf) + 27);
  }

  rlen = 32; wlen = 2;
  buf[0] = htons(FLI_USBCAM_DEVICENAME);
  IO(dev, buf, &wlen, &rlen);

  if ((DEVICE->devinfo.devnam = (char *)xmalloc(rlen + 1)) == NULL)
    return -ENOMEM;
  memcpy(DEVICE->devinfo.devnam, buf, rlen);
  DEVICE->devinfo.devnam[rlen] = '\0';

	if(DEVICE->devinfo.model == NULL)
	{
		DEVICE->devinfo.model = xstrdup(DEVICE->devinfo.devnam);
	}

  rlen = 4; wlen = 2;
  buf[0] = htons(FLI_USBCAM_ARRAYSIZE);
  IO(dev, buf, &wlen, &rlen);
  cam->ccd.array_area.ul.x = 0;
  cam->ccd.array_area.ul.y = 0;
  cam->ccd.array_area.lr.x = ntohs(buf[0]);
  cam->ccd.array_area.lr.y = ntohs(buf[1]);

  rlen = 4; wlen = 2;
  buf[0] = htons(FLI_USBCAM_IMAGEOFFSET);
  IO(dev, buf, &wlen, &rlen);
  cam->ccd.visible_area.ul.x = ntohs(buf[0]);
  cam->ccd.visible_area.ul.y = ntohs(buf[1]);

  rlen = 4; wlen = 2;
  buf[0] = htons(FLI_USBCAM_IMAGESIZE);
  IO(dev, buf, &wlen, &rlen);
  cam->ccd.visible_area.lr.x = cam->ccd.visible_area.ul.x + ntohs(buf[0]);
  cam->ccd.visible_area.lr.y = cam->ccd.visible_area.ul.y + ntohs(buf[1]);

  debug(FLIDEBUG_INFO, "     Name: %s", DEVICE->devinfo.devnam);
  debug(FLIDEBUG_INFO, "    Array: (%4d,%4d),(%4d,%4d)",
	cam->ccd.array_area.ul.x,
	cam->ccd.array_area.ul.y,
	cam->ccd.array_area.lr.x,
	cam->ccd.array_area.lr.y);
  debug(FLIDEBUG_INFO, "  Visible: (%4d,%4d),(%4d,%4d)",
	cam->ccd.visible_area.ul.x,
	cam->ccd.visible_area.ul.y,
	cam->ccd.visible_area.lr.x,
	cam->ccd.visible_area.lr.y);

  debug(FLIDEBUG_INFO, " Pix Size: (%g, %g)", cam->ccd.pixelwidth, cam->ccd.pixelheight);
  debug(FLIDEBUG_INFO, "    Temp.: T = AD x %g + %g", cam->tempslope, cam->tempintercept);

  /* Initialize all varaibles to something */

  cam->vflushbin = 4;
  cam->hflushbin = 4;
  cam->vbin = 1;
  cam->hbin = 1;
  cam->image_area.ul.x = cam->ccd.visible_area.ul.x;
  cam->image_area.ul.y = cam->ccd.visible_area.ul.y;
  cam->image_area.lr.x = cam->ccd.visible_area.lr.x;
  cam->image_area.lr.y = cam->ccd.visible_area.lr.y;
  cam->exposure = 100;
  cam->frametype = FLI_FRAME_TYPE_NORMAL;
  cam->flushes = 0;
  cam->bitdepth = FLI_MODE_16BIT;
  cam->exttrigger = 0;
  cam->exttriggerpol = 0;

  cam->grabrowwidth =
    (cam->image_area.lr.x - cam->image_area.ul.x) / cam->hbin;
  cam->grabrowcount = 1;
  cam->grabrowcounttot = cam->grabrowcount;
  cam->grabrowindex = 0;
  cam->grabrowbatchsize = 1;
  cam->grabrowbufferindex = cam->grabrowcount;
  cam->flushcountbeforefirstrow = 0;
  cam->flushcountafterlastrow = 0;

  return 0;
}

long fli_camera_usb_get_array_area(flidev_t dev, long *ul_x, long *ul_y,
				   long *lr_x, long *lr_y)
{
  flicamdata_t *cam;
  long rlen, wlen;
  unsigned short buf[8];

  cam = DEVICE->device_data;

  rlen = 4; wlen = 2;
  buf[0] = htons(FLI_USBCAM_ARRAYSIZE);
  IO(dev, buf, &wlen, &rlen);
  cam->ccd.array_area.ul.x = 0;
  cam->ccd.array_area.ul.y = 0;
  cam->ccd.array_area.lr.x = ntohs(buf[0]);
  cam->ccd.array_area.lr.y = ntohs(buf[1]);

  *ul_x = cam->ccd.array_area.ul.x;
  *ul_y = cam->ccd.array_area.ul.y;
  *lr_x = cam->ccd.array_area.lr.x;
  *lr_y = cam->ccd.array_area.lr.y;

  return 0;
}

long fli_camera_usb_get_visible_area(flidev_t dev, long *ul_x, long *ul_y,
				     long *lr_x, long *lr_y)
{
  flicamdata_t *cam;
  long rlen, wlen;
  unsigned short buf[8];

  cam = DEVICE->device_data;

  rlen = 4; wlen = 2;
  buf[0] = htons(FLI_USBCAM_IMAGEOFFSET);
  IO(dev, buf, &wlen, &rlen);
  cam->ccd.visible_area.ul.x = ntohs(buf[0]);
  cam->ccd.visible_area.ul.y = ntohs(buf[1]);

  rlen = 4; wlen = 2;
  buf[0] = htons(FLI_USBCAM_IMAGESIZE);
  IO(dev, buf, &wlen, &rlen);
  cam->ccd.visible_area.lr.x = cam->ccd.visible_area.ul.x + ntohs(buf[0]);
  cam->ccd.visible_area.lr.y = cam->ccd.visible_area.ul.y + ntohs(buf[1]);

  *ul_x = cam->ccd.visible_area.ul.x;
  *ul_y = cam->ccd.visible_area.ul.y;
  *lr_x = cam->ccd.visible_area.lr.x;
  *lr_y = cam->ccd.visible_area.lr.y;

  return 0;
}

long fli_camera_usb_set_exposure_time(flidev_t dev, long exptime)
{
  flicamdata_t *cam;
  long rlen, wlen;
  unsigned short buf[8];

  if (exptime < 0)
    return -EINVAL;

  cam = DEVICE->device_data;

  rlen = 0; wlen = 8;
  buf[0] = htons(FLI_USBCAM_SETEXPOSURE);
  ((unsigned long *)((void *) buf))[1] = htonl(exptime);
  IO(dev, buf, &wlen, &rlen);

  cam->exposure = exptime;

  return 0;
}

long fli_camera_usb_set_image_area(flidev_t dev, long ul_x, long ul_y,
				   long lr_x, long lr_y)
{
  flicamdata_t *cam;
  long rlen, wlen;
  unsigned short buf[8];

  cam = DEVICE->device_data;

	if( (DEVICE->devinfo.fwrev < 0x0300) &&
		  ((DEVICE->devinfo.hwrev & 0xff00) == 0x0100) )
	{
		if( (lr_x > (cam->ccd.visible_area.lr.x * cam->hbin)) ||
	      (lr_y > (cam->ccd.visible_area.lr.y * cam->vbin)) )
		{
			debug(FLIDEBUG_WARN,
				"FLISetVisibleArea(), area out of bounds: (%4d,%4d),(%4d,%4d)",
				ul_x, ul_y, lr_x, lr_y);
		}
	}

	if( (ul_x < cam->ccd.visible_area.ul.x) ||
      (ul_y < cam->ccd.visible_area.ul.y) )
	{
	  debug(FLIDEBUG_FAIL,
			"FLISetVisibleArea(), area out of bounds: (%4d,%4d),(%4d,%4d)",
			ul_x, ul_y, lr_x, lr_y);
		return -EINVAL;
	}

  rlen = 0; wlen = 6;
  buf[0] = htons(FLI_USBCAM_SETFRAMEOFFSET);
  buf[1] = htons((unsigned short) cam->image_area.ul.x);
  buf[2] = htons((unsigned short) cam->image_area.ul.y);
  IO(dev, buf, &wlen, &rlen);

  cam->image_area.ul.x = ul_x;
  cam->image_area.ul.y = ul_y;
  cam->image_area.lr.x = lr_x;
  cam->image_area.lr.y = lr_y;
  cam->grabrowwidth =
    (cam->image_area.lr.x - cam->image_area.ul.x) / cam->hbin;

  return 0;
}

long fli_camera_usb_set_hbin(flidev_t dev, long hbin)
{
  flicamdata_t *cam;
  long rlen, wlen;
  unsigned short buf[8];

  cam = DEVICE->device_data;

  if ((hbin < 1) || (hbin > 16))
    return -EINVAL;

  rlen = 0; wlen = 6;
  buf[0] = htons(FLI_USBCAM_SETBINFACTORS);
  buf[1] = htons((unsigned short) hbin);
  buf[2] = htons((unsigned short) cam->vbin);
  IO(dev, buf, &wlen, &rlen);

  cam->hbin = hbin;
  cam->grabrowwidth =
    (cam->image_area.lr.x - cam->image_area.ul.x) / cam->hbin;

  return 0;
}

long fli_camera_usb_set_vbin(flidev_t dev, long vbin)
{
  flicamdata_t *cam;
  long rlen, wlen;
  unsigned short buf[8];

  cam = DEVICE->device_data;

  if ((vbin < 1) || (vbin > 16))
    return -EINVAL;

  rlen = 0; wlen = 6;
  buf[0] = htons(FLI_USBCAM_SETBINFACTORS);
  buf[1] = htons((unsigned short) cam->hbin);
  buf[2] = htons((unsigned short) vbin);
  IO(dev, buf, &wlen, &rlen);

  cam->vbin = vbin;

  return 0;
}

long fli_camera_usb_get_exposure_status(flidev_t dev, long *timeleft)
{
  long rlen, wlen;
  unsigned short buf[8];

  rlen = 4; wlen = 2;
  buf[0] = htons(FLI_USBCAM_EXPOSURESTATUS);
  IO(dev, buf, &wlen, &rlen);
  *timeleft = ntohl(((unsigned long *)((void *) buf))[0]);

  return 0;
}

long fli_camera_usb_set_temperature(flidev_t dev, double temperature)
{
  flicamdata_t *cam;
	unsigned short ad;
  long rlen, wlen;
  unsigned short buf[8];

	cam = DEVICE->device_data;

	if(DEVICE->devinfo.fwrev < 0x0200)
	{
		return 0;
	}

	if(cam->tempslope == 0.0)
	{
		ad = 255;
	}
	else
	{
		ad = (unsigned short) ((temperature - cam->tempintercept) / cam->tempslope);
	}

	debug(FLIDEBUG_INFO, "Temperature slope, intercept, AD val, %f %f %f %d", temperature, cam->tempslope, cam->tempintercept, ad);
	
  rlen = 0; wlen = 4;
  buf[0] = htons(FLI_USBCAM_TEMPERATURE);
	buf[1] = htons(ad);
  IO(dev, buf, &wlen, &rlen);

  return 0;
}

long fli_camera_usb_get_temperature(flidev_t dev, double *temperature)
{
  flicamdata_t *cam;
  long rlen, wlen;
  unsigned short buf[16];

  cam = DEVICE->device_data;

  rlen = 2; wlen = 2;
  buf[0] = htons(FLI_USBCAM_TEMPERATURE);
  IO(dev, buf, &wlen, &rlen);
  *temperature = cam->tempslope * (double)((ntohs(buf[0]) & 0x00ff)) +
    cam->tempintercept;

  return 0;
}

long fli_camera_usb_grab_row(flidev_t dev, void *buff, size_t width)
{
  flicamdata_t *cam;
  long x;
  long r;

  cam = DEVICE->device_data;

	if(width > (size_t) (cam->image_area.lr.x - cam->image_area.ul.x))
	{
		debug(FLIDEBUG_FAIL, "FLIGrabRow(), requested row too wide.");
		debug(FLIDEBUG_FAIL, "  Requested width: %d", width);
		debug(FLIDEBUG_FAIL, "  FLISetImageArea() width: %d",
			cam->image_area.lr.x - cam->image_area.ul.x);
		return -EINVAL;
	}

  if (cam->flushcountbeforefirstrow > 0)
  {
    if ((r = fli_camera_usb_flush_rows(dev, cam->flushcountbeforefirstrow, 1)))
      return r;

    cam->flushcountbeforefirstrow = 0;
  }

  if (cam->grabrowbufferindex >= cam->grabrowbatchsize)
  {
    /* We don't have the row in memory */
    long rlen, wlen;

    /* Do we have less than GrabRowBatchSize rows to grab? */
    if (cam->grabrowbatchsize > (cam->grabrowcounttot - cam->grabrowindex))
    {
      cam->grabrowbatchsize = cam->grabrowcounttot - cam->grabrowindex;
    }

    rlen = cam->grabrowwidth * 2 * cam->grabrowbatchsize;
    wlen = 6;
    cam->gbuf[0] = htons(FLI_USBCAM_SENDROW);
    cam->gbuf[1] = htons((unsigned short) cam->grabrowwidth);
    cam->gbuf[2] = htons((unsigned short) cam->grabrowbatchsize);
    IO(dev, cam->gbuf, &wlen, &rlen);

    for (x = 0; x < (cam->grabrowwidth * cam->grabrowbatchsize); x++)
    {
      if ((DEVICE->devinfo.hwrev & 0xff00) == 0x0100)
      {
				cam->gbuf[x] = ntohs(cam->gbuf[x]) + 32768;
      }
      else
      {
				cam->gbuf[x] = ntohs(cam->gbuf[x]);
      }
    }
    cam->grabrowbufferindex = 0;
  }

  for (x = 0; x < (long)width; x++)
  {
    ((unsigned short *)buff)[x] =
      cam->gbuf[x + (cam->grabrowbufferindex * cam->grabrowwidth)];
  }

  cam->grabrowbufferindex++;
  cam->grabrowindex++;

  if (cam->grabrowcount > 0)
  {
    cam->grabrowcount--;
    if (cam->grabrowcount == 0)
    {
      if ((r = fli_camera_usb_flush_rows(dev, cam->flushcountafterlastrow, 1)))
	return r;

      cam->flushcountafterlastrow = 0;
      cam->grabrowbatchsize = 1;
    }
  }

  return 0;
}

long fli_camera_usb_expose_frame(flidev_t dev)
{
  flicamdata_t *cam;
  short flags = 0;
  long rlen, wlen;
  unsigned short buf[16];

  cam = DEVICE->device_data;

  rlen = 0; wlen = 6;
  buf[0] = htons(FLI_USBCAM_SETFRAMEOFFSET);
  buf[1] = htons((unsigned short) cam->image_area.ul.x);
  buf[2] = htons((unsigned short) cam->image_area.ul.y);
  IO(dev, buf, &wlen, &rlen);

  rlen = 0; wlen = 6;
  buf[0] = htons(FLI_USBCAM_SETBINFACTORS);
  buf[1] = htons((unsigned short) cam->hbin);
  buf[2] = htons((unsigned short) cam->vbin);
  IO(dev, buf, &wlen, &rlen);

  rlen = 0; wlen = 6;
  buf[0] = htons(FLI_USBCAM_SETFLUSHBINFACTORS);
  buf[1] = htons((unsigned short) cam->hflushbin);
  buf[2] = htons((unsigned short) cam->vflushbin);
  IO(dev, buf, &wlen, &rlen);

  rlen = 0; wlen = 8;
  buf[0] = htons(FLI_USBCAM_SETEXPOSURE);
  ((unsigned long *)((void *) buf))[1] = htonl(cam->exposure);
  IO(dev, buf, &wlen, &rlen);

  /* What flags do we need to send... */
  /* Dark Frame */
  flags |= (cam->frametype == FLI_FRAME_TYPE_DARK) ? 0x01 : 0x00;
  /* External trigger */
  flags |= (cam->exttrigger != 0) ? 0x04 : 0x00;
  flags |= (cam->exttriggerpol != 0) ? 0x08 : 0x00;

	debug(FLIDEBUG_INFO, "Exposure flags: %04x", flags);

	debug(FLIDEBUG_INFO, "Flushing %d times.\n", cam->flushes);
  if (cam->flushes > 0)
  {
    long r;

    if ((r = fli_camera_usb_flush_rows(dev,
				       cam->ccd.array_area.lr.y - cam->ccd.array_area.ul.y,
				       cam->flushes)))
      return r;
  }

  rlen = 0; wlen = 4;
  buf[0] = htons(FLI_USBCAM_STARTEXPOSURE);
  buf[1] = htons((unsigned short) flags);
  IO(dev, buf, &wlen, &rlen);

  cam->grabrowcount = cam->image_area.lr.y - cam->image_area.ul.y;
  cam->grabrowcounttot = cam->grabrowcount;
  cam->grabrowwidth = cam->image_area.lr.x - cam->image_area.ul.x;
  cam->grabrowindex = 0;
  cam->grabrowbatchsize = USB_READ_SIZ_MAX / (cam->grabrowwidth * 2);

  /* Lets put some bounds on this... */
  if (cam->grabrowbatchsize > cam->grabrowcounttot)
    cam->grabrowbatchsize = cam->grabrowcounttot;

  if (cam->grabrowbatchsize > 64)
    cam->grabrowbatchsize = 64;

  /* We need to get a whole new buffer by default */
  cam->grabrowbufferindex = cam->grabrowbatchsize;

  cam->flushcountbeforefirstrow = cam->image_area.ul.y;
  cam->flushcountafterlastrow =
    (cam->ccd.array_area.lr.y - cam->ccd.array_area.ul.y) -
    ((cam->image_area.lr.y - cam->image_area.ul.y) * cam->vbin) -
    cam->image_area.ul.y;

  if (cam->flushcountbeforefirstrow < 0)
    cam->flushcountbeforefirstrow = 0;

  if (cam->flushcountafterlastrow < 0)
    cam->flushcountafterlastrow = 0;

  return 0;
}

long fli_camera_usb_flush_rows(flidev_t dev, long rows, long repeat)
{
  flicamdata_t *cam;
  long rlen, wlen;
  unsigned short buf[16];

  cam = DEVICE->device_data;

  if (rows < 0)
    return -EINVAL;

  if (rows == 0)
    return 0;

  rlen = 0; wlen = 6;
  buf[0] = htons(FLI_USBCAM_SETFLUSHBINFACTORS);
  buf[1] = htons((unsigned short) cam->hflushbin);
  buf[2] = htons((unsigned short) cam->vflushbin);
  IO(dev, buf, &wlen, &rlen);

  while (repeat > 0)
  {
    rlen = 0; wlen = 4;
    buf[0] = htons(FLI_USBCAM_FLUSHROWS);
    buf[1] = htons((unsigned short) rows);
    IO(dev, buf, &wlen, &rlen);
    repeat--;
  }

  return 0;
}

long fli_camera_usb_set_bit_depth(flidev_t dev, flibitdepth_t bitdepth)
{
  dev=dev; bitdepth=bitdepth;
  return -EINVAL;
}

long fli_camera_usb_read_ioport(flidev_t dev, long *ioportset)
{
  flicamdata_t *cam;
  long rlen, wlen;
  unsigned short buf[8];

  cam = DEVICE->device_data;

  rlen = 1; wlen = 2;
  buf[0] = htons(FLI_USBCAM_READIO);
  IO(dev, buf, &wlen, &rlen);
  *ioportset = ((unsigned char *)(void *)buf)[0];

  return 0;
}

long fli_camera_usb_write_ioport(flidev_t dev, long ioportset)
{
  flicamdata_t *cam;

  long rlen, wlen;
  unsigned short buf[8];

  cam = DEVICE->device_data;

  rlen = 0; wlen = 3;
  buf[0] = htons(FLI_USBCAM_WRITEIO);
  ((unsigned char *)(void *)buf)[2] = (char)ioportset;
  IO(dev, buf, &wlen, &rlen);

  return 0;
}

long fli_camera_usb_configure_ioport(flidev_t dev, long ioportset)
{
  flicamdata_t *cam;
  long rlen, wlen;
  unsigned short buf[8];

  cam = DEVICE->device_data;

  rlen = 0; wlen = 3;
  buf[0] = htons(FLI_USBCAM_WRITEDIR);
  ((unsigned char *)(void *)buf)[2] = (char)ioportset;
  IO(dev, buf, &wlen, &rlen);

  return 0;
}

long fli_camera_usb_control_shutter(flidev_t dev, long shutter)
{
  flicamdata_t *cam;
  long rlen, wlen;
  unsigned short buf[8];

  cam = DEVICE->device_data;

  rlen = 0; wlen = 3;
  buf[0] = htons(FLI_USBCAM_SHUTTER);
  ((unsigned char *)(void *)buf)[2] = (char)shutter;
  IO(dev, buf, &wlen, &rlen);

  return 0;
}

long fli_camera_usb_control_bgflush(flidev_t dev, long bgflush)
{
 flicamdata_t *cam;
 long rlen, wlen;
  unsigned short buf[8];

	cam = DEVICE->device_data;

	if(DEVICE->devinfo.fwrev < 0x0300)
	{
		debug(FLIDEBUG_WARN, "Background flush commanded on early firmware.");
		return -EFAULT;
	}

	if( (bgflush != FLI_BGFLUSH_STOP) &&
		  (bgflush != FLI_BGFLUSH_START) )
		return -EINVAL;

  rlen = 0; wlen = 4;
  buf[0] = htons(FLI_USBCAM_BGFLUSH);
	buf[1] = htons((unsigned short) bgflush);
  IO(dev, buf, &wlen, &rlen);

  return 0;
}
