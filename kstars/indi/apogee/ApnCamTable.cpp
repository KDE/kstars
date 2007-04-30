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
// ApnCamTable.cpp
//

#include "ApnCamTable.h"

#include <string.h>


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
	case APN_ALTA_KAF1401E_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF1401E_CAM_SZ );
		break;
	case APN_ALTA_KAF1001E_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF1001E_CAM_SZ );
		break;
	case APN_ALTA_KAF3200E_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF3200E_CAM_SZ );
		break;
	case APN_ALTA_KAF4202_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF4202_CAM_SZ );
		break;
	case APN_ALTA_KAF6303E_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF6303E_CAM_SZ );
		break;
	case APN_ALTA_KAF16801E_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAF16801E_CAM_SZ );
		break;
	case APN_ALTA_CCD4710LS_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD4710LS_CAM_SZ );
		break;
	case APN_ALTA_CCD4710HS_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD4710HS_CAM_SZ );
		break;
	case APN_ALTA_TH7899_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_TH7899_CAM_SZ );
		break;
	case APN_ALTA_CCD4240LS_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD4240LS_CAM_SZ );
		break;
	case APN_ALTA_CCD4240HS_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD4240HS_CAM_SZ );
		break;
	case APN_ALTA_CCD5710LS_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD5710LS_CAM_SZ );
		break;
	case APN_ALTA_CCD5710HS_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD5710HS_CAM_SZ );
		break;
	case APN_ALTA_CCD3011LS_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD3011LS_CAM_SZ );
		break;
	case APN_ALTA_CCD3011HS_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD3011HS_CAM_SZ );
		break;
	case APN_ALTA_CCD5520LS_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD5520LS_CAM_SZ );
		break;
	case APN_ALTA_CCD5520HS_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD5520HS_CAM_SZ );
		break;
	case APN_ALTA_CCD4720LS_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD4720LS_CAM_SZ );
		break;
	case APN_ALTA_CCD4720HS_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD4720HS_CAM_SZ );
		break;
	case APN_ALTA_CCD7700LS_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD7700LS_CAM_SZ );
		break;
	case APN_ALTA_CCD7700HS_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD7700HS_CAM_SZ );
		break;
	case APN_ALTA_KAI2001M_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAI2001M_CAM_SZ );
		break;
	case APN_ALTA_KAI2001MC_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAI2001MC_CAM_SZ );
		break;
	case APN_ALTA_KAI4020_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAI4020_CAM_SZ );
		break;
	case APN_ALTA_KAI11000_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAI11000_CAM_SZ );
		break;
	case APN_ALTA_KAI11000C_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_KAI11000C_CAM_SZ );
		break;

	case APN_ALTA_CCD4710LS2_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD4710LS2_CAM_SZ );
		break;
	case APN_ALTA_CCD4710LS3_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD4710LS3_CAM_SZ );
		break;
	case APN_ALTA_CCD4710LS4_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD4710LS4_CAM_SZ );
		break;
	case APN_ALTA_CCD4710LS5_CAM_ID:
		strcpy( szModelNumber, APN_ALTA_CCD4710LS5_CAM_SZ );
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

