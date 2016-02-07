
const fs = require( "fs" );
const net = require( "net" );

var socket = "r." + Math.round( 1e8 * Math.random());
console.info( "socket: " + socket );

var process_buffer = function(buf){
    console.info( "buf!", buf );
    
}

var pause = function( delay, func ){
    return new Promise( function( resolve, reject ){
        setTimeout( function(){
            if( func ) func.call(this);
            resolve(true);
        }, delay);
    });
}

var packetid = 100;
var write_packet = function( socket, packet ){
    packet.id = packetid++;
    return new Promise( function( resolve, reject ){
        socket.write( JSON.stringify( packet ) + "\n\0", function(){
           resolve( arguments ); 
        });
    });
}

var connect_cb = function(server){
    server.on( "connection", function(sock){
			console.info( "connection" );
			sock.setEncoding('utf8');
			sock.on( "readable", function(){
				var r = sock.read();
				if( r ) process_buffer( r );
			});
			sock.on( "end", function(){
				console.info( "end" );
			});
			sock.on( "close", function(){
				console.info( "sock close", arguments );
			});
			sock.on( "error", function(){
				console.info( "sock error", arguments );
			});

            pause( 100 ).then( function(){
                return write_packet( sock, {
                   command: 'rinit',
                   rhome: '/home/duncan/dev/R-3.2.3' });
            }).then( function(){
                return( pause( 2500 ));
            }).then( function(){
                return write_packet( sock, {
                   command: 'rshutdown' });
            }).then( function(){
                return( pause( 2500 ));
            }).catch( function(e){
                console.info( "Exception", e );
            });
            
            
		});
}

var server = net.createServer().listen( socket, function(){
    connect_cb( server );
});


