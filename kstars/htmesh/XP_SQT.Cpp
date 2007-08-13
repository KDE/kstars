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
#define OK_MSG  "OK"
///////////////////////////////////////////////////////////////// 
// diagnostic routines
void	printUsageLookup (SRV_PROC *pSrvProc);
void	printUsageHTM_Cover (SRV_PROC* pSrvProc);
void	printError (SRV_PROC *pSrvProc, CHAR* szErrorMsg);
#define ErrorExit(cMsg) {printError(pSrvProc,cMsg); retcode = XP_HTM_ERROR; goto exit;} 


extern void Permute(UINT64 source, char *  Target) ;
//////////////////////////////////////////////////////////////////
// xp_HTM_Foo(char[] Coordinate &BIGINT HTM_ID char[] ErrorMsg) 
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
extern "C" { SRVRETCODE __declspec(dllexport) xp_HTM_Foo (SRV_PROC* pSrvProc);} 

// Just like Lookup for now. Tetsing entry points
//

SRVRETCODE __declspec(dllexport) xp_HTM_Foo (SRV_PROC* pSrvProc){	
	// declares and initalizations.
	int         paramCount;						// number of parameters passed
    BYTE        bType1, bType2,bType3;			// types of params
    unsigned long cbMaxLen1, cbMaxLen2, cbMaxLen3; // max lengths of params
    unsigned long cbActualLen1, cbActualLen2,cbActualLen3;	// actual lenghts
    BOOL        fNull1, fNull2, fNull3;			// null flags
	char	*	pzCoordinateSpec;				// pointer to the input spec (polar or cartesian)
	INT64		result;							// the result arry that we build locally
	char	 	czErrorMsg [1000] = "IG";		// diagnostic message output
	int			retcode;
	htmSqlInterface htm;						// holds the htm object

	//----------------------------------------------------------------
	// PARSE INPUT AND OUTPUT PARAMS
    // Count the number of parameters.
    paramCount = srv_rpcparams(pSrvProc);
	//
	// DEBUG
	//
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
	result = htm.lookupID(pzCoordinateSpec);	// do the lookup to get the HTM_ID
	if (htm.err()) 
		{strcpy(czErrorMsg,htm.error());
		 if (strlen(czErrorMsg) == 0)
			strcpy(czErrorMsg,"HTM code failed to give diagnostic message");
		}	// return error string if there is one.
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

}   // end of xp_HTM_Lookup

///////////////////////////// NEIGHBORS:

//////////////////////////////////////////////////////////////////
// xp_HTM_Neighbors(&char* Coordinates &varbinary OutputVector) SQL extended stored procedure.
// 
// Coordinates is an input string giving the boarder of an area.
// There are two formats:
//		CIRCLE	J2000 		ra	dec	radius_arc_minutes		
//		CIRCLE	CARTESIAN 	x      y      z	radius_arc_minutes
//		CONVEX 	J2000		ra1 dec1 ra2 dec2 ... ran decn
//		CONVEX 	CARTESIAN	x1 y1 z1 x2 y2 z2... xn yn zn 
//		DOMAIN	k	n1          x1 y1 z1 d1 x2 y2 z2 d2 ... xn1 yn1 zn1 dn1
//		 			n2          x1 y1 z1 d1 x2 y2 z2 d2 ... xn2 yn2 zn2 dn2
//					..
//					nk          x1 y1 z1 d1 x2 y2 z2 d2 ... xnk ynk znk dnk

//   
// OutputVector is a 8000 byte array of pairs of 64 bit integers.
//		each integer is an HTM identifier.
//		the pairs of HTMs give a range which can be used for comparison.
//		These HTM's cover the area specified by the "Coordinates" parameter.
//
extern "C" { SRVRETCODE __declspec(dllexport) xp_HTM_Neighbors (SRV_PROC* pSrvProc);}

static int nbr_count = 22;

SRVRETCODE __declspec(dllexport) xp_HTM_Neighbors (SRV_PROC* pSrvProc) {
	// declares and initializations
	int         paramCount;						// number of parameters passed  
    BYTE        bType1, bType2,bType3;			// types of params
    unsigned long cbMaxLen1, cbMaxLen2, cbMaxLen3; // max lengths of params
    unsigned long cbActualLen1, cbActualLen2,cbActualLen3;	// actual lengths
    BOOL        fNull1, fNull2, fNull3;			// null flags

	char	*	pzCoordinateSpec;				// pointer to the input spec (polar or cartesian)

	char	 	czErrorMsg [1000] = OK_MSG;		// diagnostic message output
	int			retcode;
	INT64		result[MAX_OUTPUT_VECTOR_LENGTH+1]; // the result array that we build locally
	unsigned int len;							// length (in bytes) of output array

	////////////////////
	ValueVector ranges;
	size_t		nRanges;
	htmSqlInterface htm;

	///////////////////////////////////////////////////////
	// zero the memory array
	memset((void *) result, 0, 8*MAX_OUTPUT_VECTOR_LENGTH);
 
	//----------------------------------------------------------------
	// PARSE INPUT AND OUTPUT PARAMS
    // Count the number of parameters.
	paramCount = srv_rpcparams(pSrvProc);

    if (paramCount != 3)
	{  printUsageHTM_Cover(pSrvProc); retcode = XP_ERROR; goto exit;}

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
 
    // Get OutputVector datatype, and param length information  
    if (srv_paraminfo(pSrvProc, 2, &bType2, &cbMaxLen2, &cbActualLen2, NULL, &fNull2) == FAIL)
         ErrorExit("Failed to get type info on HTM Vector param...");

	// Make sure OutputVector param is a return (OUTPUT) parameter
	if ((srv_paramstatus(pSrvProc,2) & SRV_PARAMRETURN) == FAIL)
		{  ErrorExit("HTM Vector param must be an output param"); }

	// Is parameter a big char or varchar datatype
	if (bType2 != SRVBIGVARCHAR   && bType2 != SRVBIGCHAR &&
		bType2 != SRVBIGVARBINARY && bType2 != SRVBIGBINARY &&
		bType2 != SRVIMAGE        && bType2 != SRVVARBINARY)
		{ ErrorExit("HTM Vector param must be an VARBINARY(8000)"); }

    // Is it large enough to hold data
	if (cbMaxLen2 < MAX_OUTPUT_VECTOR_LENGTH)
		ErrorExit("HTM Vector param must be an VARBINARY(8000)...");

	// Get ErrorMsg param datatype, and param length information  
    if (srv_paraminfo(pSrvProc, 3, &bType3, &cbMaxLen3, &cbActualLen3, NULL, &fNull3) == FAIL)
         ErrorExit("Failed to get type info on Error Message param...");

	// Is ErrorMsg param a character string
	if (bType3 != SRVBIGVARCHAR   && bType3 != SRVBIGCHAR &&
		bType3 != SRVVARCHAR	  && bType3 != SRVCHAR)
		ErrorExit("ErrorMsg string param should be a string.");

	// Is ErrorMsg param non-null and at least 100 bytes?
	if (cbMaxLen3 < 100) { 
 		ErrorExit("ErrorMsg string param is less than 100 bytes.");
		}

	// Make sure ErrorMsg param is a return (OUTPUT) parameter
	if ((srv_paramstatus(pSrvProc,3) & SRV_PARAMRETURN) == FAIL)
		{ ErrorExit("ErrorMsg string param is must be an output param."); }

	/*********************CALL HTM CODE***************************************/
	nRanges = htm.intersect1(pzCoordinateSpec,ranges);
	if (htm.err()) {						// if error
		strcpy(czErrorMsg,htm.error());		// pass back error message
		if (strlen(czErrorMsg) == 0)		// make up error message if its missing.
			strcpy(czErrorMsg,"HTM code failed to give diagnostic message");
		len = 0;							// result vector is empty
		}									// end error case
    else if (nRanges > MAX_OUTPUT_VECTOR_LENGTH/2) {
		strcpy(czErrorMsg,					// if vector overflowed (we are in BIG trouble).
			"HTM routines returned more than 500 triangles, overflowed return vector.");
		len = 0;
		}									// end overflow case
	else 
	{									// this is the NO ERROR case
		strcpy(czErrorMsg,OK_MSG);			// construct error message
 		len = nRanges*16;					//  each range is 16 bytes (pair of int64)
		for(size_t i = 0; i < nRanges; i++)	// permute them so that they map to strings
		{
			// Permute(ranges[i].lo, (char*) &result[2*i]);
			// Permute(ranges[i].hi, (char*) &result[2*i+1]);
			Permute(nbr_count++, (char*) &result[2*i]);
			Permute(nbr_count++, (char*) &result[2*i+1]);
		}						 		// end permute
	}	// end no-error case
	/*************************************************************************/

	// Set the output parameter to the string
	if (srv_paramsetoutput(pSrvProc, 2, (BYTE *) result, len, FALSE) == FAIL)
 		  		ErrorExit( "srv_paramsetoutput failed...");

	//Set Diagnostic String
	if (srv_paramsetoutput(pSrvProc, 3, (BYTE *) czErrorMsg, strlen(czErrorMsg), FALSE) == FAIL)
				ErrorExit( "srv_paramsetoutput failed on Msg...");

    // Send results complete message the the ODS library.
    if (srv_senddone(pSrvProc, (SRV_DONE_COUNT | SRV_DONE_MORE), 0, 1) == FAIL)
				ErrorExit( "srv_senddone fail...");  
		 
	 
	retcode = XP_NOERROR;
	// return success to the caller
exit:  
	return(retcode);

}   // end of xp_HTM
