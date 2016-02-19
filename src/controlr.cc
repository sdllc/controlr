
#include "controlr.h"

using namespace std;

using json = nlohmann::json;

uv_loop_t *loop;

uv_pipe_t _pipe;
uv_tcp_t _tcp;

uv_stream_t *client;

uv_timer_t timer;
uv_async_t async;
uv_mutex_t async_mutex;

// FIXME: tune 
#define INITIAL_TIMER_TICK 100
#define TIMER_TICK 100

bool closing_sequence = false;
bool initialized = false;

std::string arg0;

// from docs:
// ...
// Warning libuv will coalesce calls to uv_async_send(), that is, not every 
// call to it will yield an execution of the callback. For example: if 
// uv_async_send() is called 5 times in a row before the callback is called, 
// the callback will only be called once. If uv_async_send() is called again 
// after the callback was called, it will be called again.
// ...
// so in other words, don't use the data field of the async handle.

typedef std::vector< std::string > STRVECTOR;
STRVECTOR async_data;

void write_callback(uv_write_t *req, int status) ;

/** 
 * write a packet plus a newline (which we use as a separator) 
 */
__inline void writeJSON( json &j, uv_stream_t* client, uv_write_cb cb, std::vector< char* > *pool = 0){
	
	std::string str = j.dump();
	int len = (int)str.length();
		
	char *sz = (char*)( malloc( len + 1 ));
	memcpy( sz, str.c_str(), len );
	sz[len++] = '\n';

	uv_write_t *req = (uv_write_t *) malloc(sizeof(uv_write_t));

	if( pool ){
		pool->push_back(sz);
		req->data = 0;
	}
	else {
		req->data = sz;
	}
		
	uv_buf_t wrbuf = uv_buf_init(sz, len);
	uv_write(req, client, &wrbuf, 1, cb);
	
}

/** callback from R */
void direct_callback( const char *channel, const char *data ){
	
	// FIXME: don't do this twice
	// FIXME: proper handling (at least for debug purposes)

	try {		
		json j = json::parse(data);
		json response = {{"type", channel}, {"data", j}};
		if( uv_is_writable( client ))
			writeJSON( response, client, write_callback);
        else cout << " ** WARNING: not writable" << endl;	
	}
	catch( ... ){
		cout << "JSON parse exception (unknown)" << endl;
		cout << data << endl;
	}
		
}

/**
 * this function is called by R when R code calls into the
 * exe via .Call.  we need to get onto the appropriate thread
 * and then send the message [NOTE: we are on the right thread,
 * all this async business is unecessary].
 */
void external_callback( json &j ){
	
	/*
	std::string s = j.dump(); 
	uv_mutex_lock(&async_mutex);
	async_data.push_back(s);
	uv_mutex_unlock(&async_mutex);
	uv_async_send(&async);
	*/
	
	// we are on the right thread!

	if( uv_is_writable( client )){
		json msg = {{"type", "callback"}, {"data", j}};
			writeJSON( msg, client, write_callback);
	}	
    else cout << " ** WARNING: not writable" << endl;	
	
}

/** 
 * callback on write op.  this class always allocates
 * buffers and stores them in data, so always free.
 *
 * FIXME: don't do that.
 */
void write_callback(uv_write_t *req, int status) {
	
	if (status < 0) {
		fprintf(stderr, "Write error %s\n", uv_err_name(status));
	}
	if( req->data ) {
		free( req->data );
	} 
	free(req);
	
	if( closing_sequence ){
		uv_close( (uv_handle_t*)client, NULL );
	}
	
}

/**
 * on the correct thread 
 */
void async_callback( uv_async_t *handle ){

	STRVECTOR vector;

	uv_mutex_lock(&async_mutex);
	for( STRVECTOR::iterator iter = async_data.begin(); iter != async_data.end(); iter++ ){
		vector.push_back(*iter);
	}
	async_data.clear();
	uv_mutex_unlock(&async_mutex);

	for( STRVECTOR::iterator iter = vector.begin(); iter != vector.end(); iter++ ){
		// there's an unnecessary parse/unparse step here
		json data = json::parse(*iter);
		cout << *iter << endl;
		json j = {{"type", "callback"}, {"data", data}};
		writeJSON( j, client, write_callback);
	}
}

/**
 * call process events in R to support threaded operations
 * (like the httpd server) and tcl stuff
 */
void timer_callback(uv_timer_t *handle){
	uv_timer_stop( &timer );
	if( initialized ) r_tick();
	uv_timer_start(&timer, timer_callback, TIMER_TICK, TIMER_TICK);
}

/**
 * output from R, pass to parent process as a formatted object
 * NOTE that we are buffering until there's a newline
 */
void log_message( const char *buf, int len = -1, bool console = false ){
	static std::ostringstream os;
	os << buf;
	if( len < 0 ) len = strlen(buf);
	if( len > 0 && buf[len-1] == '\n' )
	{
		json j = {{"type", "console"}, {"message", os.str()}, {"flag", console}};
		writeJSON( j, client, write_callback);
		os.str("");
		os.clear();
	}
}

/** standard alloc method */
void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
  buf->base = (char*)malloc(suggested_size);
  buf->len = suggested_size;
}

/** handle inbound command */
void processCommand( json &j ){

	json response = {{"type", "response"}};
	std::string cmd = j["command"];
 
    if (j.find("id") != j.end()) {
        response["id"] = j["id"];
	}
 
	// cout << "---\n" << j.dump() << "\n---" << endl;

	// toll the timer while we're operating
	uv_timer_stop( &timer );
	
	if( !cmd.compare( "break" )){
		
		// NOTE: this command does not send a response
		// nor trigger a callback.  therefore we exit 
		// the function here, make sure not to allocate
		// anything prior to this exit
	
		cout << "sending break" << endl;
		r_set_user_break();
		
		// restart the timer.  ugly.
		uv_timer_start(&timer, timer_callback, TIMER_TICK, TIMER_TICK);
		return;
		
	}
	else if( !cmd.compare( "rinit" )){
		
		std::string rhome;
		std::string ruser;

		if (j.find("rhome") != j.end()) rhome = j["rhome"].get<std::string>();
		if (j.find("ruser") != j.end()) ruser = j["ruser"].get<std::string>();
		
		char *proc = new char[ arg0.length() + 1 ];
		char nosave[] = "--no-save";
		strcpy( proc, arg0.c_str());
		char* argv[] = { proc, nosave };
		r_init( rhome.c_str(), ruser.c_str(), 2, argv );
		delete[] proc;
		initialized = true;
	
	}
	else if( !cmd.compare( "exec" ) || !cmd.compare( "internal" )){
		
		int err = 0;
		PARSE_STATUS_2 ps = PARSE2_EOF;
		bool internal = !cmd.compare("internal");
		
		if (j.find("commands") != j.end()) {
			
			std::vector < std::string > strvec;
			json commands = j["commands"];
			for( json::iterator iter = commands.begin(); iter != commands.end(); iter++ ){
				std::string str = iter->get<std::string>();
				strvec.push_back( str );
			}
			
			if( internal ){
				json rslt;
				response["response"] = exec_to_json( rslt, strvec, &err, &ps, false );
			}
			else r_exec_vector(strvec, &err, &ps, true, false );
		}
		
		response["parsestatus"] = ps;
		if( err ) response["err"] = err;
	}
	else if( !cmd.compare( "rshutdown" )){
		
		closing_sequence = true;
		initialized = false;
		uv_close( (uv_handle_t*)&async, NULL );
		r_shutdown();

	}
	else {
		response["err"] = 2;
		response["message"] = "Unknown command";
		response["command"] = cmd;
	}

	writeJSON( response, client, write_callback);

	if( !closing_sequence )
		uv_timer_start(&timer, timer_callback, TIMER_TICK, TIMER_TICK);

	
}

void read_cb(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {

    if (nread < 0) {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t*) client, NULL);
        return;
    }

	json j = json::parse(std::string( buf->base, buf->len ));
	if (j.find("command") != j.end()) {
		processCommand( j );
	}
	else cout << "command not found" << endl;
	 
	free(buf->base);
}

void connect_cb(uv_connect_t* req, int status){

	cout << "connect_cb, status=" << status << endl;
	
	if( !status ){
		if( uv_is_readable( client )){
			
			cout << "readable, calling read start" << endl;
			uv_read_start( client, alloc_buffer, read_cb );
		}
		else cout <<"client not readable!" << endl;
	}
}

int main( int argc, char **argv ){

	// TODO: support tcp sockets as well 

	if( argc <= 1 )
	{
		cout << "controlr" << endl;
		return 0;
	}

	arg0 = argv[0];

	loop = uv_default_loop();
	
	uv_connect_t req;

	uv_mutex_init( &async_mutex );
	uv_timer_init( loop, &timer );
	uv_timer_start( &timer, timer_callback, INITIAL_TIMER_TICK, TIMER_TICK );
	uv_async_init( loop, &async, async_callback );

	if( argc > 2 ){
		cout << "connecting tcp: " << argv[1] << ":" << atoi(argv[2]) << endl;
		uv_tcp_init(loop, &_tcp);
		client = (uv_stream_t*)&_tcp;
		struct sockaddr_in dest;
		uv_ip4_addr( argv[1], atoi( argv[2] ), &dest);
		uv_tcp_connect( &req, &_tcp, (const struct sockaddr*)&dest, connect_cb);
	}
	else {
		cout << "connecting pipe" << endl;
		client = (uv_stream_t*)&_pipe;
		uv_pipe_init(loop, &_pipe, 0 /* ipc */);
		uv_pipe_connect( &req, &_pipe, argv[1], connect_cb);
	}	

	return uv_run(loop, UV_RUN_DEFAULT);
	
	// clean up
	uv_mutex_destroy(&async_mutex);
	cout << "process exit" << endl << flush;

}

