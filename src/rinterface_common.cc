

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

using json = nlohmann::json;

using std::cout;
using std::cerr;
using std::endl;

json &SEXP2JSON( SEXP sexp, json &jresult, bool compress_array = true );

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
			
			// I don't know why I can't get R_Visible properly,
			// but it seems to work using withVisible.
			
			wv = PROTECT(Rf_lang2(Rf_install("withVisible"), Rf_lang2(Rf_install("eval"), cmdexpr)));
			ans = R_tryEval(wv, R_GlobalEnv, &errorOccurred);
			UNPROTECT(1);

			if( errorOccurred ){
				if (err) *err = errorOccurred;
			}
			else {
				if (*(LOGICAL(VECTOR_ELT(ans, 1)))) {
					
					// Rf_PrintValue can throw an exception.  using this wrapper call 
					// protects us against the error, although there's probably a cheaper 
					// way to do that.  
					
					// for some reason this call takes a long time to unwind the error
					// (if there's an error), much longer than calling from R itself.
					// something to do with contexts?
					
					R_tryEval(Rf_lang2(Rf_install("print"), VECTOR_ELT(ans, 0)), R_GlobalEnv, &errorOccurred );
					
					//Rf_PrintValue(VECTOR_ELT(ans, 0));
					
				}
			}

#ifdef WIN32
			Rf_PrintWarnings();
#endif 

            // FIXME: linux? this symbol is not exported, AFAICT.  
            // Check the REPL code for the linux F-E.
			
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

	//SEXP rslt = PROTECT(exec_r(vec, err, &ps, true));
	exec_r(vec, err, &ps, true);

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
	
	// UNPROTECT(1);

}

json &SEXP2JSON( SEXP sexp, json &jresult, bool compress_array ){

	jresult.clear(); // jic

	// check null (and exit early)
	if (!sexp){
		jresult = nullptr;
		return jresult;
	}

	int len = Rf_length(sexp);
	int rtype = TYPEOF(sexp);
	
	// cout << "len " << len << ", type " << rtype << endl;

	if( len == 0 ) return jresult;
	
	// environment
	else if( Rf_isEnvironment( sexp )){
		
		int err;
		std::string strname;
		jresult["$type"] = "environment";
		SEXP names = PROTECT(R_tryEval(Rf_lang2(R_NamesSymbol, sexp), R_GlobalEnv, &err));
		if (!err && names != R_NilValue) {
			int len = Rf_length(names);
			for (int c = 0; c < len; c++)
			{
				SEXP name = STRING_ELT(names, c);
				const char *tmp = translateChar(name);
				if (tmp[0]){
					strname = tmp;
					SEXP elt = PROTECT(R_tryEval(Rf_lang2(Rf_install("get"), Rf_mkString(tmp)), sexp, &err));
					if( !err ){
						json jsonelt;
						SEXP2JSON( elt, jsonelt );
						jresult[strname] = jsonelt; 
					}
					UNPROTECT(1);
				} 
			}
		}
		UNPROTECT(1);
	}
	/*
	// list (or frame)
	else if( Rf_isList( sexp ) || Rf_isFrame( sexp )){

		cout << " + list (frame? " << (Rf_isFrame(sexp) ? "T":"F") << ")" << endl;

		jresult["$type"] = (Rf_isFrame( sexp )) ? "frame" : "list";
		
		// names
		SEXP names = getAttrib(sexp, R_NamesSymbol);
		if( names && TYPEOF(names) != 0 ){
			json jnames;
			jresult["$names"] = SEXP2JSON( names, jnames );
		}

		// dimnames (for rows, if frame)
		SEXP dimnames = getAttrib(sexp, R_DimNamesSymbol);
		if( dimnames && TYPEOF(dimnames) != 0 ){
			json jdimnames;
			jresult["$dimnames"] = SEXP2JSON( dimnames, jdimnames );
		}
		
		// and data
		std::vector < json > data;
		for( int i = 0; i< len; i++ ){
			json elt;
			SEXP x = VECTOR_ELT( sexp, i );
			data.push_back( SEXP2JSON( VECTOR_ELT(sexp, i), elt ));
		}
		jresult["$data"] = data;
	}
	*/
	/*
	// vector; may be multidimensional
	else if( Rf_isVector(sexp) ){

		cout << " + vector, len " << len << ", type " << TYPEOF(sexp) << endl;
	
		if( Rf_isMatrix(sexp) ){
			jresult["$type"] = "matrix";
			jresult["$nrows"] = Rf_nrows(sexp);
			jresult["$ncols"] = Rf_ncols(sexp);
		}
		else jresult["$type"] = "vector";
	
		SEXP dimnames = getAttrib(sexp, R_DimNamesSymbol);
		if( dimnames ){
			json jdimnames;
			jresult["$dimnames"] = SEXP2JSON( dimnames, jdimnames );
		}
		
		std::vector < json > data;
		for( int i = 0; i< len; i++ ){
			json elt;
			
			cout << "BC" << endl;
			SEXP x = VECTOR_ELT( sexp, i );
			cout << "/BC" << endl;

			data.push_back( SEXP2JSON( VECTOR_ELT(sexp, i), elt ));
		}
		jresult["$data"] = data;
		
	}
	*/
	
	// intrinsic type; although these are implicitly vectors
	else {
		
		// cout << " + intrinsic; type " << rtype << ", len " << len << endl;
		
		bool attrs = false;
		bool names = false; // separate from other attrs

		if( Rf_isMatrix(sexp) ){
			jresult["$type"] = "matrix";
			jresult["$nrows"] = Rf_nrows(sexp);
			jresult["$ncols"] = Rf_ncols(sexp);
			attrs = true;
		}
		
		SEXP dimnames = getAttrib(sexp, R_DimNamesSymbol);
		if( dimnames && TYPEOF(dimnames) != 0 ){
			json jdimnames;
			jresult["$dimnames"] = SEXP2JSON( dimnames, jdimnames );
			attrs = true;
		}
		
		SEXP rnames = getAttrib(sexp, R_NamesSymbol);
		json jnames;
		if( rnames && TYPEOF(rnames) != 0 ){
			SEXP2JSON( rnames, jnames, false );
			names = true;
		}
		
		std::vector< json > vector;

		if( Rf_isNull(sexp)){
			json j = {{"$type", "null"}, {"null", true }}; // FIXME: pick one
			vector.push_back(j);
			attrs = true;
		}
		else if (isLogical(sexp)) 
		{
			// it seems wasteful having this first, 
			// but I think one of the other types is 
			// intercepting it.  figure out how to do
			// tighter checks and then move this down
			// so real->integer->string->logical->NA->?

			// this is weird, but NA seems to be a logical
			// with a particular value.
			
			for( int i = 0; i< len; i++ ){
				int lgl = (INTEGER(sexp))[i];
				if (lgl == NA_LOGICAL) vector.push_back( nullptr );
				else vector.push_back( lgl ? true : false );
			}
			
		}
		else if (Rf_isComplex(sexp))
		{
			for( int i = 0; i< len; i++ ){
				Rcomplex cpx = (COMPLEX(sexp))[i];
				vector.push_back({{"$type", "complex"}, {"r", cpx.r }, {"i", cpx.i}});
			}
		}
		else if( Rf_isFactor(sexp)){
			
			SEXP levels = Rf_getAttrib(sexp, R_LevelsSymbol);
			jresult["$type"] = "factor";
			json jlevels;
			jresult["$levels"] = SEXP2JSON(levels, jlevels);
			attrs = true;
			for( int i = 0; i< len; i++ ){ vector.push_back( (INTEGER(sexp))[i] ); }
		}
		else if (Rf_isInteger(sexp))
		{
			for( int i = 0; i< len; i++ ){ vector.push_back( (INTEGER(sexp))[i] ); }
		}
		else if (isReal(sexp) || Rf_isNumber(sexp))
		{
			for( int i = 0; i< len; i++ ){ vector.push_back( (REAL(sexp))[i] ); }
		}
		else if (isString(sexp))
		{
			for( int i = 0; i< len; i++ ){
				/*
				const char *tmp = translateChar(STRING_ELT(sexp, i));
				if (tmp[0]) vector.push_back(tmp);
				else vector.push_back( "" );
				*/
				
				// FIXME: need to unify this in some fashion
				
				SEXP strsxp = STRING_ELT( sexp, i );
				const char *tmp = translateChar(STRING_ELT(sexp, i));
				if( tmp[0] ){
					std::string str = CHAR(Rf_asChar(strsxp));
					vector.push_back(str);
				}
				else vector.push_back( "" );
				
			}
		}
		else if( rtype == VECSXP ){
			
			if( Rf_isFrame( sexp )) jresult["$type"] = "frame";
			else jresult["$type"] = "list";
			attrs = true;
			
			for( int i = 0; i< len; i++ ){
				json elt;
				vector.push_back( SEXP2JSON( VECTOR_ELT(sexp, i), elt ));
			}
		}
		else {
		
			attrs = true;
			jresult["$unparsed"] = true;
			switch( TYPEOF(sexp)){
			case NILSXP: jresult["$type"] = "NILSXP"; break;
			case SYMSXP: jresult["$type"] = "SYMSXP"; break;
			case LISTSXP: jresult["$type"] = "LISTSXP"; break;
			case CLOSXP: jresult["$type"] = "CLOSXP"; break;
			case ENVSXP: jresult["$type"] = "ENVSXP"; break;
			case PROMSXP: jresult["$type"] = "PROMSXP"; break;
			case LANGSXP: jresult["$type"] = "LANGSXP"; break;
			case SPECIALSXP: jresult["$type"] = "SPECIALSXP"; break;
			case BUILTINSXP: jresult["$type"] = "BUILTINSXP"; break;
			case CHARSXP: jresult["$type"] = "CHARSXP"; break;
			case LGLSXP: jresult["$type"] = "LGLSXP"; break;
			case INTSXP: jresult["$type"] = "INTSXP"; break;
			case REALSXP: jresult["$type"] = "REALSXP"; break;
			case CPLXSXP: jresult["$type"] = "CPLXSXP"; break;
			case STRSXP: jresult["$type"] = "STRSXP"; break;
			case DOTSXP: jresult["$type"] = "DOTSXP"; break;
			case ANYSXP: jresult["$type"] = "ANYSXP"; break;
			case VECSXP: jresult["$type"] = "VECSXP"; break;
			case EXPRSXP: jresult["$type"] = "EXPRSXP"; break;
			case BCODESXP: jresult["$type"] = "BCODESXP"; break;
			case EXTPTRSXP: jresult["$type"] = "EXTPTRSXP"; break;
			case WEAKREFSXP: jresult["$type"] = "WEAKREFSXP"; break;
			case RAWSXP: jresult["$type"] = "RAWSXP"; break;
			case S4SXP: jresult["$type"] = "S4SXP"; break;
			case NEWSXP: jresult["$type"] = "NEWSXP"; break;
			case FREESXP: jresult["$type"] = "FREESXP"; break;
			case FUNSXP: jresult["$type"] = "FUNSXP"; break;
			default: jresult["$type"] = "unknwon";
			};
			
		}
		
		// FIXME: make this optional or use a flag.  this sets name->value
		// pairs as in the R list.  however in javascript/json, order isn't 
		// necessarily retained, so it's questionable whether this is a good thing.
		
		if( names ){
			json hash;
			char buffer[64];
			json *target = ( attrs ? &hash : &jresult );
			for( int i = 0; i< len; i++ ){
				std::string strname = jnames[i].get<std::string>();
				if( strname.length() == 0 ){
					sprintf_s( buffer, "$%d", i + 1 ); // 1-based per R convention
					strname = buffer;
				}
				(*target)[strname] = vector[i];
			}
			if( attrs ) jresult["$data"] = hash;
			else jresult["$type"] = rtype;
		}
		else {
			if( attrs ){
				jresult["$data"] = vector;
			}
			else if( len > 1 || !compress_array ) jresult = vector;
			else jresult = vector[0];
		}
				
	}
	
	return jresult;
}

/**
 * in general, we want to reduce to the appropriate type: boolean, 
 * number (don't care about int/double), string.  lists are objects
 * and arrays are arrays.
 *
 * where type information is useful, such as in NULL (intrinsic in R)
 * and complex, we'll include a $type field.  $type is technically a
 * legal symbol in R, but it should be pretty rare as it's hard to use.
  */
json& SEXP2JSON_y( SEXP sexp, json &j, bool compress_array ){

	j.clear(); // jic

	if (!sexp)
	{
		j = nullptr;
		return j;
	}

	int len = Rf_length(sexp);
	int type = TYPEOF(sexp);

	// std::cout << "SEXP2JSON: type " << type << ", len " << len << std::endl;
	
	// FIXME: there's starting to be a lot of repeated code here...

	if (Rf_isFrame(sexp))
	{
		int nr = 0, nc = Rf_length(sexp);
		if( nc > 0 ){
			SEXP c = VECTOR_ELT(sexp, 0);
			nr = Rf_length(c);
		}

		SEXP rownames = getAttrib(sexp, R_DimNamesSymbol);
		if( rownames ){
			json jrownames;
			j["rownames"] = SEXP2JSON( rownames, jrownames );
		}
		
		SEXP colnames = getAttrib(sexp, R_NamesSymbol);
		if( colnames ){
			json jcolnames;
			j["colnames"] = SEXP2JSON( colnames, jcolnames );
		}

		//std::vector< std::vector< json >> cols;
		std::vector< json > cols;

		SEXP strsxp;
	
		for (int i = 0; i < nc; i++)
		{
			std::vector< json > col;
			SEXP c = VECTOR_ELT(sexp, i);
			type = TYPEOF(c);
			
			for (int j = 0; j < nr; j++)
			{
				json jval;
				switch (type)
				{
				case INTSXP:	
					jval = (INTEGER(c))[j];
					break;
					
				case REALSXP:
					if (ISNA( (REAL(c))[j] )) jval = nullptr;
					else jval = (REAL(c))[j];
					break;

				case STRSXP:	
					strsxp = STRING_ELT(c, j);
					jval = CHAR(Rf_asChar(strsxp));
					break;

				case CPLXSXP:
					jval = "complex";
					break;

				default:
					printf("Unexpected type in list: %d\n", type);
					fflush(0);
					break;

				}
				
				col.push_back( jval );
				
			}

			if( Rf_isFactor( c )){
				SEXP levels = Rf_getAttrib(c, R_LevelsSymbol);
				json jlevels;
				SEXP2JSON(levels, jlevels);
				json factor = {{"$type", "factor"}, {"data", col}, {"levels", jlevels}};
				cols.push_back(factor);
			}
			else { 
				json jcol = col;
				cols.push_back(jcol);
			}
			
		}
		
		j["data"] = cols;
		j["$type"] = "frame";
		
	////////////////////////	
	
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
			std::cout << "VECSXP + is matrix [?]" << std::endl;
			
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
			// std::cout << "* not env?" << std::endl;
			
			// len is nc?

			// this is properly treated as an object, but it might not have
			// names, in which case it uses integer indexes -- but we can't 
			// mix the two in json.  in the event there are no names, use
			// $index as the naming convention.

			char buffer[128];
			std::string strname;
			SEXP names = PROTECT(getAttrib(sexp, R_NamesSymbol));
			if( names != R_NilValue ) j["$type"] = "list";
				
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
				
				json elt;
				SEXP2JSON( v, elt );
				j[strname] = elt; 
				
			}
			
			UNPROTECT(1);
			
		}
	}
	else if( Rf_isEnvironment( sexp )){
		
		std::string strname;
		int err;
		j["$type"] = "environment";
		
		SEXP names = PROTECT(R_tryEval(Rf_lang2(R_NamesSymbol, sexp), R_GlobalEnv, &err));
		if (!err && names != R_NilValue) {
		
			int len = Rf_length(names);
			// std::cout << "names len " << len << std::endl;
			
			for (int c = 0; c < len; c++)
			{
				SEXP name = STRING_ELT(names, c);
				const char *tmp = translateChar(name);
				if (tmp[0]){
					strname = tmp;
					SEXP elt = PROTECT(R_tryEval(Rf_lang2(Rf_install("get"), Rf_mkString(tmp)), sexp, &err));
					if( !err ){
						json jsonelt;
						SEXP2JSON( elt, jsonelt );
						j[strname] = jsonelt; 
					}
					UNPROTECT(1);
				} 
			}
			
		}
		// else std::cout << "names = mil;" << std::endl;
		
		UNPROTECT(1);
		
	}
	else if (len > 1)
	{
		// for normal vector, use rows
		int nc = 1, nr = len;

		// for matrix, guaranteed to be 2 dimensions and we 
		// can use the convenient functions to get r/c

		if (Rf_isMatrix(sexp))
		{
			nc = Rf_ncols(sexp);
			nr = Rf_nrows(sexp);
		}

	//	std::vector< std::vector< json >> cols;
		std::vector< json > cols;

		// FIXME: LOOP ON THE INSIDE (CHECK OPTIM)

		int idx = 0;
		SEXP strsxp;
		
//		std::cout << "NR " << nr << ", NC " << nc << std::endl;

		int err;
		char buffer[64]; // buffer for $x names
		SEXP names = PROTECT(R_tryEval(Rf_lang2(R_NamesSymbol, sexp), R_GlobalEnv, &err));
		
		if( names != R_NilValue ) j["$type"] = "list";
		
		for (int i = 0; i < nc; i++)
		{
			std::vector< json > col;
			for (int j = 0; j < nr; j++)
			{
				json jval;
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
					fflush(0);
					break;

				}

				idx++;
				if( names == R_NilValue ) col.push_back( jval );
				else {
					if( col.size() == 0 ){
						json list;
						col.push_back(list);
					}
					
					// see list
					sprintf( buffer, "$%d", j + 1 );
					std::string strname = buffer;
					SEXP name = STRING_ELT(names, j);
					const char *tmp = translateChar(name);
					if( tmp[0] ) strname = tmp;
					col[0][strname] = jval;
				}
				
			}
		
			if( Rf_isFactor(sexp)){
				SEXP levels = Rf_getAttrib(sexp, R_LevelsSymbol);
				json jlevels;
				SEXP2JSON(levels, jlevels);
				json factor = {{"$type", "factor"}, {"data", col}, {"levels", jlevels}};
				cols.push_back(factor);
			}
			else {
				json jcol = col;
				cols.push_back(jcol);
			}
			
		}
		
		// destructure a little bit.  if it's a single column, compress to 
		// rows.  if it has names, remove the array structure.
		
		if( cols.size() == 1 ){
			if( cols[0].size() == 1 ){
				j = cols[0][0];
			}
			else {
				j = cols[0];
			}
		}
		else {
			
			// FIXME: names?
			
			j = cols;
		}
		
		
		UNPROTECT(1);
		
	}
	else
	{
	
		// single value

		if( Rf_isNull(sexp)){
			j = {{"$type", "null"}, {"null", true }}; // FIXME: pick one
		}
		else if (isLogical(sexp)) 
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
				j = nullptr;
			}
			else
			{
				//rslt->xltype = xltypeBool;
				//rslt->val.xbool = lgl ? true : false;
				j = lgl ? true : false;
			}
		}
		else if (Rf_isComplex(sexp))
		{
			Rcomplex cpx = Rf_asComplex(sexp);
			j = {{"$type", "complex"}, {"r", cpx.r }, {"i", cpx.i}};
		}
		else if( Rf_isFactor(sexp)){
			
			SEXP levels = Rf_getAttrib(sexp, R_LevelsSymbol);
			json jlevels;
			SEXP2JSON(levels, jlevels);
			
			// FIXME: should this match up with the list factor type, having a data array of length 1?
			j = {{"$type", "factor"}, {"value", Rf_asInteger(sexp)}, {"levels", jlevels}}; 
			
		}
		else if (Rf_isInteger(sexp))
		{
			j = Rf_asInteger(sexp);
		}
		else if (isReal(sexp) || Rf_isNumber(sexp))
		{
			j = Rf_asReal(sexp);
		}
		else if (isString(sexp))
		{
			j = CHAR(Rf_asChar(sexp));
		}
		else {
			//std::cerr << "unhandled type " << TYPEOF(sexp) << std::endl;
			j = {{"unparsed", true}, {"$type", TYPEOF(sexp)}};
		}
	}
	
	return j;
	
}

json& get_srcref( json &srcref ){
	SEXP2JSON( R_Srcref, srcref );
	return srcref;
}

json& exec_to_json( json &result, 
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
	json json;
	SEXP2JSON( sexp, json );
	direct_callback( channel, json.dump().c_str());
}
