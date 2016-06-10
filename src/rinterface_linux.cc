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

#include "controlr_common.h"
#include "controlr_rinterface.h"

extern "C" {
	
	extern void Rf_mainloop(void);
	
	// although the types are masked, the objects 
	// are exposed and the interface uses pointers.
	
	extern void R_runHandlers(void *handlers, fd_set *readMask);
	extern void *R_InputHandlers;
	extern fd_set *R_checkActivity(int, int);

}

void r_set_user_break( const char *msg ){
    
	// if you're using pthreads, raise() calls pthread_kill(self), 
	// so don't use that.  just a heads up.
	
	kill( getpid(), SIGINT );	

	// FIXME: use the flag in log_message here to signal it's a system message

	if( msg ) console_message( msg, 0, 1 );
	else console_message("user break\n", 0, 1);

}

void r_tick()
{
	R_runHandlers(R_InputHandlers, R_checkActivity(0, 0));
}

int R_ReadConsole( const char * prompt, unsigned char *buf, int len, int addtohistory){
	
	static bool final_init = false;
	
	if( !final_init ){
		final_init = true;
		R_RegisterCCallable("ControlR", "CallbackJSON", (DL_FUNC)direct_callback_json);
		R_RegisterCCallable("ControlR", "CallbackSEXP", (DL_FUNC)direct_callback_sexp);
		R_RegisterCCallable("ControlR", "CallbackSync", (DL_FUNC)direct_callback_sync);
	}
	
	const char *cprompt = CHAR(STRING_ELT(GetOption1(install("continue")), 0));
	bool is_continuation = ( !strcmp( cprompt, prompt ));
	return input_stream_read( prompt, (char*)buf, len, addtohistory, is_continuation );
}
	
void R_ShowMessage( char *msg ){
	
	std::cout << "RSM| " << msg << std::endl;
	
}

void R_WriteConsoleEx( const char* message, int len, int status ){
	console_message( message, len, status );
}

int r_loop( const char *rhome, const char *ruser, int argc, char ** argv ){

    // we can't use Rf_initEmbeddedR because we need
    // to set pointer (and set NULLs) in between calls
    // to Rf_initialize_R and setup_Rmainloop.

    Rf_initialize_R(argc, (char**)argv);

	ptr_R_WriteConsole = NULL;
	ptr_R_WriteConsoleEx = R_WriteConsoleEx;
	ptr_R_ReadConsole = R_ReadConsole;   	
    ptr_R_ShowMessage = R_ShowMessage; // ??

	/*	
	ptr_R_ResetConsole = R_ResetConsole;
	ptr_R_FlushConsole = R_FlushConsole;
	ptr_R_ClearerrConsole = R_ClearErrConsole;
	ptr_R_ProcessEvents = R_ProcessEvents;
	*/
	
    R_Outputfile = NULL;
    R_Consolefile = NULL;
	R_Interactive = TRUE;
	
	Rf_mainloop();
	Rf_endEmbeddedR(0);

    return 0;
}
