

#include <stdio.h>
#include <signal.h>

#include <string>
#include <vector>
#include <iostream>

#include "controlr_rinterface.h"

#include <Rinternals.h>
#include <Rembedded.h>

#ifdef WIN32

	#include <graphapp.h>

#else // #ifdef WIN32

	#include <unistd.h>
	#include <Rinterface.h>

#endif // #ifdef WIN32

#include <R_ext/Parse.h>
#include <R_ext/Rdynload.h>

std::vector< std::string > cmdBuffer;

// try to store fuel now, you jerks
#undef clear
#undef length

extern "C" {
	extern void Rf_PrintWarnings();
	extern Rboolean R_Visible;
}

SEXP exec_r(std::vector < std::string > &vec, int *err, ParseStatus *pStatus, bool printResult )
{
	ParseStatus status ;
	SEXP cmdSexp, wv = R_NilValue, cmdexpr = R_NilValue;
	SEXP ans = 0;
	int i, errorOccurred;
    
	if (vec.size() == 0) return R_NilValue;

//	::WaitForSingleObject(muxExecR, INFINITE);

	PROTECT(cmdSexp = Rf_allocVector(STRSXP, vec.size()));
	for (unsigned int ui = 0; ui < vec.size(); ui++)
	{
		SET_STRING_ELT(cmdSexp, ui, Rf_mkChar(vec[ui].c_str()));
	}
	cmdexpr = PROTECT(R_ParseVector(cmdSexp, -1, &status, R_NilValue));

	if (err) *err = 0;
	if (pStatus) *pStatus = status;

	switch (status){
	case PARSE_OK:

		if ( printResult )
		{
/*			
	PROTECT(R_CurrentExpr);
	R_Busy(1);
	lastExpr = R_CurrentExpr;
	R_CurrentExpr = eval(R_CurrentExpr, rho);
	SET_SYMVALUE(R_LastvalueSymbol, R_CurrentExpr);
	wasDisplayed = R_Visible;
	if (R_Visible)
	    PrintValueEnv(R_CurrentExpr, rho);
	if (R_CollectWarnings)
	    PrintWarnings();
	Rf_callToplevelHandlers(lastExpr, R_CurrentExpr, TRUE, wasDisplayed);
	UNPROTECT(1);
	*/
			
			// I don't know why I can't get R_Visible properly,
			// but it seems to work using withVisible.
			
			wv = PROTECT(Rf_lang2(Rf_install("withVisible"), Rf_lang2(Rf_install("eval"), cmdexpr)));
			ans = R_tryEval(wv, R_GlobalEnv, &errorOccurred);
			if (err && errorOccurred) *err = errorOccurred;
			UNPROTECT(1);

			if( errorOccurred ){
				if (err) *err = errorOccurred;
			}
			else {
				if (*(LOGICAL(VECTOR_ELT(ans, 1)))) {
					Rf_PrintValue(VECTOR_ELT(ans, 0));
				}
			}
			
			Rf_PrintWarnings();
			
		}
		else
		{
	
			// Loop is needed here as EXPSEXP might be of length > 1
			for (i = 0; i < Rf_length(cmdexpr); i++){
				SEXP cmd = VECTOR_ELT(cmdexpr, i);
				ans = R_tryEval(cmd, R_GlobalEnv, &errorOccurred);
				if (errorOccurred) {
					if (err) *err = errorOccurred;
					UNPROTECT(2);
					return 0;
				}
			}
		
		}
		break;

	case PARSE_INCOMPLETE:
		break;

	case PARSE_NULL:
		break;

	case PARSE_ERROR:
		break;

	case PARSE_EOF:
		break;

	default:
		break;
	}

	UNPROTECT(2);
//	::ReleaseMutex(muxExecR);

	return ans;

}


void r_exec_vector(std::vector<std::string> &vec, int *err, PARSE_STATUS_2 *status, bool printResult, bool excludeFromHistory)
{
	ParseStatus ps;

	// if you want the history() command to appear on the history
	// stack, like bash, you need to add the line(s) to the buffer
	// here; and then potentially remove them if you get an INCOMPLETE
	// response (b/c in that case we'll see it again)

	SEXP rslt = PROTECT(exec_r(vec, err, &ps, true));

	if (status)
	{
		switch (ps)
		{
		case PARSE_OK: *status = PARSE2_OK; break;
		case PARSE_INCOMPLETE: *status = PARSE2_INCOMPLETE; break;
		case PARSE_ERROR: *status = PARSE2_ERROR; break;
		case PARSE_EOF: *status = PARSE2_EOF; break;
		default:
		case PARSE_NULL:
			*status = PARSE2_NULL; break;
		}
	}

	if (!excludeFromHistory) {
		if (ps == PARSE_OK || ps == PARSE_ERROR) {
			while (cmdBuffer.size() >= MAX_CMD_HISTORY) cmdBuffer.erase(cmdBuffer.begin());
			for (std::vector< std::string > ::iterator iter = vec.begin(); iter != vec.end(); iter++)
				cmdBuffer.push_back(iter->c_str());
		}
	}

/*
	if (ps == PARSE_OK && printResult && rslt)
	{
        // rslt will be ( result, visible ) so check the
        // second element to determine whether to print
        // the result
        
//		int *pVisible = LOGICAL(VECTOR_ELT(rslt, 1));

		if (*(LOGICAL(VECTOR_ELT(rslt, 1)))) {
//			::WaitForSingleObject(muxExecR, INFINITE);
			Rf_PrintValue(VECTOR_ELT(rslt, 0));
//			::ReleaseMutex(muxExecR);
		}
		// FIXME: print warnings...

		//if( R_CollectWarnings) 
		Rf_PrintWarnings();
		
	}
	*/	
	
	UNPROTECT(1);

}

nlohmann::json& SEXP2JSON( SEXP sexp, nlohmann::json &json ){

	json.clear(); // jic

	if (!sexp)
	{
		json = nullptr;
		return json;
	}

	int len = Rf_length(sexp);
	int type = TYPEOF(sexp);

	// FIXME: there's starting to be a lot of repeated code here...

	if (Rf_isFrame(sexp))
	{
		/*
		int err;
		int nc = len + 1; 
		SEXP s = VECTOR_ELT(sexp, 0);
		int nr = Rf_length(s) + 1;

		// data frames always have row, column names

		rslt->xltype = xltypeMulti;
		rslt->val.array.rows = nr;
		rslt->val.array.columns = nc;
		rslt->val.array.lparray = new XLOPER12[nr*nc];

		// init or excel will show errors for ALL elements
		// include a "" for the top-left corner

		rslt->val.array.lparray[0].xltype = xltypeStr;
		rslt->val.array.lparray[0].val.str = new XCHAR[1];
		rslt->val.array.lparray[0].val.str[0] = 0;

		for (int i = 1; i < nr*nc; i++) rslt->val.array.lparray[i].xltype = xltypeMissing;
		
		// column (member) names

		SEXP cn = PROTECT(R_tryEval(Rf_lang2(R_NamesSymbol, sexp), R_GlobalEnv, &err));
		if (cn)
		{
			len = Rf_length(cn);
			type = TYPEOF(cn);

			if (len != nc-1)
			{
				DebugOut("** Len != NC-1\n");
			}

			for (int c = 0; c < len; c++)
			{
				int idx = c + 1;
				switch (type)
				{
				case INTSXP:	//  13	  // integer vectors 
					if (!ISNA((INTEGER(cn))[c]))
					{
						rslt->val.array.lparray[idx].xltype = xltypeInt;
						rslt->val.array.lparray[idx].val.w = (INTEGER(cn))[c];
					}
					break;
				case REALSXP:	//  14	   real variables  
					rslt->val.array.lparray[idx].xltype = xltypeNum;
					rslt->val.array.lparray[idx].val.num = (REAL(cn))[c];
					break;
				case STRSXP:	//  16	  string vectors - legal?  
					STRSXP2XLOPER(&(rslt->val.array.lparray[idx]), STRING_ELT(cn, c));
					break;
				case CPLXSXP:	//	15	   complex variables 
					CPLXSXP2XLOPER(&(rslt->val.array.lparray[idx]), (COMPLEX(cn))[c]);
					break;
				default:
					DebugOut("** Unexpected type in data frame (col) names: %d\n", type);
					break;
				}
			}

		}
		UNPROTECT(1);

		// get row names, we'll stick them in column 0

		SEXP rn = PROTECT(R_tryEval(Rf_lang2( R_RowNamesSymbol, sexp), R_GlobalEnv, &err));
		if (rn)
		{
			len = Rf_length(rn);
			type = TYPEOF(rn);

			if (len != nr-1)
			{
				DebugOut("** Len != NR-1\n");
			}
			for (int r = 0; r < len; r++)
			{
				int idx = (r+1) * nc + 0;
				switch (type)
				{
				case INTSXP:	//  13	  / integer vectors 
					rslt->val.array.lparray[idx].xltype = xltypeInt;
					rslt->val.array.lparray[idx].val.w = (INTEGER(rn))[r];
					break;
				case STRSXP:	//  16	  * string vectors - legal? * 
					STRSXP2XLOPER(&(rslt->val.array.lparray[idx]), STRING_ELT(rn, r));
					break;
				case CPLXSXP:	//	15	 * complex variables *
					CPLXSXP2XLOPER(&(rslt->val.array.lparray[idx]), (COMPLEX(rn))[r]);
					break;

				default:
					DebugOut( "** Unexpected type in data frame row names: %d\n", type);
					break;
				}
			}
		}
		UNPROTECT(1);

		for (int i = 0; i < nc - 1; i++) 
		{
			s = VECTOR_ELT(sexp, i);
			type = TYPEOF(s);
			len = Rf_length(s);

			if (len != nr)
			{
				DebugOut("** Len != NR\n");
			}

			for (int r = 0; r < len; r++)
			{
				// offset for column names, row names
				int idx = (r+1) * nc + i + 1; 

				switch (type)
				{
				case INTSXP:	//  13	  / * integer vectors * /
					rslt->val.array.lparray[idx].xltype = xltypeInt;
					rslt->val.array.lparray[idx].val.w = (INTEGER(s))[r];
					break;
				case REALSXP:	//  14	  / * real variables * /  
					rslt->val.array.lparray[idx].xltype = xltypeNum;
					rslt->val.array.lparray[idx].val.num = (REAL(s))[r];
					break;
				case STRSXP:	//  16	  / * string vectors - legal? * / 
					STRSXP2XLOPER(&(rslt->val.array.lparray[idx]), STRING_ELT(s, r));
					break;
				case CPLXSXP:	//	15	  / * complex variables * /
					CPLXSXP2XLOPER(&(rslt->val.array.lparray[idx]), (COMPLEX(s))[r]);
					break;

				default:
					DebugOut("** Unexpected type in data frame: %d\n", type);
					break;
				}
			}

		}
		*/
	}
	else if (type == VECSXP)
	{
		// printf( "vecsxp\n" );
		
		// this could be a vector-of-vectors, or it could 
		// be a matrix.  for a matrix, we can only handle
		// two dimensions.

		int nc = len;

		if (Rf_isMatrix(sexp))
		{
			printf( "VECSXP + is matrix [?]\n" );
			/*
			nr = Rf_nrows(sexp);
			nc = Rf_ncols(sexp);

			rslt->xltype = xltypeMulti;
			rslt->val.array.rows = nr;
			rslt->val.array.columns = nc;
			rslt->val.array.lparray = new XLOPER12[nr*nc];

			int idx = 0;
			for (int i = 0; i < nc; i++)
			{
				for (int j = 0; j < nr; j++)
				{
					SEXP v = VECTOR_ELT(sexp, idx);
					type = TYPEOF(v);

					switch (type)
					{
					case INTSXP:	//  13	  / * integer vectors * /

						// this is not working as expected...
						if (!ISNA((INTEGER(v))[0]) && (INTEGER(v))[0] != NA_INTEGER )
						{
							rslt->val.array.lparray[j*nc + i].xltype = xltypeInt;
							rslt->val.array.lparray[j*nc + i].val.w = (INTEGER(v))[0];
						}
						else
						{
							rslt->val.array.lparray[j*nc + i].xltype = xltypeStr;
							rslt->val.array.lparray[j*nc + i].val.str = new XCHAR[1];
							rslt->val.array.lparray[j*nc + i].val.str[0] = 0;
						}
						break;

					case REALSXP:	//  14	  / * real variables * /  
						rslt->val.array.lparray[j*nc + i].xltype = xltypeNum;
						rslt->val.array.lparray[j*nc + i].val.num = (REAL(v))[0];
						break;

					case STRSXP:	//  16	  / * string vectors * / 
						STRSXP2XLOPER(&(rslt->val.array.lparray[j*nc + i]), STRING_ELT(v, 0));
						// ParseResult(&(rslt->val.array.lparray[j*nc + i]), STRING_ELT(sexp, idx)); // this is lazy
						break;

					case CPLXSXP:	//	15	  / * complex variables * /
						CPLXSXP2XLOPER(&(rslt->val.array.lparray[j*nc + i]), (COMPLEX(v))[0]);
						break;

					default:
						DebugOut("Unexpected type in list: %d\n", type);
						break;

					}

					idx++;
				}
			}
			*/
		}
		else // list
		{
			printf( "* not env?\n" );
			
			// len is nc?

			// this is properly treated as an object, but it might not have
			// names, in which case it uses integer indexes -- but we can't 
			// mix the two in json.  in the event there are no names, use
			// $index as the naming convention.

			char buffer[128];
			std::string strname;
			SEXP names = PROTECT(getAttrib(sexp, R_NamesSymbol));
						
			for (int c = 0; c < nc; c++)
			{
				SEXP v = VECTOR_ELT(sexp, c);
				
				// NOTE: per R convention, 1-based
				
				sprintf( buffer, "$%d", c + 1 );
				strname = buffer;
				
				if (names != R_NilValue) {
					const char *tmp = translateChar(STRING_ELT(names, c));
					if (tmp[0]) strname = tmp;
				}
				
				nlohmann::json elt;
				SEXP2JSON( v, elt );
				json[strname] = elt; 
				
			}
			
			UNPROTECT(1);
			
		}
	}
	else if( Rf_isEnvironment( sexp )){
		
		std::string strname;
		int err;
		
		SEXP names = PROTECT(R_tryEval(Rf_lang2(R_NamesSymbol, sexp), R_GlobalEnv, &err));
		if (!err && names != R_NilValue) {
		
			int len = Rf_length(names);
			printf( "names len %d\n", len );
			for (int c = 0; c < len; c++)
			{
				SEXP name = STRING_ELT(names, c);
				const char *tmp = translateChar(name);
				if (tmp[0]){
					strname = tmp;
					SEXP elt = PROTECT(R_tryEval(Rf_lang2(Rf_install("get"), Rf_mkString(tmp)), sexp, &err));
					if( !err ){
						nlohmann::json jsonelt;
						SEXP2JSON( elt, jsonelt );
						json[strname] = jsonelt; 
					}
					UNPROTECT(1);
				} 
			}
			
		}
		else printf( "names = mil;\n" );
		
		UNPROTECT(1);
		
	}
	else if (len > 1)
	{
//		printf( " * VECTOR VALUE\n" );

		// for normal vector, use rows

		int nc = 1, nr = len;

		// for matrix, guaranteed to be 2 dimensions and we 
		// can use the convenient functions to get r/c

		if (Rf_isMatrix(sexp))
		{
			nc = Rf_ncols(sexp);
			nr = Rf_nrows(sexp);
		}

		std::vector< std::vector< nlohmann::json >> cols;

		// FIXME: LOOP ON THE INSIDE (CHECK OPTIM)

		int idx = 0;
		SEXP strsxp;
		
		printf( "NR %d, NC %d\n", nr, nc );
		
		for (int i = 0; i < nc; i++)
		{
			std::vector< nlohmann::json > col;
			for (int j = 0; j < nr; j++)
			{
				nlohmann::json jval;
				switch (type)
				{
				case INTSXP:	
					jval = (INTEGER(sexp))[idx];
					break;

				case REALSXP:
					if (ISNA( (REAL(sexp))[idx] )) jval = nullptr;
					else jval = (REAL(sexp))[idx];
					break;

				case STRSXP:	
					strsxp = STRING_ELT(sexp, idx);
					jval = CHAR(Rf_asChar(strsxp));
					break;

				case CPLXSXP:
					jval = "complex";
					break;

				default:
					printf("Unexpected type in list: %d\n", type);
					break;

				}

				idx++;
				col.push_back( jval );
			}
			nlohmann::json jcol = col;
			cols.push_back(jcol);
		}
		if( cols.size() == 1 ) json = cols[0];
		else json = cols;
		
	}
	else
	{
//		printf( " * SINGLE VALUE\n" );
		
		// single value

		if (isLogical(sexp)) 
		{
			// it seems wasteful having this first, 
			// but I think one of the other types is 
			// intercepting it.  figure out how to do
			// tighter checks and then move this down
			// so real->integer->string->logical->NA->?

			// this is weird, but NA seems to be a logical
			// with a particular value.

			int lgl = Rf_asLogical(sexp);
			if (lgl == NA_LOGICAL)
			{
				// rslt->xltype = xltypeMissing;
				json = nullptr;
			}
			else
			{
				//rslt->xltype = xltypeBool;
				//rslt->val.xbool = lgl ? true : false;
				json = lgl ? true : false;
			}
		}
		else if (Rf_isComplex(sexp))
		{
			json = "complex";
			// CPLXSXP2XLOPER(rslt, *(COMPLEX(sexp)));
		}
		else if (Rf_isInteger(sexp))
		{
			json = Rf_asInteger(sexp);
		}
		else if (isReal(sexp) || Rf_isNumber(sexp))
		{
			json = Rf_asReal(sexp);
		}
		else if (isString(sexp))
		{
			json = CHAR(Rf_asChar(sexp));
		}
	}
	
	return json;
	
}

nlohmann::json& exec_to_json( nlohmann::json &result, 
	std::vector< std::string > &vec, int *err, PARSE_STATUS_2 *ps2, bool withVisible ){
	
	ParseStatus ps;
	SEXP sexp = PROTECT( exec_r( vec, err, &ps, withVisible ));	
	if( ps2 ) *ps2 = (PARSE_STATUS_2)ps;
	
	SEXP2JSON( sexp, result );
	UNPROTECT(1);
	
	return result;
}

void direct_callback_json( const char *channel, const char *json ){
	direct_callback( channel, json );
}

void direct_callback_sexp( const char *channel, SEXP sexp ){
	nlohmann::json json;
	SEXP2JSON( sexp, json );
	direct_callback( channel, json.dump().c_str());
}
