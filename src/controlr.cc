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

uv_tcp_t _tcp;
uv_pipe_t _pipe;
uv_stream_t *client;

locked_vector < JSONDocument* > command_queue2;
locked_vector < JSONDocument* > input_queue2;
locked_vector < JSONDocument* > response_queue2;

uv_async_t async_on_thread_loop;
uv_timer_t console_buffer_timer;

uv_cond_t input_condition;
uv_mutex_t input_condition_mutex;

uv_cond_t init_condition;
uv_mutex_t init_condition_mutex;

// FIXME: tune
// what is the unit? ns?

#define CONSOLE_BUFFER_EVENTS_TICK_MS 75
#define CONSOLE_BUFFER_EVENTS_TICK (1000 * 1000 * CONSOLE_BUFFER_EVENTS_TICK_MS)

// heartbeat, as a function of tick 
#define HEARTBEAT_TICK_COUNT (1000/CONSOLE_BUFFER_EVENTS_TICK_MS)

// FIXME: tune
#define CONSOLE_BUFFER_TICK 25

bool closing_sequence = false;
bool initialized = false;

std::string str_connection;
int i_port;

locked_ostringstream os_buffer;
locked_ostringstream os_buffer_err;

int heartbeat_counter = 0;


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

__inline void push_response( JSONDocument *j, bool buffered = false ){
	
	response_queue2.locked_push_back( j );
	uv_async_send( &async_on_thread_loop );

}

/** 
 * write a packet plus a newline (which we use as a separator) 
 */
__inline void writeJSON( JSONDocument *j, std::vector< char* > *pool = 0){
	
//	std::string str = j.dump();
    std::string str = j->toString();
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
	
	int rslt = uv_write(req, client, &wrbuf, 1, write_callback);

	// we _should_ not get EAGAIN here, as it will be queued nonetheless.
	// the function try_write returns EAGAINs.

	if( rslt < 0 ){
		
		initialized = false;
		closing_sequence = true;
		
		if( !uv_is_closing( (uv_handle_t*)client)) uv_close( (uv_handle_t*)client, NULL );
		if( !uv_is_closing( (uv_handle_t*)&async_on_thread_loop)) uv_close( (uv_handle_t*)&async_on_thread_loop, NULL );
		if( !uv_is_closing( (uv_handle_t*)&console_buffer_timer )) uv_close( (uv_handle_t*)&console_buffer_timer, NULL );

	}

}

int input_stream_read( const char *prompt, char *buf, int len, int addtohistory, bool is_continuation ){

    JSONDocument *response = new JSONDocument();
    response->add( "type", "prompt" );

    JSONDocument tmp;
    tmp.add( "prompt", prompt );
    tmp.add( "continuation", is_continuation );

    JSONDocument srcref;
    tmp.add( "srcref", get_srcref2( srcref ));

    response->add( "data", tmp );
	push_response( response );

	while( !closing_sequence ){
	
		// NOTE: from docs:
		//
		//   Note Callers should be prepared to deal with spurious 
		//   wakeups on uv_cond_wait() and uv_cond_timedwait().
		//
		// that's from the underlying pthreads implementation.  we need 
		// a mechanism to determine if that happens.  nevertheless we like 
		// conditions because there's a wait available with a timeout.
		
		uv_cond_timedwait( &input_condition, &input_condition_mutex, CONSOLE_BUFFER_EVENTS_TICK ); 

		deallocate_on_deref < JSONDocument* > commands;
		input_queue2.locked_consume( commands );
		
		if( !commands.size()){

			// if commands is empty, this is either a timeout or one
			// of the stray signals.  in either case, process events.
			// this takes the place of the timer, which is blocked.
			
			if( initialized ) r_tick();
			
			// heartbeat: we are testing for dead sockets, which can
			// leave processes running if the parent terminates without
			// a clean shutdown.
			
			if( heartbeat_counter++ >= HEARTBEAT_TICK_COUNT ){
				heartbeat_counter = 0;
				heartbeat();
			}
			
		}
		else {

			heartbeat_counter = 0; // toll

			for( std::vector< JSONDocument* >::iterator iter = commands.begin(); iter != commands.end(); iter++ ){
				JSONDocument *jdoc = *iter;
                if (jdoc->has("command")) {	
					
                    JSONDocument *response = new JSONDocument();
                    response->add( "type", "response" );

                    std::string cmd = (*jdoc)["command"];

					// are we using this?  it's not necessarily a good fit
					// for some of the unbalanced calls. 
					if (jdoc->has("id")) response->add( "id", ((std::string)((*jdoc)["id"])).c_str());

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
					
						if (jdoc->has("commands")) {
							
							std::ostringstream os;
                            JSONValue commands = (*jdoc)["commands"];
                            if( commands.is_array()){
                                unsigned int alen = commands.length();
                                for( unsigned int i = 0; i< alen; i++ ){
                                    JSONValue jv = commands[i];
                                    std::string str = jv;
                                    os << str;
                                }   
                            }
                            else if( commands.is_string()) os << (std::string)commands;

							std::string command = os.str();
							len -= 2;
							if( len > 0 && command.length() < (unsigned int)len ) len = (int)command.length();
							strncpy( buf, command.c_str(), len );
							buf[len++] = '\n';
							buf[len++] = '\0';
							
							return len; // just !0
						}
					
					}
					else if( !cmd.compare( "internal" )){
						
						int err = 0;
						PARSE_STATUS_2 ps = PARSE2_EOF;
						if (jdoc->has("commands")) {
							
							std::vector < std::string > strvec;
                            JSONValue commands = (*jdoc)["commands"];

                            if( commands.is_array()){
                                unsigned int alen = commands.length();
                                for( unsigned int i = 0; i< alen; i++ ){
                                    JSONValue jv = commands[i];
                                    strvec.push_back(jv);
                                }   
                            }							
                            else if( commands.is_string()) strvec.push_back(commands);

                            JSONDocument rslt;
                            response->add( "response", exec_to_json2( rslt, strvec, &err, &ps, false ));


						}
						response->add( "parsestatus", ps );
                        if( err ) response->add( "err", err );

					}
					else {
						response->add( "err", 2 );
						response->add( "message", "Unknown command" );
						response->add( "command", cmd.c_str() );
					}
					push_response( response );
				}
				else cerr << "Unexpected packet\n" << jdoc->toString() << endl;
			}

		}
		
	}
	
	return 0;
	
}

/** 
 * callback from R 
 * @param buffered: does nothing at the moment
 */
void direct_callback( const char *channel, const char *data, bool buffered ){
	
	// FIXME: don't do this twice
	// FIXME: proper handling (at least for debug purposes)

    JSONDocument jsrc(data);
    JSONDocument *response = new JSONDocument();
    response->add( "type", channel );
    response->add( "data", jsrc );

    push_response( response, buffered );
		
}

/**
 * sync callback.  this will send out a message with the channel
 * "sync-request"; the client MUST respond with something, even
 * a null message, for the operation to complete.
 */
JSONDocument * sync_callback2( const char *data, bool buffered ){

	direct_callback( "sync-request", data, buffered );
	deallocate_on_deref < JSONDocument* > commands;

	// FIXME: timeout?
	
	while( true ){
		uv_cond_timedwait( &input_condition, &input_condition_mutex, CONSOLE_BUFFER_EVENTS_TICK ); 
		input_queue2.locked_consume( commands );
		if( commands.size()) break;
		if( initialized ) r_tick();
	}
	
	if( !commands.size()) return nullptr;
	JSONDocument *command = commands[0];
	if (command->has("response")) return nullptr;	

    JSONDocument *jdoc = new JSONDocument();
    JSONValue src = (*command)[ "response" ];
    jdoc->take(src);
    return jdoc;

	// jFIXME // return command["response"];
	// return nullptr;
}

/** 
 * callback on write op.  this class always allocates
 * buffers and stores them in data, so always free.
 *
 * FIXME: don't do that.
 */
void write_callback(uv_write_t *req, int status) {
	
	if (status < 0) {
		// fprintf(stderr, "Write error %s\n", uv_err_name(status));
		// FIXME: close?
	}
	if( req->data ) {
		free( req->data );
	} 
	free(req);
	
	if( closing_sequence ){
		// cout << "closing handles in write callback" << endl;
		if( !uv_is_closing( (uv_handle_t*)client)) uv_close( (uv_handle_t*)client, NULL );
		if( !uv_is_closing( (uv_handle_t*)&async_on_thread_loop)) uv_close( (uv_handle_t*)&async_on_thread_loop, NULL );
	}
	
}

__inline void flushConsoleBuffer(){

	if( closing_sequence ) return;

	// is there anything on the console buffer?
	// FIXME: need a better way to test this -- flag?

	std::string s;

	os_buffer_err.locked_consume(s);
	if( s.length()){
        JSONDocument jdoc;
		jdoc.add( "type", "console" );
        jdoc.add( "message", s.c_str() );
        jdoc.add( "flag", 1 );
		writeJSON( &jdoc );
	}

	os_buffer.locked_consume(s);
	if( s.length()){
        JSONDocument jdoc;
		jdoc.add( "type", "console" );
        jdoc.add( "message", s.c_str() );
        jdoc.add( "flag", 0 );
		writeJSON( &jdoc );
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
	
	deallocate_on_deref < JSONDocument* > messages;
	response_queue2.locked_consume(messages);
	
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
	
	for( std::vector< JSONDocument* >::iterator iter = messages.begin();
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
	buf->len = (int)suggested_size;
}

void read_cb(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {

    if (nread < 0) {
        if (nread != UV_EOF){
			// fprintf(stderr, "Read error %s\n", uv_err_name(nread));
		}
        uv_close((uv_handle_t*) client, NULL);
        return;
    }

    JSONDocument *jdoc = new JSONDocument(std::string( buf->base, buf->len ).c_str());

	// in the new structure, when R is running everything has 
	// to get signaled by the condition in the stream loop, 
	// excep INIT which we handle via async and BREAK which 
	// we handle directly.
	
	// UPDATE: initializing R in the async callback was a 
	// problem, even on the main thread, because it was using 
	// setjmp/longjmp and the stack was being invalidated.  we 
	// have to be a little careful about where we initialize...

	bool handled = false;

    if( jdoc->has( "command" )){
        std::string cmd = (*jdoc)["command"]; //j["command"];
		if( !cmd.compare( "rinit" )){
			command_queue2.locked_push_back( jdoc );
			uv_cond_signal( &init_condition );
			handled = true;
		}
		else if( !cmd.compare( "break" )){
			
			handled = true;
			r_set_user_break();
			
			// response? watch out for overlaps... this needs a special 
			// response class (if we want to respond).
			
            delete jdoc;

		}
	}
	
	// not handled: signal the main thread.

	if( !handled && initialized ){
		input_queue2.locked_push_back(jdoc);
		uv_cond_signal( &input_condition );
	}
	
	free(buf->base);
}

/** on successful connection, start read loop */
void connect_cb(uv_connect_t* req, int status){

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
	
	// done; close and clean up	
	
	uv_loop_close(&threadloop);
	
	if( !uv_is_closing( (uv_handle_t*)&console_buffer_timer )) uv_close( (uv_handle_t*)&console_buffer_timer, NULL );

}

void heartbeat(){
	
	if( closing_sequence ) return;

	// test connection.  we actually need to send something,
	// just checking for readability/writability will not
	// detect a broken stream.
	
    JSONDocument *response = new JSONDocument();
    response->add( "type", "heartbeat" );

    // jFIXME: static!

	push_response( response );
	
}



void exit_on_error( const char *msg ){
    

    JSONDocument *err = new JSONDocument();
	err->add( "type", "error" );
    if( msg ) err->add( "message", msg );

	closing_sequence = true;
	initialized = false;
	
	push_response( err );
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
	
	deallocate_on_deref< JSONDocument* > commands;
	while( true ){
		uv_cond_wait( &init_condition, &init_condition_mutex );	
		command_queue2.locked_consume( commands );
		if( commands.size() > 0 ) break;
	}

    JSONDocument *jdoc = commands[0];
	if( jdoc->has( "command" )){
	
        std::string cmd = (*jdoc)["command"];
		if( !cmd.compare( "rinit" )){
		
			std::string rhome;
			if (jdoc->has("rhome")){
                rhome = std::string((*jdoc)["rhome"]);
            }

			char nosave[] = "--no-save";
			char norestore[] = "--no-restore";
			
			char* args[] = { argv[0], nosave, norestore };
			initialized = true;
			if( r_loop( rhome.c_str(), "", 3, args )){
				exit_on_error( "R loop failed" );
			}
		
		}
	}
	
	// cout << "waiting for exit..." << endl;
	
	uv_thread_join( &thread_id );
	// cout << "thread complete" << endl;
	
	uv_cond_destroy( &input_condition );
	// cout << "process complete" << endl << flush;

	return 0;

}


