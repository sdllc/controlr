
// #include "Shlwapi.h"

#define Win32

#include <windows.h>
#include <stdio.h>
#include <Rversion.h>

#include "controlr_rinterface.h"

#include <string>
#include <vector>
#include <iostream>

// #define USE_RINTERNALS

#include <Rinternals.h>
#include <Rembedded.h>
#include <graphapp.h>
#include <R_ext\RStartup.h>
#include <signal.h>
#include <R_ext\Parse.h>
#include <R_ext\Rdynload.h>

std::string dllpath;

HANDLE muxLog;
HANDLE muxExecR;

#undef clear
#undef length

//
// for whatever reason these are not exposed in the embedded headers.
//
extern "C" {

extern void R_RestoreGlobalEnvFromFile(const char *, Rboolean);
extern void R_SaveGlobalEnvToFile(const char *);
extern void R_ProcessEvents(void);

}

extern void RibbonClearUserButtons();
extern void RibbonAddUserButton(std::string &strLabel, std::string &strFunc, std::string &strImgMso);
extern void log_message( const char *buf, int len = -1, bool console = false );

extern void direct_callback_json( const char *channel, const char *json );
extern void direct_callback_sexp( const char *channel, SEXP sexp );


SEXP exec_r(std::vector< std::string > &vec, int *err = 0, ParseStatus *pStatus = 0, bool printResult = false );


/*
void log_message(const char *buf, int len = -1, bool console = false ){
	
	std::cout << "LM " << buf << std::endl;
	
}
*/


int R_ReadConsole(const char *prompt, char *buf, int len, int addtohistory)
{
	std::cout << "READ CONSOLE CALLED" << std::endl;
	
	return debug_read( prompt, buf, len, addtohistory );
	
	// unless we call one of the repl functions 
	// this will never get called, so we can ignore it
	
	return 0;
	
}

void R_WriteConsole(const char *buf, int len)
{
	log_message(buf, len);
}

void R_CallBack(void)
{
	/* called during i/o, eval, graphics in ProcessEvents */
	// DebugOut("R_CallBack\n");
	// printf(" * R_CallBack\n");
}

void myBusy(int which)
{
	/* set a busy cursor ... if which = 1, unset if which = 0 */
	// DebugOut("busy\n");
	printf(" * myBusy\n");
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
	
	std::cout << "EAT SIGNALS" << std::endl;
	
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

	muxLog = ::CreateMutex(0, 0, 0);
	muxExecR = ::CreateMutex(0, 0, 0);

	R_setStartTime();
	R_DefParams(Rp);

/*
	if (!CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, RHome, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_R_HOME))
	{
		ExpandEnvironmentStringsA(DEFAULT_R_HOME, RHome, MAX_PATH);
	}

	if (!CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, RUser, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_R_USER))
	{
		ExpandEnvironmentStringsA(DEFAULT_R_USER, RUser, MAX_PATH);
	}
	
	if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dwPreserve, REGISTRY_KEY, REGISTRY_VALUE_PRESERVE_ENV)) {
		dwPreserve = DEFAULT_R_PRESERVE_ENV;
	}
*/

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
	//Rp->CharacterMode = RGui;
	
	Rp->ReadConsole = R_ReadConsole;
	Rp->WriteConsole = R_WriteConsole;
	Rp->CallBack = R_CallBack;
	Rp->ShowMessage = R_AskOk;
	Rp->YesNoCancel = R_AskYesNoCancel;
	Rp->Busy = myBusy;

	Rp->R_Quiet = FALSE;// TRUE;        /* Default is FALSE */

	Rp->RestoreAction = SA_RESTORE;
	Rp->SaveAction = SA_NOSAVE;
	
	/*
	
	printf( "R_Quiet? %s\n", Rp->R_Quiet ? "true" : "false" );
	printf( "R_Slave? %s\n", Rp->R_Slave ? "true" : "false" );
	printf( "R_Interactive? %s\n", Rp->R_Interactive ? "true" : "false" );
	printf( "R_Verbose? %s\n", Rp->R_Verbose ? "true" : "false" );
	printf( "CharacterMode? %d\n", (int)(Rp->CharacterMode));

	*/

	R_SetParams(Rp);
	R_set_command_line_arguments(0, 0);

	FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));

//	signal(SIGBREAK, my_onintr);

	GA_initapp(0, 0);
	setup_Rmainloop();
	R_ReplDLLinit();
	R_RegisterCCallable("ControlR", "CallbackJSON", (DL_FUNC)direct_callback_json);
	R_RegisterCCallable("ControlR", "CallbackSEXP", (DL_FUNC)direct_callback_sexp);

	/*
	::WaitForSingleObject(muxExecR, INFINITE );

	::ReleaseMutex(muxExecR);
	*/
	
	// load up

	
	
	return 0;

}

void r_shutdown()
{
	char RUser[MAX_PATH];
	DWORD dwPreserve = 0;

	Rf_endEmbeddedR(0);
	CloseHandle(muxLog);
	CloseHandle(muxExecR);

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
