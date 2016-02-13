
/* global process */
/* global __dirname */

"use strict"

// switching to bluebird promises (at least temporarily)
const Promise = require("bluebird");

const fs = require( "fs" );
const os = require( "os");
const net = require( "net" );
const path = require( "path" );
const events = require( "events" );
const child_process = require( "child_process" );

// tcp defaults.  we use port 1440 but can climb up if it's in
// use (for multiple instances, for example); there's not a 
// lot of well-known stuff operating nearby

const DEFAULT_TCP_PORT_START = 1440;
const DEFAULT_TCP_PORT_END = 1480;
const DEFAULT_TCP_HOST = "127.0.0.1";

/**
 * start child process. set necessary environment info on various
 * platforms.  pass in arguments to child (generally the pipe name
 * or tcp address).
 */
var start_child_process = function( opts, args ){
	
	var env = {}, file, ext = "";
	for( var key in process.env ){ env[key] = process.env[key]; }

	if( process.platform.match( /win/ )){
		env.R_HOME = opts.rhome;
		env.PATH = path.join( opts.rhome, 'bin', 'x64')
				+ ";" + path.join( opts.basedir ? opts.basedir : __dirname, "..", "build", "Release" )
				+ ";" + process.env.PATH;
		ext = ".exe";
	}
	else {
		env.R_HOME = opts.rhome;
		env.LD_LIBRARY_PATH = path.join( opts.rhome, 'lib');
	}

 	file = opts.file || path.join( opts.basedir ? opts.basedir : __dirname, "..", "build", "Release", "controlr" + ext );
    
    if( opts.debug ){
        console.info( `file: ${file}` );
        console.info( "env:", env );
    }
    
    var proc = child_process.spawn( file, args, { env: env });
    
    if( opts.debug ){
    
        proc.stdout.on('data', (data) => {
            console.log(`child.stdout: ${data}`);
        });

        proc.stderr.on('data', (data) => {
            console.log(`child.stderr: ${data}`);
        });

        proc.on('close', (code) => {
            console.log(`child process exited with code ${code}`);
        });
    
    }
        
    return proc;
    
};

var create_pipe_name = function(){

	var count = 0; // sanity
	var pipe;
	do {
		if( process.platform.match( /win/ )){
			pipe = "\\\\.\\pipe\\r." + Math.round( 1e8 * Math.random());
		}
		else {
			pipe = path.join( os.tmpdir(), "r." + Math.round( 1e8 * Math.random()));
		}
	}
	while( fs.existsSync(pipe) && count++ < 1024 ); // FIXME: does this actually work on windows with named pipes?  ...
	
	if( count >= 1024 ) throw( "Can't create new pipe file" );
	return pipe;
}

/**
 * returns (via promise) the first available port starting at [port]
 * up to [port+count], or rejects.
 */
var check_port = function( port, host, count ) {
	return new Promise( function( resolve, reject ){
		var testserver = net.createServer()
				.once('error', function (err) {
					if( count > 0 ){
						check_port( port + 1, host, count - 1 ).then( function(x){
							resolve(x);
						}).catch(function(e){
							reject(e);
						});
					}
					//else resolve(false);
					else reject( "no ports available in range" );
				})
				.once('listening', function() {
					testserver.once('close', function() { resolve(port); })
					.close()
				})
				.listen(port, host);
	});
};
	
var pause = function( delay, func ){
    return new Promise( function( resolve, reject ){
        setTimeout( function(){
            if( func ) func.call(this);
            resolve(true);
        }, delay);
    });
};

var write_packet = function( socket, packet ){
    return new Promise( function( resolve, reject ){
        socket.write( JSON.stringify( packet ) + "\n\0", function(){
           resolve( arguments ); 
        });
    });
};

var on_read = function( socket, buffer, callback ){
    
	var str = socket.read();
	if( !str ) return;
	 
	var elts = str.split( "\n" );
	elts.map( function(x){

		if( buffer.length ) x = buffer + x;
        
		// valid packet (probably? cheaper than try/catch)
    	if( x.trim().endsWith( '}' )){		
			buffer = "";
			if( x.trim().length ){
				var json = null;
				try { json = JSON.parse(x); } 
				catch( e ){
					console.info(`parse exception: ${e}`, x);
					buffer = x;
				}
				if( json ) callback(json);
			}
		}
		else buffer = x;
	});
	
};
    
var ControlR = function(){
    
    var busy = false;
    var opts = null;
    var notify = null;
    var socket_file = null; 
    var socket = null;
    var server = null;
    var buffer = "";

    var instance = this;

    var read_callback = function( packet ){
        if( packet.type ){
            if( packet.type === "response" ){
                if( notify ) notify.call( this, packet );
            }
            else if( packet.type === "console" ){
                instance.emit( 'console', packet.message );
            }
				else if( packet.type === "graphics" 
					|| packet.type === "browser"  
					|| packet.type === "pager" ){
                instance.emit( packet.type, packet.data );
				}
				else if( packet.type === "watch" ){
					
					// we could actually handle this one here -- not
					// sure if it makes sense or if we should let the 
					// shell do it
					
					instance.emit( packet.type, packet.data );
					
				}
				else console.info( "unexpected packet type", packet );
        }
		  else console.info( "Packet missing type", packet );
    };

    var close = function(){
        server.close( this, function(){
         
            // FIXME: if this is the last call in the application,
            // is there a possibility it will exit before removing 
            // this file?  I think not, but could perhaps verify.
           	if( socket_file ) fs.unlink( socket_file );
				  
        });
    };

	 /**
	  * generic exec function.  
	  */
    var exec_packet = function( packet ){
        return new Promise( function( resolve, reject ){
            if( busy ) reject( "busy" );
            else {
                busy = true;
                instance.emit( 'state-change', busy );
                notify = function( response ){
                    busy = false;
                    notify = null;
                    if( response ){
							  if( response.parsestatus === 3 ) reject(response); // parse err
							  resolve( response );
						  }
                    else reject();
                    instance.emit( 'state-change', busy );
                };
                write_packet( socket, packet );
            }
        });
    };

	 /** get state */
	 this.busy = function(){ return busy; }

    /** execute a command.  any output will print to the console */
    this.exec = function( cmds ){
        return exec_packet({
           command: 'exec', commands: cmds 
        });
    };
    
    /** execute a command and return the response (via promise) */
    this.internal = function( cmds ){
        return exec_packet({
           command: 'internal', commands: cmds 
        });
    };

    /** shutdown and clean up */    
    this.shutdown = function(){
        if( socket ){
            exec_packet({ command: 'rshutdown' }).then( function(){
                close();    
                server = null;
            }).catch(function(e){
                close();    
                server = null;
            });
        }
        else if( server ){
            close();
            server = null;
        }
        else throw( "invalid state" );
    };

    /**
     * init the connection.  why an init method and not just init in the 
     * constructor? (1) it's symmetric with the shutdown method, which we
     * need to clean up, and (2) you can set up event handlers before 
     * initializing.
     */
    this.init = function(){

        opts = arguments[0] || {};
        if( opts.debug ) console.info( "controlr init", opts );
		  
		  return new Promise( function( resolve, reject ){
            
            if( server ) reject( "already initialized" );
				var args = [];
			
				// default to pipe, but support "tcp".  in the case of tcp, 
				// additional options [default]:
				// host ["127.0.0.1"]
				// port [1440]
				// last_port [1480]
				
				if( opts.connection_type === "tcp" ){
					var host = opts.host || DEFAULT_TCP_HOST;
					var start_port = DEFAULT_TCP_PORT_START;
					var end_port = DEFAULT_TCP_PORT_END;
					
					if( opts.port ){
						start_port = opts.port;
						end_port = ( opts.last_port && opts.last_port > opts.port ) ? opts.last_port : opts.port;
					}

					check_port( start_port, host, end_port - start_port ).then(function(port){
						args = [host, port];
   		         server = net.createServer().listen( port, host, connect_callback.bind( this, opts, args, resolve, reject ));
					}).catch( function(e){
						reject( e );
					});

				}
				else {
					socket_file = create_pipe_name();
					args = [socket_file];
	   	      server = net.createServer().listen( socket_file, connect_callback.bind( this, opts, args, resolve, reject ));
				}
				
        });
		  
	 };
	 
	 var connect_callback = function(opts, args, resolve, reject){
            
		// set up to catch the child connection
		server.on( "connection", function(){
						 
			if( opts.debug ) console.info( "connection" );
			if( socket ) reject( "too many clients" );
							
			socket = arguments[0];
			socket.setEncoding('utf8');
			socket.on( "readable", on_read.bind( this, socket, buffer, read_callback ));
			socket.on( "close", function(){
				if( opts.debug ) console.info( "socket close", arguments );
				socket = null;
			});
			socket.on( "error", function(){
				if( opts.debug ) console.info( "socket error", arguments );
			});

			pause( 100 ).then( function(){
				if( opts.debug ) console.info( "calling init" );
				return exec_packet({
						command: 'rinit',
						rhome: opts.rhome || "" });
			}).then( function(){
				if( opts.debug ) console.info( "init complete" ); 
				resolve(instance);
			}).catch( function(e){
				console.info( "Exception in init", e );
				reject(e);
			});
			
		});

		// then create the child
		//start_child_process( opts, [socket_file] );
		start_child_process( opts, args );

	 };
	 
    // notwithstanding the above comment, you can init right away if you want
    if( arguments[0] ) this.init.apply( this, arguments );
        
};

// inherits from event emitter
for (var x in events.EventEmitter.prototype){
	ControlR.prototype[x] = events.EventEmitter.prototype[x];
}

module.exports = ControlR;

