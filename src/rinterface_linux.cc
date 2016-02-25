
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include <string>
#include <vector>
#include <iostream>

#include "controlr_rinterface.h"

// #define USE_RINTERNALS
#define R_INTERFACE_PTRS

#include <Rversion.h>
#include <Rinternals.h>
#include <Rembedded.h>
#include <Rinterface.h>

#include <R_ext/RStartup.h>
#include <R_ext/Parse.h>
#include <R_ext/Rdynload.h>

#include <setjmp.h>

//bool g_buffering = false;
//std::vector< std::string > logBuffer;
//std::vector< std::string > cmdBuffer;

extern void direct_callback_json( const char *channel, const char *json );
extern void direct_callback_sexp( const char *channel, SEXP sexp );


#undef length

// -------------------------------------------------------------

extern void log_message( const char *buf, int len = -1, bool console = false );

void r_set_user_break( const char *msg ){
    
}

void flush_log(){}

void r_tick()
{
	// ?? // R_ProcessEvents();
}

int R_ReadConsole( const char * prompt, unsigned char *buf, int len, int addtohistory){

	//std::cout << "READ CONSOLE CALLED" << std::endl;
	//return debug_read( prompt, (char*)buf, len, addtohistory );
	return input_stream_read( prompt, (char*)buf, len, addtohistory );
	
}
	
void R_ShowMessage( char *msg ){
	
	std::cout << "RSM| " << msg << std::endl;
	
}

void R_WriteConsoleEx( const char* message, int len, int status ){
    
    // in an effort to reduce messages, and smooth flow,
    // we are buffering until there's a newline.
    
    // FIXME: use stringstream?
    /*
    static std::string buffer;
    buffer += message;
    if( len <= 0 ) len = strlen( message );
    if( len > 0 && message[len-1] == '\n' ){
       log_message( buffer.c_str(), buffer.length()); 
       buffer.clear();
    }
    else if( len > 0 ){
		std::cout << "LM| " << message << std::endl;
	}
	*/
    // 
	log_message( message, len );
}

void R_FlushConsole(){
	std::cout << "RFC" << std::endl;
}

void R_ClearErrConsole(){
	std::cout << "RCEC" << std::endl;
}

void R_ResetConsole(){
	std::cout << "RRC" << std::endl;
}

void R_ProcessEvents(){
	std::cout << "RPE" << std::endl;
}

int r_init( const char *rhome, const char *ruser, int argc, char ** argv ){

    // we can't use Rf_initEmbeddedR because we need
    // to set pointer (and set NULLs) in between calls
    // to Rf_initialize_R and setup_Rmainloop.

    Rf_initialize_R(argc, (char**)argv);

	ptr_R_WriteConsole = NULL;
	ptr_R_WriteConsoleEx = R_WriteConsoleEx;
	ptr_R_ReadConsole = R_ReadConsole;   	
    ptr_R_ShowMessage = R_ShowMessage;
		
	ptr_R_ResetConsole = R_ResetConsole;
	ptr_R_FlushConsole = R_FlushConsole;
	ptr_R_ClearerrConsole = R_ClearErrConsole;

//	ptr_R_ProcessEvents = R_ProcessEvents;

    R_Outputfile = NULL;
    R_Consolefile = NULL;
	R_Interactive = TRUE;
	
	setup_Rmainloop();
	R_ReplDLLinit();
	R_RegisterCCallable("ControlR", "CallbackJSON", (DL_FUNC)direct_callback_json);
	R_RegisterCCallable("ControlR", "CallbackSEXP", (DL_FUNC)direct_callback_sexp);

	while(R_ReplDLLdo1() > 0);
	
	Rf_endEmbeddedR(0);

    // printf( "r_init exit\n" );
    return 0;
}


void r_shutdown(){

    // printf( "r_shutdown\n" );
    // Rf_endEmbeddedR(0);

}



