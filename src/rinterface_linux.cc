
#include <stdio.h>
#include <signal.h>

#include <unistd.h>

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
// #include <graphapp.h>

#include <R_ext/RStartup.h>
#include <R_ext/Parse.h>
#include <R_ext/Rdynload.h>

//bool g_buffering = false;
//std::vector< std::string > logBuffer;
//std::vector< std::string > cmdBuffer;

// -------------------------------------------------------------

extern void log_message( const char *buf, int len = -1, bool console = false );

void r_set_user_break( const char *msg ){
    
}

void flush_log(){}

void r_tick()
{
	// ?? // R_ProcessEvents();
}

// extern void (*ptr_R_WriteConsole)(const char *, int);
void R_WriteConsole( const char* message, int len ){
    //std::cout << "R| " << message;
    log_message( message, len );
}

int r_init( const char *rhome, const char *ruser, int argc, char ** argv ){

    // we can't use Rf_initEmbeddedR because we need
    // to set pointer (and set NULLs) in between calls
    // to Rf_initialize_R and setup_Rmainloop.

    Rf_initialize_R(argc, (char**)argv);

    ptr_R_WriteConsole = R_WriteConsole;
    R_Outputfile = NULL;
    R_Consolefile = NULL;

    setup_Rmainloop();
    R_ReplDLLinit();

  

    printf( "r_init exit\n" );
    
    return 0;
}

void r_shutdown(){

    printf( "r_shutdown\n" );
  
    Rf_endEmbeddedR(0);

}



