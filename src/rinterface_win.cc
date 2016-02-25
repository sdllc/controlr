
// #include "Shlwapi.h"

#define Win32

#include <windows.h>
#include <stdio.h>
#include <Rversion.h>

#include "controlr_rinterface.h"

#include <string>
#include <vector>
#include <iostream>
#include <exception>

// #define USE_RINTERNALS

#include <Rinternals.h>
#include <Rembedded.h>
#include <graphapp.h>
#include <R_ext\RStartup.h>
#include <signal.h>
#include <R_ext\Parse.h>
#include <R_ext\Rdynload.h>

std::string dllpath;

//HANDLE muxLog;
//HANDLE muxExecR;

#undef clear
#undef length

extern "C" {

extern void Rf_mainloop(void);

extern void R_RestoreGlobalEnvFromFile(const char *, Rboolean);
extern void R_SaveGlobalEnvToFile(const char *);
extern void R_ProcessEvents(void);

}

extern void log_message( const char *buf, int len = -1, bool console = false );

extern void direct_callback_json( const char *channel, const char *json );
extern void direct_callback_sexp( const char *channel, SEXP sexp );


SEXP exec_r(std::vector< std::string > &vec, int *err = 0, ParseStatus *pStatus = 0, bool printResult = false );


/*
void log_message(const char *buf, int len = -1, bool console = false ){
	
	std::cout << "LM " << buf << std::endl;
	
}
*/

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
	
	return input_stream_read( prompt, buf, len, addtohistory );
}

void R_WriteConsole(const char *buf, int len)
{
	log_message(buf, len);
}

void R_CallBack(void)
{
	// std::cout << "callback" << std::endl;
}

void myBusy(int which)
{
	// std::cout << "busy: " << which << std::endl;
}

/** 
 * "ask ok" has no return value.
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

static void my_onintr(int sig) { 
	
	UserBreak = 1; 
}



void r_set_user_break( const char *msg ) {

	// FIXME: synchronize (actually that's probably not helpful, unless we
	// can synchronize with whatever is clearing it inside R, which we can't)

	UserBreak = 1;
	if( msg ) log_message( msg, 0, 1 );
	else log_message("user break", 0, 1);

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

	/////
	/*
	 printf( "R_Quiet? %s\n", Rp->R_Quiet ? "true" : "false" );
	 printf( "R_Slave? %s\n", Rp->R_Slave ? "true" : "false" );
	 printf( "R_Interactive? %s\n", Rp->R_Interactive ? "true" : "false" );
	 printf( "R_Verbose? %s\n", Rp->R_Verbose ? "true" : "false" );
	*/
	
	/////

	// typedef enum {RGui, RTerm, LinkDLL} UImode;
	Rp->CharacterMode = LinkDLL;
	Rp->R_Interactive = TRUE;
	//Rp->CharacterMode = RGui;
	
	Rp->ReadConsole = R_ReadConsole;
	Rp->WriteConsole = R_WriteConsole;
	Rp->CallBack = R_CallBack;
	Rp->ShowMessage = R_AskOk;
	Rp->YesNoCancel = R_AskYesNoCancel;
	Rp->Busy = myBusy;

//	Rp->R_Quiet = FALSE;// TRUE;        /* Default is FALSE */

	Rp->RestoreAction = SA_RESTORE;
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
//	CloseHandle(muxLog);
//	CloseHandle(muxExecR);

}

void r_tick()
{
	R_ProcessEvents();
}

/*
#ifdef WIN32

extern void external_callback( nlohmann::json &json );
extern nlohmann::json& SEXP2JSON( SEXP sexp, nlohmann::json &json );

void external_callback_wrapper( SEXP cmd, SEXP data, SEXP data2 ){

	nlohmann::json json, jcmd, jdata, jdata2;
	json["cmd"] = SEXP2JSON(cmd, jcmd);
	json["data"] = SEXP2JSON(data, jdata);
	json["data2"] = SEXP2JSON(data2, jdata2);
	external_callback(json);
}

extern "C" {
	__declspec( dllexport ) SEXP wrapr_callback( SEXP cmd, SEXP data, SEXP data2 ){
		external_callback_wrapper( cmd, data, data2 );
		return R_NilValue;
	}
}

#endif
*/
