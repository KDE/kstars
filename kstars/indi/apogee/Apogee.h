/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
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

#define	Apn_tqStatus int
#define	Apn_tqStatus_DataError -2
#define	Apn_tqStatus_PatternError	 -1
#define	Apn_tqStatus_Idle	 0
#define	Apn_tqStatus_Exposing  1
#define	Apn_tqStatus_ImagingActive  2
#define	Apn_tqStatus_ImageReady  3
#define	Apn_tqStatus_Flushing  4
#define	Apn_tqStatus_WaitingOnTrigger 5

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

#define	Apn_CoolertqStatus int
#define	Apn_CoolertqStatus_Off 0
#define	Apn_CoolertqStatus_RampingToSetPoint 1
#define	Apn_CoolertqStatus_AtSetPoint 2
#define	Apn_CoolertqStatus_Revision 3

#define	Apn_FanMode int
#define	Apn_FanMode_Off	0
#define	Apn_FanMode_Low 1
#define	Apn_FanMode_Medium 2
#define	Apn_FanMode_High 3


#define	Camera_tqStatus int
#define	Camera_tqStatus_Idle 0
#define	Camera_tqStatus_Waiting 1
#define	Camera_tqStatus_Exposing 2
#define	Camera_tqStatus_Downloading 3
#define	Camera_tqStatus_LineReady 4
#define	Camera_tqStatus_ImageReady 5
#define	Camera_tqStatus_Flushing 6

#define	Camera_CoolertqStatus int
#define	Camera_CoolertqStatus_Off	 0
#define	Camera_CoolertqStatus_RampingToSetPoint 1
#define	Camera_CoolertqStatus_Correcting 2
#define	Camera_CoolertqStatus_RampingToAmbient 3
#define	Camera_CoolertqStatus_AtAmbient 4
#define	Camera_CoolertqStatus_AtMax 5
#define	Camera_CoolertqStatus_AtMin 6 
#define	Camera_CoolertqStatus_AtSetPoint 7

#define	Camera_CoolerMode int
#define	Camera_CoolerMode_Off 0
#define	Camera_CoolerMode_On  1
#define	Camera_CoolerMode_Shutdown 2

#endif



