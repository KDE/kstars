//////////////////////////////////////////////////////////////////////
//
// ApnCamera.cpp: implementation of the CApnCamera class.
//
// Copyright (c) 2003-2006 Apogee Instruments, Inc.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "ApnCamera.h"
#include "ApnCamTable.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CApnCamera::CApnCamera()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::CApnCamera()" );

	m_ApnSensorInfo		= NULL;
}

CApnCamera::~CApnCamera()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::~CApnCamera()" );

	if ( m_ApnSensorInfo != NULL )
	{
		delete m_ApnSensorInfo;
		m_ApnSensorInfo = NULL;
                CloseDriver();
	}
}

bool CApnCamera::Expose( double Duration, bool Light )
{
	unsigned long	ExpTime;
	unsigned short	BitsPerPixel = 0;
	unsigned short	UnbinnedRoiX;
	unsigned short	UnbinnedRoiY;		
	unsigned short	PreRoiSkip, PostRoiSkip;
	unsigned short	PreRoiRows, PostRoiRows;
	unsigned short	PreRoiVBinning, PostRoiVBinning;

	unsigned short	RegStartCmd = 0;
	unsigned short	RoiRegBuffer[15];
	unsigned short	RoiRegData[15];

	unsigned short	TotalHPixels;
	unsigned short	TotalVPixels;

	unsigned long	TestImageSize;

	unsigned long	WaitCounter;

	char			szOutputText[128];


	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::Expose( Duration = %f, Light = %d ) -> BEGIN", Duration, Light );
	AltaDebugOutputString( szOutputText );


	WaitCounter = 0;
	while ( read_ImagingStatus() != Apn_Status_Flushing )
	{
		Sleep( 20 );

		WaitCounter++;
		
		if ( WaitCounter > 150 )
		{
			// we've waited longer than 3s to start flushing in the camera head
			// something is amiss...abort the expose command to avoid an infinite loop
			AltaDebugOutputString( "APOGEE.DLL - CApnCamera::Expose() -> ERROR: Timed out waiting for flushing!!" );
			
			return false;
		}
	}

	// Validate the "Duration" parameter
	if ( Duration < APN_EXPOSURE_TIME_MIN )
		Duration = APN_EXPOSURE_TIME_MIN;

	// Validate the ROI params
	UnbinnedRoiX	= m_pvtRoiPixelsH * m_pvtRoiBinningH;

	PreRoiSkip		= m_pvtRoiStartX;

	PostRoiSkip		= m_ApnSensorInfo->m_TotalColumns -
					  m_ApnSensorInfo->m_ClampColumns -
					  PreRoiSkip - 
					  UnbinnedRoiX;

	TotalHPixels = UnbinnedRoiX + PreRoiSkip + PostRoiSkip + m_ApnSensorInfo->m_ClampColumns;

	if ( TotalHPixels != m_ApnSensorInfo->m_TotalColumns )
	{
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::Expose() -> ERROR: Horizontal geometry incorrect" );		
		
		return false;
	}

	UnbinnedRoiY	= m_pvtRoiPixelsV * m_pvtRoiBinningV;

	PreRoiRows		= m_ApnSensorInfo->m_UnderscanRows + 
					  m_pvtRoiStartY;
	
	PostRoiRows		= m_ApnSensorInfo->m_TotalRows -
					  PreRoiRows -
					  UnbinnedRoiY;

	TotalVPixels = UnbinnedRoiY + PreRoiRows + PostRoiRows;

	if ( TotalVPixels != m_ApnSensorInfo->m_TotalRows )
	{
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::Expose() -> ERROR: Vertical geometry incorrect" );		
		
		return false;
	}

	m_pvtExposurePixelsV = m_pvtRoiPixelsV;
	m_pvtExposurePixelsH = m_pvtRoiPixelsH;

	if ( read_CameraMode() == Apn_CameraMode_Test )
	{
		TestImageSize = m_pvtExposurePixelsV * m_pvtExposurePixelsH;
		Write( FPGA_REG_TEST_COUNT_UPPER, (USHORT)(TestImageSize>>16) );
		Write( FPGA_REG_TEST_COUNT_LOWER, (USHORT)(TestImageSize & 0xFFFF) );
	}

	if ( m_pvtDataBits == Apn_Resolution_SixteenBit )
	{
		BitsPerPixel = 16;
	}
	else if ( m_pvtDataBits == Apn_Resolution_TwelveBit )
	{
		BitsPerPixel = 12;
	}

	if ( PreStartExpose( BitsPerPixel ) != 0 )
	{
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::Expose() -> ERROR: Failed PreStartExpose()!!" );		
		
		return false;
	}

	// Calculate the vertical parameters
	PreRoiVBinning	= m_ApnSensorInfo->m_RowOffsetBinning;
	PostRoiVBinning	= 1;

	// For interline CCDs, set "Fast Dump" mode if the particular array is NOT digitized
	/*
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
	*/

	// Set up the geometry for a full frame device
	if ( m_ApnSensorInfo->m_EnableSingleRowOffset )
	{
		// PreRoiVBinning	+= PreRoiRows;
		PreRoiVBinning	= PreRoiRows;
		PostRoiVBinning	= PostRoiRows;

		PreRoiVBinning	|= FPGA_BIT_ARRAY_FASTDUMP;
		PostRoiVBinning	|= FPGA_BIT_ARRAY_FASTDUMP;

		PreRoiRows	= 1;
		PostRoiRows	= 1;
	}

	// Calculate the exposure time to program to the camera
	ExpTime = ((unsigned long)(Duration / APN_TIMER_RESOLUTION)) + APN_TIMER_OFFSET_COUNT;

	Write( FPGA_REG_TIMER_LOWER, (unsigned short)(ExpTime & 0xFFFF));
	ExpTime = ExpTime >> 16;
	Write( FPGA_REG_TIMER_UPPER, (unsigned short)(ExpTime & 0xFFFF));

	// Set up the registers for the exposure
	ResetSystemNoFlush();

	// Issue the reset
	// RoiRegBuffer[0] = FPGA_REG_COMMAND_B;
	RoiRegBuffer[0] = FPGA_REG_SCRATCH;
	RoiRegData[0]	= FPGA_BIT_CMD_RESET;

	// Program the horizontal settings
	RoiRegBuffer[1]	= FPGA_REG_PREROI_SKIP_COUNT;
	RoiRegData[1]	= PreRoiSkip;

	RoiRegBuffer[2]	= FPGA_REG_ROI_COUNT;
	// Number of ROI pixels.  Adjust the 12bit operation here to account for an extra 
	// 10 pixel shift as a result of the A/D conversion.
	if ( m_pvtDataBits == Apn_Resolution_SixteenBit )
	{
		RoiRegData[2] = m_pvtExposurePixelsH + 1;
	}
	else if ( m_pvtDataBits == Apn_Resolution_TwelveBit )
	{
		RoiRegData[2] = m_pvtExposurePixelsH + 10;
	}

	RoiRegBuffer[3]	= FPGA_REG_POSTROI_SKIP_COUNT;
	RoiRegData[3]	= PostRoiSkip;

	// Program the vertical settings
	if ( m_pvtFirmwareVersion < 11 )
	{
		RoiRegBuffer[4] = FPGA_REG_A1_ROW_COUNT;
		RoiRegData[4]	= PreRoiRows;
		RoiRegBuffer[5] = FPGA_REG_A1_VBINNING;
		RoiRegData[5]	= PreRoiVBinning;

		RoiRegBuffer[6] = FPGA_REG_A2_ROW_COUNT;
		RoiRegData[6]	= m_pvtRoiPixelsV;
		RoiRegBuffer[7] = FPGA_REG_A2_VBINNING;
		RoiRegData[7]	= (m_pvtRoiBinningV | FPGA_BIT_ARRAY_DIGITIZE);

		RoiRegBuffer[8] = FPGA_REG_A3_ROW_COUNT;
		RoiRegData[8]	= PostRoiRows;
		RoiRegBuffer[9] = FPGA_REG_A3_VBINNING;
		RoiRegData[9]	= PostRoiVBinning;

		RoiRegBuffer[10]	= FPGA_REG_SCRATCH;
		RoiRegData[10]		= 0;
		RoiRegBuffer[11]	= FPGA_REG_SCRATCH;
		RoiRegData[11]		= 0;
		RoiRegBuffer[12]	= FPGA_REG_SCRATCH;
		RoiRegData[12]		= 0;
		RoiRegBuffer[13]	= FPGA_REG_SCRATCH;
		RoiRegData[13]		= 0;
	}
	else
	{
		if ( m_ApnSensorInfo->m_EnableSingleRowOffset )
		{
			RoiRegBuffer[4] = FPGA_REG_A1_ROW_COUNT;
			RoiRegData[4]	= 0;
			RoiRegBuffer[5] = FPGA_REG_A1_VBINNING;
			RoiRegData[5]	= 0;

			RoiRegBuffer[6] = FPGA_REG_A2_ROW_COUNT;
			RoiRegData[6]	= PreRoiRows;
			RoiRegBuffer[7] = FPGA_REG_A2_VBINNING;
			RoiRegData[7]	= PreRoiVBinning;

			RoiRegBuffer[8] = FPGA_REG_A3_ROW_COUNT;
			RoiRegData[8]	= m_pvtRoiPixelsV;
			RoiRegBuffer[9] = FPGA_REG_A3_VBINNING;
			RoiRegData[9]	= (m_pvtRoiBinningV | FPGA_BIT_ARRAY_DIGITIZE);
			
			RoiRegBuffer[10] = FPGA_REG_A4_ROW_COUNT;
			RoiRegData[10]	= 0;
			RoiRegBuffer[11] = FPGA_REG_A4_VBINNING;
			RoiRegData[11]	= 0;

			RoiRegBuffer[12] = FPGA_REG_A5_ROW_COUNT;
			RoiRegData[12]	= PostRoiRows;
			RoiRegBuffer[13] = FPGA_REG_A5_VBINNING;
			RoiRegData[13]	= PostRoiVBinning;
		}
		else
		{
			if ( PreRoiRows > 70 )
			{
				RoiRegBuffer[4] = FPGA_REG_A1_ROW_COUNT;
				RoiRegData[4]	= 1;
				RoiRegBuffer[5] = FPGA_REG_A1_VBINNING;
				RoiRegData[5]	= (PreRoiRows-70);

				RoiRegBuffer[6] = FPGA_REG_A2_ROW_COUNT;
				RoiRegData[6]	= 70;
				RoiRegBuffer[7] = FPGA_REG_A2_VBINNING;
				RoiRegData[7]	= 1;
			}
			else
			{
				RoiRegBuffer[4] = FPGA_REG_A1_ROW_COUNT;
				RoiRegData[4]	= 0;
				RoiRegBuffer[5] = FPGA_REG_A1_VBINNING;
				RoiRegData[5]	= 0;

				RoiRegBuffer[6] = FPGA_REG_A2_ROW_COUNT;
				RoiRegData[6]	= PreRoiRows;
				RoiRegBuffer[7] = FPGA_REG_A2_VBINNING;
				RoiRegData[7]	= PreRoiVBinning;
			}
			
			RoiRegBuffer[8] = FPGA_REG_A3_ROW_COUNT;
			RoiRegData[8]	= m_pvtRoiPixelsV;
			RoiRegBuffer[9] = FPGA_REG_A3_VBINNING;
			RoiRegData[9]	= (m_pvtRoiBinningV | FPGA_BIT_ARRAY_DIGITIZE);
			
			if ( PostRoiRows > 70 )
			{
				RoiRegBuffer[10] = FPGA_REG_A4_ROW_COUNT;
				RoiRegData[10]	= 1;
				RoiRegBuffer[11] = FPGA_REG_A4_VBINNING;
				RoiRegData[11]	= (PostRoiRows-70);

				RoiRegBuffer[12] = FPGA_REG_A5_ROW_COUNT;
				RoiRegData[12]	= 70;
				RoiRegBuffer[13] = FPGA_REG_A5_VBINNING;
				RoiRegData[13]	= 1;
			}
			else
			{
				RoiRegBuffer[10] = FPGA_REG_A4_ROW_COUNT;
				RoiRegData[10]	= 0;
				RoiRegBuffer[11] = FPGA_REG_A4_VBINNING;
				RoiRegData[11]	= 0;

				RoiRegBuffer[12] = FPGA_REG_A5_ROW_COUNT;
				RoiRegData[12]	= PostRoiRows;
				RoiRegBuffer[13] = FPGA_REG_A5_VBINNING;
				RoiRegData[13]	= PostRoiVBinning;
			}
		}
	}

	// Issue the reset
	RoiRegBuffer[14]	= FPGA_REG_COMMAND_B;
	RoiRegData[14]		= FPGA_BIT_CMD_RESET;

	// Send the instruction sequence to the camera
	WriteMultiMRMD( RoiRegBuffer, RoiRegData, 15 );

	// Issue the flush for interlines, or if using the external shutter
	if ( (m_ApnSensorInfo->m_InterlineCCD && m_pvtFastSequence) || (m_pvtExternalShutter) )
	{
		// Make absolutely certain that flushing starts first
		// in order to use Progressive Scan/Ratio Mode
		Write( FPGA_REG_COMMAND_A, FPGA_BIT_CMD_FLUSH );
        
		for ( int i=0; i<2; i++ )
		{
			Write( FPGA_REG_SCRATCH, 0x8086 );
			Write( FPGA_REG_SCRATCH, 0x8088 );
		}
        }

	m_pvtExposureExternalShutter = m_pvtExternalShutter;

	switch ( m_pvtCameraMode )
	{
	case Apn_CameraMode_Normal:
		if ( Light )
			RegStartCmd		= FPGA_BIT_CMD_EXPOSE;
		else
			RegStartCmd		= FPGA_BIT_CMD_DARK;
		
		if ( m_pvtTriggerNormalGroup )
			m_pvtExposureTriggerGroup = true;
		else
			m_pvtExposureTriggerGroup = false;

		if ( m_pvtTriggerNormalEach )
			m_pvtExposureTriggerEach = true;
		else
			m_pvtExposureTriggerEach = false;

		break;
	case Apn_CameraMode_TDI:
		RegStartCmd			= FPGA_BIT_CMD_TDI;

		if ( m_pvtTriggerTdiKineticsGroup )
			m_pvtExposureTriggerGroup = true;
		else
			m_pvtExposureTriggerGroup = false;

		if ( m_pvtTriggerTdiKineticsEach )
			m_pvtExposureTriggerEach = true;
		else
			m_pvtExposureTriggerEach = false;

		break;
	case Apn_CameraMode_Test:
		if ( Light )
			RegStartCmd		= FPGA_BIT_CMD_TEST;
		else
			RegStartCmd		= FPGA_BIT_CMD_TEST;
		break;
	case Apn_CameraMode_ExternalTrigger:
		RegStartCmd			= FPGA_BIT_CMD_TRIGGER_EXPOSE;
		break;
	case Apn_CameraMode_Kinetics:
		RegStartCmd			= FPGA_BIT_CMD_KINETICS;

		if ( m_pvtTriggerTdiKineticsGroup )
			m_pvtExposureTriggerGroup = true;
		else
			m_pvtExposureTriggerGroup = false;

		if ( m_pvtTriggerTdiKineticsEach )
			m_pvtExposureTriggerEach = true;
		else
			m_pvtExposureTriggerEach = false;

		break;
	}

	// Send the instruction sequence to the camera
	Write( FPGA_REG_COMMAND_A, RegStartCmd );

	// Set our flags to mark the start of a new exposure
	m_pvtImageInProgress = true;
	m_pvtImageReady		 = false;

	// Debug output for leaving this function
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::Expose( Duration = %f, Light = %d ) -> END", Duration, Light );
	AltaDebugOutputString( szOutputText );
      

	return true;
}

bool CApnCamera::ResetSystem()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::ResetSystem()" );

	// Reset the camera engine
	Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_RESET );

	// A little delay before we start flushing
	Write( FPGA_REG_SCRATCH, 0x8086 );
	Write( FPGA_REG_SCRATCH, 0x8088 );
	Write( FPGA_REG_SCRATCH, 0x8086 );
	Write( FPGA_REG_SCRATCH, 0x8088 );
	Write( FPGA_REG_SCRATCH, 0x8086 );
	Write( FPGA_REG_SCRATCH, 0x8088 );

	// Start flushing
	Write( FPGA_REG_COMMAND_A, FPGA_BIT_CMD_FLUSH );

	// A little delay once we've started flushing
	Write( FPGA_REG_SCRATCH, 0x8086 );
	Write( FPGA_REG_SCRATCH, 0x8088 );
      
	return true;
}

bool CApnCamera::ResetSystemNoFlush()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::ResetSystemNoFlush()" );


	// Reset the camera engine
	Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_RESET );

	// A little delay before we start flushing
	Write( FPGA_REG_SCRATCH, 0x8086 );
	Write( FPGA_REG_SCRATCH, 0x8088 );
	Write( FPGA_REG_SCRATCH, 0x8086 );
	Write( FPGA_REG_SCRATCH, 0x8088 );
	Write( FPGA_REG_SCRATCH, 0x8086 );
	Write( FPGA_REG_SCRATCH, 0x8088 );
      

	return true;
}

bool CApnCamera::PauseTimer( bool PauseState )
{
	unsigned short RegVal;
	bool		   CurrentState;

	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::PauseTimer( PauseState = %d)", PauseState );
	AltaDebugOutputString( szOutputText );


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
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::StopExposure( DigitizeData = %d) -> BEGIN", DigitizeData );
	AltaDebugOutputString( szOutputText );


	if ( m_pvtImageInProgress )
	{
		Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_END_EXPOSURE );

		if ( PostStopExposure( DigitizeData ) != 0 )
		{
			return false;
		}
	}

	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::StopExposure( DigitizeData = %d) -> END", DigitizeData );
	AltaDebugOutputString( szOutputText );

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
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_AvailableMemory()" );

	long AvailableMemory = 0;

	switch( GetCameraInterface() )
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
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_FirmwareVersion()" );

	return m_pvtFirmwareVersion;
}

bool CApnCamera::read_ShutterState()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ShutterState()" );

	UpdateGeneralStatus();

	return m_pvtShutterState;
}

bool CApnCamera::read_DisableShutter()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_DisableShutter()" );

	unsigned short RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	return ( (RegVal & FPGA_BIT_DISABLE_SHUTTER) != 0 );
}

void CApnCamera::write_DisableShutter( bool DisableShutter )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_DisableShutter( DisableShutter = %d)", DisableShutter );
	AltaDebugOutputString( szOutputText );

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
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ForceShutterOpen()" );

	unsigned short RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	return ( (RegVal & FPGA_BIT_FORCE_SHUTTER) != 0 );
}

void CApnCamera::write_ForceShutterOpen( bool ForceShutterOpen )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_ForceShutterOpen( ForceShutterOpen = %d)", ForceShutterOpen );
	AltaDebugOutputString( szOutputText );

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
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ShutterAmpControl()" );

	unsigned short RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	return ( (RegVal & FPGA_BIT_SHUTTER_AMP_CONTROL ) != 0 );
}

void CApnCamera::write_ShutterAmpControl( bool ShutterAmpControl )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_ShutterAmpControl( ShutterAmpControl = %d)", ShutterAmpControl );
	AltaDebugOutputString( szOutputText );

	unsigned short RegVal;
	Read( FPGA_REG_OP_A, RegVal );

	if ( ShutterAmpControl )
		RegVal |= FPGA_BIT_SHUTTER_AMP_CONTROL;
	else
		RegVal &= ~FPGA_BIT_SHUTTER_AMP_CONTROL;

	Write( FPGA_REG_OP_A, RegVal );
}

bool CApnCamera::read_DisableFlushCommands()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_DisableFlushCommands()" );

	unsigned short RegVal;
	Read( FPGA_REG_OP_B, RegVal );
	return ( (RegVal & FPGA_BIT_DISABLE_FLUSH_COMMANDS ) != 0 );
}

void CApnCamera::write_DisableFlushCommands( bool DisableFlushCommands )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_DisableFlushCommands( DisableFlushCommands = %d)", DisableFlushCommands );
	AltaDebugOutputString( szOutputText );

	unsigned short RegVal;
	Read( FPGA_REG_OP_B, RegVal );

	if ( DisableFlushCommands )
		RegVal |= FPGA_BIT_DISABLE_FLUSH_COMMANDS;
	else
		RegVal &= ~FPGA_BIT_DISABLE_FLUSH_COMMANDS;

	Write( FPGA_REG_OP_B, RegVal );
}

bool CApnCamera::read_DisablePostExposeFlushing()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_DisablePostExposeFlushing()" );

	unsigned short RegVal;
	Read( FPGA_REG_OP_B, RegVal );
	return ( (RegVal & FPGA_BIT_DISABLE_POST_EXP_FLUSH ) != 0 );
}

void CApnCamera::write_DisablePostExposeFlushing( bool DisablePostExposeFlushing )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_DisablePostExposeFlushing( DisablePostExposeFlushing = %d)", DisablePostExposeFlushing );
	AltaDebugOutputString( szOutputText );

	unsigned short RegVal;
	Read( FPGA_REG_OP_B, RegVal );

	if ( DisablePostExposeFlushing )
		RegVal |= FPGA_BIT_DISABLE_POST_EXP_FLUSH;
	else
		RegVal &= ~FPGA_BIT_DISABLE_POST_EXP_FLUSH;

	Write( FPGA_REG_OP_B, RegVal );
}

bool CApnCamera::read_ExternalIoReadout()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ExternalIoReadout()" );

	unsigned short	RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	return ( (RegVal & FPGA_BIT_SHUTTER_MODE) != 0 );
}

void CApnCamera::write_ExternalIoReadout( bool ExternalIoReadout )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_ExternalIoReadout( ExternalIoReadout = %d)", ExternalIoReadout );
	AltaDebugOutputString( szOutputText );

	unsigned short	RegVal;
	Read( FPGA_REG_OP_A, RegVal );

	if ( ExternalIoReadout )
		RegVal |= FPGA_BIT_SHUTTER_MODE;
	else
		RegVal &= ~FPGA_BIT_SHUTTER_MODE;

	Write( FPGA_REG_OP_A, RegVal );
}

bool CApnCamera::read_ExternalShutter()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ExternalShutter()" );

	unsigned short	RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	return ( (RegVal & FPGA_BIT_SHUTTER_SOURCE) != 0 );
}

void CApnCamera::write_ExternalShutter( bool ExternalShutter )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_ExternalShutter( ExternalShutter = %d)", ExternalShutter );
	AltaDebugOutputString( szOutputText );

	unsigned short	RegVal;
	Read( FPGA_REG_OP_A, RegVal );

	if ( ExternalShutter )
		RegVal |= FPGA_BIT_SHUTTER_SOURCE;
	else
		RegVal &= ~FPGA_BIT_SHUTTER_SOURCE;

	Write( FPGA_REG_OP_A, RegVal );

	m_pvtExternalShutter = ExternalShutter;
}

bool CApnCamera::read_FastSequence()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_FastSequence()" );

	if ( m_ApnSensorInfo->m_InterlineCCD == false )
		return false;

	unsigned short	RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	return ( (RegVal & FPGA_BIT_RATIO) != 0 );
}

void CApnCamera::write_FastSequence( bool FastSequence )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_FastSequence( FastSequence = %d)", FastSequence );
	AltaDebugOutputString( szOutputText );

	// fast sequence/progressive scan is for interline only
	if ( m_ApnSensorInfo->m_InterlineCCD == false )
		return;

	// do not allow triggers on each progressive scanned image
	if ( m_pvtTriggerNormalEach )
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

	m_pvtFastSequence = FastSequence;
}

Apn_NetworkMode CApnCamera::read_NetworkTransferMode()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_NetworkTransferMode()" );

	return m_pvtNetworkTransferMode;
}

void CApnCamera::write_NetworkTransferMode( Apn_NetworkMode TransferMode )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_NetworkTransferMode( TransferMode = %d)", TransferMode );
	AltaDebugOutputString( szOutputText );

	SetNetworkTransferMode( TransferMode );

	m_pvtNetworkTransferMode = TransferMode;
}

Apn_CameraMode CApnCamera::read_CameraMode()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_CameraMode()" );

	return m_pvtCameraMode;
}

void CApnCamera::write_CameraMode( Apn_CameraMode CameraMode )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_CameraMode( CameraMode = %d)", CameraMode );
	AltaDebugOutputString( szOutputText );

	unsigned short RegVal;

	// The Apn_CameraMode_ExternalShutter mode was deprecated as of 
	// version 3.0.15.  If an application sends this mode, it is now 
	// converted to Apn_CameraMode_Normal.  Applications should use the
	// External_Shutter property to enable an external shutter
	if ( CameraMode == Apn_CameraMode_ExternalShutter )
		CameraMode = Apn_CameraMode_Normal;

	// Only allow Apn_CameraMode_Kinetics if our firmware is v17 or higher.
	// If the firmware doesn't support the mode, switch to Apn_CameraMode_Normal
	if ( (m_pvtFirmwareVersion < 17 ) && (CameraMode == Apn_CameraMode_Kinetics) )
		CameraMode = Apn_CameraMode_Normal;

	// If we are an interline CCD, do not allow the mode to be set to 
	// Apn_CameraMode_TDI or Apn_CameraMode_Kinetics
	if ( m_ApnSensorInfo->m_InterlineCCD == true )
	{
		if ( (CameraMode == Apn_CameraMode_TDI) || (CameraMode == Apn_CameraMode_Kinetics) )
			CameraMode = Apn_CameraMode_Normal;
	}

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
		RegVal = read_IoPortAssignment();
		RegVal &= 0x3E;						// External trigger is pin one (bit zero)
		write_IoPortAssignment( RegVal );
		break;
	case Apn_CameraMode_Kinetics:
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
		RegVal = read_IoPortAssignment();
		RegVal |= 0x01;						// External trigger is pin one (bit zero)
		write_IoPortAssignment( RegVal );
		break;
	case Apn_CameraMode_Kinetics:
		break;
	}

	m_pvtCameraMode = CameraMode;
}

Apn_Resolution CApnCamera::read_DataBits()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_DataBits()" );

	return m_pvtDataBits;
}

void CApnCamera::write_DataBits( Apn_Resolution BitResolution )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_DataBits( BitResolution = %d)", BitResolution );
	AltaDebugOutputString( szOutputText );

	unsigned short	RegVal;

	if ( GetCameraInterface() == Apn_Interface_NET )
	{
		// The network interface is 16bpp only.  Changing the resolution 
		// for network cameras has no effect.
		return;
	}

	if ( m_ApnSensorInfo->m_AlternativeADType == ApnAdType_None )
	{
		// No 12bit A/D converter is supported.  Changing the resolution
		// for these systems has no effect
		return;
	}


	if ( m_pvtDataBits != BitResolution )
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

		m_pvtDataBits = BitResolution;
	
		if ( BitResolution == Apn_Resolution_TwelveBit )
			Write( FPGA_REG_HRAM_INV_MASK, m_ApnSensorInfo->m_RoiPatternTwelve.Mask );

		if ( BitResolution == Apn_Resolution_SixteenBit )
			Write( FPGA_REG_HRAM_INV_MASK, m_ApnSensorInfo->m_RoiPatternSixteen.Mask );

		LoadClampPattern();
		LoadSkipPattern();
		LoadRoiPattern( m_pvtRoiBinningH );

		// Reset the camera and start flushing
		ResetSystem();
	}
}

Apn_Status CApnCamera::read_ImagingStatus()
{
	bool Exposing, Active, Done, Flushing, WaitOnTrigger;
	bool DataHalted, RamError;


//	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImagingStatus()" );

	UpdateGeneralStatus();

	// Update the ImagingStatus
	Exposing		= false;
	Active			= false;
	Done			= false;
	Flushing		= false;
	WaitOnTrigger	= false;
	DataHalted		= false;
	RamError		= false;

	if ( (GetCameraInterface() == Apn_Interface_USB) && 
		 (m_pvtQueryStatusRetVal == CAPNCAMERA_ERR_CONNECT) )
	{
		m_pvtImagingStatus = Apn_Status_ConnectionError;
		return m_pvtImagingStatus;
	}

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

	// set the previous imaging status to whatever the current status is
	// this previous status will only be used for stopping a triggered
	// exposure, in the case where the hw trigger was not yet received.
	m_pvtPrevImagingStatus = m_pvtImagingStatus;
	
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

				if ( m_pvtExposureExternalShutter && Active && Exposing )
				{
					m_pvtImagingStatus = Apn_Status_Exposing;
				}
			}
			else
			{
				if ( Done && m_pvtImageInProgress && (m_pvtCameraMode != Apn_CameraMode_TDI) )
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
						{
							if ( m_pvtImageInProgress && (m_pvtCameraMode == Apn_CameraMode_TDI) )
							{
								// Driver defined status...not all rows have been returned to the application
								m_pvtImagingStatus = Apn_Status_ImagingActive;
							}
							else
							{
								m_pvtImagingStatus = Apn_Status_Idle;
							}

							if ( m_pvtPrevImagingStatus == Apn_Status_WaitingOnTrigger )
							{
								// we've transitioned from waiting on the trigger
								// to an idle state.  this means the trigger was
								// never received by the hardware.  reset the hardware
								// and start flushing the sensor.
							}
						}
					}
				}
			}
		}
	}

#ifdef  APOGEE_DLL_IMAGING_STATUS_OUTPUT
	char szMsg[512];
	sprintf( szMsg, "APOGEE.DLL - CApnCamera::read_ImagingStatus() - Flags: Active=%d; Exposing=%d; Done=%d; Flushing=%d; WaitOnTrigger=%d", Active, Exposing, Done, Flushing, WaitOnTrigger );
	AltaDebugOutputString( szMsg );

	switch( m_pvtImagingStatus )
	{
	case Apn_Status_DataError:
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImagingStatus() returning Apn_Status_DataError" );
		break;
	case Apn_Status_PatternError:
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImagingStatus() returning Apn_Status_PatternError" );
		break;
	case Apn_Status_Idle:
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImagingStatus() returning Apn_Status_Idle" );
		break;
	case Apn_Status_Exposing:
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImagingStatus() returning Apn_Status_Exposing" );
		break;
	case Apn_Status_ImagingActive:
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImagingStatus() returning Apn_Status_ImagingActive" );
		break;
	case Apn_Status_ImageReady:
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImagingStatus() returning Apn_Status_ImageReady" );
		break;
	case Apn_Status_Flushing:
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImagingStatus() returning Apn_Status_Flushing" );
		break;
	case Apn_Status_WaitingOnTrigger:
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImagingStatus() returning Apn_Status_WaitingOnTrigger" );
		break;
	default:
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImagingStatus() returning UNDEFINED!!" );
		break;
	}
#endif

	return m_pvtImagingStatus;
}

Apn_LedMode CApnCamera::read_LedMode()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_LedMode()" );

	return m_pvtLedMode;
}

void CApnCamera::write_LedMode( Apn_LedMode LedMode )
{
	unsigned short	RegVal;

	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_LedMode( LedMode = %d)", LedMode );
	AltaDebugOutputString( szOutputText );

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
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_LedState()" );

	Apn_LedState RetVal = 0;

	if ( LedId == 0 )				// LED A
		RetVal = m_pvtLedStateA;
	
	if ( LedId == 1 )				// LED B
		RetVal = m_pvtLedStateB;

	return RetVal;
}

void CApnCamera::write_LedState( unsigned short LedId, Apn_LedState LedState )
{
	unsigned short	RegVal;

	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_LedState( LedId = %d, LedState = %d)", LedId, LedState );
	AltaDebugOutputString( szOutputText );

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
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_CoolerEnable()" );

	return m_pvtCoolerEnable;
}

void CApnCamera::write_CoolerEnable( bool CoolerEnable )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_CoolerEnable( CoolerEnable = %d)", CoolerEnable );
	AltaDebugOutputString( szOutputText );

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

	// AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_CoolerStatus()" );

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

	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_CoolerSetPoint()" );

	Read( FPGA_REG_TEMP_DESIRED, RegVal );
	
	RegVal &= 0x0FFF;

	TempVal = ( RegVal - APN_TEMP_SETPOINT_ZERO_POINT ) * APN_TEMP_DEGREES_PER_BIT;

	return TempVal;
}

void CApnCamera::write_CoolerSetPoint( double SetPoint )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_CoolerSetPoint( SetPoint = %f)", SetPoint );
	AltaDebugOutputString( szOutputText );


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
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_CoolerBackoffPoint()" );

	return ( m_pvtCoolerBackoffPoint );
}

void CApnCamera::write_CoolerBackoffPoint( double BackoffPoint )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_CoolerBackoffPoint( BackoffPoint = %f)", BackoffPoint );
	AltaDebugOutputString( szOutputText );


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
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_CoolerDrive()" );

	UpdateGeneralStatus();

	return m_pvtCoolerDrive;
}

double CApnCamera::read_TempCCD()
{
	double	TempAvg;
	double	TempTotal;

//	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_TempCCD()" );

	TempTotal = 0;

	for ( int i=0; i<8; i++ )
	{
		UpdateGeneralStatus();
		TempTotal += m_pvtCurrentCcdTemp;
	}

	TempAvg = TempTotal / 8;

	return TempAvg;
}

double CApnCamera::read_TempHeatsink()
{
	double	TempAvg;
	double	TempTotal;


	TempTotal = 0;

	for ( int i=0; i<8; i++ )
	{
		UpdateGeneralStatus();
		TempTotal += m_pvtCurrentHeatsinkTemp;
	}

	TempAvg = TempTotal / 8;

	return TempAvg;
}

Apn_FanMode	CApnCamera::read_FanMode()
{
	return m_pvtFanMode;
}

void CApnCamera::write_FanMode( Apn_FanMode FanMode )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_FanMode( FanMode = %d)", FanMode );
	AltaDebugOutputString( szOutputText );


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
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ShutterStrobePosition" );

	return m_pvtShutterStrobePosition;
}

void CApnCamera::write_ShutterStrobePosition( double Position )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_ShutterStrobePosition( Position = %f)", Position );
	AltaDebugOutputString( szOutputText );


	unsigned short RegVal;
	
	if ( Position < APN_STROBE_POSITION_MIN )
		Position = APN_STROBE_POSITION_MIN;

	RegVal = (unsigned short)((Position - APN_STROBE_POSITION_MIN) / APN_TIMER_RESOLUTION);

	Write( FPGA_REG_SHUTTER_STROBE_POSITION, RegVal );

	m_pvtShutterStrobePosition = Position;
}

double CApnCamera::read_ShutterStrobePeriod()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ShutterStrobePosition" );

	return m_pvtShutterStrobePeriod;
}

void CApnCamera::write_ShutterStrobePeriod( double Period )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_ShutterStrobePeriod( Period = %f)", Period );
	AltaDebugOutputString( szOutputText );


	unsigned short RegVal;

	if ( Period < APN_STROBE_PERIOD_MIN )
		Period = APN_STROBE_PERIOD_MIN;

	RegVal = (unsigned short)((Period - APN_STROBE_PERIOD_MIN) / APN_PERIOD_TIMER_RESOLUTION);

	Write( FPGA_REG_SHUTTER_STROBE_PERIOD, RegVal );
	
	m_pvtShutterStrobePeriod = Period;
}

double CApnCamera::read_ShutterCloseDelay()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ShutterCloseDelay" );

	return m_pvtShutterCloseDelay;
}

void CApnCamera::write_ShutterCloseDelay( double ShutterCloseDelay )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_ShutterCloseDelay( ShutterCloseDelay = %f)", ShutterCloseDelay );
	AltaDebugOutputString( szOutputText );


	unsigned short RegVal;

	if ( ShutterCloseDelay < APN_SHUTTER_CLOSE_DIFF )
		ShutterCloseDelay = APN_SHUTTER_CLOSE_DIFF;

	RegVal = (unsigned short)( (ShutterCloseDelay - APN_SHUTTER_CLOSE_DIFF) / APN_TIMER_RESOLUTION );

	Write( FPGA_REG_SHUTTER_CLOSE_DELAY, RegVal );

	m_pvtShutterCloseDelay = ShutterCloseDelay;
}

bool CApnCamera::read_SequenceBulkDownload()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_SequenceBulkDownload" );

	return m_pvtSequenceBulkDownload;
}

void CApnCamera::write_SequenceBulkDownload( bool SequenceBulkDownload )
{
	char szOutputText[128];
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::write_SequenceBulkDownload( SequenceBulkDownload = %d)", SequenceBulkDownload );
	AltaDebugOutputString( szOutputText );

	if ( GetCameraInterface() == Apn_Interface_NET )
	{
		m_pvtSequenceBulkDownload = true;
		return;
	}

	m_pvtSequenceBulkDownload = SequenceBulkDownload;
}

double CApnCamera::read_SequenceDelay()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_SequenceDelay" );

	return m_pvtSequenceDelay;
}

void CApnCamera::write_SequenceDelay( double Delay )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_SequenceDelay( Delay = %f)", Delay );
	AltaDebugOutputString( szOutputText );


	unsigned short RegVal;

	if ( Delay > APN_SEQUENCE_DELAY_MAXIMUM )
		Delay = APN_SEQUENCE_DELAY_MAXIMUM;

	if ( Delay < APN_SEQUENCE_DELAY_MINIMUM )
		Delay = APN_SEQUENCE_DELAY_MINIMUM;

	m_pvtSequenceDelay = Delay;

	RegVal = (unsigned short)(Delay / APN_SEQUENCE_DELAY_RESOLUTION);

	Write( FPGA_REG_SEQUENCE_DELAY, RegVal );
}

bool CApnCamera::read_VariableSequenceDelay()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_VariableSequenceDelay" );


	unsigned short RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	// variable delay occurs when the bit is 0
	return ( (RegVal & FPGA_BIT_DELAY_MODE) == 0 );	
}

void CApnCamera::write_VariableSequenceDelay( bool VariableSequenceDelay )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_VariableSequenceDelay( VariableSequenceDelay = %d)", VariableSequenceDelay );
	AltaDebugOutputString( szOutputText );


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
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImageCount" );


	return m_pvtImageCount;
}

void CApnCamera::write_ImageCount( unsigned short Count )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_ImageCount( Count = %d)", Count );
	AltaDebugOutputString( szOutputText );


	if ( Count == 0 )
		Count = 1;

	Write( FPGA_REG_IMAGE_COUNT, Count );
	
	m_pvtImageCount = Count;
}

unsigned short CApnCamera::read_FlushBinningV()
{
	return m_pvtFlushBinningV;
}

void CApnCamera::write_FlushBinningV( unsigned short FlushBinningV )
{
	// Do not allow FlushBinningV = 0
	if ( FlushBinningV == 0 ) FlushBinningV = 1;


	if ( FlushBinningV != m_pvtFlushBinningV )
	{
		ResetSystemNoFlush();

		Write( FPGA_REG_VFLUSH_BINNING, FlushBinningV );
		m_pvtFlushBinningV = FlushBinningV;

		ResetSystem();
	}
}

unsigned short CApnCamera::read_RoiBinningH()
{
	return m_pvtRoiBinningH;
}

void CApnCamera::write_RoiBinningH( unsigned short RoiBinningH )
{
	unsigned short NewRoiBinningH;

	if ( RoiBinningH == 0 )
		NewRoiBinningH = 1;
	else
		NewRoiBinningH = RoiBinningH;


	// Check to see if we actually need to update the binning
	if ( NewRoiBinningH != m_pvtRoiBinningH )
	{
		// Reset the camera
		// Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_RESET );
		ResetSystemNoFlush();

		LoadRoiPattern( NewRoiBinningH );
		m_pvtRoiBinningH = NewRoiBinningH;

		// Reset the camera and start flushing
		ResetSystem();
	}
}

unsigned short CApnCamera::read_RoiBinningV()
{
	return m_pvtRoiBinningV;
}

void CApnCamera::write_RoiBinningV( unsigned short RoiBinningV )
{
	unsigned short NewRoiBinningV;

	if ( RoiBinningV == 0 )
		NewRoiBinningV = 1;
	else
		NewRoiBinningV = RoiBinningV;


	// Check to see if we actually need to update the binning
	if ( NewRoiBinningV != m_pvtRoiBinningV )
	{
		m_pvtRoiBinningV = NewRoiBinningV;
	}
}

unsigned short CApnCamera::read_RoiPixelsH()
{
	return m_pvtRoiPixelsH;
}

void CApnCamera::write_RoiPixelsH( unsigned short RoiPixelsH )
{
	unsigned short NewRoiPixelsH;


	if ( RoiPixelsH == 0 )
		NewRoiPixelsH = 1;
	else
		NewRoiPixelsH = RoiPixelsH;

	m_pvtRoiPixelsH = NewRoiPixelsH;
}

unsigned short CApnCamera::read_RoiPixelsV()
{
	return m_pvtRoiPixelsV;
}

void CApnCamera::write_RoiPixelsV( unsigned short RoiPixelsV )
{
	unsigned short NewRoiPixelsV;


	if ( RoiPixelsV == 0 )
		NewRoiPixelsV = 1;
	else
		NewRoiPixelsV = RoiPixelsV;

	m_pvtRoiPixelsV = NewRoiPixelsV;
}

unsigned short CApnCamera::read_RoiStartX()
{
	return m_pvtRoiStartX;
}

void CApnCamera::write_RoiStartX( unsigned short RoiStartX )
{
	m_pvtRoiStartX = RoiStartX;
}

unsigned short CApnCamera::read_RoiStartY()
{
	return m_pvtRoiStartY;
}

void CApnCamera::write_RoiStartY( unsigned short RoiStartY )
{
	m_pvtRoiStartY = RoiStartY;
}

bool CApnCamera::read_DigitizeOverscan()
{
	return m_pvtDigitizeOverscan;
}

void CApnCamera::write_DigitizeOverscan( bool DigitizeOverscan )
{
	m_pvtDigitizeOverscan = DigitizeOverscan;
}

unsigned short CApnCamera::read_OverscanColumns()
{
	return m_ApnSensorInfo->m_OverscanColumns;
}

unsigned short CApnCamera::read_MaxBinningH()
{
	return APN_HBINNING_MAX;
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
	UpdateGeneralStatus();

	if ( m_pvtSequenceBulkDownload )
		return m_pvtSequenceCounter;
	else
		return m_pvtReadyFrame;
}

bool CApnCamera::read_ContinuousImaging()
{
	// CI requires v17 or higher firmware support
	if ( m_pvtFirmwareVersion < 17 )
		return false;


	unsigned short RegVal;
	Read( FPGA_REG_OP_B, RegVal );
	return ( (RegVal & FPGA_BIT_CONT_IMAGE_ENABLE) == 1 );	
}

void CApnCamera::write_ContinuousImaging( bool ContinuousImaging )
{
	// CI requires v17 or higher firmware support
	if ( m_pvtFirmwareVersion < 17 )
		return;


	unsigned short RegVal;

	Read( FPGA_REG_OP_B, RegVal );

	if ( ContinuousImaging )
		RegVal |= FPGA_BIT_CONT_IMAGE_ENABLE;		// enable
	else
		RegVal &= ~FPGA_BIT_CONT_IMAGE_ENABLE;		// disable

	Write( FPGA_REG_OP_B, RegVal );
}

unsigned short CApnCamera::read_TDICounter()
{
	unsigned short Counter;

	char szOutputText[128];

	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_TDICounter()" );


	UpdateGeneralStatus();

	if ( m_pvtSequenceBulkDownload )
		Counter = m_pvtTDICounter;
	else
		Counter = m_pvtReadyFrame;

	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::read_TDICounter() returning %d", Counter );
	AltaDebugOutputString( szOutputText );

	return Counter;
}

unsigned short CApnCamera::read_TDIRows()
{
	char szOutputText[128];

	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_TDIRows()" );

	return m_pvtTDIRows;

	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::read_TDIRows() returning %d", m_pvtTDIRows );
	AltaDebugOutputString( szOutputText );
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

unsigned short CApnCamera::read_TDIBinningV()
{
	return m_pvtTDIBinningV;
}

void CApnCamera::write_TDIBinningV( unsigned short TdiBinningV )
{
	if ( TdiBinningV == 0 )		// Make sure the TDI binning is at least 1
		TdiBinningV = 1;

	Write( FPGA_REG_TDI_BINNING, TdiBinningV );
	m_pvtTDIBinningV = TdiBinningV;
}

unsigned short CApnCamera::read_KineticsSections()
{
	return read_TDIRows();
}

void CApnCamera::write_KineticsSections( unsigned short KineticsSections )
{
	write_TDIRows( KineticsSections );
}

double CApnCamera::read_KineticsShiftInterval()
{
	return read_TDIRate();
}

void CApnCamera::write_KineticsShiftInterval( double KineticsShiftInterval )
{
	write_TDIRate( KineticsShiftInterval );
}

unsigned short CApnCamera::read_KineticsSectionHeight()
{
	return read_TDIBinningV();
}

void CApnCamera::write_KineticsSectionHeight( unsigned short KineticsSectionHeight )
{
	write_TDIBinningV( KineticsSectionHeight );
}

bool CApnCamera::read_TriggerNormalEach()
{
	// This trigger option requires v17 or higher firmware
	if ( m_pvtFirmwareVersion < 17 )
		return false;


	unsigned short RegVal;
	Read( FPGA_REG_OP_C, RegVal );
	return ( (RegVal & FPGA_BIT_IMAGE_TRIGGER_EACH) == 1 );	
}

void CApnCamera::write_TriggerNormalEach( bool TriggerNormalEach )
{
	// This trigger option requires v17 or higher firmware
	if ( m_pvtFirmwareVersion < 17 )
		return;

	// do not allow triggers on each progressive scanned image
	if ( m_pvtFastSequence )
		return;

	// If our current state var is equal to the new state, no need to do anything
	if ( m_pvtTriggerNormalEach == TriggerNormalEach )
		return;


	unsigned short RegVal, IoRegVal;

	Read( FPGA_REG_OP_C, RegVal );

	if ( TriggerNormalEach )
	{
		RegVal |= FPGA_BIT_IMAGE_TRIGGER_EACH;	// Enable

		// We only need to set the I/O port bit if it hasn't been set already
		if ( (!m_pvtTriggerNormalGroup) && (!m_pvtTriggerTdiKineticsEach) && (!m_pvtTriggerTdiKineticsGroup) )
		{
			IoRegVal = read_IoPortAssignment();
			IoRegVal |= 0x01;						// External trigger is pin one (bit zero)
			write_IoPortAssignment( IoRegVal );
		}
	}
	else
	{
		RegVal &= ~FPGA_BIT_IMAGE_TRIGGER_EACH;	// Disable

		// We only disable the I/O port bit if it is unused
		if ( (!m_pvtTriggerNormalGroup) && (!m_pvtTriggerTdiKineticsEach) && (!m_pvtTriggerTdiKineticsGroup) )
		{
			IoRegVal = read_IoPortAssignment();
			IoRegVal &= 0x3E;						// External trigger is pin one (bit zero)
			write_IoPortAssignment( IoRegVal );
		}
	}

	Write( FPGA_REG_OP_C, RegVal );

	m_pvtTriggerNormalEach = TriggerNormalEach;
}

bool CApnCamera::read_TriggerNormalGroup()
{
	// This trigger option requires v17 or higher firmware
	if ( m_pvtFirmwareVersion < 17 )
		return false;


	unsigned short RegVal;
	Read( FPGA_REG_OP_C, RegVal );
	return ( (RegVal & FPGA_BIT_IMAGE_TRIGGER_GROUP) == 1 );	
}

void CApnCamera::write_TriggerNormalGroup( bool TriggerNormalGroup )
{
	// This trigger option requires v17 or higher firmware
	if ( m_pvtFirmwareVersion < 17 )
		return;

	// If our current state var is equal to the new state, no need to do anything
	if ( m_pvtTriggerNormalGroup == TriggerNormalGroup )
		return;


	unsigned short RegVal, IoRegVal;

	Read( FPGA_REG_OP_C, RegVal );

	if ( TriggerNormalGroup )
	{
		RegVal |= FPGA_BIT_IMAGE_TRIGGER_GROUP;		// enable

		// We only need to set the I/O port bit if it hasn't been set already
		if ( (!m_pvtTriggerNormalEach) && (!m_pvtTriggerTdiKineticsEach) && (!m_pvtTriggerTdiKineticsGroup) )
		{
			IoRegVal = read_IoPortAssignment();
			IoRegVal |= 0x01;						// External trigger is pin one (bit zero)
			write_IoPortAssignment( IoRegVal );
		}
	}
	else
	{
		RegVal &= ~FPGA_BIT_IMAGE_TRIGGER_GROUP;		// disable

		// We only disable the I/O port bit if it is unused
		if ( (!m_pvtTriggerNormalEach) && (!m_pvtTriggerTdiKineticsEach) && (!m_pvtTriggerTdiKineticsGroup) )
		{
			IoRegVal = read_IoPortAssignment();
			IoRegVal &= 0x3E;						// External trigger is pin one (bit zero)
			write_IoPortAssignment( IoRegVal );
		}
	}

	Write( FPGA_REG_OP_C, RegVal );

	m_pvtTriggerNormalGroup = TriggerNormalGroup;
}

bool CApnCamera::read_TriggerTdiKineticsEach()
{
	// This trigger option requires v17 or higher firmware
	if ( m_pvtFirmwareVersion < 17 )
		return false;


	unsigned short RegVal;
	Read( FPGA_REG_OP_C, RegVal );
	return ( (RegVal & FPGA_BIT_TDI_TRIGGER_EACH) == 1 );	
}

void CApnCamera::write_TriggerTdiKineticsEach( bool TriggerTdiKineticsEach )
{
	// This trigger option requires v17 or higher firmware
	if ( m_pvtFirmwareVersion < 17 )
		return;

	// If our current state var is equal to the new state, no need to do anything
	if ( m_pvtTriggerTdiKineticsEach == TriggerTdiKineticsEach )
		return;


	unsigned short RegVal, IoRegVal;

	Read( FPGA_REG_OP_C, RegVal );

	if ( TriggerTdiKineticsEach )
	{
		RegVal |= FPGA_BIT_TDI_TRIGGER_EACH;		// enable

		// We only need to set the I/O port bit if it hasn't been set already
		if ( (!m_pvtTriggerNormalEach) && (!m_pvtTriggerNormalGroup) && (!m_pvtTriggerTdiKineticsGroup) )
		{
			IoRegVal = read_IoPortAssignment();
			IoRegVal |= 0x01;						// External trigger is pin one (bit zero)
			write_IoPortAssignment( IoRegVal );
		}
	}
	else
	{
		RegVal &= ~FPGA_BIT_TDI_TRIGGER_EACH;		// disable

		// We only disable the I/O port bit if it is unused
		if ( (!m_pvtTriggerNormalEach) && (!m_pvtTriggerNormalGroup) && (!m_pvtTriggerTdiKineticsGroup) )
		{
			IoRegVal = read_IoPortAssignment();
			IoRegVal &= 0x3E;						// External trigger is pin one (bit zero)
			write_IoPortAssignment( IoRegVal );
		}
	}

	Write( FPGA_REG_OP_C, RegVal );

	m_pvtTriggerTdiKineticsEach = TriggerTdiKineticsEach;
}

bool CApnCamera::read_TriggerTdiKineticsGroup()
{
	if ( m_pvtFirmwareVersion < 17 )
		return false;


	unsigned short RegVal;
	Read( FPGA_REG_OP_C, RegVal );
	return ( (RegVal & FPGA_BIT_TDI_TRIGGER_GROUP) == 1 );
}

void CApnCamera::write_TriggerTdiKineticsGroup( bool TriggerTdiKineticsGroup )
{
	// This trigger option requires v17 or higher firmware
	if ( m_pvtFirmwareVersion < 17 )
		return;

	// If our current state var is equal to the new state, no need to do anything
	if ( m_pvtTriggerTdiKineticsGroup == TriggerTdiKineticsGroup )
		return;


	unsigned short RegVal, IoRegVal;

	Read( FPGA_REG_OP_C, RegVal );

	if ( TriggerTdiKineticsGroup )
	{
		RegVal |= FPGA_BIT_TDI_TRIGGER_GROUP;		// enable

		// We only need to set the I/O port bit if it hasn't been set already
		if ( (!m_pvtTriggerNormalEach) && (!m_pvtTriggerNormalGroup) && (!m_pvtTriggerTdiKineticsEach) )
		{
			IoRegVal = read_IoPortAssignment();
			IoRegVal |= 0x01;						// External trigger is pin one (bit zero)
			write_IoPortAssignment( IoRegVal );
		}
	}
	else
	{
		RegVal &= ~FPGA_BIT_TDI_TRIGGER_GROUP;		// disable

		// We only disable the I/O port bit if it is unused
		if ( (!m_pvtTriggerNormalEach) && (!m_pvtTriggerNormalGroup) && (!m_pvtTriggerTdiKineticsEach) )
		{
			IoRegVal = read_IoPortAssignment();
			IoRegVal &= 0x3E;						// External trigger is pin one (bit zero)
			write_IoPortAssignment( IoRegVal );
		}
	}

	Write( FPGA_REG_OP_C, RegVal );

	m_pvtTriggerTdiKineticsGroup = TriggerTdiKineticsGroup;
}

bool CApnCamera::read_ExposureTriggerEach()
{
	return m_pvtExposureTriggerEach;
}

bool CApnCamera::read_ExposureTriggerGroup()
{
	return m_pvtExposureTriggerGroup;
}

bool CApnCamera::read_ExposureExternalShutter()
{
	return m_pvtExposureExternalShutter;
}

unsigned short CApnCamera::read_IoPortAssignment()
{
	return m_pvtIoPortAssignment;
}

void CApnCamera::write_IoPortAssignment( unsigned short IoPortAssignment )
{
	IoPortAssignment &= FPGA_MASK_IO_PORT_ASSIGNMENT;
	Write( FPGA_REG_IO_PORT_ASSIGNMENT, IoPortAssignment );
	m_pvtIoPortAssignment = IoPortAssignment;
}

unsigned short CApnCamera::read_IoPortDirection()
{
	return m_pvtIoPortDirection;
}

void CApnCamera::write_IoPortDirection( unsigned short IoPortDirection )
{
	IoPortDirection &= FPGA_MASK_IO_PORT_DIRECTION;
	Write( FPGA_REG_IO_PORT_DIRECTION, IoPortDirection );
	m_pvtIoPortDirection = IoPortDirection;
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

unsigned short CApnCamera::read_TwelveBitOffset()
{
	return m_pvtTwelveBitOffset;
}

void CApnCamera::write_TwelveBitOffset( unsigned short TwelveBitOffset )
{
	unsigned short NewVal;
	unsigned short StartVal;
	unsigned short FirstBit;


	NewVal		= 0x0;
	StartVal	= TwelveBitOffset & 0xFF;

	for ( int i=0; i<8; i++ )
	{
		FirstBit = ( StartVal & 0x0001 );
		NewVal |= ( FirstBit << (10-i) );
		StartVal = StartVal >> 1;
	}

	NewVal |= 0x2000;

	Write( FPGA_REG_AD_CONFIG_DATA, NewVal );
	Write( FPGA_REG_COMMAND_B,		FPGA_BIT_CMD_AD_CONFIG );

	m_pvtTwelveBitOffset = TwelveBitOffset;
}

bool CApnCamera::read_DualReadout()
{
	return m_pvtDualReadout;
}

void CApnCamera::write_DualReadout( bool DualReadout )
{
	// Dual A/D readout requires v17 or higher firmware
	if ( m_pvtFirmwareVersion < 17 )
		return;


	unsigned short RegVal;

	Read( FPGA_REG_OP_A, RegVal );

	if ( DualReadout )
		RegVal |= FPGA_BIT_DUAL_AD_READOUT;		// enable
	else
		RegVal &= ~FPGA_BIT_DUAL_AD_READOUT;	// disable

	Write( FPGA_REG_OP_A, RegVal );

	m_pvtDualReadout = DualReadout;
}

unsigned short CApnCamera::read_Alta2ADGainSixteen()
{
	return m_pvtAlta2GainSixteen;
}

void CApnCamera::write_Alta2ADGainSixteen( unsigned short GainValue )
{
	unsigned short NewVal;


	NewVal = GainValue & 0x003F;

	NewVal |= 0x2000;

	Write( FPGA_REG_AD_CONFIG_DATA, NewVal );
	Write( FPGA_REG_COMMAND_B,		FPGA_BIT_CMD_AD_CONFIG );

	m_pvtAlta2GainSixteen = GainValue & 0x003F;
}

unsigned short CApnCamera::read_Alta2ADOffsetSixteen()
{
	return m_pvtAlta2OffsetSixteen;
}

void CApnCamera::write_Alta2ADOffsetSixteen( unsigned short OffsetValue )
{
	unsigned short NewVal;


	NewVal = OffsetValue & 0x01FF;

	NewVal |= 0x5000;

	Write( FPGA_REG_AD_CONFIG_DATA, NewVal );
	Write( FPGA_REG_COMMAND_B,		FPGA_BIT_CMD_AD_CONFIG );

	m_pvtAlta2OffsetSixteen = OffsetValue & 0x01FF;
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

	if ( m_pvtDataBits == Apn_Resolution_SixteenBit )
	{
		WriteHorizontalPattern( &m_ApnSensorInfo->m_ClampPatternSixteen, 
								FPGA_REG_HCLAMP_INPUT, 
								1 );
	}
	else if ( m_pvtDataBits == Apn_Resolution_TwelveBit )
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

	if ( m_pvtDataBits == Apn_Resolution_SixteenBit )
	{
		WriteHorizontalPattern( &m_ApnSensorInfo->m_SkipPatternSixteen, 
								FPGA_REG_HSKIP_INPUT, 
								1 );
	}
	else if ( m_pvtDataBits == Apn_Resolution_TwelveBit )
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

	if ( m_pvtDataBits == Apn_Resolution_SixteenBit )
	{
		WriteHorizontalPattern( &m_ApnSensorInfo->m_RoiPatternSixteen, 
								FPGA_REG_HRAM_INPUT, 
								binning );
	}
	else if ( m_pvtDataBits == Apn_Resolution_TwelveBit )
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
	unsigned short	RegVal;
	unsigned short	ShutterDelay;

	unsigned short	PreRoiRows, PostRoiRows;
	unsigned short	PreRoiVBinning, PostRoiVBinning;
	unsigned short	UnbinnedRoiY;		// Vertical ROI pixels
	
	double			CloseDelay;


	// Read the Camera ID register
	Read( FPGA_REG_CAMERA_ID, m_pvtCameraID );
	m_pvtCameraID &= FPGA_MASK_CAMERA_ID;

	// Read and store the firmware version for reference
	Read( FPGA_REG_FIRMWARE_REV, m_pvtFirmwareVersion );

	// Look up the ID and dynamically create the m_ApnSensorInfo object
	switch ( m_pvtCameraID )
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
	case APN_ALTA_KAF1001E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF1001E;
		break;
	case APN_ALTA_KAF1001ENS_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF1001ENS;
		break;
	case APN_ALTA_KAF10011105_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF10011105;
		break;
	case APN_ALTA_KAF3200E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF3200E;
		break;
	case APN_ALTA_KAF6303E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF6303E;
		break;
	case APN_ALTA_KAF16801E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF16801E;
		break;
        case APN_ALTA_KAF16803_CAM_ID:
                m_ApnSensorInfo = new CApnCamData_KAF16803;
                break;
	case APN_ALTA_KAF09000_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF09000;
		break;
	case APN_ALTA_KAF0401EB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF0401EB;
		break;
	case APN_ALTA_KAF1602EB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF1602EB;
		break;
	case APN_ALTA_KAF0261EB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF0261EB;
		break;
	case APN_ALTA_KAF1301EB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF1301EB;
		break;
	case APN_ALTA_KAF1001EB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF1001EB;
		break;
	case APN_ALTA_KAF6303EB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF6303EB;
		break;
	case APN_ALTA_KAF3200EB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF3200EB;
		break;

	case APN_ALTA_TH7899_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_TH7899;
		break;

	case APN_ALTA_CCD4710_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD4710;
		break;
	case APN_ALTA_CCD4710ALT_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD4710ALT;
		break;
	case APN_ALTA_CCD4240_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD4240;
		break;
	case APN_ALTA_CCD5710_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD5710;
		break;
	case APN_ALTA_CCD3011_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD3011;
		break;
	case APN_ALTA_CCD5520_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD5520;
		break;
	case APN_ALTA_CCD4720_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD4720;
		break;
	case APN_ALTA_CCD7700_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD7700;
		break;

	case APN_ALTA_CCD4710B_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD4710B;
		break;
	case APN_ALTA_CCD4240B_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD4240B;
		break;
	case APN_ALTA_CCD5710B_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD5710B;
		break;
	case APN_ALTA_CCD3011B_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD3011B;
		break;
	case APN_ALTA_CCD5520B_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD5520B;
		break;
	case APN_ALTA_CCD4720B_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD4720B;
		break;
	case APN_ALTA_CCD7700B_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD7700B;
		break;

	case APN_ALTA_KAI2001ML_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI2001ML;
		break;
	case APN_ALTA_KAI2020ML_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI2020ML;
		break;
	case APN_ALTA_KAI4020ML_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI4020ML;
		break;
	case APN_ALTA_KAI11000ML_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI11000ML;
		break;
	case APN_ALTA_KAI2001CL_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI2001CL;
		break;
	case APN_ALTA_KAI2020CL_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI2020CL;
		break;
	case APN_ALTA_KAI4020CL_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI4020CL;
		break;
	case APN_ALTA_KAI11000CL_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI11000CL;
		break;

	case APN_ALTA_KAI2020MLB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI2020MLB;
		break;
	case APN_ALTA_KAI4020MLB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI4020MLB;
		break;
	case APN_ALTA_KAI2020CLB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI2020CLB;
		break;
	case APN_ALTA_KAI4020CLB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI4020CLB;
		break;
	
	default:
		return 1;
		break;
	}

	// New Reset command
	ResetSystemNoFlush();

	// we created the object, now set everything
	m_ApnSensorInfo->Initialize();

	// Initialize private variables
	write_CameraMode( Apn_CameraMode_Normal );

	write_DigitizeOverscan( false );

	write_DisableFlushCommands( false );
	write_DisablePostExposeFlushing( false );

	m_pvtDataBits				= Apn_Resolution_SixteenBit;

	m_pvtExternalShutter		= false;
	m_pvtNetworkTransferMode	= Apn_NetworkMode_Tcp;

	// Initialize variables used for imaging
	m_pvtRoiStartX		= 0;
	m_pvtRoiStartY		= 0;
	m_pvtRoiPixelsH		= m_ApnSensorInfo->m_ImagingColumns;
	m_pvtRoiPixelsV		= m_ApnSensorInfo->m_ImagingRows;

	m_pvtRoiBinningH	= 1;
	m_pvtRoiBinningV	= 1;

        fprintf(stderr,"Camera ID is %u\n",m_pvtCameraID);
        fprintf(stderr,"sensor = %s\n",                 m_ApnSensorInfo->m_Sensor);
	fprintf(stderr,"model = %s\n",m_ApnSensorInfo->m_CameraModel);
        fprintf(stderr,"interline = %u\n",m_ApnSensorInfo->m_InterlineCCD);
        fprintf(stderr,"serialA = %u\n",m_ApnSensorInfo->m_SupportsSerialA);
        fprintf(stderr,"serialB = %u\n",m_ApnSensorInfo->m_SupportsSerialB);
        fprintf(stderr,"ccdtype = %u\n",m_ApnSensorInfo->m_SensorTypeCCD);
        fprintf(stderr,"Tcolumns = %u\n",m_ApnSensorInfo->m_TotalColumns);
        fprintf(stderr,"ImgColumns = %u\n",m_ApnSensorInfo->m_ImagingColumns);
        fprintf(stderr,"ClampColumns = %u\n",m_ApnSensorInfo->m_ClampColumns);
        fprintf(stderr,"PreRoiSColumns = %u\n",m_ApnSensorInfo->m_PreRoiSkipColumns);
        fprintf(stderr,"PostRoiSColumns = %u\n",m_ApnSensorInfo->m_PostRoiSkipColumns);
        fprintf(stderr,"OverscanColumns = %u\n",m_ApnSensorInfo->m_OverscanColumns);
        fprintf(stderr,"TRows = %u\n",m_ApnSensorInfo->m_TotalRows);
        fprintf(stderr,"ImgRows = %u\n",m_ApnSensorInfo->m_ImagingRows);
        fprintf(stderr,"UnderscanRows = %u\n",m_ApnSensorInfo->m_UnderscanRows);
        fprintf(stderr,"OverscanRows = %u\n",m_ApnSensorInfo->m_OverscanRows);
        fprintf(stderr,"VFlushBinning = %u\n",m_ApnSensorInfo->m_VFlushBinning);
        fprintf(stderr,"HFlushDisable = %u\n",m_ApnSensorInfo->m_HFlushDisable);
        fprintf(stderr,"ShutterCloseDelay = %u\n",m_ApnSensorInfo->m_ShutterCloseDelay);
        fprintf(stderr,"PixelSizeX = %lf\n",m_ApnSensorInfo->m_PixelSizeX);
        fprintf(stderr,"PixelSizeY = %lf\n",m_ApnSensorInfo->m_PixelSizeY);
        fprintf(stderr,"Color = %u\n",m_ApnSensorInfo->m_Color);
//      fprintf(stderr,"ReportedGainTwelveBit = %lf\n",m_ApnSensorInfo->m_ReportedGainTwelveBit);
        fprintf(stderr,"ReportedGainSixteenBit = %lf\n",m_ApnSensorInfo->m_ReportedGainSixteenBit);
        fprintf(stderr,"MinSuggestedExpTime = %lf\n",m_ApnSensorInfo->m_MinSuggestedExpTime);
        fprintf(stderr,"CoolingSupported = %u\n",m_ApnSensorInfo->m_CoolingSupported);
        fprintf(stderr,"RegulatedCoolingSupported = %u\n",m_ApnSensorInfo->m_RegulatedCoolingSupported);
        fprintf(stderr,"TempSetPoint = %lf\n",m_ApnSensorInfo->m_TempSetPoint);
//      fprintf(stderr,"TempRegRate = %u\n",m_ApnSensorInfo->m_TempRegRate);
        fprintf(stderr,"TempRampRateOne = %u\n",m_ApnSensorInfo->m_TempRampRateOne);
        fprintf(stderr,"TempRampRateTwo = %u\n",m_ApnSensorInfo->m_TempRampRateTwo);
        fprintf(stderr,"TempBackoffPoint = %lf\n",m_ApnSensorInfo->m_TempBackoffPoint);
        fprintf(stderr,"DefaultGainTwelveBit = %u\n",m_ApnSensorInfo->m_DefaultGainTwelveBit);
        fprintf(stderr,"DefaultOffsetTwelveBit = %u\n",m_ApnSensorInfo->m_DefaultOffsetTwelveBit);
        fprintf(stderr,"DefaultRVoltage = %u\n",m_ApnSensorInfo->m_DefaultRVoltage);
                                                                           
                                                                           
                                                                           
        fprintf(stderr,"RoiPixelsH is %u\n",m_pvtRoiPixelsH);
        fprintf(stderr,"RoiPixelsV is %u\n",m_pvtRoiPixelsV);
                                                                           

	// Issue a clear command, so the registers are zeroed out
	// This will put the camera in a known state for us.
	Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_CLEAR_ALL );

	// Reset the camera
	ResetSystemNoFlush();

	// Load Inversion Masks
	Write( FPGA_REG_VRAM_INV_MASK, m_ApnSensorInfo->m_VerticalPattern.Mask );
	Write( FPGA_REG_HRAM_INV_MASK, m_ApnSensorInfo->m_RoiPatternSixteen.Mask );

	// Load Pattern Files
	LoadVerticalPattern();
	LoadClampPattern();
	LoadSkipPattern();
	LoadRoiPattern( m_pvtRoiBinningH );

	// Program default camera settings
	Write( FPGA_REG_CLAMP_COUNT,		m_ApnSensorInfo->m_ClampColumns );	
	Write( FPGA_REG_PREROI_SKIP_COUNT,	m_ApnSensorInfo->m_PreRoiSkipColumns );	
	Write( FPGA_REG_ROI_COUNT,			m_ApnSensorInfo->m_ImagingColumns );	
	Write( FPGA_REG_POSTROI_SKIP_COUNT,	m_ApnSensorInfo->m_PostRoiSkipColumns +
										m_ApnSensorInfo->m_OverscanColumns );		
	
	// Since the default state of m_DigitizeOverscan is false, set the count to zero.
	Write( FPGA_REG_OVERSCAN_COUNT,		0x0 );	

	// Now calculate the vertical settings
	UnbinnedRoiY	= m_pvtRoiPixelsV * m_pvtRoiBinningV;

	PreRoiRows		= m_ApnSensorInfo->m_UnderscanRows + 
					  m_pvtRoiStartY;
	
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
	if ( m_pvtFirmwareVersion < 11 )
	{
		Write( FPGA_REG_A1_ROW_COUNT, PreRoiRows );	
		Write( FPGA_REG_A1_VBINNING,  PreRoiVBinning );
		
		Write( FPGA_REG_A2_ROW_COUNT, m_pvtRoiPixelsV );	
		Write( FPGA_REG_A2_VBINNING,  (m_pvtRoiBinningV | FPGA_BIT_ARRAY_DIGITIZE) );	
		
		Write( FPGA_REG_A3_ROW_COUNT, PostRoiRows );	
		Write( FPGA_REG_A3_VBINNING,  PostRoiVBinning );	
	}
	else
	{
		Write( FPGA_REG_A1_ROW_COUNT, 0 );	
		Write( FPGA_REG_A1_VBINNING,  0 );
		
		Write( FPGA_REG_A2_ROW_COUNT, PreRoiRows );	
		Write( FPGA_REG_A2_VBINNING,  PreRoiVBinning );
		
		Write( FPGA_REG_A3_ROW_COUNT, m_pvtRoiPixelsV );	
		Write( FPGA_REG_A3_VBINNING,  (m_pvtRoiBinningV | FPGA_BIT_ARRAY_DIGITIZE) );	
		
		Write( FPGA_REG_A4_ROW_COUNT, 0 );	
		Write( FPGA_REG_A4_VBINNING,  0 );	
		
		Write( FPGA_REG_A5_ROW_COUNT, PostRoiRows );	
		Write( FPGA_REG_A5_VBINNING,  PostRoiVBinning );	
	}

	// we don't use write_FlushBinningV() here because that would include additional RESETs
	m_pvtFlushBinningV = m_ApnSensorInfo->m_VFlushBinning;
	Write( FPGA_REG_VFLUSH_BINNING, m_pvtFlushBinningV );

	if ( m_ApnSensorInfo->m_ShutterCloseDelay == 0 )
	{
		// This is the case for interline cameras
		ShutterDelay = 0;

		m_pvtShutterCloseDelay = ShutterDelay;
	}
	else
	{
		CloseDelay	 = (double)m_ApnSensorInfo->m_ShutterCloseDelay / 1000;

		m_pvtShutterCloseDelay = CloseDelay;

		ShutterDelay = (unsigned short)
			( (CloseDelay - APN_SHUTTER_CLOSE_DIFF) / APN_TIMER_RESOLUTION );
	}

	if ( m_ApnSensorInfo->m_HFlushDisable )
	{
		Read( FPGA_REG_OP_A, RegVal );

		RegVal |= FPGA_BIT_DISABLE_H_CLK; 
		
		Write( FPGA_REG_OP_A, RegVal );
	}

	// If we are a USB2 camera, set all the 12bit variables for the 12bit
	// A/D processor
	if ( m_ApnSensorInfo->m_PrimaryADType != ApnAdType_None )
	{
		if ( GetCameraInterface() == Apn_Interface_USB )
		{
			if ( m_ApnSensorInfo->m_PrimaryADType == ApnAdType_Alta2_Sixteen )
			{
				InitAlta2AD();
				write_Alta2ADGainSixteen( m_ApnSensorInfo->m_DefaultGainTwelveBit );
				write_Alta2ADOffsetSixteen( m_ApnSensorInfo->m_DefaultOffsetTwelveBit );
			}
		}
	}
	if ( m_ApnSensorInfo->m_AlternativeADType != ApnAdType_None )
	{
		if ( GetCameraInterface() == Apn_Interface_USB )
		{
			InitTwelveBitAD();
			write_TwelveBitGain( m_ApnSensorInfo->m_DefaultGainTwelveBit );
			write_TwelveBitOffset( m_ApnSensorInfo->m_DefaultOffsetTwelveBit );
		}
	}

	// Reset the camera and start flushing
	ResetSystem();

	write_SequenceBulkDownload( true );

	write_ImageCount( 1 );
	write_SequenceDelay( 0.000327 );
	write_VariableSequenceDelay( true );

	Write( FPGA_REG_SHUTTER_CLOSE_DELAY, ShutterDelay );

	// Set the Fan State.  Setting the private var first to make sure the write_FanMode
	// call thinks we're doing a state transition.
	// On write_FanMode return, our state will be Apn_FanMode_Medium
	m_pvtFanMode = Apn_FanMode_Off;		// we're going to set this
	write_FanMode( Apn_FanMode_Low );

	// Initialize the LED states and the LED mode.  There is nothing to output
	// to the device since we issued our CLEAR early in the init() process, and 
	// we are now in a known state.
	m_pvtLedStateA	= Apn_LedState_Expose;
	m_pvtLedStateB	= Apn_LedState_Expose;
	m_pvtLedMode	= Apn_LedMode_EnableAll;

	// The CLEAR puts many vars into their default state
	m_pvtTriggerNormalEach			= false;
	m_pvtTriggerNormalGroup			= false;
	m_pvtTriggerTdiKineticsEach		= false;
	m_pvtTriggerTdiKineticsGroup	= false;

	m_pvtFastSequence				= false;

	// Default value for test LED is 0%
	m_pvtTestLedBrightness = 0.0;

	// Default values for I/O Port - the CLEAR op doesn't clear these values
	// This will also init our private vars to 0x0
	write_IoPortAssignment( 0x0 );
	write_IoPortDirection( 0x0 );

	// Set the default TDI variables.  These also will automatically initialize
	// the "virtual" kinetics mode variables.
	write_TDIRate( APN_TDI_RATE_DEFAULT );
	write_TDIRows( 1 );
	write_TDIBinningV( 1 );

	// Set the shutter strobe values to their defaults
	write_ShutterStrobePeriod( APN_STROBE_PERIOD_DEFAULT );
	write_ShutterStrobePosition( APN_STROBE_POSITION_DEFAULT );

	// Program our initial cooler values.  The only cooler value that we reset
	// at init time is the backoff point.  Everything else is left untouched, and
	// state information is determined from the camera controller.
	m_pvtCoolerBackoffPoint = m_ApnSensorInfo->m_TempBackoffPoint;
	write_CoolerBackoffPoint( m_pvtCoolerBackoffPoint );
	Write( FPGA_REG_TEMP_RAMP_DOWN_A,	m_ApnSensorInfo->m_TempRampRateOne );
	Write( FPGA_REG_TEMP_RAMP_DOWN_B,	m_ApnSensorInfo->m_TempRampRateTwo );
	// the cooler code not only determines the m_pvtCoolerEnable state, but
	// also implicitly calls UpdateGeneralStatus() as part of read_CoolerStatus()
	if ( read_CoolerStatus() == Apn_CoolerStatus_Off )
		m_pvtCoolerEnable = false;
	else
		m_pvtCoolerEnable = true;

	m_pvtImageInProgress	= false;
	m_pvtImageReady			= false;

	m_pvtMostRecentFrame	= 0;
	m_pvtReadyFrame			= 0;
	m_pvtCurrentFrame		= 0;

	return 0;
}

long CApnCamera::InitTwelveBitAD()
{
	// Write( FPGA_REG_AD_CONFIG_DATA, 0x0028 );
	Write( FPGA_REG_AD_CONFIG_DATA, 0x0008 );
	Write( FPGA_REG_COMMAND_B,		FPGA_BIT_CMD_AD_CONFIG );

	return 0;
}

long CApnCamera::InitAlta2AD()
{
	Write( FPGA_REG_AD_CONFIG_DATA, 0x0058 );
	Write( FPGA_REG_COMMAND_B,		FPGA_BIT_CMD_AD_CONFIG );
	
	Write( FPGA_REG_AD_CONFIG_DATA, 0x10C0 );
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
	unsigned short	MostRecentFrame;
	unsigned short	ReadyFrame;
	unsigned short	CurrentFrame;


//	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::UpdateGeneralStatus()" );

	
	// Read the general status register of the device
	m_pvtQueryStatusRetVal = QueryStatusRegs( StatusReg, 
											  HeatsinkTempReg, 
											  CcdTempReg, 
											  CoolerDriveReg,
											  VoltageReg,
											  TdiCounterReg,
											  SequenceCounterReg,
											  MostRecentFrame,
											  ReadyFrame,
											  CurrentFrame );

	/*
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::UpdateGeneralStatus() -> StatusReg = 0x%04X", StatusReg );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::UpdateGeneralStatus() -> HeatsinkTempReg = 0x%04X", HeatsinkTempReg );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::UpdateGeneralStatus() -> CcdTempReg = 0x%04X", CcdTempReg );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::UpdateGeneralStatus() -> CoolerDriveReg = 0x%04X", CoolerDriveReg );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::UpdateGeneralStatus() -> VoltageReg = 0x%04X", VoltageReg );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::UpdateGeneralStatus() -> TdiCounterReg = 0x%04X", TdiCounterReg );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::UpdateGeneralStatus() -> SequenceCounterReg = 0x%04X", SequenceCounterReg );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::UpdateGeneralStatus() -> MostRecentFrame = %d", MostRecentFrame );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::UpdateGeneralStatus() -> ReadyFrame = %d", ReadyFrame );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::UpdateGeneralStatus() -> CurrentFrame = %d", CurrentFrame );
	AltaDebugOutputString( szOutputText );
        */	

	m_pvtStatusReg	= StatusReg;

	HeatsinkTempReg &= FPGA_MASK_TEMP_PARAMS;
	CcdTempReg		&= FPGA_MASK_TEMP_PARAMS;
	CoolerDriveReg	&= FPGA_MASK_TEMP_PARAMS;
	VoltageReg		&= FPGA_MASK_INPUT_VOLTAGE;

	if ( CoolerDriveReg > 3200 )
		m_pvtCoolerDrive = 100.0;
	else
		m_pvtCoolerDrive = ( (double)(CoolerDriveReg - 600) / 2600.0 ) * 100.0;
	
	// Don't return a negative value
	if ( m_pvtCoolerDrive < 0.0 )
		m_pvtCoolerDrive = 0.0;

	m_pvtCurrentCcdTemp			= ( (CcdTempReg - APN_TEMP_SETPOINT_ZERO_POINT) 
									* APN_TEMP_DEGREES_PER_BIT );

	m_pvtCurrentHeatsinkTemp	= ( (HeatsinkTempReg - APN_TEMP_HEATSINK_ZERO_POINT) 
									* APN_TEMP_DEGREES_PER_BIT );
	
	m_pvtInputVoltage			= VoltageReg * APN_VOLTAGE_RESOLUTION;

	// Update ShutterState
	m_pvtShutterState = ( (m_pvtStatusReg & FPGA_BIT_STATUS_SHUTTER_OPEN) != 0 );

	// Update counters
	m_pvtSequenceCounter	= SequenceCounterReg;
	m_pvtTDICounter			= TdiCounterReg;

	// Update USB frame info (for images in a sequence)
	// Network systems will just set these vars to zero (they are not used)
	m_pvtMostRecentFrame	= MostRecentFrame;
	m_pvtReadyFrame			= ReadyFrame;
	m_pvtCurrentFrame		= CurrentFrame;
}


bool CApnCamera::ImageReady()
{
	return m_pvtImageReady;
}


bool CApnCamera::ImageInProgress()
{
	return m_pvtImageInProgress;
}


void CApnCamera::SignalImagingDone()
{
	m_pvtImageInProgress = false;
}
