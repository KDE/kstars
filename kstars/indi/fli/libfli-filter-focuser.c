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
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>

#include "libfli-libfli.h"
#include "libfli-mem.h"
#include "libfli-debug.h"
#include "libfli-filter-focuser.h"

/*
Array of filterwheel info
	Pos = # of filters
	Off = Offset of 0 filter from magnetic stop,
	X - y = number of steps from filter x to filter y
*/
static const wheeldata_t wheeldata[] =
{
  /* POS OFF   0-1 1-2 2-3 3-4 4-5 5-6 6-7 7-8 8-9 9-A A-B B-C C-D D-E F-F F-0 */
  { 0,  0, {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  { 1,  0, {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  { 2,  0, {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  { 3, 48, { 80, 80, 80, 80,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  { 4,  0, {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  { 5,  0, { 48, 48, 48, 48, 48,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  { 6,  0, {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  { 7, 14, { 34, 34, 35, 34, 34, 35, 35,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  { 8, 18, { 30, 30, 30, 30, 30, 30, 30, 30,  0,  0,  0,  0,  0,  0,  0,  0} },
  { 9,  0, {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  {10,  0, { 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,  0,  0,  0,  0,  0,  0} },
  {11,  0, {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  {12,  6,{ 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,  0,  0,  0,  0} },
  {13,  0, {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  {14,  0, {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  {15,  0, { 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48} },
};

static long fli_stepmotor(flidev_t dev, long steps);
static long fli_getsteppos(flidev_t dev, long *pos);
static long fli_setfilterpos(flidev_t dev, long pos);


long fli_filter_focuser_open(flidev_t dev)
{
#define FWSTRING "Filter Wheel (%ld position)"
#define FOCSTRING "Focuser"
#define MODEL_LEN (sizeof(FWSTRING) + 30)
  int err = 0;
  unsigned long ndev;
  long rlen, wlen;
  unsigned short buf[16];
  flifilterdata_t *fdata = NULL;

  CHKDEVICE(dev);

  DEVICE->io_timeout = 1000;

  wlen = 2;
  rlen = 2;
  buf[0] = htons(0x8000);
  IO(dev, buf, &wlen, &rlen);
  if (ntohs(buf[0]) != 0x8000)
  {
    debug(FLIDEBUG_WARN, "Invalid echo, device not recognized, got %04x instead of %04x.", ntohs(buf[0]), 0x8000);
    err = -ENODEV;
    goto done;
  }

  wlen = 2;
  rlen = 2;
  buf[0] = htons(0x8001);
  IO(dev, buf, &wlen, &rlen);
  DEVICE->devinfo.fwrev = ntohs(buf[0]);
  if ((DEVICE->devinfo.fwrev & 0xff00) != 0x8000)
  {
    debug(FLIDEBUG_WARN, "Invalid echo, device not recognized.");
    err = -ENODEV;
    goto done;
  }

  if ((DEVICE->device_data = xmalloc(sizeof(flifilterdata_t))) == NULL)
  {
    err = -ENOMEM;
    goto done;
  }
  fdata = DEVICE->device_data;
  fdata->numslots = 0;
  fdata->stepspersec = 100;
  fdata->currentslot = -1;

  if (DEVICE->devinfo.fwrev == 0x8001)  /* Old level of firmware */
  {
    if (DEVICE->devinfo.type != FLIDEVICE_FILTERWHEEL)
    {
      err = -ENODEV;
      goto done;
    }

    debug(FLIDEBUG_INFO, "Device is old fashioned filter wheel.");
    fdata->numslots = 5;

    /* FIX: should model info be set first? */
    return 0;
  }

  debug(FLIDEBUG_INFO, "New version of hardware found.");
  wlen = 2;
  rlen = 2;
  buf[0] = htons(0x8002);
  IO(dev, buf, &wlen, &rlen);
  ndev = ntohs(buf[0]);

  if ((ndev & 0xff00) != 0x8000)
  {
    err = -ENODEV;
    goto done;
  }

  if ((DEVICE->devinfo.model = (char *)xmalloc(MODEL_LEN)) == NULL)
  {
    debug(FLIDEBUG_FAIL, "Could not allocate memory for model information.");
    err = -ENOMEM;
    goto done;
  }

  ndev &= 0x00ff;

  /* switch based on the jumper settings on the filter/focuser dongle, determines how many slots in the filter wheel */
  switch (ndev)
  {
	case 0x00:
	if (DEVICE->devinfo.type != FLIDEVICE_FILTERWHEEL)
	{
		err = -ENODEV;
		goto done;
	}
	fdata->numslots = 5;
	snprintf(DEVICE->devinfo.model, MODEL_LEN, FWSTRING, fdata->numslots);
    break;

  case 0x01:
    if (DEVICE->devinfo.type != FLIDEVICE_FILTERWHEEL)
    {
      err = -ENODEV;
      goto done;
    }
    fdata->numslots = 3;
    snprintf(DEVICE->devinfo.model, MODEL_LEN, FWSTRING, fdata->numslots);
    break;

  case 0x02:
    if (DEVICE->devinfo.type != FLIDEVICE_FILTERWHEEL)
    {
      err = -ENODEV;
      goto done;
    }
    fdata->numslots = 7;
    snprintf(DEVICE->devinfo.model, MODEL_LEN, FWSTRING, fdata->numslots);
    break;

  case 0x03:
    fdata->numslots = 8;
    if (DEVICE->devinfo.type != FLIDEVICE_FILTERWHEEL)
    {
      err = -ENODEV;
      goto done;
    }
    snprintf(DEVICE->devinfo.model, MODEL_LEN, FWSTRING, fdata->numslots);
    break;

  case 0x04:
    fdata->numslots = 15;
		fdata->stepspersec= 16;/* // 1/.06 */
		if (DEVICE->devinfo.type != FLIDEVICE_FILTERWHEEL)
    {
      err = -ENODEV;
      goto done;
    }
    snprintf(DEVICE->devinfo.model, MODEL_LEN, FWSTRING, fdata->numslots);
    break;


  case 0x05:
    if (DEVICE->devinfo.type != FLIDEVICE_FILTERWHEEL)
    {
      err = -ENODEV;
      goto done;
    }
    fdata->numslots = 12;
	fdata->stepspersec= 16; /*//   1/.06*/

    snprintf(DEVICE->devinfo.model, MODEL_LEN, FWSTRING, fdata->numslots);
    break;

  case 0x06:
    fdata->numslots = 10;
	fdata->stepspersec= 16; /*//   1/.06*/
    if (DEVICE->devinfo.type != FLIDEVICE_FILTERWHEEL)
	{
      err = -ENODEV;
      goto done;
    }
    snprintf(DEVICE->devinfo.model, MODEL_LEN, FWSTRING, fdata->numslots);
    break;


  case 0x07:
    if (DEVICE->devinfo.type != FLIDEVICE_FOCUSER)
    {
      err = -ENODEV;
      goto done;
    }
    snprintf(DEVICE->devinfo.model, MODEL_LEN, FOCSTRING);
    break;

  default:
    err = -ENODEV;
    goto done;
  }

 done:

  if (err)
  {
    if (DEVICE->devinfo.model != NULL)
    {
      xfree(DEVICE->devinfo.model);
      DEVICE->devinfo.model = NULL;
    }

    if (DEVICE->device_data != NULL)
    {
      xfree(DEVICE->device_data);
      DEVICE->device_data = NULL;
    }

    return err;
  }

  debug(FLIDEBUG_INFO, "Found a %ld slot filter wheel or a focuser.",
	fdata->numslots);

#undef FWSTRING
#undef FOCSTRING
#undef MODEL_LEN

  return 0;
}

long fli_filter_focuser_close(flidev_t dev)
{
  CHKDEVICE(dev);

  if (DEVICE->devinfo.model != NULL)
  {
    xfree(DEVICE->devinfo.model);
    DEVICE->devinfo.model = NULL;
  }

  if (DEVICE->device_data != NULL)
  {
    xfree(DEVICE->device_data);
    DEVICE->device_data = NULL;
  }

  return 0;
}

long fli_filter_command(flidev_t dev, int cmd, int argc, ...)
{
  flifilterdata_t *fdata;
  long r;
  va_list ap;

  va_start(ap, argc);
  CHKDEVICE(dev);
  fdata = DEVICE->device_data;

  switch (cmd)
  {
  case FLI_SET_FILTER_POS:
    if (argc != 1)
      r = -EINVAL;
    else
    {
      long pos;

      pos = *va_arg(ap, long *);
      r = fli_setfilterpos(dev, pos);
    }
    break;

  case FLI_GET_FILTER_POS:
    if (argc != 1)
      r = -EINVAL;
    else
    {
      long *cslot;

      cslot = va_arg(ap, long *);
      *cslot = fdata->currentslot;
      r = 0;
    }
    break;

  case FLI_GET_FILTER_COUNT:
    if (argc != 1)
      r = -EINVAL;
    else
    {
      long *nslots;

      nslots = va_arg(ap, long *);
      *nslots = fdata->numslots;
      r = 0;
    }
    break;

  case FLI_STEP_MOTOR:
    if (argc != 1)
      r = -EINVAL;
    else
    {
      long *steps;

      steps = va_arg(ap, long *);
      r = fli_stepmotor(dev, *steps);
    }
    break;

  case FLI_GET_STEPPER_POS:
    if (argc != 1)
      r = -EINVAL;
    else
    {
      long *pos;

      pos = va_arg(ap, long *);
      r = fli_getsteppos(dev, pos);
    }
    break;

  default:
    r = -EINVAL;
  }

  va_end(ap);

  return r;
}

long fli_focuser_command(flidev_t dev, int cmd, int argc, ...)
{
  long r;
  va_list ap;

  va_start(ap, argc);
  CHKDEVICE(dev);

  switch (cmd)
  {
  case FLI_STEP_MOTOR:
    if (argc != 1)
      r = -EINVAL;
    else
    {
      long *steps;

      steps = va_arg(ap, long *);
      r = fli_stepmotor(dev, *steps);
    }
    break;

  case FLI_GET_STEPPER_POS:
    if (argc != 1)
      r = -EINVAL;
    else
    {
      long *pos;

      pos = va_arg(ap, long *);
      r = fli_getsteppos(dev, pos);
    }
    break;

  case FLI_HOME_FOCUSER:
    if (argc != 0)
      r = -EINVAL;
    else
      r =  fli_setfilterpos(dev, FLI_FILTERPOSITION_HOME);
    break;

  default:
    r = -EINVAL;
  }

  va_end(ap);

  return r;
}

static long fli_stepmotor(flidev_t dev, long steps)
{
  flifilterdata_t *fdata;
  long dir, timeout, move, stepsleft;
  long rlen,wlen;
  unsigned short buf[16];
  clock_t begin;

  if (steps == 0)
    return 0;

  fdata = DEVICE->device_data;

  dir = steps;
  steps = abs(steps);
  while (steps > 0)
  {
    if (steps > 2048)
      move = 2048;
    else
      move = steps;

    debug(FLIDEBUG_INFO, "Stepping %d steps.", move);

    steps -= move;
    timeout = (move / fdata->stepspersec) + 2;

    rlen = 2;
    wlen = 2;
    if (dir < 0)
    {
      buf[0] = htons((unsigned short) (0xa000 | (unsigned short) move));
      IO(dev, buf, &wlen, &rlen);
      if ((ntohs(buf[0]) & 0xf000) != 0xa000)
      {
	debug(FLIDEBUG_WARN, "Invalid echo.");
	return -EIO;
      }
    }
    else
    {
      buf[0] = htons((unsigned short) (0x9000 | (unsigned short) move));
      IO(dev, buf, &wlen, &rlen);
      if ((ntohs(buf[0]) & 0xf000) != 0x9000)
      {
	debug(FLIDEBUG_WARN, "Invalid echo.");
	return -EIO;
      }
    }

    begin = clock();
    stepsleft = 0;
    while (stepsleft != 0x7000)
    {
      buf[0] = htons(0x7000);
      IO(dev, buf, &wlen, &rlen);
      stepsleft = ntohs(buf[0]);

      if (((clock() - begin) / CLOCKS_PER_SEC) > timeout)
      {
	debug(FLIDEBUG_WARN, "A device timeout has occurred.");
	return -EIO;
      }
    }
  }

  return 0;
}

static long fli_getsteppos(flidev_t dev, long *pos)
{
  long poslow, poshigh;
  long rlen, wlen;
  unsigned short buf[16];

  rlen = 2; wlen = 2;
  buf[0] = htons(0x6000);
  IO(dev, buf, &wlen, &rlen);
  poslow = ntohs(buf[0]);
  if ((poslow & 0xf000) != 0x6000)
    return -EIO;

  buf[0] = htons(0x6001);
  IO(dev, buf, &wlen, &rlen);
  poshigh = ntohs(buf[0]);
  if ((poshigh & 0xf000) != 0x6000)
    return -EIO;

  if ((poshigh & 0x0080) > 0)
  {
    *pos = ((~poslow) & 0xff) + 1;
    *pos += (256 * ((~poshigh) & 0xff));
    *pos = -(*pos);
  }
  else
  {
    *pos = (poslow & 0xff) + 256 * (poshigh & 0xff);
  }
  return 0;
}


static long fli_setfilterpos(flidev_t dev, long pos)
{
  flifilterdata_t *fdata;
  long rlen, wlen;
  unsigned short buf[16];
  long move, i, steps;

  fdata = DEVICE->device_data;

  if (pos == FLI_FILTERPOSITION_HOME)
    fdata->currentslot = FLI_FILTERPOSITION_HOME;

  if (fdata->currentslot < 0)
  {
    debug(FLIDEBUG_INFO, "Home filter wheel/focuser.");
	/*	//set the timeout*/
    DEVICE->io_timeout = (DEVICE->devinfo.type == FLIDEVICE_FILTERWHEEL ? 5000 : 30000);
		/*//10,12,15 pos filterwheel  needs a longer timeout t*/
		if(fdata->numslots == 12||fdata->numslots == 10)
		{
			DEVICE->io_timeout = 120000;
		}
		if(fdata->numslots == 15)
		{
			DEVICE->io_timeout = 200000;
		}

    wlen = 2;
    rlen = 2;
    buf[0] = htons(0xf000);
    IO(dev, buf, &wlen, &rlen);
    if (ntohs(buf[0]) != 0xf000)
      return -EIO;

    DEVICE->io_timeout = 1000;

    debug(FLIDEBUG_INFO, "Moving %d steps to home position.",
		wheeldata[fdata->numslots].n_offset);

    COMMAND(fli_stepmotor(dev, - (wheeldata[fdata->numslots].n_offset)));
    fdata->currentslot = 0;
  }

  if (pos == FLI_FILTERPOSITION_HOME)
    return 0;

  if (pos >= fdata->numslots)
  {
    debug(FLIDEBUG_WARN, "Requested slot (%d) exceeds number of slots.", pos);
    return -EINVAL;
  }

  if (pos == fdata->currentslot)
    return 0;

  move = pos - fdata->currentslot;

  if (move < 0)
    move += fdata->numslots;

  steps = 0;
  for (i=0; i < move; i++)
    steps += wheeldata[fdata->numslots].n_steps[i % fdata->numslots];

  debug(FLIDEBUG_INFO, "Move filter wheel %d steps.", steps);

  COMMAND(fli_stepmotor(dev, - (steps)));
  fdata->currentslot = pos;

  return 0;
}
