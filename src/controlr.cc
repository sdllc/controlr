
#include "controlr.h"

using namespace std;
using json = nlohmann::json;

uv_pipe_t _pipe;
uv_tcp_t _tcp;

uv_stream_t *client;

uv_timer_t timer;

locked_vector < json > command_queue;
locked_vector < json > response_queue;
locked_vector < json > debug_queue;

uv_async_t async_on_main_loop;
uv_async_t async_on_thread_loop;

uv_timer_t console_buffer_timer;

uv_cond_t debug_condition;
uv_mutex_t debug_condition_mutex;

// FIXME: tune 
#define INITIAL_TIMER_TICK 100
#define TIMER_TICK 100

// FIXME: tune
// what is the unit? ns?
#define CONSOLE_BUFFER_EVENTS_TICK (1000 * 1000 * 100)

// FIXME: tune
#define CONSOLE_BUFFER_TICK 25

bool closing_sequence = false;
bool initialized = false;

std::string str_connection;
int i_port;

std::string arg0;
locked_ostringstream os_buffer;

// from docs:
// ...
// Warning libuv will coalesce calls to uv_async_send(), that is, not every 
// call to it will yield an execution of the callback. For example: if 
// uv_async_send() is called 5 times in a row before the callback is called, 
// the callback will only be called once. If uv_async_send() is called again 
// after the callback was called, it will be called again.
// ...
// so in other words, don't use the data field of the async handle.

void write_callback(uv_write_t *req, int status) ;

__inline void push_response( json &j ){
	
	response_queue.locked_push_back( j );
	uv_async_send( &async_on_thread_loop );

}

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

int debug_read(const char *prompt, char *buf, int len, int addtohistory){

	json srcref;
	json response = {{"type", "debug"}, {"data", {{"prompt", prompt}, {"srcref", get_srcref(srcref)}}}};
	push_response( response );

	cout << "waiting on condition (max " << len << ")..." << endl;
	
	while( true ){
	
		// NOTE: from docs:
		//
		//   Note Callers should be prepared to deal with spurious 
		//   wakeups on uv_cond_wait() and uv_cond_timedwait().
		//
		// that's from the underlying pthreads implementation.  we need 
		// a mechanism to determine if that happens.  nevertheless we like 
		// conditions because there's a wait available with a timeout.
		
		uv_cond_timedwait( &debug_condition, &debug_condition_mutex, CONSOLE_BUFFER_EVENTS_TICK ); 

		std::vector < json > commands;
		debug_queue.locked_give_all( commands );
		
		if( !commands.size()){

			// if commands is empty, this is either a timeout or one
			// of the stray signals.  in either case, process events.
			// this takes the place of the timer, which is blocked.
			
			if( initialized ) r_tick();
			
		}
		else {

			cout << "signaled" << endl;
			
			json &j = commands[0];
			
			if (j.find("data") != j.end()) {
				std::string data = j["data"];
				cout << "DATA: * " << data << endl;
				len -= 2;
				if( data.length() < len ) len = data.length();
				strncpy( buf, data.c_str(), len );
				buf[len++] = '\n';
				buf[len++] = '\0';
				return len + 1;
			}
			else if( j.find( "internal" ) != j.end()){

				std::vector < std::string > strvec;
				json commands = j["internal"];
				for( json::iterator iter = commands.begin(); iter != commands.end(); iter++ ){
					std::string str = iter->get<std::string>();
					strvec.push_back( str );
				}
			
				int err;
				PARSE_STATUS_2 ps;
				json rslt;
				
				exec_to_json( rslt, strvec, &err, &ps, false );
				
				json response = {{"type", "debug"}, {"data", {{"response", rslt}}}};
				push_response( response );
				
			}
			else {
				cout << "unknown debug packet type" << endl;
				return 0;
			}
		}
		
	}
	
	return 0;
	
}


/** callback from R */
void direct_callback( const char *channel, const char *data ){
	
	// FIXME: don't do this twice
	// FIXME: proper handling (at least for debug purposes)

	try {		
		json j = json::parse(data);
		json response = {{"type", channel}, {"data", j}};
		push_response( response );
	}
	catch( ... ){
		cout << "JSON parse exception (unknown)" << endl;
		cout << data << endl;
	}
		
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
		cout << "closing handles in write callback -- neat" << endl;
		
		uv_close( (uv_handle_t*)client, NULL );
		uv_close( (uv_handle_t*)&async_on_thread_loop, NULL );
	}
	
}

__inline void writeConsoleBuffer(){

	// is there anything on the console buffer?
	// FIXME: need a better way to test this -- flag?

	std::string s;
	os_buffer.locked_give(s);
	
	if( s.length()){
		json j = {{"type", "console"}, {"message", s.c_str()}};
		writeJSON( j, client, write_callback );
	}

}

/**
 * buffered write for console output.  
 */
void console_timer_callback( uv_timer_t* handle ){
	writeConsoleBuffer();	
}

/**
 * this is for the R thread to notify the communications thread,
 * generally outgoing messages
 */
void async_thread_loop_callback( uv_async_t *handle ){

	// FIXME: we can potentially do some console buffering
	// here, which would be good on linux with tiny writes.
	
	// if there are no formal messages, only console output,
	// set a timer for [X]ms (like currently done on the js
	// side) and process the console output in the timer 
	// callback.
	
	std::vector < json > messages;
	response_queue.locked_give_all(messages);
	
	if( messages.size() == 0 ){
		
		// presumably that means it's console-only.
		// FIXME: that might change if we start 
		// doing debug stuff... 
		
		uv_timer_stop( &console_buffer_timer );
		uv_timer_start( &console_buffer_timer, console_timer_callback, CONSOLE_BUFFER_TICK, 0 );
		return;
				
	}

	// write this FIRST.  that's in the event we bring back 
	// buffering in some form, we want to make sure this gets
	// cleared out before any "response" message is sent.

	writeConsoleBuffer();
	
	for( std::vector< json >::iterator iter = messages.begin();
			iter != messages.end(); iter++ ){
		writeJSON( *iter, client, write_callback );
	}
	
}

/**
 * this is for the communications thread to notify the R thread,
 * incoming messages and control packets
 */
void async_main_loop_callback( uv_async_t *handle ){
	
	std::vector < json > commands;
	command_queue.locked_give_all( commands );

	for( std::vector< json >::iterator iter = commands.begin();
		iter != commands.end(); iter++ ){
			
		json &j = *iter;
		if (j.find("command") != j.end()) {
			processCommand( j );
		}
		else cout << "command not found" << endl;

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
 * output from R.  write to the output stream; the comms thread 
 * will construct and send packets, potentially buffering.
 */
void log_message( const char *buf, int len = -1, bool console = false ){

	os_buffer.lock();
	os_buffer << buf;
	os_buffer.unlock();
	uv_async_send( &async_on_thread_loop );

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
	// uv_timer_stop( &timer );
	
	if( !cmd.compare( "break" )){
		
		// NOTE: this command does not send a response
		// nor trigger a callback.  therefore we exit 
		// the function here, make sure not to allocate
		// anything prior to this exit
	
		cout << "sending break" << endl;
		r_set_user_break();
		
		// restart the timer.  ugly.
		// uv_timer_start(&timer, timer_callback, TIMER_TICK, TIMER_TICK);
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
		
		// shutdown stuff on the main thread -- timer and async
		
		uv_timer_stop( &timer );
		uv_close( (uv_handle_t*)&async_on_main_loop, NULL );
		
		closing_sequence = true;
		initialized = false;

		// notify the thread.  it will shut down on write
		// (actually this isn't necessary, it will get notified
		// when the response is written).
		
		// uv_async_send( &async_on_thread_loop );
		
		r_shutdown();

	}
	else {
		
		response["err"] = 2;
		response["message"] = "Unknown command";
		response["command"] = cmd;
		
	}

	push_response( response );
	
}

void read_cb(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {

    if (nread < 0) {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t*) client, NULL);
        return;
    }

	json j = json::parse(std::string( buf->base, buf->len ));

	// special case: debug packets.  we need to capture and 
	// process them from this thread 
	
	bool handled = false;
	
	if (j.find("command") != j.end()) {
		std::string cmd = j["command"];
		if( !cmd.compare( "debug" )){
			
			cout << "DEBUG PACKET" << endl;
			debug_queue.locked_push_back(j);
			uv_cond_signal( &debug_condition );
			
		}
	}
	
	if( !handled ){
		command_queue.locked_push_back( j );
		uv_async_send( &async_on_main_loop );
	}
		 
	free(buf->base);
}

void connect_cb(uv_connect_t* req, int status){
	
	if( !status ){
		if( uv_is_readable( client )){
			cout << "calling read start" << endl;
			uv_read_start( client, alloc_buffer, read_cb );
		}
		else cout <<"client not readable!" << endl;
	}
}

void thread_func( void *data ){
	
	uv_loop_t threadloop;
	
	uv_loop_init( &threadloop );
	uv_async_init( &threadloop, &async_on_thread_loop, async_thread_loop_callback );
	uv_timer_init( &threadloop, &console_buffer_timer );

	uv_connect_t req;

	if( i_port ){
		uv_tcp_init(&threadloop, &_tcp);
		client = (uv_stream_t*)&_tcp;
		struct sockaddr_in dest;
		uv_ip4_addr( str_connection.c_str(), i_port, &dest);
		uv_tcp_connect( &req, &_tcp, (const struct sockaddr*)&dest, connect_cb);
	}
	else {
		client = (uv_stream_t*)&_pipe;
		uv_pipe_init( &threadloop, &_pipe, 0 );
		cout << "calling connect: pipe " << str_connection.c_str() << endl;
		uv_pipe_connect( &req, &_pipe, str_connection.c_str(), connect_cb);
	}
		
	cout << "running loop" << endl;
	uv_run( &threadloop, UV_RUN_DEFAULT );
	
	cout << "thread loop complete" << endl;
	uv_loop_close(&threadloop);
	cout << "thread loop closed" << endl;

	uv_close( (uv_handle_t*)&async_on_main_loop, NULL );
	uv_close( (uv_handle_t*)&console_buffer_timer, NULL );

	cout << "thread proc complete" << endl;
	
}

int main( int argc, char **argv ){

	if( argc <= 1 )
	{
		cout << "controlr" << endl;
		return 0;
	}

	arg0 = argv[0];

	uv_loop_t *loop = uv_default_loop();
	uv_async_init( loop, &async_on_main_loop, async_main_loop_callback );

	uv_mutex_init( &debug_condition_mutex );
	uv_cond_init( &debug_condition );

	uv_thread_t thread_id;
	str_connection = argv[1];
	i_port = ( argc > 2 ) ? atoi( argv[2] ) : 0;
	
    uv_thread_create(&thread_id, thread_func, 0);
	uv_timer_init( loop, &timer );
	uv_timer_start( &timer, timer_callback, INITIAL_TIMER_TICK, TIMER_TICK );
    	
	uv_run(loop, UV_RUN_DEFAULT);
	cout << "main loop complete" << endl;
	
	uv_thread_join( &thread_id );
	cout << "thread exited" << endl;
	
	uv_cond_destroy( &debug_condition );
	
	cout << "process exit" << endl << flush;

	return 0;
}

