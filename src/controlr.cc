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
 
#include "controlr.h"
#include "controlr_version.h"

using namespace std;
using json = nlohmann::json;

uv_tcp_t _tcp;
uv_pipe_t _pipe;

uv_stream_t *client;

locked_vector < json > command_queue;
locked_vector < json > response_queue;
locked_vector < json > input_queue;

uv_async_t async_on_thread_loop;

uv_timer_t console_buffer_timer;

uv_cond_t input_condition;
uv_mutex_t input_condition_mutex;

uv_cond_t init_condition;
uv_mutex_t init_condition_mutex;

// FIXME: tune
// what is the unit? ns?
#define CONSOLE_BUFFER_EVENTS_TICK (1000 * 1000 * 100)

// FIXME: tune
#define CONSOLE_BUFFER_TICK 25

bool closing_sequence = false;
bool initialized = false;

std::string str_connection;
int i_port;

locked_ostringstream os_buffer;
locked_ostringstream os_buffer_err;


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
__inline void writeJSON( json &j, std::vector< char* > *pool = 0){
	
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
	uv_write(req, client, &wrbuf, 1, write_callback);
	
}

int input_stream_read( const char *prompt, char *buf, int len, int addtohistory, bool is_continuation ){

	json srcref;
	json response = {{"type", "prompt"}, {"data", {{"prompt", prompt}, {"continuation", is_continuation}, {"srcref", get_srcref(srcref)}}}};
	push_response( response );
	
	cout << "ISR" << endl;

	// cout << "(PROMPT: " << prompt << ")" << endl;
	
	while( true ){
	
		// NOTE: from docs:
		//
		//   Note Callers should be prepared to deal with spurious 
		//   wakeups on uv_cond_wait() and uv_cond_timedwait().
		//
		// that's from the underlying pthreads implementation.  we need 
		// a mechanism to determine if that happens.  nevertheless we like 
		// conditions because there's a wait available with a timeout.
		
		uv_cond_timedwait( &input_condition, &input_condition_mutex, CONSOLE_BUFFER_EVENTS_TICK ); 

		std::vector < json > commands;
		input_queue.locked_give_all( commands );
		
		if( !commands.size()){

			// if commands is empty, this is either a timeout or one
			// of the stray signals.  in either case, process events.
			// this takes the place of the timer, which is blocked.
			
			if( initialized ) r_tick();
			
		}
		else {

			for( std::vector< json >::iterator iter = commands.begin(); iter != commands.end(); iter++ ){
				json &j = *iter;
				if (j.find("command") != j.end()) {	
					
					json response = {{"type", "response"}};
					std::string cmd = j["command"];

					// are we using this?  it's not necessarily a good fit
					// for some of the unbalanced calls. 
					if (j.find("id") != j.end()) response["id"] = j["id"];

					if( !cmd.compare( "rshutdown" )){

						// start shutdown, handles will close when the last
						// messages go out.
						
						closing_sequence = true;
						initialized = false;
						push_response( response );
						
						return 0; // this (EOF) will end the REPL
						
					}
					else if( !cmd.compare( "exec" )){
					
						// for this version we expect single-string exec --
						// R will handle incomplete parse -- but for legacy 
						// reasons we still have a vector coming in.
						
						// FIXME: no vectors in exec
						// FIXME: don't let client send in >= 4k (here we'll fail silently).
					
						if (j.find("commands") != j.end()) {
							
							std::ostringstream os;
							json commands = j["commands"];
							for( json::iterator iter = commands.begin(); iter != commands.end(); iter++ ){
								std::string str = iter->get<std::string>();
								os << str;
							}
							
							std::string command = os.str();
							len -= 2;
							if( command.length() < len ) len = command.length();
							strncpy( buf, command.c_str(), len );
							buf[len++] = '\n';
							buf[len++] = '\0';
							
							return len; // just !0
						}
					
					}
					else if( !cmd.compare( "internal" )){
						
						int err = 0;
						PARSE_STATUS_2 ps = PARSE2_EOF;
						if (j.find("commands") != j.end()) {
							
							std::vector < std::string > strvec;
							json commands = j["commands"];
							for( json::iterator iter = commands.begin(); iter != commands.end(); iter++ ){
								std::string str = iter->get<std::string>();
								strvec.push_back( str );
							}
							
							json rslt;
							response["response"] = exec_to_json( rslt, strvec, &err, &ps, false );

						}
						response["parsestatus"] = ps;
						if( err ) response["err"] = err;
						
					}
					else {
						response["err"] = 2;
						response["message"] = "Unknown command";
						response["command"] = cmd;
					}
					push_response( response );
				}
				else cerr << "Unexpected packet\n" << j.dump() << endl;
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
		cout << "closing handles in write callback" << endl;
		
		uv_close( (uv_handle_t*)client, NULL );
		uv_close( (uv_handle_t*)&async_on_thread_loop, NULL );
	}
	
}

__inline void flushConsoleBuffer(){

	// is there anything on the console buffer?
	// FIXME: need a better way to test this -- flag?

	std::string s;

	os_buffer_err.locked_give(s);
	if( s.length()){
		json j = {{"type", "console"}, {"message", s.c_str()}, {"flag", 1}};
		writeJSON( j );
	}

	os_buffer.locked_give(s);
	if( s.length()){
		json j = {{"type", "console"}, {"message", s.c_str()}, {"flag", 0}};
		writeJSON( j );
	}

}

/**
 * buffered write for console output.  
 */
void console_timer_callback( uv_timer_t* handle ){
	flushConsoleBuffer();	
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

	flushConsoleBuffer();
	
	for( std::vector< json >::iterator iter = messages.begin();
			iter != messages.end(); iter++ ){
		writeJSON( *iter );
	}
	
}

/**
 * output from R.  write to the output stream; the comms thread 
 * will construct and send packets, potentially buffering.
 */
void console_message( const char *buf, int len, int flag ){

	if( flag ){
		os_buffer_err.lock();
		os_buffer_err << buf;
		os_buffer_err.unlock();
	}
	else {
		os_buffer.lock();
		os_buffer << buf;
		os_buffer.unlock();
	}
	
	uv_async_send( &async_on_thread_loop );

}

/** standard alloc method */
void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
	buf->base = (char*)malloc(suggested_size);
	buf->len = suggested_size;
}

void read_cb(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {

    if (nread < 0) {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t*) client, NULL);
        return;
    }

	json j = json::parse(std::string( buf->base, buf->len ));

	// in the new structure, when R is running everything has 
	// to get signaled by the condition in the stream loop, 
	// excep INIT which we handle via async and BREAK which 
	// we handle directly.
	
	// UPDATE: initializing R in the async callback was a 
	// problem, even on the main thread, because it was using 
	// setjmp/longjmp and the stack was being invalidated.  we 
	// have to be a little careful about where we initialize...

	bool handled = false;

	if (j.find("command") != j.end()) {
		std::string cmd = j["command"];
		if( !cmd.compare( "rinit" )){
			command_queue.locked_push_back( j );
			uv_cond_signal( &init_condition );
			handled = true;
		}
		else if( !cmd.compare( "break" )){
			
			handled = true;
			r_set_user_break();
			
			// response? watch out for overlaps... this needs a special 
			// response class (if we want to respond).
			
		}
	}
	
	// not handled: signal the main thread.

	if( !handled && initialized ){
		input_queue.locked_push_back(j);
		uv_cond_signal( &input_condition );
	}
	
	free(buf->base);
}

/** on successful connection, start read loop */
void connect_cb(uv_connect_t* req, int status){
	
	cout << "CONNECT_CB" << endl;
	
	if( !status ){
		if( uv_is_readable( client )){
			uv_read_start( client, alloc_buffer, read_cb );
		}
		else cerr << "ERR: client not readable" << endl;
	}
}

/**
 * comms thread body.  runs until shutdown by closing in/out handles.
 */
void thread_func( void *data ){
	
	uv_loop_t threadloop;
	uv_connect_t req;

	// set up libuv loop, async, timer
	
	uv_loop_init( &threadloop );
	uv_async_init( &threadloop, &async_on_thread_loop, async_thread_loop_callback );
	uv_timer_init( &threadloop, &console_buffer_timer );

	// connect either tcp or port (named pipe)

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
		uv_pipe_connect( &req, &_pipe, str_connection.c_str(), connect_cb);
	}
	
	// run the loop as long as there are open handles
		
	uv_run( &threadloop, UV_RUN_DEFAULT );

	cout << "UV_RUN END" << endl;
	
	// done; close and clean up	
	
	uv_loop_close(&threadloop);
	uv_close( (uv_handle_t*)&console_buffer_timer, NULL );
	
}

int main( int argc, char **argv ){

	if( argc <= 1 )
	{
		cout << "controlr " 
			<< VERSION_MAJOR << "." 
			<< VERSION_MINOR << "." 
			<< VERSION_PATCH << endl;
			
		return 0;
	}

	uv_mutex_init( &input_condition_mutex );
	uv_cond_init( &input_condition ); 

	uv_thread_t thread_id;
	str_connection = argv[1];
	i_port = ( argc > 2 ) ? atoi( argv[2] ) : 0;
	
	uv_cond_init( &init_condition );
	uv_mutex_init( &init_condition_mutex );
    uv_thread_create(&thread_id, thread_func, 0);
	
	std::vector< json > commands;
	while( true ){
		uv_cond_wait( &init_condition, &init_condition_mutex );	
		command_queue.locked_give_all( commands );
		if( commands.size() > 0 ) break;
	}

	cout << "INIT SIGNALED" << endl;
	
	json j = commands[0];
	if( j.find( "command" ) != j.end()){
	
		std::string cmd = j["command"];
		if( !cmd.compare( "rinit" )){
		
			std::string rhome;
			if (j.find("rhome") != j.end()) rhome = j["rhome"].get<std::string>();

			char nosave[] = "--no-save";
			char* args[] = { argv[0], nosave };
			initialized = true;
			r_init( rhome.c_str(), "", 2, args );
		
		}
	}
	
	uv_thread_join( &thread_id );
	cout << "thread exited" << endl;
	
	uv_cond_destroy( &input_condition );
	cout << "process exit" << endl << flush;

	return 0;
}

