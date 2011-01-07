// ApnCamera.cpp: implementation of the CApnCamera class.
//
// Copyright (c) 2003, 2004 Apogee Instruments, Inc.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "ApnCamera.h"
#include "ApnCamTable.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CApnCamera::CApnCamera()
{
	m_ApnSensorInfo = NULL;
}

CApnCamera::~CApnCamera()
{
	if ( m_ApnSensorInfo != NULL )
	{
		delete m_ApnSensorInfo;
		m_ApnSensorInfo = NULL;
	}
	CloseDriver();
}

bool CApnCamera::Expose( double Duration, bool Light )
{
	ULONG			ExpTime;
	unsigned short	BitsPerPixel(0);
	unsigned short	UnbinnedRoiX;
	unsigned short	UnbinnedRoiY;		
	unsigned short	PreRoiSkip, PostRoiSkip;
	unsigned short	PreRoiRows, PostRoiRows;
	unsigned short	PreRoiVBinning, PostRoiVBinning;

	unsigned short	RoiRegBuffer[12];
	unsigned short	RoiRegData[12];

	unsigned short	TotalHPixels;
	unsigned short	TotalVPixels;


	while ( read_ImagingStatus() != Apn_Status_Flushing )
	{
		Sleep( 20 );
	}

	// Validate the "Duration" parameter
	if ( Duration < APN_EXPOSURE_TIME_MIN )
		Duration = APN_EXPOSURE_TIME_MIN;

	// Validate the ROI params
	UnbinnedRoiX	= m_RoiPixelsH * m_RoiBinningH;

	PreRoiSkip		= m_RoiStartX;

	PostRoiSkip		= m_ApnSensorInfo->m_TotalColumns -
					  m_ApnSensorInfo->m_ClampColumns -
					  PreRoiSkip - 
					  UnbinnedRoiX;

	TotalHPixels = UnbinnedRoiX + PreRoiSkip + PostRoiSkip + m_ApnSensorInfo->m_ClampColumns;

	if ( TotalHPixels != m_ApnSensorInfo->m_TotalColumns )
		return false;

	UnbinnedRoiY	= m_RoiPixelsV * m_RoiBinningV;

	PreRoiRows		= m_ApnSensorInfo->m_UnderscanRows + 
					  m_RoiStartY;
	
	PostRoiRows		= m_ApnSensorInfo->m_TotalRows -
					  PreRoiRows -
					  UnbinnedRoiY;

	TotalVPixels = UnbinnedRoiY + PreRoiRows + PostRoiRows;

	if ( TotalVPixels != m_ApnSensorInfo->m_TotalRows )
		return false;

	// Calculate the exposure time to program to the camera
	ExpTime = ((unsigned long)(Duration / APN_TIMER_RESOLUTION)) + APN_TIMER_OFFSET_COUNT;

	Write( FPGA_REG_TIMER_LOWER, (unsigned short)(ExpTime & 0xFFFF));
	ExpTime = ExpTime >> 16;
	Write( FPGA_REG_TIMER_UPPER, (unsigned short)(ExpTime & 0xFFFF));

	m_pvtExposurePixelsV = m_RoiPixelsV;
	m_pvtExposurePixelsH = m_RoiPixelsH;

	if ( m_DataBits == Apn_Resolution_SixteenBit )
	{
		BitsPerPixel = 16;
	}
	else if ( m_DataBits == Apn_Resolution_TwelveBit )
	{
		BitsPerPixel = 12;
	}

	if ( PreStartExpose( BitsPerPixel ) != 0 )
	{
		return false;
	}

	// Calculate the vertical parameters
	PreRoiVBinning	= m_ApnSensorInfo->m_RowOffsetBinning;

	PostRoiVBinning	= 1;

	// For interline CCDs, set "Fast Dump" mode if the particular array is NOT digitized
	if ( m_ApnSensorInfo->m_InterlineCCD )
	{
		// use the fast dump feature to get rid of the data quickly.
		// one row, binning to the original row count
		// note that we only are not digitized in arrays 1 and 3
		PreRoiVBinning	= PreRoiRows;
		PostRoiVBinning	= PostRoiRows;

		PreRoiVBinning	|= FPGA_BIT_ARRAY_FASTDUMP;
		PostRoiVBinning	|= FPGA_BIT_ARRAY_FASTDUMP;

		PreRoiRows	= 1;
		PostRoiRows	= 1;
	}

        // Set up the geometry for a full frame device
        if ( m_ApnSensorInfo->m_EnableSingleRowOffset )
        {
                PreRoiVBinning  += PreRoiRows;
                PostRoiVBinning = PostRoiRows;
                                                                            
                PreRoiVBinning  |= FPGA_BIT_ARRAY_FASTDUMP;
                PostRoiVBinning |= FPGA_BIT_ARRAY_FASTDUMP;
                                                                            
                PreRoiRows      = 1;
                PostRoiRows     = 1;
        }
                                                                            

	// Issue the reset
	RoiRegBuffer[0] = FPGA_REG_COMMAND_B;
	RoiRegData[0]	= FPGA_BIT_CMD_RESET;

	// Program the horizontal settings
	RoiRegBuffer[1]	= FPGA_REG_PREROI_SKIP_COUNT;
	RoiRegData[1]	= PreRoiSkip;

	RoiRegBuffer[2]	= FPGA_REG_ROI_COUNT;
	// Number of ROI pixels.  Adjust the 12bit operation here to account for an extra 
	// 10 pixel shift as a result of the A/D conversion.
	if ( m_DataBits == Apn_Resolution_SixteenBit )
	{
		RoiRegData[2]	= m_pvtExposurePixelsH + 1;
	}
	else if ( m_DataBits == Apn_Resolution_TwelveBit )
	{
		RoiRegData[2]	= m_pvtExposurePixelsH + 10;
	}

	RoiRegBuffer[3]	= FPGA_REG_POSTROI_SKIP_COUNT;
	RoiRegData[3]	= PostRoiSkip;

	// Program the vertical settings
	RoiRegBuffer[4] = FPGA_REG_A1_ROW_COUNT;
	RoiRegData[4]	= PreRoiRows;
	RoiRegBuffer[5] = FPGA_REG_A1_VBINNING;
	RoiRegData[5]	= PreRoiVBinning;

	RoiRegBuffer[6] = FPGA_REG_A2_ROW_COUNT;
	RoiRegData[6]	= m_RoiPixelsV;
	RoiRegBuffer[7] = FPGA_REG_A2_VBINNING;
	RoiRegData[7]	= (m_RoiBinningV | FPGA_BIT_ARRAY_DIGITIZE);

	RoiRegBuffer[8] = FPGA_REG_A3_ROW_COUNT;
	RoiRegData[8]	= PostRoiRows;
	RoiRegBuffer[9] = FPGA_REG_A3_VBINNING;
	RoiRegData[9]	= PostRoiVBinning;

	// Issue the reset
	RoiRegBuffer[10]	= FPGA_REG_COMMAND_B;
	RoiRegData[10]		= FPGA_BIT_CMD_RESET;

	switch ( m_pvtCameraMode )
	{
	case Apn_CameraMode_Normal:
		if ( Light )
		{
			RoiRegBuffer[11]	= FPGA_REG_COMMAND_A;
			RoiRegData[11]		= FPGA_BIT_CMD_EXPOSE;
		}
		else
		{
			RoiRegBuffer[11]	= FPGA_REG_COMMAND_A;
			RoiRegData[11]		= FPGA_BIT_CMD_DARK;
		}
		break;
	case Apn_CameraMode_TDI:
		RoiRegBuffer[11]		= FPGA_REG_COMMAND_A;
		RoiRegData[11]			= FPGA_BIT_CMD_TDI;
		break;
	case Apn_CameraMode_Test:
		if ( Light )
		{
			RoiRegBuffer[11]	= FPGA_REG_COMMAND_A;
			RoiRegData[11]		= FPGA_BIT_CMD_EXPOSE;
		}
		else
		{
			RoiRegBuffer[11]	= FPGA_REG_COMMAND_A;
			RoiRegData[11]		= FPGA_BIT_CMD_DARK;
		}
		break;
	case Apn_CameraMode_ExternalTrigger:
		RoiRegBuffer[11]		= FPGA_REG_COMMAND_A;
		RoiRegData[11]			= FPGA_BIT_CMD_TRIGGER_EXPOSE;
		break;
	case Apn_CameraMode_ExternalShutter:
		break;
	}

	// Send the instruction sequence to the camera
	WriteMultiMRMD( RoiRegBuffer, RoiRegData, 12 );

	m_pvtImageInProgress = true;
	m_pvtImageReady		 = false;

	return true;
}

bool CApnCamera::ResetSystem()
{
	// Reset the camera engine
	Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_RESET );

	// Start flushing
	Write( FPGA_REG_COMMAND_A, FPGA_BIT_CMD_FLUSH );

	return true;
}

bool CApnCamera::PauseTimer( bool PauseState )
{
	unsigned short RegVal;
	bool		   CurrentState;

	Read( FPGA_REG_OP_A, RegVal );

	CurrentState = ( RegVal & FPGA_BIT_PAUSE_TIMER ) == FPGA_BIT_PAUSE_TIMER;

	if ( CurrentState != PauseState )
	{
		if ( PauseState )
		{
			RegVal |= FPGA_BIT_PAUSE_TIMER;
		}
		else
		{
			RegVal &= ~FPGA_BIT_PAUSE_TIMER;
		}
		Write ( FPGA_REG_OP_A, RegVal );
	}

	return true;
}

bool CApnCamera::StopExposure( bool DigitizeData )
{
	if ( m_pvtImageInProgress )
	{
		Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_END_EXPOSURE );

		if ( PostStopExposure( DigitizeData ) != 0 )
		{
			return false;
		}
	}

	return true;
}
                                                                           

unsigned short CApnCamera::GetExposurePixelsH()
{
	return m_pvtExposurePixelsH;
}

unsigned short CApnCamera::GetExposurePixelsV()
{
	return m_pvtExposurePixelsV;
}

double CApnCamera::read_InputVoltage()
{
	UpdateGeneralStatus();

	return m_pvtInputVoltage;
}

long CApnCamera::read_AvailableMemory()
{
        long AvailableMemory(0);

	switch( m_CameraInterface )
	{
	case Apn_Interface_NET:
		AvailableMemory = 28 * 1024;
		break;
	case Apn_Interface_USB:
		AvailableMemory = 32 * 1024;
		break;
	default:
		break;
	}

	return AvailableMemory;
}

unsigned short CApnCamera::read_FirmwareVersion()
{
	unsigned short FirmwareVersion;
	Read( FPGA_REG_FIRMWARE_REV, FirmwareVersion );
	return FirmwareVersion;
}

bool CApnCamera::read_ShutterState()
{
	UpdateGeneralStatus();

	return m_pvtShutterState;
}

bool CApnCamera::read_DisableShutter()
{
	unsigned short RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	return ( (RegVal & FPGA_BIT_DISABLE_SHUTTER) != 0 );
}

void CApnCamera::write_DisableShutter( bool DisableShutter )
{
	unsigned short RegVal;
	Read( FPGA_REG_OP_A, RegVal );

	if ( DisableShutter )
		RegVal |= FPGA_BIT_DISABLE_SHUTTER;
	else
		RegVal &= ~FPGA_BIT_DISABLE_SHUTTER;

	Write( FPGA_REG_OP_A, RegVal );
}

bool CApnCamera::read_ForceShutterOpen()
{
	unsigned short RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	return ( (RegVal & FPGA_BIT_FORCE_SHUTTER) != 0 );
}

void CApnCamera::write_ForceShutterOpen( bool ForceShutterOpen )
{
	unsigned short RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	
	if ( ForceShutterOpen )
		RegVal |= FPGA_BIT_FORCE_SHUTTER;
	else 
		RegVal &= ~FPGA_BIT_FORCE_SHUTTER;

	Write( FPGA_REG_OP_A, RegVal );
}

bool CApnCamera::read_ShutterAmpControl()
{
	unsigned short RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	return ( (RegVal & FPGA_BIT_SHUTTER_AMP_CONTROL ) != 0 );
}

void CApnCamera::write_ShutterAmpControl( bool ShutterAmpControl )
{
	unsigned short RegVal;
	Read( FPGA_REG_OP_A, RegVal );

	if ( ShutterAmpControl )
		RegVal |= FPGA_BIT_SHUTTER_AMP_CONTROL;
	else
		RegVal &= ~FPGA_BIT_SHUTTER_AMP_CONTROL;

	Write( FPGA_REG_OP_A, RegVal );
}

bool CApnCamera::read_ExternalIoReadout()
{
	unsigned short	RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	return ( (RegVal & FPGA_BIT_SHUTTER_MODE) != 0 );
}

void CApnCamera::write_ExternalIoReadout( bool ExternalIoReadout )
{
	unsigned short	RegVal;
	Read( FPGA_REG_OP_A, RegVal );

	if ( ExternalIoReadout )
		RegVal |= FPGA_BIT_SHUTTER_MODE;
	else
		RegVal &= ~FPGA_BIT_SHUTTER_MODE;

	Write( FPGA_REG_OP_A, RegVal );
}

bool CApnCamera::read_FastSequence()
{
	if ( m_ApnSensorInfo->m_InterlineCCD == false )
		return false;

	unsigned short	RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	return ( (RegVal & FPGA_BIT_RATIO) != 0 );
}

void CApnCamera::write_FastSequence( bool FastSequence )
{
	if ( m_ApnSensorInfo->m_InterlineCCD == false )
		return;

	unsigned short	RegVal;
	Read( FPGA_REG_OP_A, RegVal );

	if ( FastSequence )
	{
		RegVal |= FPGA_BIT_RATIO;
		Write( FPGA_REG_SHUTTER_CLOSE_DELAY, 0x0 );
	}
	else
	{
		RegVal &= ~FPGA_BIT_RATIO;
	}

	Write( FPGA_REG_OP_A, RegVal );
}

Apn_NetworkMode CApnCamera::read_NetworkTransferMode()
{
	return m_pvtNetworkTransferMode;
}

void CApnCamera::write_NetworkTransferMode( Apn_NetworkMode TransferMode )
{
	SetNetworkTransferMode( TransferMode );

	m_pvtNetworkTransferMode = TransferMode;
}

Apn_CameraMode CApnCamera::read_CameraMode()
{
	return m_pvtCameraMode;
}

void CApnCamera::write_CameraMode( Apn_CameraMode CameraMode )
{
	unsigned short RegVal;


	// If our state isn't going to change, do nothing
	if ( m_pvtCameraMode == CameraMode )
		return;

	// If we didn't return already, then if we know our state is going to
	// change.  If we're in test mode, clear the appropriate FPGA bit(s).
	switch( m_pvtCameraMode )
	{
	case Apn_CameraMode_Normal:
		break;
	case Apn_CameraMode_TDI:
		break;
	case Apn_CameraMode_Test:
		Read( FPGA_REG_OP_B, RegVal );
		RegVal &= ~FPGA_BIT_AD_SIMULATION;
		Write( FPGA_REG_OP_B, RegVal );
		break;
	case Apn_CameraMode_ExternalTrigger:
		break;
	case Apn_CameraMode_ExternalShutter:
		Read( FPGA_REG_OP_A, RegVal );
		RegVal &= ~FPGA_BIT_SHUTTER_SOURCE;
		Write( FPGA_REG_OP_A, RegVal );
		break;
	}
	
	switch ( CameraMode )
	{
	case Apn_CameraMode_Normal:
		break;
	case Apn_CameraMode_TDI:
		break;
	case Apn_CameraMode_Test:
		Read( FPGA_REG_OP_B, RegVal );
		RegVal |= FPGA_BIT_AD_SIMULATION;
		Write( FPGA_REG_OP_B, RegVal );
		break;
	case Apn_CameraMode_ExternalTrigger:
                Read( FPGA_REG_IO_PORT_ASSIGNMENT, RegVal );
                RegVal |= 0x01;
                Write( FPGA_REG_IO_PORT_ASSIGNMENT, RegVal );
		break;
	case Apn_CameraMode_ExternalShutter:
		Read( FPGA_REG_OP_A, RegVal );
		RegVal |= FPGA_BIT_SHUTTER_SOURCE;
		Write( FPGA_REG_OP_A, RegVal );
		break;
	}

	m_pvtCameraMode = CameraMode;
}

void CApnCamera::write_DataBits( Apn_Resolution BitResolution )
{
	unsigned short	RegVal;

	if ( m_CameraInterface == Apn_Interface_NET )
	{
		// The network interface is 16bpp only.  Changing the resolution 
		// for network cameras has no effect.
		return;
	}

	if ( m_DataBits != BitResolution )
	{
		// Reset the camera
		Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_RESET );

		// Change bit setting after the reset
		Read( FPGA_REG_OP_A, RegVal );
		
		if ( BitResolution == Apn_Resolution_TwelveBit )
			RegVal |= FPGA_BIT_DIGITIZATION_RES;

		if ( BitResolution == Apn_Resolution_SixteenBit )
			RegVal &= ~FPGA_BIT_DIGITIZATION_RES;

		Write( FPGA_REG_OP_A, RegVal );

		m_DataBits = BitResolution;
		
		LoadClampPattern();
		LoadSkipPattern();
		LoadRoiPattern( m_RoiBinningH );

		// Reset the camera
		Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_RESET );
		// Start flushing
		Write( FPGA_REG_COMMAND_A, FPGA_BIT_CMD_FLUSH );
	}
}

Apn_Status CApnCamera::read_ImagingStatus()
{
	bool Exposing, Active, Done, Flushing, WaitOnTrigger;
	bool DataHalted, RamError;


	UpdateGeneralStatus();

	// Update the ImagingStatus
	Exposing		= false;
	Active			= false;
	Done			= false;
	Flushing		= false;
	WaitOnTrigger	= false;
	DataHalted		= false;
	RamError		= false;

	if ( (m_pvtStatusReg & FPGA_BIT_STATUS_IMAGING_ACTIVE) != 0 )
		Active = true;
	
	if ( (m_pvtStatusReg & FPGA_BIT_STATUS_IMAGE_EXPOSING) != 0 )
		Exposing = true;
	
	if ( (m_pvtStatusReg & FPGA_BIT_STATUS_IMAGE_DONE) != 0 )
		Done = true;
	
	if ( (m_pvtStatusReg & FPGA_BIT_STATUS_FLUSHING) != 0 )
		Flushing = true;
	
	if ( (m_pvtStatusReg & FPGA_BIT_STATUS_WAITING_TRIGGER) != 0 )
		WaitOnTrigger = true;

	if ( (m_pvtStatusReg & FPGA_BIT_STATUS_DATA_HALTED) != 0 )
		DataHalted = true;

	if ( (m_pvtStatusReg & FPGA_BIT_STATUS_PATTERN_ERROR) != 0 )
		RamError = true;

	if ( RamError )
	{
		m_pvtImagingStatus = Apn_Status_PatternError;
	}
	else
	{
		if ( DataHalted )
		{
			m_pvtImagingStatus = Apn_Status_DataError;
		}
		else
		{
			if ( WaitOnTrigger )
			{
				m_pvtImagingStatus = Apn_Status_WaitingOnTrigger;
			}
			else
			{
				if ( Done && m_pvtImageInProgress )
				{
					m_pvtImageReady			= true;
					m_pvtImagingStatus		= Apn_Status_ImageReady;
				}
				else
				{
					if ( Active )
					{
						if ( Exposing )
							m_pvtImagingStatus = Apn_Status_Exposing;
						else
							m_pvtImagingStatus = Apn_Status_ImagingActive;
					}
					else
					{
						if ( Flushing )
							m_pvtImagingStatus = Apn_Status_Flushing;
						else
							m_pvtImagingStatus = Apn_Status_Idle;
					}
				}
			}
		}
	}

	/*
	switch( m_pvtImagingStatus )
	{
	case Apn_Status_DataError:
		OutputDebugString( "ImagingStatus: Apn_Status_DataError" );
		break;
	case Apn_Status_PatternError:
		OutputDebugString( "ImagingStatus: Apn_Status_PatternError" );
		break;
	case Apn_Status_Idle:
		OutputDebugString( "ImagingStatus: Apn_Status_Idle" );
		break;
	case Apn_Status_Exposing:
		OutputDebugString( "ImagingStatus: Apn_Status_Exposing" );
		break;
	case Apn_Status_ImagingActive:
		OutputDebugString( "ImagingStatus: Apn_Status_ImagingActive" );
		break;
        case Apn_Status_ImageReady:
                OutputDebugString( "ImagingStatus: Apn_Status_ImageReady" );
                break;
	case Apn_Status_Flushing:
		OutputDebugString( "ImagingStatus: Apn_Status_Flushing" );
		break;
	case Apn_Status_WaitingOnTrigger:
		OutputDebugString( "ImagingStatus: Apn_Status_WaitingOnTrigger" );
		break;
	default:
		OutputDebugString( "ImagingStatus: UNDEFINED!!" );
		break;
	}
	*/

	return m_pvtImagingStatus;
}

Apn_LedMode CApnCamera::read_LedMode()
{
	return m_pvtLedMode;
}

void CApnCamera::write_LedMode( Apn_LedMode LedMode )
{
	unsigned short	RegVal;

	Read( FPGA_REG_OP_A, RegVal );

	switch ( LedMode )
	{
	case Apn_LedMode_DisableAll:
		RegVal |= FPGA_BIT_LED_DISABLE;
		break;
	case Apn_LedMode_DisableWhileExpose:
		RegVal &= ~FPGA_BIT_LED_DISABLE;
		RegVal |= FPGA_BIT_LED_EXPOSE_DISABLE;
		break;
	case Apn_LedMode_EnableAll:
		RegVal &= ~FPGA_BIT_LED_DISABLE;
		RegVal &= ~FPGA_BIT_LED_EXPOSE_DISABLE;
		break;
	}

	m_pvtLedMode = LedMode;

	Write( FPGA_REG_OP_A, RegVal );
}

Apn_LedState CApnCamera::read_LedState( unsigned short LedId )
{
        Apn_LedState RetVal(0);

	if ( LedId == 0 )				// LED A
		RetVal = m_pvtLedStateA;
	
	if ( LedId == 1 )				// LED B
		RetVal = m_pvtLedStateB;

	return RetVal;
}

void CApnCamera::write_LedState( unsigned short LedId, Apn_LedState LedState )
{
	unsigned short	RegVal;


	RegVal = 0x0;

	if ( LedId == 0 )			// LED A
	{
		RegVal = (m_pvtLedStateB << 4);	// keep the current settings for LED B
		RegVal |= LedState;				// program new settings
		m_pvtLedStateA = LedState;
	}
	else if ( LedId == 1 )		// LED B
	{
		RegVal = m_pvtLedStateA;		// keep the current settings for LED A
		RegVal |= (LedState << 4);		// program new settings
		m_pvtLedStateB = LedState;
	}

	Write( FPGA_REG_LED_SELECT, RegVal );
}

bool CApnCamera::read_CoolerEnable()
{
	return m_pvtCoolerEnable;
}

void CApnCamera::write_CoolerEnable( bool CoolerEnable )
{
	if ( CoolerEnable )
	{
		Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_RAMP_TO_SETPOINT );
	}
	else
	{
		Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_RAMP_TO_AMBIENT );
	}

	m_pvtCoolerEnable = CoolerEnable;
}

Apn_CoolerStatus CApnCamera::read_CoolerStatus()
{
	bool CoolerAtTemp;
	bool CoolerActive;
	bool CoolerTempRevised;


	UpdateGeneralStatus();

	// Update CoolerStatus
	CoolerActive		= false;
	CoolerAtTemp		= false;
	CoolerTempRevised	= false;

	if ( (m_pvtStatusReg & FPGA_BIT_STATUS_TEMP_AT_TEMP) != 0 )
		CoolerAtTemp = true;
	
	if ( (m_pvtStatusReg & FPGA_BIT_STATUS_TEMP_ACTIVE) != 0 )
		CoolerActive = true;

	if ( (m_pvtStatusReg & FPGA_BIT_STATUS_TEMP_REVISION) != 0 )
		CoolerTempRevised = true;

	// Now derive our cooler state
	if ( !CoolerActive )
	{
		m_pvtCoolerStatus = Apn_CoolerStatus_Off;
	}
	else
	{
		if ( CoolerTempRevised )
		{
			m_pvtCoolerStatus = Apn_CoolerStatus_Revision;
		}
		else
		{
			if ( CoolerAtTemp )
				m_pvtCoolerStatus = Apn_CoolerStatus_AtSetPoint;
			else
				m_pvtCoolerStatus = Apn_CoolerStatus_RampingToSetPoint;

		}
	}

	return m_pvtCoolerStatus;
}

double CApnCamera::read_CoolerSetPoint()
{
	unsigned short	RegVal;
	double			TempVal;

	Read( FPGA_REG_TEMP_DESIRED, RegVal );
	
	RegVal &= 0x0FFF;

	TempVal = ( RegVal - APN_TEMP_SETPOINT_ZERO_POINT ) * APN_TEMP_DEGREES_PER_BIT;

	return TempVal;
}

void CApnCamera::write_CoolerSetPoint( double SetPoint )
{
	unsigned short	RegVal;
	double			TempVal;
	

	TempVal = SetPoint;

	if ( SetPoint < (APN_TEMP_SETPOINT_MIN - APN_TEMP_KELVIN_SCALE_OFFSET) )
		TempVal = APN_TEMP_SETPOINT_MIN;

	if ( SetPoint > (APN_TEMP_SETPOINT_MAX - APN_TEMP_KELVIN_SCALE_OFFSET) )
		TempVal = APN_TEMP_SETPOINT_MAX;

	RegVal = (unsigned short)( (TempVal / APN_TEMP_DEGREES_PER_BIT) + APN_TEMP_SETPOINT_ZERO_POINT );
	
	Write( FPGA_REG_TEMP_DESIRED, RegVal );
}

double CApnCamera::read_CoolerBackoffPoint()
{
	return ( m_pvtCoolerBackoffPoint );
}

void CApnCamera::write_CoolerBackoffPoint( double BackoffPoint )
{
	unsigned short	RegVal;
	double			TempVal;
	
	TempVal = BackoffPoint;

	// BackoffPoint must be a positive number!
	if ( BackoffPoint < 0.0 )
		TempVal = 0.0;

	if ( BackoffPoint < (APN_TEMP_SETPOINT_MIN - APN_TEMP_KELVIN_SCALE_OFFSET) )
		TempVal = APN_TEMP_SETPOINT_MIN;

	if ( BackoffPoint > (APN_TEMP_SETPOINT_MAX - APN_TEMP_KELVIN_SCALE_OFFSET) )
		TempVal = APN_TEMP_SETPOINT_MAX;

	m_pvtCoolerBackoffPoint = TempVal;

	RegVal = (unsigned short)( TempVal / APN_TEMP_DEGREES_PER_BIT );
	
	Write( FPGA_REG_TEMP_BACKOFF, RegVal );
}

double CApnCamera::read_CoolerDrive()
{
	UpdateGeneralStatus();

	return m_pvtCoolerDrive;
}

double CApnCamera::read_TempCCD()
{
	// UpdateGeneralStatus();

	unsigned short	TempReg;
	unsigned short	TempAvg;
	unsigned long	TempTotal;
        int		don;

	TempTotal = 0;
        don = 8;
        if ( m_CameraInterface == Apn_Interface_NET ) {
           don = 1;
        }

	for ( int i=0; i<don; i++ )
	{
		Read( FPGA_REG_TEMP_CCD, TempReg );
		TempTotal += TempReg;
	}

	TempAvg = (unsigned short)(TempTotal / don);

	m_pvtCurrentCcdTemp	= ( (TempAvg - APN_TEMP_SETPOINT_ZERO_POINT) 
									* APN_TEMP_DEGREES_PER_BIT );

	return m_pvtCurrentCcdTemp;
}

double CApnCamera::read_TempHeatsink()
{
	// UpdateGeneralStatus();

	unsigned short	TempReg;
	unsigned short	TempAvg;
	unsigned long	TempTotal;
        int don;

	TempTotal = 0;
        don = 8;
        if ( m_CameraInterface == Apn_Interface_NET ) {
           don = 1;
        }

	for ( int i=0; i<don; i++ )
	{
		Read( FPGA_REG_TEMP_HEATSINK, TempReg );
		TempTotal += TempReg;
	}

	TempAvg = (unsigned short)(TempTotal / don);

	m_pvtCurrentHeatsinkTemp = ( (TempAvg - APN_TEMP_HEATSINK_ZERO_POINT) 
									* APN_TEMP_DEGREES_PER_BIT );

	return m_pvtCurrentHeatsinkTemp;
}

Apn_FanMode	CApnCamera::read_FanMode()
{
	return m_pvtFanMode;
}

void CApnCamera::write_FanMode( Apn_FanMode FanMode )
{
	unsigned short	RegVal;
	unsigned short	OpRegA;


	if ( m_pvtFanMode == FanMode )
		return;

	if ( m_pvtCoolerEnable )
	{
		Read( FPGA_REG_OP_A, OpRegA );
		OpRegA |= FPGA_BIT_TEMP_SUSPEND;
		Write( FPGA_REG_OP_A, OpRegA );

		do 
		{ 
			Read( FPGA_REG_GENERAL_STATUS, RegVal );
		} while ( (RegVal & FPGA_BIT_STATUS_TEMP_SUSPEND_ACK) == 0 );
		
	}

	switch ( FanMode )
	{
	case Apn_FanMode_Off:
		RegVal = APN_FAN_SPEED_OFF;
		break;
	case Apn_FanMode_Low:
		RegVal = APN_FAN_SPEED_LOW;
		break;
	case Apn_FanMode_Medium:
		RegVal = APN_FAN_SPEED_MEDIUM;
		break;
	case Apn_FanMode_High:
		RegVal = APN_FAN_SPEED_HIGH;
		break;
	}

	Write( FPGA_REG_FAN_SPEED_CONTROL, RegVal );

	Read( FPGA_REG_OP_B, RegVal );
	RegVal |= FPGA_BIT_DAC_SELECT_ZERO;
	RegVal &= ~FPGA_BIT_DAC_SELECT_ONE;
	Write( FPGA_REG_OP_B, RegVal );

	Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_DAC_LOAD );

	m_pvtFanMode = FanMode;

	if ( m_pvtCoolerEnable )
	{
		OpRegA &= ~FPGA_BIT_TEMP_SUSPEND;
		Write( FPGA_REG_OP_A, OpRegA );
	}
}

double CApnCamera::read_ShutterStrobePosition()
{
	return m_pvtShutterStrobePosition;
}

void CApnCamera::write_ShutterStrobePosition( double Position )
{
	unsigned short RegVal;
	
	if ( Position < APN_STROBE_POSITION_MIN )
		Position = APN_STROBE_POSITION_MIN;

	RegVal = (unsigned short)((Position - APN_STROBE_POSITION_MIN) / APN_TIMER_RESOLUTION);

	Write( FPGA_REG_SHUTTER_STROBE_POSITION, RegVal );

	m_pvtShutterStrobePosition = Position;
}

double CApnCamera::read_ShutterStrobePeriod()
{
	return m_pvtShutterStrobePeriod;
}

void CApnCamera::write_ShutterStrobePeriod( double Period )
{
	unsigned short RegVal;

	if ( Period < APN_STROBE_PERIOD_MIN )
		Period = APN_STROBE_PERIOD_MIN;

	RegVal = (unsigned short)((Period - APN_STROBE_PERIOD_MIN) / APN_PERIOD_TIMER_RESOLUTION);

	Write( FPGA_REG_SHUTTER_STROBE_PERIOD, RegVal );
	
	m_pvtShutterStrobePeriod = Period;
}

double CApnCamera::read_SequenceDelay()
{
	return m_pvtSequenceDelay;
}

void CApnCamera::write_SequenceDelay( double Delay )
{
	unsigned short RegVal;

	if ( Delay > APN_SEQUENCE_DELAY_LIMIT )
		Delay = APN_SEQUENCE_DELAY_LIMIT;

	m_pvtSequenceDelay = Delay;

	RegVal = (unsigned short)(Delay / APN_SEQUENCE_DELAY_RESOLUTION);

	Write( FPGA_REG_SEQUENCE_DELAY, RegVal );
}

bool CApnCamera::read_VariableSequenceDelay()
{
	unsigned short RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	// variable delay occurs when the bit is 0
	return ( (RegVal & FPGA_BIT_DELAY_MODE) == 0 );	
}

void CApnCamera::write_VariableSequenceDelay( bool VariableSequenceDelay )
{
	unsigned short RegVal;

	Read( FPGA_REG_OP_A, RegVal );

	if ( VariableSequenceDelay )
		RegVal &= ~FPGA_BIT_DELAY_MODE;		// variable when zero
	else
		RegVal |= FPGA_BIT_DELAY_MODE;		// constant when one

	Write( FPGA_REG_OP_A, RegVal );
}

unsigned short CApnCamera::read_ImageCount()
{
	return m_pvtImageCount;
}

void CApnCamera::write_ImageCount( unsigned short Count )
{
	if ( Count == 0 )
		Count = 1;

	Write( FPGA_REG_IMAGE_COUNT, Count );
	
	m_pvtImageCount = Count;
}

void CApnCamera::write_RoiBinningH( unsigned short BinningH )
{
	// Check to see if we actually need to update the binning
	if ( BinningH != m_RoiBinningH )
	{
		// Reset the camera
		Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_RESET );

		LoadRoiPattern( BinningH );
		m_RoiBinningH = BinningH;

		// Reset the camera
		Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_RESET );

		// Start flushing
		Write( FPGA_REG_COMMAND_A, FPGA_BIT_CMD_FLUSH );
	}
}

void CApnCamera::write_RoiBinningV( unsigned short BinningV )
{
	// Check to see if we actually need to update the binning
	if ( BinningV != m_RoiBinningV )
	{
		m_RoiBinningV = BinningV;
	}
}

void CApnCamera::write_RoiPixelsV( unsigned short PixelsV )
{
	m_RoiPixelsV = PixelsV;
}

void CApnCamera::write_RoiStartY( unsigned short StartY )
{
	m_RoiStartY = StartY;
}

unsigned short CApnCamera::read_OverscanColumns()
{
	return m_ApnSensorInfo->m_OverscanColumns;
}

unsigned short CApnCamera::read_MaxBinningV()
{
        if ( m_ApnSensorInfo->m_ImagingRows < APN_VBINNING_MAX )
                return m_ApnSensorInfo->m_ImagingRows;
        else
                return APN_VBINNING_MAX;
}

unsigned short CApnCamera::read_SequenceCounter()
{
	unsigned short RegVal;
	Read( FPGA_REG_SEQUENCE_COUNTER, RegVal );
	return RegVal;
}

unsigned short CApnCamera::read_TDICounter()
{
	unsigned short RegVal;
	Read( FPGA_REG_TDI_COUNTER, RegVal );
	return RegVal;
}

unsigned short CApnCamera::read_TDIRows()
{
	return m_pvtTDIRows;
}

void CApnCamera::write_TDIRows( unsigned short TdiRows )
{
	if ( TdiRows == 0 )		// Make sure the TDI row count is at least 1
		TdiRows = 1;

	Write( FPGA_REG_TDI_COUNT, TdiRows );
	m_pvtTDIRows = TdiRows;
}

double CApnCamera::read_TDIRate()
{
	return m_pvtTDIRate;
}

void CApnCamera::write_TDIRate( double TdiRate )
{
	unsigned short RegVal;

	if ( TdiRate < APN_TDI_RATE_MIN )
		TdiRate = APN_TDI_RATE_MIN;
	
	if ( TdiRate > APN_TDI_RATE_MAX )
		TdiRate = APN_TDI_RATE_MAX;

	RegVal = (unsigned short)( TdiRate / APN_TDI_RATE_RESOLUTION );

	Write( FPGA_REG_TDI_RATE, RegVal );

	m_pvtTDIRate = TdiRate;
}

unsigned short CApnCamera::read_IoPortAssignment()
{
	unsigned short RegVal;
	Read( FPGA_REG_IO_PORT_ASSIGNMENT, RegVal );
	RegVal &= FPGA_MASK_IO_PORT_ASSIGNMENT;
	return RegVal;
}

void CApnCamera::write_IoPortAssignment( unsigned short IoPortAssignment )
{
	IoPortAssignment &= FPGA_MASK_IO_PORT_ASSIGNMENT;
	Write( FPGA_REG_IO_PORT_ASSIGNMENT, IoPortAssignment );
}

unsigned short CApnCamera::read_IoPortDirection()
{
	unsigned short RegVal;
	Read( FPGA_REG_IO_PORT_DIRECTION, RegVal );
	RegVal &= FPGA_MASK_IO_PORT_DIRECTION;
	return RegVal;
}

void CApnCamera::write_IoPortDirection( unsigned short IoPortDirection )
{
	IoPortDirection &= FPGA_MASK_IO_PORT_DIRECTION;
	Write( FPGA_REG_IO_PORT_DIRECTION, IoPortDirection );
}

unsigned short CApnCamera::read_IoPortData()
{
	unsigned short RegVal;
	Read( FPGA_REG_IO_PORT_READ, RegVal );
	RegVal &= FPGA_MASK_IO_PORT_DATA;
	return RegVal;
}

void CApnCamera::write_IoPortData( unsigned short IoPortData )
{
	IoPortData &= FPGA_MASK_IO_PORT_DATA;
	Write( FPGA_REG_IO_PORT_WRITE, IoPortData );
}

unsigned short CApnCamera::read_TwelveBitGain()
{
	return m_pvtTwelveBitGain;
}

void CApnCamera::write_TwelveBitGain( unsigned short TwelveBitGain )
{
	unsigned short NewVal;
	unsigned short StartVal;
	unsigned short FirstBit;

	NewVal		= 0x0;
	StartVal	= TwelveBitGain & 0x3FF;

	for ( int i=0; i<10; i++ )
	{
		FirstBit = ( StartVal & 0x0001 );
		NewVal |= ( FirstBit << (10-i) );
		StartVal = StartVal >> 1;
	}

	NewVal |= 0x4000;

	Write( FPGA_REG_AD_CONFIG_DATA, NewVal );
	Write( FPGA_REG_COMMAND_B,		0x8000 );

	m_pvtTwelveBitGain = TwelveBitGain & 0x3FF;
}

double CApnCamera::read_MaxExposureTime()
{
	return APN_EXPOSURE_TIME_MAX;
}

double CApnCamera::read_TestLedBrightness()
{
	return m_pvtTestLedBrightness;
}

void CApnCamera::write_TestLedBrightness( double TestLedBrightness )
{
	unsigned short	RegVal;
	unsigned short	OpRegA;


	if ( TestLedBrightness == m_pvtTestLedBrightness )
		return;

	if ( m_pvtCoolerEnable )
	{
		Read( FPGA_REG_OP_A, OpRegA );
		OpRegA |= FPGA_BIT_TEMP_SUSPEND;
		Write( FPGA_REG_OP_A, OpRegA );

		do 
		{ 
			Read( FPGA_REG_GENERAL_STATUS, RegVal );
		} while ( (RegVal & FPGA_BIT_STATUS_TEMP_SUSPEND_ACK) == 0 );
		
	}

	RegVal = (unsigned short)( (double)FPGA_MASK_LED_ILLUMINATION * (TestLedBrightness/100.0) );
	
	Write( FPGA_REG_LED_DRIVE, RegVal );

	Read( FPGA_REG_OP_B, RegVal );
	RegVal &= ~FPGA_BIT_DAC_SELECT_ZERO;
	RegVal |= FPGA_BIT_DAC_SELECT_ONE;
	Write( FPGA_REG_OP_B, RegVal );

	Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_DAC_LOAD );

	m_pvtTestLedBrightness = TestLedBrightness;

	if ( m_pvtCoolerEnable )
	{
		OpRegA &= ~FPGA_BIT_TEMP_SUSPEND;
		Write( FPGA_REG_OP_A, OpRegA );
	}
}








long CApnCamera::LoadVerticalPattern()
{
	unsigned short RegData;

	// Prime the RAM (Enable)
	Read( FPGA_REG_OP_B, RegData );
	RegData |= FPGA_BIT_VRAM_ENABLE;
	Write( FPGA_REG_OP_B, RegData );

	WriteMultiSRMD( FPGA_REG_VRAM_INPUT, 
					m_ApnSensorInfo->m_VerticalPattern.PatternData,
					m_ApnSensorInfo->m_VerticalPattern.NumElements );

	// RAM is now loaded (Disable)
	Read( FPGA_REG_OP_B, RegData );
	RegData &= ~FPGA_BIT_VRAM_ENABLE;
	Write( FPGA_REG_OP_B, RegData );

	return 0;
}


long CApnCamera::LoadClampPattern()
{
	unsigned short RegData;

	// Prime the RAM (Enable)
	Read( FPGA_REG_OP_B, RegData );
	RegData |= FPGA_BIT_HCLAMP_ENABLE;
	Write( FPGA_REG_OP_B, RegData );

	if ( m_DataBits == Apn_Resolution_SixteenBit )
	{
		WriteHorizontalPattern( &m_ApnSensorInfo->m_ClampPatternSixteen, 
								FPGA_REG_HCLAMP_INPUT, 
								1 );
	}
	else if ( m_DataBits == Apn_Resolution_TwelveBit )
	{
		WriteHorizontalPattern( &m_ApnSensorInfo->m_ClampPatternTwelve, 
								FPGA_REG_HCLAMP_INPUT, 
								1 );
	}

	// RAM is now loaded (Disable)
	Read( FPGA_REG_OP_B, RegData );
	RegData &= ~FPGA_BIT_HCLAMP_ENABLE;
	Write( FPGA_REG_OP_B, RegData );

	return 0;
}


long CApnCamera::LoadSkipPattern()
{
	unsigned short	RegData;

	// Prime the RAM (Enable)
	Read( FPGA_REG_OP_B, RegData );
	RegData |= FPGA_BIT_HSKIP_ENABLE;
	Write( FPGA_REG_OP_B, RegData );

	if ( m_DataBits == Apn_Resolution_SixteenBit )
	{
		WriteHorizontalPattern( &m_ApnSensorInfo->m_SkipPatternSixteen, 
								FPGA_REG_HSKIP_INPUT, 
								1 );
	}
	else if ( m_DataBits == Apn_Resolution_TwelveBit )
	{
		WriteHorizontalPattern( &m_ApnSensorInfo->m_SkipPatternTwelve, 
								FPGA_REG_HSKIP_INPUT, 
								1 );
	}

	// RAM is now loaded (Disable)
	Read( FPGA_REG_OP_B, RegData );
	RegData &= ~FPGA_BIT_HSKIP_ENABLE;
	Write( FPGA_REG_OP_B, RegData );

	return 0;
}


long CApnCamera::LoadRoiPattern( unsigned short binning )
{
	unsigned short	RegData;

	// Prime the RAM (Enable)
	Read( FPGA_REG_OP_B, RegData );
	RegData |= FPGA_BIT_HRAM_ENABLE;
	Write( FPGA_REG_OP_B, RegData );

	if ( m_DataBits == Apn_Resolution_SixteenBit )
	{
		WriteHorizontalPattern( &m_ApnSensorInfo->m_RoiPatternSixteen, 
								FPGA_REG_HRAM_INPUT, 
								binning );
	}
	else if ( m_DataBits == Apn_Resolution_TwelveBit )
	{
		WriteHorizontalPattern( &m_ApnSensorInfo->m_RoiPatternTwelve, 
								FPGA_REG_HRAM_INPUT, 
								binning );
	}

	// RAM is now loaded (Disable)
	Read( FPGA_REG_OP_B, RegData );
	RegData &= ~FPGA_BIT_HRAM_ENABLE;
	Write( FPGA_REG_OP_B, RegData );

	return 0;
}


long CApnCamera::WriteHorizontalPattern( APN_HPATTERN_FILE *Pattern, 
										 unsigned short RamReg, 
										 unsigned short Binning )
{
	unsigned short	i;
	unsigned short	DataCount;
	unsigned short	*DataArray;
	unsigned short	Index;
	unsigned short	BinNumber;


	Index	  = 0;
	BinNumber = Binning - 1;				// arrays are zero-based

	DataCount = Pattern->RefNumElements +
				Pattern->BinNumElements[BinNumber] +
				Pattern->SigNumElements;

	DataArray = (unsigned short *)malloc(DataCount * sizeof(unsigned short));

	for ( i=0; i<Pattern->RefNumElements; i++ )
	{
		DataArray[Index] = Pattern->RefPatternData[i];
		Index++;
	}
	
	for ( i=0; i<Pattern->BinNumElements[BinNumber]; i++ )
	{
		DataArray[Index] = Pattern->BinPatternData[BinNumber][i];
		Index++;
	}

	for ( i=0; i<Pattern->SigNumElements; i++ )
	{
		DataArray[Index] = Pattern->SigPatternData[i];
		Index++;
	}

	WriteMultiSRMD( RamReg, DataArray, DataCount );

	// cleanup
	free( DataArray );

	return 0;
}


long CApnCamera::InitDefaults()
{
	unsigned short RegVal;
	unsigned short CameraID;
	unsigned short ShutterDelay;

	unsigned short PreRoiRows, PostRoiRows;
	unsigned short PreRoiVBinning, PostRoiVBinning;
	unsigned short UnbinnedRoiY;		// Vertical ROI pixels
	

	// Read the Camera ID register
	Read( FPGA_REG_CAMERA_ID, CameraID );
	CameraID &= FPGA_MASK_CAMERA_ID;

	// Look up the ID and dynamically create the m_ApnSensorInfo object
	switch ( CameraID )
	{
	case APN_ALTA_KAF0401E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF0401E;
		break;
	case APN_ALTA_KAF1602E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF1602E;
		break;
	case APN_ALTA_KAF0261E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF0261E;
		break;
	case APN_ALTA_KAF1301E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF1301E;
		break;
	case APN_ALTA_KAF1401E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF1401E;
		break;
	case APN_ALTA_KAF1001E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF1001E;
		break;
	case APN_ALTA_KAF3200E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF3200E;
		break;
	case APN_ALTA_KAF4202_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF4202;
		break;
	case APN_ALTA_KAF6303E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF6303E;
		break;
	case APN_ALTA_KAF16801E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF16801E;
		break;
	case APN_ALTA_TH7899_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_TH7899;
		break;
        case APN_ALTA_CCD4710LS_CAM_ID:
                m_ApnSensorInfo = new CApnCamData_CCD4710LS;
                break;
        case APN_ALTA_CCD4710HS_CAM_ID:
                m_ApnSensorInfo = new CApnCamData_CCD4710HS;
                break;
        case APN_ALTA_CCD4240LS_CAM_ID:
                m_ApnSensorInfo = new CApnCamData_CCD4240LS;
                break;
        case APN_ALTA_CCD4240HS_CAM_ID:
                m_ApnSensorInfo = new CApnCamData_CCD4240HS;
                break;
        case APN_ALTA_CCD5710LS_CAM_ID:
                m_ApnSensorInfo = new CApnCamData_CCD5710LS;
                break;
        case APN_ALTA_CCD5710HS_CAM_ID:
                m_ApnSensorInfo = new CApnCamData_CCD5710HS;
                break;
        case APN_ALTA_CCD3011LS_CAM_ID:
                m_ApnSensorInfo = new CApnCamData_CCD3011LS;
                break;
        case APN_ALTA_CCD3011HS_CAM_ID:
                m_ApnSensorInfo = new CApnCamData_CCD3011HS;
                break;
        case APN_ALTA_CCD5520LS_CAM_ID:
                m_ApnSensorInfo = new CApnCamData_CCD5520LS;
                break;
        case APN_ALTA_CCD5520HS_CAM_ID:
                m_ApnSensorInfo = new CApnCamData_CCD5520HS;
                break;
        case APN_ALTA_CCD4720LS_CAM_ID:
                m_ApnSensorInfo = new CApnCamData_CCD4720LS;
                break;
        case APN_ALTA_CCD4720HS_CAM_ID:
                m_ApnSensorInfo = new CApnCamData_CCD4720HS;
                break;
        case APN_ALTA_CCD7700LS_CAM_ID:
                m_ApnSensorInfo = new CApnCamData_CCD7700LS;
                break;
        case APN_ALTA_CCD7700HS_CAM_ID:
                m_ApnSensorInfo = new CApnCamData_CCD7700HS;
                break;
        case APN_ALTA_CCD4710LS2_CAM_ID:
                m_ApnSensorInfo = new CApnCamData_CCD4710LS2;
                break;
        case APN_ALTA_CCD4710LS3_CAM_ID:
                m_ApnSensorInfo = new CApnCamData_CCD4710LS3;
                break;
        case APN_ALTA_CCD4710LS4_CAM_ID:
                m_ApnSensorInfo = new CApnCamData_CCD4710LS4;
                break;
        case APN_ALTA_CCD4710LS5_CAM_ID:
                m_ApnSensorInfo = new CApnCamData_CCD4710LS5;
                break;
	default:
		return 1;
		break;
	}

	// we created the object, now set everything
	m_ApnSensorInfo->Initialize();

	// Initialize public variables
	m_DigitizeOverscan			= false;
	m_DataBits					= Apn_Resolution_SixteenBit;

	// Initialize private variables
	m_pvtCameraMode				= Apn_CameraMode_Normal;
	m_pvtNetworkTransferMode	= Apn_NetworkMode_Tcp;

	// Initialize variables used for imaging
	m_RoiStartX		= 0;
	m_RoiStartY		= 0;
	m_RoiPixelsH	= m_ApnSensorInfo->m_ImagingColumns;
	m_RoiPixelsV	= m_ApnSensorInfo->m_ImagingRows;

	m_RoiBinningH	= 1;
	m_RoiBinningV	= 1;
        printf ("Camera ID is %u\n",CameraID);
	printf("sensor = %s\n",			m_ApnSensorInfo->m_Sensor);
	printf("model = %s\n",m_ApnSensorInfo->m_CameraModel);
	printf("interline = %u\n",m_ApnSensorInfo->m_InterlineCCD);
	printf("serialA = %u\n",m_ApnSensorInfo->m_SupportsSerialA);
	printf("serialB = %u\n",m_ApnSensorInfo->m_SupportsSerialB);
	printf("ccdtype = %u\n",m_ApnSensorInfo->m_SensorTypeCCD);
	printf("Tcolumns = %u\n",m_ApnSensorInfo->m_TotalColumns);
	printf("ImgColumns = %u\n",m_ApnSensorInfo->m_ImagingColumns);
	printf("ClampColumns = %u\n",m_ApnSensorInfo->m_ClampColumns);
	printf("PreRoiSColumns = %u\n",m_ApnSensorInfo->m_PreRoiSkipColumns);
	printf("PostRoiSColumns = %u\n",m_ApnSensorInfo->m_PostRoiSkipColumns);
	printf("OverscanColumns = %u\n",m_ApnSensorInfo->m_OverscanColumns);
	printf("TRows = %u\n",m_ApnSensorInfo->m_TotalRows);
	printf("ImgRows = %u\n",m_ApnSensorInfo->m_ImagingRows);
	printf("UnderscanRows = %u\n",m_ApnSensorInfo->m_UnderscanRows);
	printf("OverscanRows = %u\n",m_ApnSensorInfo->m_OverscanRows);
	printf("VFlushBinning = %u\n",m_ApnSensorInfo->m_VFlushBinning);
	printf("HFlushDisable = %u\n",m_ApnSensorInfo->m_HFlushDisable);
	printf("ShutterCloseDelay = %u\n",m_ApnSensorInfo->m_ShutterCloseDelay);
	printf("PixelSizeX = %lf\n",m_ApnSensorInfo->m_PixelSizeX);
	printf("PixelSizeY = %lf\n",m_ApnSensorInfo->m_PixelSizeY);
	printf("Color = %u\n",m_ApnSensorInfo->m_Color);
//	printf("ReportedGainTwelveBit = %lf\n",m_ApnSensorInfo->m_ReportedGainTwelveBit);
	printf("ReportedGainSixteenBit = %lf\n",m_ApnSensorInfo->m_ReportedGainSixteenBit);
	printf("MinSuggestedExpTime = %lf\n",m_ApnSensorInfo->m_MinSuggestedExpTime);
	printf("CoolingSupported = %u\n",m_ApnSensorInfo->m_CoolingSupported);	
	printf("RegulatedCoolingSupported = %u\n",m_ApnSensorInfo->m_RegulatedCoolingSupported);
	printf("TempSetPoint = %lf\n",m_ApnSensorInfo->m_TempSetPoint);
//	printf("TempRegRate = %u\n",m_ApnSensorInfo->m_TempRegRate);
	printf("TempRampRateOne = %u\n",m_ApnSensorInfo->m_TempRampRateOne);
	printf("TempRampRateTwo = %u\n",m_ApnSensorInfo->m_TempRampRateTwo);
	printf("TempBackoffPoint = %lf\n",m_ApnSensorInfo->m_TempBackoffPoint);
	printf("DefaultGainTwelveBit = %u\n",m_ApnSensorInfo->m_DefaultGainTwelveBit);
	printf("DefaultOffsetTwelveBit = %u\n",m_ApnSensorInfo->m_DefaultOffsetTwelveBit);
	printf("DefaultRVoltage = %u\n",m_ApnSensorInfo->m_DefaultRVoltage);



        printf ("RoiPixelsH is %u\n",m_RoiPixelsH);
        printf ("RoiPixelsV is %u\n",m_RoiPixelsV);


	// Issue a clear command, so the registers are zeroed out
	// This will put the camera in a known state for us.
	Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_CLEAR_ALL );

	// Reset the camera
	Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_RESET );

	// Load Inversion Masks
	Write( FPGA_REG_VRAM_INV_MASK, m_ApnSensorInfo->m_VerticalPattern.Mask );
	Write( FPGA_REG_HRAM_INV_MASK, m_ApnSensorInfo->m_RoiPatternSixteen.Mask );

	// Load Pattern Files
	LoadVerticalPattern();
	LoadClampPattern();
	LoadSkipPattern();
	LoadRoiPattern( m_RoiBinningH );

	// Program default camera settings
	Write( FPGA_REG_CLAMP_COUNT,		m_ApnSensorInfo->m_ClampColumns );	
	Write( FPGA_REG_PREROI_SKIP_COUNT,	m_ApnSensorInfo->m_PreRoiSkipColumns );	
	Write( FPGA_REG_ROI_COUNT,			m_ApnSensorInfo->m_ImagingColumns );	
	Write( FPGA_REG_POSTROI_SKIP_COUNT,	m_ApnSensorInfo->m_PostRoiSkipColumns +
										m_ApnSensorInfo->m_OverscanColumns );		
	
	// Since the default state of m_DigitizeOverscan is false, set the count to zero.
	Write( FPGA_REG_OVERSCAN_COUNT,		0x0 );	

	// Now calculate the vertical settings
	UnbinnedRoiY	= m_RoiPixelsV * m_RoiBinningV;

	PreRoiRows		= m_ApnSensorInfo->m_UnderscanRows + 
					  m_RoiStartY;
	
	PostRoiRows		= m_ApnSensorInfo->m_TotalRows -
					  PreRoiRows -
					  UnbinnedRoiY;

	PreRoiVBinning	= 1;
	PostRoiVBinning	= 1;

	// For interline CCDs, set "Fast Dump" mode if the particular array is NOT digitized
	if ( m_ApnSensorInfo->m_InterlineCCD )
	{
		// use the fast dump feature to get rid of the data quickly.
		// one row, binning to the original row count
		// note that we only are not digitized in arrays 1 and 3
		PreRoiVBinning	= PreRoiRows;
		PostRoiVBinning	= PostRoiRows;

		PreRoiVBinning	|= FPGA_BIT_ARRAY_FASTDUMP;
		PostRoiVBinning	|= FPGA_BIT_ARRAY_FASTDUMP;

		PreRoiRows	= 1;
		PostRoiRows	= 1;
	}

	// Program the vertical settings
	Write( FPGA_REG_A1_ROW_COUNT, PreRoiRows );	
	Write( FPGA_REG_A1_VBINNING,  PreRoiVBinning );
	
	Write( FPGA_REG_A2_ROW_COUNT, m_RoiPixelsV );	
	Write( FPGA_REG_A2_VBINNING,  (m_RoiBinningV | FPGA_BIT_ARRAY_DIGITIZE) );	
	
	Write( FPGA_REG_A3_ROW_COUNT, PostRoiRows );	
	Write( FPGA_REG_A3_VBINNING,  PostRoiVBinning );	

	Write( FPGA_REG_VFLUSH_BINNING, m_ApnSensorInfo->m_VFlushBinning );

	double CloseDelay = (double)m_ApnSensorInfo->m_ShutterCloseDelay / 1000;
	ShutterDelay = (unsigned short)
		( (CloseDelay - APN_SHUTTER_CLOSE_DIFF) / APN_TIMER_RESOLUTION );

	Write( FPGA_REG_SHUTTER_CLOSE_DELAY, ShutterDelay );

	Write( FPGA_REG_IMAGE_COUNT,	1 );
	Write( FPGA_REG_SEQUENCE_DELAY, 0 );

	if ( m_ApnSensorInfo->m_HFlushDisable )
	{
		Read( FPGA_REG_OP_A, RegVal );

		RegVal |= FPGA_BIT_DISABLE_H_CLK; 
		
		Write( FPGA_REG_OP_A, RegVal );
	}

	// Reset the camera again
	Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_RESET );

	// Start flushing
	Write( FPGA_REG_COMMAND_A, FPGA_BIT_CMD_FLUSH );

	// If we are a USB2 camera, set all the 12bit variables for the 12bit
	// A/D processor
	if ( m_CameraInterface == Apn_Interface_USB )
	{
		InitTwelveBitAD();
		write_TwelveBitGain( m_ApnSensorInfo->m_DefaultGainTwelveBit );
		WriteTwelveBitOffset();
	}

	// Set the Fan State.  Setting the private var first to make sure the write_FanMode
	// call thinks we're doing a state transition.
	// On write_FanMode return, our state will be Apn_FanMode_Medium
	m_pvtFanMode = Apn_FanMode_Off;		// we're going to set this
	write_FanMode( Apn_FanMode_Medium );

	// Initialize the LED states and the LED mode.  There is nothing to output
	// to the device since we issued our CLEAR early in the init() process, and 
	// we are now in a known state.
	m_pvtLedStateA	= Apn_LedState_Expose;
	m_pvtLedStateB	= Apn_LedState_Expose;
	m_pvtLedMode	= Apn_LedMode_EnableAll;

	// Default value for test LED is 0%
	m_pvtTestLedBrightness = 0.0;

	// Program our initial cooler values.  The only cooler value that we reset
	// at init time is the backoff point.  Everything else is left untouched, and
	// state information is determined from the camera controller.
	m_pvtCoolerBackoffPoint = m_ApnSensorInfo->m_TempBackoffPoint;
	write_CoolerBackoffPoint( m_pvtCoolerBackoffPoint );
	Write( FPGA_REG_TEMP_RAMP_DOWN_A,	m_ApnSensorInfo->m_TempRampRateOne );
	Write( FPGA_REG_TEMP_RAMP_DOWN_B,	m_ApnSensorInfo->m_TempRampRateTwo );
	// the collor code not only determines the m_pvtCoolerEnable state, but
	// also implicitly calls UpdateGeneralStatus() as part of read_CoolerStatus()
	if ( read_CoolerStatus() == Apn_CoolerStatus_Off )
		m_pvtCoolerEnable = false;
	else
		m_pvtCoolerEnable = true;

	m_pvtImageInProgress	= false;
	m_pvtImageReady			= false;
	
	return 0;
}

long CApnCamera::InitTwelveBitAD()
{
	Write( FPGA_REG_AD_CONFIG_DATA, 0x0028 );
	Write( FPGA_REG_COMMAND_B,		FPGA_BIT_CMD_AD_CONFIG );

	return 0;
}

long CApnCamera::WriteTwelveBitOffset()
{
	unsigned short NewVal;
	unsigned short StartVal;
	unsigned short FirstBit;


	NewVal		= 0x0;
	StartVal	= m_ApnSensorInfo->m_DefaultOffsetTwelveBit & 0xFF;

	for ( int i=0; i<8; i++ )
	{
		FirstBit = ( StartVal & 0x0001 );
		NewVal |= ( FirstBit << (10-i) );
		StartVal = StartVal >> 1;
	}

	NewVal |= 0x2000;

	Write( FPGA_REG_AD_CONFIG_DATA, NewVal );
	Write( FPGA_REG_COMMAND_B,		FPGA_BIT_CMD_AD_CONFIG );

	return 0;
}

void CApnCamera::UpdateGeneralStatus()
{
	unsigned short	StatusReg;
	unsigned short	HeatsinkTempReg;
	unsigned short	CcdTempReg;
	unsigned short	CoolerDriveReg;
	unsigned short	VoltageReg;
	unsigned short	TdiCounterReg;
	unsigned short	SequenceCounterReg;



	// Read the general status register of the device
	QueryStatusRegs( StatusReg, 
					 HeatsinkTempReg, 
					 CcdTempReg, 
					 CoolerDriveReg,
					 VoltageReg,
					 TdiCounterReg,
					 SequenceCounterReg );

	m_pvtStatusReg	= StatusReg;

	HeatsinkTempReg &= FPGA_MASK_TEMP_PARAMS;
	CcdTempReg		&= FPGA_MASK_TEMP_PARAMS;
	CoolerDriveReg	&= FPGA_MASK_TEMP_PARAMS;
	VoltageReg		&= FPGA_MASK_INPUT_VOLTAGE;

	if ( CoolerDriveReg > 3200 )
		m_pvtCoolerDrive = 100.0;
	else
		m_pvtCoolerDrive = ( (double)(CoolerDriveReg - 600) / 2600.0 ) * 100.0;
	
	m_pvtCurrentCcdTemp			= ( (CcdTempReg - APN_TEMP_SETPOINT_ZERO_POINT) 
									* APN_TEMP_DEGREES_PER_BIT );

	m_pvtCurrentHeatsinkTemp	= ( (HeatsinkTempReg - APN_TEMP_HEATSINK_ZERO_POINT) 
									* APN_TEMP_DEGREES_PER_BIT );
	
	m_pvtInputVoltage			= VoltageReg * APN_VOLTAGE_RESOLUTION;

	// Update ShutterState
	m_pvtShutterState = ( (m_pvtStatusReg & FPGA_BIT_STATUS_SHUTTER_OPEN) != 0 );
}


bool CApnCamera::ImageReady()
{
	return m_pvtImageReady;
}


void CApnCamera::SignalImagingDone()
{
	m_pvtImageInProgress = false;
}





