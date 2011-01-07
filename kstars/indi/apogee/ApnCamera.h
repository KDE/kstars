/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
// ApnCamera.h: interface for the CApnCamera class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_APNCAMERA_H__CF513996_359F_4103_BBA6_2C730AE2C301__INCLUDED_)
#define AFX_APNCAMERA_H__CF513996_359F_4103_BBA6_2C730AE2C301__INCLUDED_

#include "Apogee.h"
#include "Apn.h"
#include "FpgaRegs.h"

#include "ApnCamData.h"
#include "ApnCamData_CCD3011HS.h"
#include "ApnCamData_CCD3011LS.h"
#include "ApnCamData_CCD4240HS.h"
#include "ApnCamData_CCD4240LS.h"
#include "ApnCamData_CCD4710HS.h"
#include "ApnCamData_CCD4710LS.h"
#include "ApnCamData_CCD4720HS.h"
#include "ApnCamData_CCD4720LS.h"
#include "ApnCamData_CCD5520HS.h"
#include "ApnCamData_CCD5520LS.h"
#include "ApnCamData_CCD5710HS.h"
#include "ApnCamData_CCD5710LS.h"
#include "ApnCamData_CCD7700HS.h"
#include "ApnCamData_CCD7700LS.h"
#include "ApnCamData_KAF0261E.h"
#include "ApnCamData_KAF0401E.h"
#include "ApnCamData_KAF1001E.h"
#include "ApnCamData_KAF1301E.h"
#include "ApnCamData_KAF1401E.h"
#include "ApnCamData_KAF1602E.h"
#include "ApnCamData_KAF16801E.h"
#include "ApnCamData_KAF3200E.h"
#include "ApnCamData_KAF4202.h"
#include "ApnCamData_KAF6303E.h"
#include "ApnCamData_TH7899.h"

#include "ApnCamData_CCD4710LS2.h"
#include "ApnCamData_CCD4710LS3.h"
#include "ApnCamData_CCD4710LS4.h"
#include "ApnCamData_CCD4710LS5.h"


class CApnCamera  
{
public:
	CApnCamera();
	~CApnCamera();

	bool InitDriver( unsigned long	CamIdA, 
							 unsigned short	CamIdB, 
							 unsigned long	Option );

	bool CloseDriver();
	long PreStartExpose( unsigned short BitsPerPixel );
	long PostStopExposure( bool DigitizeData );

	bool GetImageData( unsigned short *pImageData, 
							   unsigned short &Width,
							   unsigned short &Height,
							   unsigned long  &Count );

	bool GetLineData( unsigned short *pLineBuffer,
							  unsigned short &Size );		

	long Read( unsigned short reg, unsigned short& val );
	long Write( unsigned short reg, unsigned short val );

	long WriteMultiSRMD( unsigned short reg, 
								 unsigned short val[], 
								 unsigned short count );
	
	long WriteMultiMRMD( unsigned short reg[], 
								 unsigned short val[], 
								 unsigned short count );
	
	long QueryStatusRegs( unsigned short& StatusReg,
								  unsigned short& HeatsinkTempReg,
								  unsigned short& CcdTempReg,
								  unsigned short& CoolerDriveReg,
								  unsigned short& VoltageReg,
								  unsigned short& TdiCounter,
								  unsigned short& SequenceCounter );

        void SetNetworkTransferMode( Apn_NetworkMode TransferMode );

	long InitDefaults();

	bool Expose( double Duration, bool Light );
	bool BufferImage(char *bufferName );
	bool BufferDriftScan(char *bufferName, int delay, int rowCount, int nblock , int npipe);

	bool StopExposure( bool DigitizeData );
	
	bool ResetSystem();
	bool PauseTimer( bool PauseState );
	

	unsigned short		GetExposurePixelsH();
	unsigned short		GetExposurePixelsV();

	bool				read_Present();
	unsigned short		read_FirmwareVersion();

	bool				read_ShutterState();
	bool				read_DisableShutter();
	void				write_DisableShutter( bool DisableShutter );
	bool				read_ForceShutterOpen();
	void				write_ForceShutterOpen( bool ForceShutterOpen );
	bool				read_ShutterAmpControl();
	void				write_ShutterAmpControl( bool ShutterAmpControl );
	
	bool				read_ExternalIoReadout();
	void				write_ExternalIoReadout( bool ExternalIoReadout );
	bool				read_FastSequence();
	void				write_FastSequence( bool FastSequence );

	Apn_CameraMode		read_CameraMode();
	void				write_CameraMode( Apn_CameraMode CameraMode );

	void				write_DataBits( Apn_Resolution BitResolution );

	Apn_Status			read_ImagingStatus();

	Apn_LedMode			read_LedMode();
	void				write_LedMode( Apn_LedMode LedMode );
	Apn_LedState		read_LedState( unsigned short LedId );
	void				write_LedState( unsigned short LedId, Apn_LedState LedState );

	bool				read_CoolerEnable();
	void				write_CoolerEnable( bool CoolerEnable );
	Apn_CoolerStatus	read_CoolerStatus();
	double				read_CoolerSetPoint();
	void				write_CoolerSetPoint( double SetPoint );
	double				read_CoolerBackoffPoint();
	void				write_CoolerBackoffPoint( double BackoffPoint );
	double				read_CoolerDrive();
	double				read_TempCCD();
	double				read_TempHeatsink();
	Apn_FanMode			read_FanMode();
	void				write_FanMode( Apn_FanMode FanMode );

	void				write_RoiBinningH( unsigned short BinningH );
	void				write_RoiBinningV( unsigned short BinningV );

	void				write_RoiPixelsV( unsigned short PixelsV );

	void				write_RoiStartY( unsigned short StartY );

        unsigned short          read_MaxBinningV();
	unsigned short		read_OverscanColumns();

	double				read_ShutterStrobePosition();
	void				write_ShutterStrobePosition( double Position );
	double				read_ShutterStrobePeriod();
	void				write_ShutterStrobePeriod( double Period );

	double				read_SequenceDelay();
	void				write_SequenceDelay( double Delay );
	bool				read_VariableSequenceDelay();
	void				write_VariableSequenceDelay( bool VariableSequenceDelay );
	unsigned short		read_ImageCount();
	void				write_ImageCount( unsigned short Count );

	unsigned short		read_SequenceCounter();
	unsigned short		read_TDICounter();
	unsigned short		read_TDIRows();
	void				write_TDIRows( unsigned short TdiRows );
	double				read_TDIRate();
	void				write_TDIRate( double TdiRate );
	unsigned short		read_IoPortAssignment();
	void				write_IoPortAssignment( unsigned short IoPortAssignment );
	unsigned short		read_IoPortDirection();
	void				write_IoPortDirection( unsigned short IoPortDirection );
	unsigned short		read_IoPortData();
	void				write_IoPortData( unsigned short IoPortData );

	unsigned short		read_TwelveBitGain();
	void				write_TwelveBitGain( unsigned short TwelveBitGain );

	double				read_InputVoltage();
	long				read_AvailableMemory();

	double				read_MaxExposureTime();

	Apn_NetworkMode		read_NetworkTransferMode();
	void				write_NetworkTransferMode( Apn_NetworkMode TransferMode );

	double				read_TestLedBrightness();
	void				write_TestLedBrightness( double TestLedBrightness );


	// Public helper function
	bool				ImageReady();
	void				SignalImagingDone();


	// Variables
	Apn_Interface	m_CameraInterface;

	CApnCamData		*m_ApnSensorInfo;

	unsigned short	m_RoiStartX, m_RoiStartY;
	unsigned short	m_RoiPixelsH, m_RoiPixelsV;
	unsigned short	m_RoiBinningH, m_RoiBinningV;
	
	bool			m_DigitizeOverscan;
	Apn_Resolution	m_DataBits;

/* was private: */

	// General helper functions
	long LoadVerticalPattern();
	long LoadClampPattern();
	long LoadSkipPattern();
	long LoadRoiPattern( unsigned short Binning );

	long WriteHorizontalPattern( APN_HPATTERN_FILE *Pattern, 
								 unsigned short reg, 
								 unsigned short binning );
	
	long InitTwelveBitAD();
	long WriteTwelveBitOffset();

	void UpdateGeneralStatus();

	// Internal private variables
	bool			m_ResetVerticalArrays;

	// Camera state variables
	Apn_CameraMode	m_pvtCameraMode;

	Apn_NetworkMode	m_pvtNetworkTransferMode;
	
	unsigned short	m_pvtImageCount;
	unsigned short	m_pvtTDIRows;
	double			m_pvtTDIRate;

	double			m_pvtSequenceDelay;
	double			m_pvtShutterStrobePosition;
	double			m_pvtShutterStrobePeriod;

	unsigned short	m_pvtExposurePixelsH, m_pvtExposurePixelsV;

	unsigned short	m_pvtTwelveBitGain;

	Apn_LedMode		m_pvtLedMode;
	Apn_LedState	m_pvtLedStateA;
	Apn_LedState	m_pvtLedStateB;
	
	double			m_pvtTestLedBrightness;

	bool			m_pvtCoolerEnable;
	Apn_FanMode		m_pvtFanMode;

	double			m_pvtCoolerBackoffPoint;

	Apn_CoolerStatus	m_pvtCoolerStatus;
	Apn_Status			m_pvtImagingStatus;
	bool				m_pvtShutterState;
	bool				m_pvtImageInProgress;
	bool				m_pvtImageReady;

	unsigned short	m_pvtStatusReg;

	double			m_pvtCoolerDrive;
	double			m_pvtCurrentHeatsinkTemp;
	double			m_pvtCurrentCcdTemp;
	
	double			m_pvtInputVoltage;
	unsigned short	m_pvtIoPortAssignment;
	unsigned short	m_pvtIoPortDirection;

/* added USB/NET specifics */
	unsigned short m_pvtBitsPerPixel;
	unsigned short m_pvtWidth;
	unsigned short m_pvtHeight;


/* added sensor data mirrors */
	bool 		sensorInfo();
	char		m_Sensor[20];
	char		m_CameraModel[20];
	unsigned short	m_CameraId;
	bool		m_InterlineCCD;
	bool		m_SupportsSerialA;
	bool		m_SupportsSerialB;
	bool		m_SensorTypeCCD;
	unsigned short	m_TotalColumns;
	unsigned short	m_ImagingColumns;
	unsigned short	m_ClampColumns;
	unsigned short	m_PreRoiSkipColumns;
	unsigned short	m_PostRoiSkipColumns;
	unsigned short	m_OverscanColumns;
	unsigned short	m_TotalRows;
	unsigned short	m_ImagingRows;
	unsigned short	m_UnderscanRows;
	unsigned short	m_OverscanRows;
	unsigned short	m_VFlushBinning;
	bool		m_HFlushDisable;
	unsigned short	m_ShutterCloseDelay;
	double		m_PixelSizeX;
	double		m_PixelSizeY;
	bool		m_Color;
//	double		m_ReportedGainTwelveBit;
	double		m_ReportedGainSixteenBit;
	double		m_MinSuggestedExpTime;
//	unsigned short	m_TempRegRate;
	unsigned short	m_TempRampRateOne;
	unsigned short	m_TempRampRateTwo;
	unsigned short	m_DefaultGainTwelveBit;
	unsigned short	m_DefaultOffsetTwelveBit;
	unsigned short	m_DefaultRVoltage;

};

#endif // !defined(AFX_APNCAMERA_H__CF513996_359F_4103_BBA6_2C730AE2C301__INCLUDED_)






