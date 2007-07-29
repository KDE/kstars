
////////////////////////////////////////////////////////////////////
//#     Filename:       XP_Fast.cpp
//#
//#     SQL extended stored procedures (a DLL) for Hierarchical Triangular Mesh..
//#
//#     Author:         Jim Gray  Gray@Microsoft.com
//#
//#     Date:           September, 2002
//#						Gyorgy (George) Fekete, CAS, Johns Hopkins University
//#
//# This program is free software; you can redistribute it and/or

#define	TRUSTED

///////////////////////////////////////////////////////////////// 
// the SQL declares
#include   <srv.h>
///////////////////////////////////////////////////////////////// 
// standard includes
#include <stdio.h>
#include <windows.h>
#include <winerror.h>
///////////////////////////////////////////////////////////////// 
// HTM includes
#include "sqlInterface.h"
#include "SpatialIndex.h"
///////////////////////////////////////////////////////////////// 
// get the linkage options for a DLL.
#ifndef LINKAGE
#define	LINKAGE	 
#endif

///////////////////////////////////////////////////////////////// 
// return codes
#define XP_NOERROR      0
#define XP_ERROR        1
#define MAX_SERVER_ERROR 20000
#define XP_HTM_ERROR MAX_SERVER_ERROR+1

///////////////////////////////////////////////////////////////// 
// literals
#define MAX_OUTPUT_VECTOR_LENGTH  1000 

//////////////////////////////////////////////////////////////////
// xp_HTM_FLookup(char[] Coordinate &BIGINT HTM_ID char[] ErrorMsg) 
//                   SQL extended stored procedure.
// 
// Coordinate is an input string giving the point in one of thes formats
// There are two formats:
//
//      J2000 depth ra dec	This is polar coordinates.
//								240.0 is the right asension
//								 38.0 is the declination
//		CARTESIAN depth x y z   where x, y, z are numbers that give the
//								position ofthe point on the spere.
//
//		depth is an interger in the range 2..14 giving the mesh depth.
//		ra, dec, x, y, z are floats.					 
//   
// HTM_ID is the BIGINT that defines the point (or the triagle at that depth.
//      actually we return the BIGINT in string form (BINARY(8)) to work around SQL2k bug
//
// If things go well, ErrorMsg is set to 'OK'
// If things go badly, HTM_ID is zero and ErrorMsg gets a diagnostic message.
//

// diagnostic routines
extern void	printUsageLookup (SRV_PROC *pSrvProc);
extern void Permute(UINT64 source, char *  Target);
void	printError (SRV_PROC *pSrvProc, CHAR* szErrorMsg);

#define ErrorExit(cMsg) {printError(pSrvProc,cMsg); retcode = XP_HTM_ERROR; goto exit;} 

extern "C" { SRVRETCODE __declspec(dllexport) xp_HTM_FLookup (SRV_PROC* pSrvProc);} 
extern "C" { SRVRETCODE __declspec(dllexport) xp_HTM_FNSLookup (SRV_PROC* pSrvProc);} 

extern "C" {
	int cc_parseVectors(char *spec, int *level, double *ra, double *dec);
}


#define OK_MSG  "OK"
static htmSqlInterface *global_htm;						// holds the htm object
static SpatialIndex *gl_index;


SRVRETCODE __declspec(dllexport) xp_HTM_FLookup (SRV_PROC* pSrvProc){	
	// declares and initalizations.
	int         paramCount;						// number of parameters passed
    BYTE        bType1, bType2,bType3;			// types of params
    unsigned long cbMaxLen1, cbMaxLen2, cbMaxLen3; // max lengths of params
    unsigned long cbActualLen1, cbActualLen2,cbActualLen3;	// actual lenghts
    BOOL        fNull1, fNull2, fNull3;			// null flags
	char	*	pzCoordinateSpec;				// pointer to the input spec (polar or cartesian)
	INT64		result;							// the result arry that we build locally
	char	 	czErrorMsg [1000] = OK_MSG;		// diagnostic message output
	int			retcode;
//	htmSqlInterface *htm;						// holds the htm object

	
	//----------------------------------------------------------------
	// PARSE INPUT AND OUTPUT PARAMS
    // Count the number of parameters.
    paramCount = srv_rpcparams(pSrvProc);
    if (3 != paramCount)
		{ printUsageLookup (pSrvProc); retcode = XP_ERROR; goto exit;}

	// Get Coordinates param datatype, and param length information of 
    if (srv_paraminfo(pSrvProc, 1, &bType1, &cbMaxLen1, &cbActualLen1, NULL, &fNull1) == FAIL)
         ErrorExit("srv_paraminfo failed...");

	// Is Coordinate param a character string
	if (bType1 != SRVBIGVARCHAR   && bType1 != SRVBIGCHAR &&
		bType1 != SRVVARCHAR	  && bType1 != SRVCHAR)
		ErrorExit("Coordinate param should be a string.");

	// Is Coordinate param non-null
	if (fNull1 || cbActualLen1 < 1 || cbMaxLen1 <= cbActualLen1) // ##JG1 need zero termination.
		ErrorExit("Coordinate param is null.");

	// Get pointer to Coordinate param
	pzCoordinateSpec = (char *) srv_paramdata (pSrvProc, 1);
	if (pzCoordinateSpec == NULL)
		ErrorExit("Coordinate param is null.");
	pzCoordinateSpec[cbActualLen1] = 0;							// ##JG1  zero terminate the string
 
    // Get OutputBigInt datatype   
    if (srv_paraminfo(pSrvProc, 2, &bType2, &cbMaxLen2, &cbActualLen2, NULL, &fNull2) == FAIL)
         ErrorExit("srv_paraminfo failed to get HTM_ID param");

	// Make sure OutputVector param is a return (OUTPUT) parameter
	if ((srv_paramstatus(pSrvProc,2) & SRV_PARAMRETURN) == FAIL)
		{ ErrorExit("HTM_ID param should be an OUTPUT param"); }

	// Is parameter a big int datatype
	if ((bType2 != SRVINT8) && // this should work but it does not.
		(bType2 != SRVVARCHAR) && (bType2 != SRVBIGVARCHAR) && 
		(bType2 != SRVVARBINARY) && (bType2 != SRVBIGVARBINARY) && 
		(bType2 != SRVBINARY) &&(bType2 != SRVBIGBINARY) &&
		(bType2 != SRVIMAGE) 
		)  { 
 		 ErrorExit( "HTM_ID param should be an OUTPUT param should be BINARY(8)");
		} 

	// Get ErrorMsg param datatype, and param length information  
    if (srv_paraminfo(pSrvProc, 3, &bType3, &cbMaxLen3, &cbActualLen3, NULL, &fNull3) == FAIL)
         ErrorExit("srv_paraminfo failed on ErrorMsg string Param");

	// Is ErrorMsg param a character string
	if (bType3 != SRVBIGVARCHAR   && bType3 != SRVBIGCHAR &&
		bType3 != SRVVARCHAR	  && bType3 != SRVCHAR)
		ErrorExit("ErrorMsg string param should be a string.");

	// Is ErrorMsg param non-null and at least 100 bytes?
	if (cbMaxLen3 < 100)
	{ /*sprintf(czErrorMsg,"\nThe error msg string length is  %d  and max len is %d :\n", cbActualLen3,cbMaxLen3);
  		srv_sendmsg(pSrvProc, SRV_MSG_ERROR, XP_HTM_ERROR, SRV_INFO, 1,
            NULL, 0, (DBUSMALLINT) __LINE__,  czErrorMsg , SRV_NULLTERM);*/
 		ErrorExit("ErrorMsg string param is less than 100 bytes.");
		}

	// Make sure ErrorMsg param is a return (OUTPUT) parameter
	if ((srv_paramstatus(pSrvProc,3) & SRV_PARAMRETURN) == FAIL)
		{ ErrorExit("ErrorMsg string param is must be an output param."); }
  
	/*********************CALL HTM CODE***************************************/
	//ckp1();
	if (!global_htm)
		global_htm = new htmSqlInterface(); // we will save this for ever, and ever....

	
	result = global_htm->lookupID(pzCoordinateSpec);	// do the lookup to get the HTM_ID
	if (global_htm->err()) 
		{strcpy(czErrorMsg, global_htm->error());
		 if (strlen(czErrorMsg) == 0)
			strcpy(czErrorMsg,"HTM code failed to give diagnostic message");
		}	// return error string if there is one.	

	// Not the global one though delete htm;
	//ckp2();
	//s2s1();

    /**********************************************************************/ 

	// Permute the Int64 so that it can cast from string to BIGINT in SQL
	Permute(result, (char*) &result);
	// Set the output parameter to the string
    if (srv_paramsetoutput(pSrvProc, 2, (BYTE *) &result, 8, FALSE) == FAIL)
		  		ErrorExit( "srv_paramsetoutput failed on HTM_ID...");
	
    //Set Diagnostic String
	if (srv_paramsetoutput(pSrvProc, 3, (BYTE *) czErrorMsg, strlen(czErrorMsg), FALSE) == FAIL)
				ErrorExit( "srv_paramsetoutput failed on Msg...");

    // Send results complete message the the ODS library.
    srv_senddone(pSrvProc, (SRV_DONE_COUNT | SRV_DONE_MORE), 0, 1);
	 
	retcode = XP_NOERROR;
	// return success to the caller
exit:  
	return(retcode);

}   // end of xp_HTM_FLookup


SRVRETCODE __declspec(dllexport) xp_HTM_FNSLookup (SRV_PROC* pSrvProc){	
	// declares and initalizations.
	int         paramCount;						// number of parameters passed
    BYTE        bType1, bType2,bType3;			// types of params
    unsigned long cbMaxLen1, cbMaxLen2, cbMaxLen3; // max lengths of params
    unsigned long cbActualLen1, cbActualLen2,cbActualLen3;	// actual lenghts
    BOOL        fNull1, fNull2, fNull3;			// null flags
	char	*	pzCoordinateSpec;				// pointer to the input spec (polar or cartesian)
	INT64		result;							// the result arry that we build locally
	char	 	czErrorMsg [1000] = OK_MSG;		// diagnostic message output
	int			retcode;
	
	//----------------------------------------------------------------
	// PARSE INPUT AND OUTPUT PARAMS
    // Count the number of parameters.
#ifdef TRUSTED
//   paramCount = srv_rpcparams(pSrvProc);
//   if (3 != paramCount)
//		{ printUsageLookup (pSrvProc); retcode = XP_ERROR; goto exit;}

	// Get Coordinates param datatype, and param length information of 
    if (srv_paraminfo(pSrvProc, 1, &bType1, &cbMaxLen1, &cbActualLen1, NULL, &fNull1) == FAIL)
         ErrorExit("srv_paraminfo failed...");

	// Is Coordinate param a character string
//	if (bType1 != SRVBIGVARCHAR   && bType1 != SRVBIGCHAR &&
//		bType1 != SRVVARCHAR	  && bType1 != SRVCHAR)
//		ErrorExit("Coordinate param should be a string.");

	// Is Coordinate param non-null
//	if (fNull1 || cbActualLen1 < 1 || cbMaxLen1 <= cbActualLen1) // ##JG1 need zero termination.
//		ErrorExit("Coordinate param is null.");

	// Get pointer to Coordinate param
	pzCoordinateSpec = (char *) srv_paramdata (pSrvProc, 1);
	if (pzCoordinateSpec == NULL)
		ErrorExit("Coordinate param is null.");
	pzCoordinateSpec[cbActualLen1] = 0;							// ##JG1  zero terminate the string
 
    // Get OutputBigInt datatype   
//    if (srv_paraminfo(pSrvProc, 2, &bType2, &cbMaxLen2, &cbActualLen2, NULL, &fNull2) == FAIL)
//         ErrorExit("srv_paraminfo failed to get HTM_ID param");

	// Make sure OutputVector param is a return (OUTPUT) parameter
//	if ((srv_paramstatus(pSrvProc,2) & SRV_PARAMRETURN) == FAIL)
//		{ ErrorExit("HTM_ID param should be an OUTPUT param"); }

	// Is parameter a big int datatype
//	if ((bType2 != SRVINT8) && // this should work but it does not.
//		(bType2 != SRVVARCHAR) && (bType2 != SRVBIGVARCHAR) && 
//		(bType2 != SRVVARBINARY) && (bType2 != SRVBIGVARBINARY) && 
//		(bType2 != SRVBINARY) &&(bType2 != SRVBIGBINARY) &&
//		(bType2 != SRVIMAGE) 
//		)  { 
// /		 ErrorExit( "HTM_ID param should be an OUTPUT param should be BINARY(8)");
//		} 

	// Get ErrorMsg param datatype, and param length information  
 //   if (srv_paraminfo(pSrvProc, 3, &bType3, &cbMaxLen3, &cbActualLen3, NULL, &fNull3) == FAIL)
 //        ErrorExit("srv_paraminfo failed on ErrorMsg string Param");

	// Is ErrorMsg param a character string
//	if (bType3 != SRVBIGVARCHAR   && bType3 != SRVBIGCHAR &&
//		bType3 != SRVVARCHAR	  && bType3 != SRVCHAR)
//		ErrorExit("ErrorMsg string param should be a string.");

	// Is ErrorMsg param non-null and at least 100 bytes?
//	if (cbMaxLen3 < 100)
//	{ /*sprintf(czErrorMsg,"\nThe error msg string length is  %d  and max len is %d :\n", cbActualLen3,cbMaxLen3);
//  		srv_sendmsg(pSrvProc, SRV_MSG_ERROR, XP_HTM_ERROR, SRV_INFO, 1,
  //          NULL, 0, (DBUSMALLINT) __LINE__,  czErrorMsg , SRV_NULLTERM);*/
 //		ErrorExit("ErrorMsg string param is less than 100 bytes.");
//		}

	// Make sure ErrorMsg param is a return (OUTPUT) parameter
//	if ((srv_paramstatus(pSrvProc,3) & SRV_PARAMRETURN) == FAIL)
//		{ ErrorExit("ErrorMsg string param is must be an output param."); }
#endif
#ifndef TRUSTED
    paramCount = srv_rpcparams(pSrvProc);
    if (3 != paramCount)
		{ printUsageLookup (pSrvProc); retcode = XP_ERROR; goto exit;}

	// Get Coordinates param datatype, and param length information of 
    if (srv_paraminfo(pSrvProc, 1, &bType1, &cbMaxLen1, &cbActualLen1, NULL, &fNull1) == FAIL)
         ErrorExit("srv_paraminfo failed...");

	// Is Coordinate param a character string
	if (bType1 != SRVBIGVARCHAR   && bType1 != SRVBIGCHAR &&
		bType1 != SRVVARCHAR	  && bType1 != SRVCHAR)
		ErrorExit("Coordinate param should be a string.");

	// Is Coordinate param non-null
	if (fNull1 || cbActualLen1 < 1 || cbMaxLen1 <= cbActualLen1) // ##JG1 need zero termination.
		ErrorExit("Coordinate param is null.");

	// Get pointer to Coordinate param
	pzCoordinateSpec = (char *) srv_paramdata (pSrvProc, 1);
	if (pzCoordinateSpec == NULL)
		ErrorExit("Coordinate param is null.");
	pzCoordinateSpec[cbActualLen1] = 0;							// ##JG1  zero terminate the string
 
    // Get OutputBigInt datatype   
    if (srv_paraminfo(pSrvProc, 2, &bType2, &cbMaxLen2, &cbActualLen2, NULL, &fNull2) == FAIL)
         ErrorExit("srv_paraminfo failed to get HTM_ID param");

	// Make sure OutputVector param is a return (OUTPUT) parameter
	if ((srv_paramstatus(pSrvProc,2) & SRV_PARAMRETURN) == FAIL)
		{ ErrorExit("HTM_ID param should be an OUTPUT param"); }

	// Is parameter a big int datatype
	if ((bType2 != SRVINT8) && // this should work but it does not.
		(bType2 != SRVVARCHAR) && (bType2 != SRVBIGVARCHAR) && 
		(bType2 != SRVVARBINARY) && (bType2 != SRVBIGVARBINARY) && 
		(bType2 != SRVBINARY) &&(bType2 != SRVBIGBINARY) &&
		(bType2 != SRVIMAGE) 
		)  { 
 		 ErrorExit( "HTM_ID param should be an OUTPUT param should be BINARY(8)");
		} 

	// Get ErrorMsg param datatype, and param length information  
    if (srv_paraminfo(pSrvProc, 3, &bType3, &cbMaxLen3, &cbActualLen3, NULL, &fNull3) == FAIL)
         ErrorExit("srv_paraminfo failed on ErrorMsg string Param");

	// Is ErrorMsg param a character string
	if (bType3 != SRVBIGVARCHAR   && bType3 != SRVBIGCHAR &&
		bType3 != SRVVARCHAR	  && bType3 != SRVCHAR)
		ErrorExit("ErrorMsg string param should be a string.");

	// Is ErrorMsg param non-null and at least 100 bytes?
	if (cbMaxLen3 < 100)
	{ /*sprintf(czErrorMsg,"\nThe error msg string length is  %d  and max len is %d :\n", cbActualLen3,cbMaxLen3);
  		srv_sendmsg(pSrvProc, SRV_MSG_ERROR, XP_HTM_ERROR, SRV_INFO, 1,
            NULL, 0, (DBUSMALLINT) __LINE__,  czErrorMsg , SRV_NULLTERM);*/
 		ErrorExit("ErrorMsg string param is less than 100 bytes.");
		}

	// Make sure ErrorMsg param is a return (OUTPUT) parameter
	if ((srv_paramstatus(pSrvProc,3) & SRV_PARAMRETURN) == FAIL)
		{ ErrorExit("ErrorMsg string param is must be an output param."); }
#endif
  
	/*********************FNSLOOKUP CALL HTM CODE************************************/
	// [ed:fns]
	// Build a tree first in the SpatialIndex object
	// recover the specs from the coordinatespecs
	// use the spatialindex object directly without rebuilding it
	// test: compare this with the ohter lookup function
	//
	if (!gl_index){
		gl_index = new SpatialIndex(6);
	}
	{
		float64 ra, dec;
		int level;
		(void) cc_parseVectors(pzCoordinateSpec, &level, &ra, &dec);
		gl_index->setMaxlevel(level); // misnomer, but what the hell
		result = gl_index->idByPoint(ra, dec);
	}
#ifdef OLDWAY
	if (!global_htm)
		global_htm = new htmSqlInterface(); // we will save this for ever, and ever....

	
	
	
	
	
	result = global_htm->lookupID(pzCoordinateSpec);	// do the lookup to get the HTM_ID
	if (global_htm->err()) 
		{strcpy(czErrorMsg, global_htm->error());
		 if (strlen(czErrorMsg) == 0)
			strcpy(czErrorMsg,"HTM code failed to give diagnostic message");
		}	// return error string if there is one.	

#endif
    /**********************************************************************/ 

	// Permute the Int64 so that it can cast from string to BIGINT in SQL
	Permute(result, (char*) &result);
	// Set the output parameter to the string
    if (srv_paramsetoutput(pSrvProc, 2, (BYTE *) &result, 8, FALSE) == FAIL)
		  		ErrorExit( "srv_paramsetoutput failed on HTM_ID...");
	
    //Set Diagnostic String
	if (srv_paramsetoutput(pSrvProc, 3, (BYTE *) czErrorMsg, strlen(czErrorMsg), FALSE) == FAIL)
				ErrorExit( "srv_paramsetoutput failed on Msg...");

    // Send results complete message the the ODS library.
    srv_senddone(pSrvProc, (SRV_DONE_COUNT | SRV_DONE_MORE), 0, 1);
	 
	retcode = XP_NOERROR;
	// return success to the caller
exit:  
	return(retcode);

}   // end of xp_HTM_FLookup

