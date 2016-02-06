
#ifndef __CONTROLR_RINTERFACE_H
#define __CONTROLR_RINTERFACE_H

#define MAX_LOGLIST_SIZE	80
#define MAX_CMD_HISTORY		2500

#include <iostream>
#include <vector>
#include <string>
#include <list>

#include "controlr_common.h"
#include "3dparty/json.hpp"

void r_exec_vector(std::vector<std::string> &vec, int *err, PARSE_STATUS_2 *status, bool printResult, bool excludeFromHistory);
void r_set_user_break( const char *msg = 0 );

void r_tick();

nlohmann::json& exec_to_json( nlohmann::json &result, 
	std::vector< std::string > &vec, int *err = 0, PARSE_STATUS_2 *ps = 0, bool withVisible = false );
	

int r_init( const char *rhome, const char *ruser, int argc, char **argv );

void r_shutdown();
void flush_log();

#endif // #ifndef __CONTROLR_RINTERFACE_H
