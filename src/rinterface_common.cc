/*
 * controlR
 * Copyright (C) 2016 Structured Data, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included 
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
 
#include "controlr_rinterface.h"
#include "controlr_common.h"

extern unsigned long long GetTimeMs64();

std::vector< std::string > cmdBuffer;

// try to store fuel now, you jerks
#undef clear
#undef length

using std::cout;
using std::cerr;
using std::endl;

JSONDocument &SEXP2JSON( SEXP sexp, JSONDocument &jresult, bool compress_array = true, std::vector < SEXP > envir_list = std::vector < SEXP > () );

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

	return ans;

}

JSONDocument& SEXP2JSON( SEXP sexp, JSONDocument &jresult, bool compress_array, std::vector < SEXP > envir_list ){

	// check null (and exit early)
	if (!sexp){
		// jresult = nullptr;
		return jresult;
	}

	int len = Rf_length(sexp);
	int rtype = TYPEOF(sexp);
	
	// cout << "len " << len << ", type " << rtype << endl;

	if( len == 0 ) return jresult;

	// environment
	else if( Rf_isEnvironment( sexp )){
		
		// make sure this is not in the list; if it is, don't parse it as that 
		// will ultimately recurse

		bool found = false;
		for( std::vector < SEXP > :: iterator iter = envir_list.begin(); iter != envir_list.end(); iter++ ){
			if( *iter == sexp ){
				jresult.add("$type", "environment (recursive)");
				found = true;
				break;
			}
		}

		if( !found ){
			
			int err;
			std::string strname;
    		jresult.add("$type", "environment");
			SEXP names = PROTECT(R_tryEval(Rf_lang2(R_NamesSymbol, sexp), R_GlobalEnv, &err));
			envir_list.push_back( sexp );

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

                            JSONDocument jsonelt;
                            SEXP2JSON( elt, jsonelt, true, envir_list );
							jresult.add( strname.c_str(), jsonelt );
							
						}
						UNPROTECT(1);
					} 
				}
			}
			UNPROTECT(1);
			
		}
	}
	else {
		
		bool attrs = false;
		bool names = false; // separate from other attrs

		if( Rf_isMatrix(sexp) ){

            jresult.add( "$type", "matrix" );
            jresult.add( "$nrows", Rf_nrows(sexp));
            jresult.add( "$ncols", Rf_ncols(sexp));
            
			attrs = true;
		}
		
		SEXP dimnames = getAttrib(sexp, R_DimNamesSymbol);
		if( dimnames && TYPEOF(dimnames) != 0 ){
            JSONDocument jdimnames;
            jresult.add( "$dimnames", SEXP2JSON( dimnames, jdimnames, true, envir_list ));
			attrs = true;
		}
		
		SEXP rnames = getAttrib(sexp, R_NamesSymbol);
        JSONDocument jnames;
		if( rnames && TYPEOF(rnames) != 0 ){
			SEXP2JSON( rnames, jnames, false, envir_list );
			names = true;
		}

		SEXP rrownames = getAttrib(sexp, R_RowNamesSymbol);
		if( rrownames && TYPEOF(rrownames) != 0 ){
			JSONDocument jrownames;
            SEXP2JSON( rrownames, jrownames, false, envir_list );
			attrs = true;
			jresult.add( "$rownames", jrownames );
		}

		SEXP rclass = getAttrib( sexp, R_ClassSymbol );
		if( rclass && !Rf_isNull( rclass )){
			JSONDocument jclass;
            attrs = true;
			jresult.add( "$class", SEXP2JSON( rclass, jclass, true ));
		}

        // the rest get stuffed into an array (at least initially)
        JSONArray vector(len);

		if( Rf_isNull(sexp)){
			JSONDocument j;
            j.add( "$type", "null" );
            j.add( "null", true );
            // vector.push_back(j);
			vector.push( j );
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
			
            // NOTE: there's no way to represent "null" or "undefined" in json -- correct?

			for( int i = 0; i< len; i++ ){
				int lgl = (INTEGER(sexp))[i];
				if (lgl == NA_LOGICAL) vector.push( false ); // FIXME: ?? // vector.push_back( nullptr );
				else vector.push( lgl ? true : false );
			}
			
		}
		else if (Rf_isComplex(sexp))
		{
			for( int i = 0; i< len; i++ ){
				Rcomplex cpx = (COMPLEX(sexp))[i];
                JSONDocument j;
                j.add( "$type", "complex" );
                j.add( "r", cpx.r );
                j.add( "i", cpx.i );
				// vector.push_back({{"$type", "complex"}, {"r", cpx.r }, {"i", cpx.i}});
                vector.push( j );
			}
		}
		else if( Rf_isFactor(sexp)){
			
			SEXP levels = Rf_getAttrib(sexp, R_LevelsSymbol);
			jresult.add( "$type", "factor" );
			JSONDocument jlevels;
            jresult.add( "$levels", SEXP2JSON(levels, jlevels, true, envir_list));
			attrs = true;
			for( int i = 0; i< len; i++ ){ vector.push( (INTEGER(sexp))[i] ); }
		}
		else if (Rf_isInteger(sexp))
		{
			for( int i = 0; i< len; i++ ){ vector.push( (INTEGER(sexp))[i] ); }
		}
		else if (isReal(sexp) || Rf_isNumber(sexp))
		{
			for( int i = 0; i< len; i++ ){ vector.push( (REAL(sexp))[i] ); }
		}
		else if (isString(sexp))
		{
			for( int i = 0; i< len; i++ ){
				/*
				const char *tmp = translateChar(STRING_ELT(sexp, i));
				if (tmp[0]) vector.push_back(tmp);
				else vector.push_back( "" );
				*/
				
				// FIXME: need to unify this in some fashion (what?)
				
				SEXP strsxp = STRING_ELT( sexp, i );
				const char *tmp = translateChar(STRING_ELT(sexp, i));
				if( tmp[0] ){
					std::string str = CHAR(Rf_asChar(strsxp));
					vector.push(str);
				}
				else vector.push( "" );
				
			}
		}
		else if( rtype == VECSXP ){
			
			if( Rf_isFrame( sexp )) jresult.add( "$type", "frame" );
			else jresult.add( "$type", "list" );
			attrs = true;
			
			for( int i = 0; i< len; i++ ){
				JSONDocument elt;
				vector.push( SEXP2JSON( VECTOR_ELT(sexp, i), elt, true, envir_list ));
			}
		}
		else {
		
			attrs = true;
			jresult.add( "$unparsed", true );
            std::string strtype;

			switch( TYPEOF(sexp)){
			case NILSXP: strtype = "NILSXP"; break;
			case SYMSXP: strtype = "SYMSXP"; break;
			case LISTSXP: strtype = "LISTSXP"; break;
			case CLOSXP: strtype = "CLOSXP"; break;
			case ENVSXP: strtype = "ENVSXP"; break;
			case PROMSXP: strtype = "PROMSXP"; break;
			case LANGSXP: strtype = "LANGSXP"; break;
			case SPECIALSXP: strtype = "SPECIALSXP"; break;
			case BUILTINSXP: strtype = "BUILTINSXP"; break;
			case CHARSXP: strtype = "CHARSXP"; break;
			case LGLSXP: strtype = "LGLSXP"; break;
			case INTSXP: strtype = "INTSXP"; break;
			case REALSXP: strtype = "REALSXP"; break;
			case CPLXSXP: strtype = "CPLXSXP"; break;
			case STRSXP: strtype = "STRSXP"; break;
			case DOTSXP: strtype = "DOTSXP"; break;
			case ANYSXP: strtype = "ANYSXP"; break;
			case VECSXP: strtype = "VECSXP"; break;
			case EXPRSXP: strtype = "EXPRSXP"; break;
			case BCODESXP: strtype = "BCODESXP"; break;
			case EXTPTRSXP: strtype = "EXTPTRSXP"; break;
			case WEAKREFSXP: strtype = "WEAKREFSXP"; break;
			case RAWSXP: strtype = "RAWSXP"; break;
			case S4SXP: strtype = "S4SXP"; break;
			case NEWSXP: strtype = "NEWSXP"; break;
			case FREESXP: strtype = "FREESXP"; break;
			case FUNSXP: strtype = "FUNSXP"; break;
			default: strtype = "unknwon";
			};
			jresult.add( "$type", strtype.c_str());
            
		}

    	// FIXME: make this optional or use a flag.  this sets name->value
		// pairs as in the R list.  however in javascript/json, order isn't 
		// necessarily retained, so it's questionable whether this is a good thing.
		
		if( names ){

            JSONDocument hash;
			char buffer[64];
			JSONDocument *target = ( attrs ? &hash : &jresult );

			for( int i = 0; i< len; i++ ){
                std::string strname = jnames[i];

				if( strname.length() == 0 ){
					snprintf( buffer, 64, "$%d", i + 1 ); // 1-based per R convention
					strname = buffer;
				}

                JSONValue jv = vector[(size_t)i];
                target->add( strname.c_str(), jv);
			}
			if( attrs ){	
				jresult.add( "$data", hash );	
				jresult.add( "$names", jnames );
			} 
			else jresult.add( "$type", rtype );
			
		}
		else {
			if( attrs ){
				jresult.add( "$data", vector );
			}
			else if( len > 1 || !compress_array ){
                jresult.take( vector );
            } 
			else {
                JSONValue src = vector[(size_t)0];
                jresult.take( src );
            }
		}

        // used to add class here.  not sure exactly why...
				
	}
	
	return jresult;
}

JSONDocument& get_srcref( JSONDocument &srcref ){
	SEXP2JSON( R_Srcref, srcref );
	return srcref;
}

JSONDocument& exec_to_json( JSONDocument &result, 
	std::vector< std::string > &vec, int *err, PARSE_STATUS_2 *ps2, bool withVisible ){

	ParseStatus ps;
	SEXP sexp = PROTECT( exec_r( vec, err, &ps, withVisible ));	
	if( ps2 ) *ps2 = (PARSE_STATUS_2)ps;
	SEXP2JSON( sexp, result );
	UNPROTECT(1);

	return result;
}

void direct_callback_json( const char *channel, const char *json, bool buffer ){
	direct_callback( channel, json, buffer );
}

void direct_callback_sexp( const char *channel, SEXP sexp, bool buffer ){
    JSONDocument doc;
	SEXP2JSON( sexp, doc );
	direct_callback( channel, doc.toString().c_str(), buffer);
}

SEXP direct_callback_sync( SEXP sexp, bool buffer ){

    JSONDocument j;
    SEXP2JSON( sexp, j );

	JSONDocument response;
    sync_callback( response, j.toString().c_str(), buffer );
	
    // FIXME: handle more, complex types? would require a JSON2SEXP method

    if( response.is_string()){
        std::string str = (std::string)(response);
        return Rf_mkString(str.c_str());
    }
    
    if( response.is_number()){
        return Rf_ScalarReal(response);
    }
    
    if( response.is_bool()){
        return Rf_ScalarLogical((bool)response ? 1 : 0);
    }

	return R_NilValue;
}

