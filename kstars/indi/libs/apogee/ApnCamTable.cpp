//////////////////////////////////////////////////////////////////////
//
// ApnCamTable.cpp
//
// Copyright (c) 2003-2005 Apogee Instruments, Inc.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>

#include "ApnCamTable.h"


#define ALTA_MODEL_PREFIX "Alta-"


void ApnCamModelLookup( unsigned short CamId, unsigned short Interface, char *szCamModel )
{
	char szModelNumber[20];
	bool Error;

	Error = false;

	switch( CamId )
	{
	case APN_ALTA_KAF0401E_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF0401E_CAM_SZ );
		break;
	case APN_ALTA_KAF1602E_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF1602E_CAM_SZ );
		break;
	case APN_ALTA_KAF0261E_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF0261E_CAM_SZ );
		break;
	case APN_ALTA_KAF1301E_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF1301E_CAM_SZ );
		break;
	case APN_ALTA_KAF1001E_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF1001E_CAM_SZ );
		break;
	case APN_ALTA_KAF1001ENS_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF1001ENS_CAM_SZ );
		break;
	case APN_ALTA_KAF10011105_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF10011105_CAM_SZ );
		break;
	case APN_ALTA_KAF3200E_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF3200E_CAM_SZ );
		break;
	case APN_ALTA_KAF6303E_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF6303E_CAM_SZ );
		break;
	case APN_ALTA_KAF16801E_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF16801E_CAM_SZ );
		break;
	case APN_ALTA_KAF09000_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF09000_CAM_SZ );
		break;
	case APN_ALTA_KAF0401EB_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF0401EB_CAM_SZ );
		break;
	case APN_ALTA_KAF1602EB_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF1602EB_CAM_SZ );
		break;
	case APN_ALTA_KAF0261EB_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF0261EB_CAM_SZ );
		break;
	case APN_ALTA_KAF1301EB_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF1301EB_CAM_SZ );
		break;
	case APN_ALTA_KAF1001EB_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF1001EB_CAM_SZ );
		break;
	case APN_ALTA_KAF6303EB_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF6303EB_CAM_SZ );
		break;
	case APN_ALTA_KAF3200EB_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF3200EB_CAM_SZ );
		break;

	case APN_ALTA_TH7899_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_TH7899_CAM_SZ );
		break;

	case APN_ALTA_CCD4710_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD4710_CAM_SZ );
		break;
	case APN_ALTA_CCD4710ALT_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD4710ALT_CAM_SZ );
		break;
	case APN_ALTA_CCD4240_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD4240_CAM_SZ );
		break;
	case APN_ALTA_CCD5710_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD5710_CAM_SZ );
		break;
	case APN_ALTA_CCD3011_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD3011_CAM_SZ );
		break;
	case APN_ALTA_CCD5520_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD5520_CAM_SZ );
		break;
	case APN_ALTA_CCD4720_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD4720_CAM_SZ );
		break;
	case APN_ALTA_CCD7700_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD7700_CAM_SZ );
		break;

	case APN_ALTA_CCD4710B_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD4710B_CAM_SZ );
		break;
	case APN_ALTA_CCD4240B_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD4240B_CAM_SZ );
		break;
	case APN_ALTA_CCD5710B_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD5710B_CAM_SZ );
		break;
	case APN_ALTA_CCD3011B_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD3011B_CAM_SZ );
		break;
	case APN_ALTA_CCD5520B_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD5520B_CAM_SZ );
		break;
	case APN_ALTA_CCD4720B_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD4720B_CAM_SZ );
		break;
	case APN_ALTA_CCD7700B_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD7700B_CAM_SZ );
		break;

	case APN_ALTA_KAI2001ML_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAI2001ML_CAM_SZ );
		break;
	case APN_ALTA_KAI2020ML_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAI2020ML_CAM_SZ );
		break;
	case APN_ALTA_KAI4020ML_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAI4020ML_CAM_SZ );
		break;
	case APN_ALTA_KAI11000ML_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAI11000ML_CAM_SZ );
		break;
	case APN_ALTA_KAI2001CL_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAI2001CL_CAM_SZ );
		break;
	case APN_ALTA_KAI2020CL_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAI2020CL_CAM_SZ );
		break;
	case APN_ALTA_KAI4020CL_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAI4020CL_CAM_SZ );
		break;
	case APN_ALTA_KAI11000CL_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAI11000CL_CAM_SZ );
		break;

	case APN_ALTA_KAI2020MLB_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAI2020MLB_CAM_SZ );
		break;
	case APN_ALTA_KAI4020MLB_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAI4020MLB_CAM_SZ );
		break;
	case APN_ALTA_KAI2020CLB_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAI2020CLB_CAM_SZ );
		break;
	case APN_ALTA_KAI4020CLB_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAI4020CLB_CAM_SZ );
		break;

	
	default:
		Error = true;
		break;
	}

	if ( Error )
	{
		strcpy( szCamModel, "Unknown" );
	}
	else
	{
		strcpy( szCamModel, ALTA_MODEL_PREFIX );

		if ( Interface == 0 )	// Network Interface
			strcat( szCamModel, "E" );

		if ( Interface == 1 )	// USB 2.0 Interface
			strcat( szCamModel, "U" );

		strcat( szCamModel, szModelNumber );
	}

}

