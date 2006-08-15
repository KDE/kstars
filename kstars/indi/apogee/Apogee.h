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
#if !defined(AFX_APOGEE__INCLUDED_)
#define AFX_APOGEE__INCLUDED_


#define	Apn_Interface int
#define	Apn_Interface_NET 0
#define	Apn_Interface_USB 1

#define	Apn_NetworkMode int
#define	Apn_NetworkMode_Tcp	0
#define	Apn_NetworkMode_Udp	1

#define	Apn_Resolution int
#define	Apn_Resolution_SixteenBit 0
#define	Apn_Resolution_TwelveBit 1

#define	Apn_CameraMode int
#define	Apn_CameraMode_Normal 0
#define	Apn_CameraMode_TDI 1
#define	Apn_CameraMode_Test 2
#define	Apn_CameraMode_ExternalTrigger 3
#define	Apn_CameraMode_ExternalShutter 4

#define	Apn_Status int
#define	Apn_Status_DataError -2
#define	Apn_Status_PatternError	 -1
#define	Apn_Status_Idle	 0
#define	Apn_Status_Exposing  1
#define	Apn_Status_ImagingActive  2
#define	Apn_Status_ImageReady  3
#define	Apn_Status_Flushing  4
#define	Apn_Status_WaitingOnTrigger 5

#define	Apn_LedMode int
#define	Apn_LedMode_DisableAll 0
#define	Apn_LedMode_DisableWhileExpose 1
#define	Apn_LedMode_EnableAll 2

#define	Apn_LedState int
#define	Apn_LedState_Expose 0
#define	Apn_LedState_ImageActive 1
#define	Apn_LedState_Flushing 2
#define	Apn_LedState_ExtTriggerWaiting 3
#define	Apn_LedState_ExtTriggerReceived 4
#define	Apn_LedState_ExtShutterInput 5
#define	Apn_LedState_ExtStartReadout 6
#define	Apn_LedState_AtTemp 7

#define	Apn_CoolerStatus int
#define	Apn_CoolerStatus_Off 0
#define	Apn_CoolerStatus_RampingToSetPoint 1
#define	Apn_CoolerStatus_AtSetPoint 2
#define	Apn_CoolerStatus_Revision 3

#define	Apn_FanMode int
#define	Apn_FanMode_Off	0
#define	Apn_FanMode_Low 1
#define	Apn_FanMode_Medium 2
#define	Apn_FanMode_High 3


#define	Camera_Status int
#define	Camera_Status_Idle 0
#define	Camera_Status_Waiting 1
#define	Camera_Status_Exposing 2
#define	Camera_Status_Downloading 3
#define	Camera_Status_LineReady 4
#define	Camera_Status_ImageReady 5
#define	Camera_Status_Flushing 6

#define	Camera_CoolerStatus int
#define	Camera_CoolerStatus_Off	 0
#define	Camera_CoolerStatus_RampingToSetPoint 1
#define	Camera_CoolerStatus_Correcting 2
#define	Camera_CoolerStatus_RampingToAmbient 3
#define	Camera_CoolerStatus_AtAmbient 4
#define	Camera_CoolerStatus_AtMax 5
#define	Camera_CoolerStatus_AtMin 6 
#define	Camera_CoolerStatus_AtSetPoint 7

#define	Camera_CoolerMode int
#define	Camera_CoolerMode_Off 0
#define	Camera_CoolerMode_On  1
#define	Camera_CoolerMode_Shutdown 2

#endif



