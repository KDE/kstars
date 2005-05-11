/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef __APN_CAM_TABLE_H__
#define __APN_CAM_TABLE_H__


#define APN_MODEL_COUNT				25	// total number of models


#define APN_ALTA_KAF0401E_CAM_ID	0
#define APN_ALTA_KAF0401E_CAM_SZ	"1"
	
#define APN_ALTA_KAF1602E_CAM_ID	1
#define APN_ALTA_KAF1602E_CAM_SZ	"2"

#define APN_ALTA_KAF0261E_CAM_ID	2
#define APN_ALTA_KAF0261E_CAM_SZ	"260"

#define APN_ALTA_KAF1301E_CAM_ID	3
#define APN_ALTA_KAF1301E_CAM_SZ	"13"

#define	APN_ALTA_KAF1401E_CAM_ID	4
#define	APN_ALTA_KAF1401E_CAM_SZ	"14"

#define APN_ALTA_KAF1001E_CAM_ID	5
#define APN_ALTA_KAF1001E_CAM_SZ	"6"

#define APN_ALTA_KAF3200E_CAM_ID	6
#define APN_ALTA_KAF3200E_CAM_SZ	"32"

#define APN_ALTA_KAF4202_CAM_ID		7
#define APN_ALTA_KAF4202_CAM_SZ		"4"

#define APN_ALTA_KAF6303E_CAM_ID	8
#define APN_ALTA_KAF6303E_CAM_SZ	"9"

#define APN_ALTA_KAF16801E_CAM_ID	9
#define APN_ALTA_KAF16801E_CAM_SZ	"16"

#define APN_ALTA_CCD4710LS_CAM_ID	10
#define APN_ALTA_CCD4710LS_CAM_SZ	"47"

#define APN_ALTA_CCD4710HS_CAM_ID	11
#define APN_ALTA_CCD4710HS_CAM_SZ	"47"

#define	APN_ALTA_TH7899_CAM_ID		14
#define	APN_ALTA_TH7899_CAM_SZ		"10"

#define	APN_ALTA_CCD4240LS_CAM_ID	16
#define	APN_ALTA_CCD4240LS_CAM_SZ	"42"

#define	APN_ALTA_CCD4240HS_CAM_ID	17
#define	APN_ALTA_CCD4240HS_CAM_SZ	"42"

#define	APN_ALTA_CCD5710LS_CAM_ID	18
#define	APN_ALTA_CCD5710LS_CAM_SZ	"57"

#define	APN_ALTA_CCD5710HS_CAM_ID	19
#define	APN_ALTA_CCD5710HS_CAM_SZ	"57"

#define	APN_ALTA_CCD3011LS_CAM_ID	20
#define	APN_ALTA_CCD3011LS_CAM_SZ	"30"

#define	APN_ALTA_CCD3011HS_CAM_ID	21
#define	APN_ALTA_CCD3011HS_CAM_SZ	"30"

#define APN_ALTA_CCD5520LS_CAM_ID	22
#define APN_ALTA_CCD5520LS_CAM_SZ	"55"

#define APN_ALTA_CCD5520HS_CAM_ID	23
#define APN_ALTA_CCD5520HS_CAM_SZ	"55"

#define APN_ALTA_CCD4720LS_CAM_ID	24
#define APN_ALTA_CCD4720LS_CAM_SZ	"4720"

#define APN_ALTA_CCD4720HS_CAM_ID	25
#define APN_ALTA_CCD4720HS_CAM_SZ	"4720"

#define APN_ALTA_CCD7700LS_CAM_ID	26
#define APN_ALTA_CCD7700LS_CAM_SZ	"77"

#define APN_ALTA_CCD7700HS_CAM_ID	27
#define APN_ALTA_CCD7700HS_CAM_SZ	"77"

#define APN_ALTA_KAI2001M_CAM_ID	64
#define APN_ALTA_KAI2001M_CAM_SZ	"2000"

#define	APN_ALTA_KAI2001MC_CAM_ID	65
#define	APN_ALTA_KAI2001MC_CAM_SZ	"2000C"

#define APN_ALTA_KAI4020_CAM_ID		66
#define APN_ALTA_KAI4020_CAM_SZ		"4000"

#define	APN_ALTA_KAI11000_CAM_ID	67
#define	APN_ALTA_KAI11000_CAM_SZ	"11000"

#define APN_ALTA_KAI11000C_CAM_ID	68
#define APN_ALTA_KAI11000C_CAM_SZ	"11000C"

#define APN_ALTA_CCD4710LS2_CAM_ID	12
#define APN_ALTA_CCD4710LS2_CAM_SZ	"4710"

#define APN_ALTA_CCD4710LS3_CAM_ID	13
#define APN_ALTA_CCD4710LS3_CAM_SZ	"4710"

#define APN_ALTA_CCD4710LS4_CAM_ID	15
#define APN_ALTA_CCD4710LS4_CAM_SZ	"4710"

#define APN_ALTA_CCD4710LS5_CAM_ID	28
#define APN_ALTA_CCD4710LS5_CAM_SZ	"4710"


// Helper function prototype

void ApnCamModelLookup( unsigned short CamId, unsigned short Interface, char *szCamModel );


#endif
