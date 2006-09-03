#if 0
   True Technology Filter Wheel
    Copyright (C) 2006 Jasem Mutlaq (mutlaqja AT ikarustech DOT com)

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "indidevapi.h"
#include "eventloop.h"
#include "indicom.h"

void ISInit(void);
void getBasicData(void);
void ISPoll(void *);
void handleExposure(void *);
void connectFilter(void);
int  manageDefaults(char errmsg[]);
int  checkPowerS(ISwitchVectorProperty *sp);
int  checkPowerN(INumberVectorProperty *np);
int  checkPowerT(ITextVectorProperty *tp);
int  getOnSwitch(ISwitchVectorProperty *sp);
int  isFilterConnected(void);

static int targetFilter;
static int fd;
static char COMM_INIT = 0xA5;
static char COMM_FILL = 0x20;
static char COMM_NL   = 0x0A; 

#define mydev           		"TurTech Wheel"
#define MAIN_GROUP			"Main Control"
#define currentFilter			FilterPositionN[0].value
#define POLLMS				1000
#define MAX_FILTER_COUNT		8
#define ERRMSG_SIZE			1024

#define CMD_SIZE			15
#define FILTER_TIMEOUT			15					/* 15 Seconds before timeout */
#define FIRST_FILTER			1
#define LAST_FILTER			6


/*INDI controls */

/* Connect/Disconnect */
static ISwitch PowerS[]          	= {{"CONNECT" , "Connect" , ISS_OFF, 0, 0},{"DISCONNECT", "Disconnect", ISS_ON, 0, 0}};
static ISwitchVectorProperty PowerSP	= { mydev, "CONNECTION" , "Connection", MAIN_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, PowerS, NARRAY(PowerS), "", 0};

/* Connection Port */
static IText PortT[]			= {{"PORT", "Port", 0, 0, 0, 0}};
static ITextVectorProperty PortTP		= { mydev, "DEVICE_PORT", "Ports", MAIN_GROUP, IP_RW, 0, IPS_IDLE, PortT, NARRAY(PortT), "", 0};

/* Home/Learn Swtich */
static ISwitch HomeS[]          	= {{"Find" , "" , ISS_OFF, 0, 0}};
static ISwitchVectorProperty HomeSP	= { mydev, "HOME" , "", MAIN_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, HomeS, NARRAY(HomeS), "", 0};

/* Filter Position */
static INumber FilterPositionN[]	  = { {"SLOT", "Active Filter", "%2.0f",  0 , MAX_FILTER_COUNT, 1, 0, 0, 0, 0}};
static INumberVectorProperty FilterPositionNP = { mydev, "FILTER_SLOT", "Filter", MAIN_GROUP, IP_RW, 0, IPS_IDLE, FilterPositionN, NARRAY(FilterPositionN), "", 0};

/* Filter Size */
static INumber FilterSizeN[]	  = { {"Size", "", "%2.0f", 0,  12, 1, 0, 0, 0, 0}};
static INumberVectorProperty FilterSizeNP = { mydev, "Filter Size", "", MAIN_GROUP, IP_RO, 0, IPS_IDLE, FilterSizeN, NARRAY(FilterSizeN), "", 0};

/* send client definitions of all properties */
void ISInit()
{
	static int isInit=0;
	
	if (isInit)
		return;
	

	targetFilter = -1;
	fd = -1;

        IEAddTimer (POLLMS, ISPoll, NULL);
	
	isInit = 1; 
}

void ISGetProperties (const char *dev)
{ 

	ISInit();
	
	if (dev && strcmp (mydev, dev))
		return;
	
	/* Main Control */
	IDDefSwitch(&PowerSP, NULL);
	IDDefText(&PortTP, NULL);
	IDDefSwitch(&HomeSP, NULL);
	IDDefNumber(&FilterPositionNP, NULL);
	IDDefNumber(&FilterSizeNP, NULL);
	
}
  
void ISNewBLOB (const char *dev, const char *name, int sizes[], char *blobs[], char *formats[], char *names[], int n)
{
  dev=dev;name=name;sizes=sizes;blobs=blobs;formats=formats;names=names;n=n;
}

void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	int err;
	char error_message[ERRMSG_SIZE];
	char cmd[CMD_SIZE];
	char cmd_response[CMD_SIZE];

	/* ignore if not ours */
	if (dev && strcmp (dev, mydev))
		return;
	    
	ISInit();
	    
	/* Connection */
	if (!strcmp (name, PowerSP.name))
	{
		IUResetSwitches(&PowerSP);
		IUUpdateSwitches(&PowerSP, states, names, n);
		connectFilter();
		return;
	}

	/* Home Search */
	if (!strcmp (name, HomeSP.name))
	{
		int nbytes=0;
		char type = 0x03;
		char chksum = COMM_INIT + COMM_FILL + type;

		if (checkPowerS(&HomeSP))
			return;

		tty_write(fd, &COMM_INIT, 1, &nbytes);
		tty_write(fd, &COMM_FILL, 1, &nbytes);
		tty_write(fd, &type, 1, &nbytes);
		err = tty_write(fd, &chksum, 1, &nbytes);
		
		/* Send Home Command */
		if (err != TTY_OK)
		{
		
			tty_error_msg(err, error_message, ERRMSG_SIZE);
			
			HomeSP.s = IPS_ALERT;
			IDSetSwitch(&HomeSP, "Sending command %s to filter failed. %s", cmd, error_message);
			IDLog("Sending command %s to filter failed. %s\n", cmd, error_message);

			return;
		}

		/* Wait for Reply */
		if ( (err = tty_read_section(fd, cmd_response, COMM_NL, FILTER_TIMEOUT, &nbytes)) != TTY_OK)
		{
			tty_error_msg(err, error_message, ERRMSG_SIZE);
			
			HomeSP.s = IPS_ALERT;
			IDSetSwitch(&HomeSP, "Reading from filter failed. %s\n", error_message);
			IDLog("Reading from filter failed. %s\n", error_message);
			return;
		}

		cmd_response[nbytes] = 0;
		HomeSP.s = IPS_OK;
		IDLog("Filter response is %s\n", cmd_response);
		IDSetSwitch(&HomeSP, "Filter response is %s", cmd_response);
		
	}
	
     
}

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	ISInit();
	
	/* ignore if not ours */ 
	if (dev && strcmp (mydev, dev))
		return;

	if (!strcmp(name, PortTP.name))
	{
		if (IUUpdateTexts(&PortTP, texts, names, n))
		  return;

		PortTP.s = IPS_OK;
		IDSetText(&PortTP, NULL);
	}
	
}


void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	long err;
	INumber *np;
	long newFilter;

	n = n;
	
	/* ignore if not ours */
	if (dev && strcmp (dev, mydev))
	    return;
	    
	ISInit();
	
	if (!strcmp(FilterPositionNP.name, name)) 
	{
		if (!isFilterConnected()) 
		{
			IDMessage(mydev, "Filter not connected.");
			FilterPositionNP.s = IPS_IDLE;
			IDSetNumber(&FilterPositionNP, NULL);
			return;
		}
		
		targetFilter = values[0];
		
		if (targetFilter < 1 || targetFilter > MAX_FILTER_COUNT)
		{
			FilterPositionNP.s = IPS_ALERT;
			IDSetNumber(&FilterPositionNP, "Error: valid range of filter is from %d to %d", FIRST_FILTER, LAST_FILTER);
			return;
		}

		FilterPositionNP.s = IPS_BUSY;
		IDSetNumber(&FilterPositionNP, "Setting current filter to slot %d", targetFilter);
		IDLog("Setting current filter to slot %d\n", targetFilter);
		
		return;
	}
}

void ISPoll(void *p)
{

  switch (FilterPositionNP.s)
  {
    case IPS_IDLE:
    case IPS_OK:
       break;
  
   
   case IPS_BUSY:
	break;

   case IPS_ALERT:
    break;
 }

   IEAddTimer (POLLMS, ISPoll, NULL);

}


int getOnSwitch(ISwitchVectorProperty *sp)
{
  int i=0;
 for (i=0; i < sp->nsp ; i++)
 {
     if (sp->sp[i].s == ISS_ON)
      return i;
 }

 return -1;

}

int checkPowerS(ISwitchVectorProperty *sp)
{

  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(sp->label, ""))
    	IDMessage (mydev, "Cannot change property %s while the wheel is offline.", sp->name);
    else
    	IDMessage (mydev, "Cannot change property %s while the wheel is offline.", sp->label);
	
    sp->s = IPS_IDLE;
    IDSetSwitch(sp, NULL);
    return -1;
  }

  return 0;
}

int checkPowerN(INumberVectorProperty *np)
{
  if (PowerSP.s != IPS_OK)
  {
     if (!strcmp(np->label, ""))
    	IDMessage (mydev, "Cannot change property %s while the wheel is offline.", np->name);
    else
    	IDMessage (mydev, "Cannot change property %s while the wheel is offline.", np->label);
    
    np->s = IPS_IDLE;
    IDSetNumber(np, NULL);
    return -1;
  }

  return 0;
}

int checkPowerT(ITextVectorProperty *tp)
{
  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(tp->label, ""))
    	IDMessage (mydev, "Cannot change property %s while the wheel is offline.", tp->name);
    else
    	IDMessage (mydev, "Cannot change property %s while the wheel is offline.", tp->label);
	
    tp->s = IPS_IDLE;
    IDSetText(tp, NULL);
    return -1;
  }

  return 0;

}

void connectFilter()
{
	int err;
	char errmsg[ERRMSG_SIZE];
 
	switch (PowerS[0].s)
	{
		case ISS_ON:
			if ( (err = tty_connect(PortT[0].text, NULL, &fd)) != TTY_OK)
			{
				PowerSP.s = IPS_ALERT;
				IDSetSwitch(&PowerSP, "Error: cannot connect to %s. Make sure the filter is connected and you have access to the port.", PortT[0].text);
				tty_error_msg(err, errmsg, ERRMSG_SIZE);
				IDLog("Error: %s\n", errmsg);
				return;
			}

			PowerSP.s = IPS_OK;
			IDSetSwitch(&PowerSP, "Filter Wheel is online.");
			break;
		
		case ISS_OFF:
			if ( (err = tty_disconnect(fd)) != TTY_OK)
			{
				PowerSP.s = IPS_ALERT;
				IDSetSwitch(&PowerSP, "Error: cannot disconnect.");
				tty_error_msg(err, errmsg, ERRMSG_SIZE);
				IDLog("Error: %s\n", errmsg);
				return;
			}

			PowerSP.s = IPS_IDLE;
			IDSetSwitch(&PowerSP, "Filter Wheel is offline.");
			break;
	}
}

/* isFilterConnected: return 1 if we have a connection, 0 otherwise */
int isFilterConnected(void)
{
   return ((PowerS[0].s == ISS_ON) ? 1 : 0);
}

