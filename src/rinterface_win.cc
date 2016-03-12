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

	// instead of the "dlldo1" loop -- this one seems to be stable in setjmp/longjmp call
	extern void Rf_mainloop(void);

	// in case we want to call these programatically (TODO)
	extern void R_RestoreGlobalEnvFromFile(const char *, Rboolean);
	extern void R_SaveGlobalEnvToFile(const char *);
	
	// for win32
	extern void R_ProcessEvents(void);

};

/**
 * we're now basing "exec" commands on the standard repl; otherwise 
 * we have to have two parallel paths for exec and debug.
 */
int R_ReadConsole(const char *prompt, char *buf, int len, int addtohistory)
{
	static bool final_init = false;
	
	if( !final_init ){
		final_init = true;
		R_RegisterCCallable("ControlR", "CallbackJSON", (DL_FUNC)direct_callback_json);
		R_RegisterCCallable("ControlR", "CallbackSEXP", (DL_FUNC)direct_callback_sexp);
	}

	const char *cprompt = CHAR(STRING_ELT(GetOption1(install("continue")), 0));
	bool is_continuation = ( !strcmp( cprompt, prompt ));
	
	return input_stream_read( prompt, buf, len, addtohistory, is_continuation );
}

/**
 * console messages are passed through.  note the signature is different on 
 * windows/linux so implementation is platform dependent.
 */
void R_WriteConsoleEx( const char *buf, int len, int flag )
{
	console_message(buf, len, flag);
}

/** 
 * "ask ok" has no return value.  I guess that means "ask, then press OK", 
 * not "ask if this is ok".
 */
void R_AskOk(const char *info) {
	::MessageBoxA(0, info, "Message from R", MB_OK);
}

/** 
 * 1 (yes) or -1 (no), I believe (based on #defines)
 */
int R_AskYesNoCancel(const char *question) {
	return (IDYES == ::MessageBoxA(0, question, "Message from R", MB_YESNOCANCEL)) ? 1 : -1;
}


void R_CallBack(void)
{
}

void R_Busy(int which)
{
}

void r_set_user_break( const char *msg ) {

	// FIXME: synchronize (actually that's probably not helpful, unless we
	// can synchronize with whatever is clearing it inside R, which we can't)

	UserBreak = 1;
	if( msg ) console_message( msg, 0, 1 );
	else console_message("user break", 0, 1);

}

int r_init( const char *rhome, const char *ruser, int argc, char ** argv ){

	structRstart rp;
	Rstart Rp = &rp;
	char Rversion[25];

	char RHome[MAX_PATH];
	char RUser[MAX_PATH];

	DWORD dwPreserve = 0;

	if( rhome ) strcpy( RHome, rhome );
	if( ruser ) strcpy( RUser, ruser );

	sprintf_s(Rversion, 25, "%s.%s", R_MAJOR, R_MINOR);
	if (strcmp(getDLLVersion(), Rversion) != 0) {
		// fprintf(stderr, "Error: R.DLL version does not match\n");
		// exit(1);
		return -1;
	}

	R_setStartTime();
	R_DefParams(Rp);

	Rp->rhome = RHome;
	Rp->home = RUser;

	// typedef enum {RGui, RTerm, LinkDLL} UImode;
	Rp->CharacterMode = LinkDLL;
	Rp->R_Interactive = TRUE;
	
	Rp->ReadConsole = R_ReadConsole;
	Rp->WriteConsole = NULL;
	Rp->WriteConsoleEx = R_WriteConsoleEx;
	
	Rp->Busy = R_Busy;
	Rp->CallBack = R_CallBack;
	Rp->ShowMessage = R_AskOk;
	Rp->YesNoCancel = R_AskYesNoCancel;

	Rp->RestoreAction = SA_RESTORE; // FIXME -- should we handle this in code?
	Rp->SaveAction = SA_NOSAVE;
	
	R_SetParams(Rp);
	R_set_command_line_arguments(0, 0);
	FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
	GA_initapp(0, 0);

	Rf_mainloop();
	Rf_endEmbeddedR(0);

	return 0;

}

void r_shutdown()
{
//	Rf_endEmbeddedR(0); // now called in init (which never exits)
}

void r_tick()
{
	R_ProcessEvents();
}
