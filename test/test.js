
var R = require( "./js/wrapr")

R.on( 'console', function(msg){ console.log( msg ); });
R.on( 'callback', function(){ console.info( "** CALLBACK", arguments ); });

console.info( "ok?" );

// ...
/*
R.init({ rhome: '../R-3.2.3' }).then( function(){
	return R.exec( "?ls" );
})
.then( function( rsp ){
	console.info( "ok...");
//	R.shutdown();
})

*/

R.init({ rhome: '../R-3.2.3' }).then( function(){
	return R.exec( "2+2" );
})
//.then( function( rsp ){
//	return R.exec( "dyn.load('build/release/wrapr_process.exe')" );
//})
.then( function( rsp ){
	return R.exec( "is.loaded('wrapr_callback')" );
})
.then( function( rsp ){
	return R.exec( "2+2" );
})
.then( function( rsp ){
	return R.exec( ".Call('wrapr_callback', 'eat', 13, NA)" );
})
//.then( function( rsp ){
//	return R.exec( "library()" );
//})
.then( function( rsp ){
	console.info( "not dead? pausing 1 sec" );
	return R.exec( "Sys.sleep(2)" );
})
.then( function( rsp ){
	return R.exec( "2+2" );
})
.then( function( rsp ){
	console.info( "shutting down...");
	R.shutdown();
})
.then( function( rsp ){
	console.info( "shutdown ok?");
})
.catch( function( e ){
	console.info( "ERR", e );
});

console.info( "end" );


/*

R.init({ rhome: '../R-3.2.3' }).then( function(){
	return R.exec( "2+2" );
})
.then( function( rsp ){
	console.info( "exec response", rsp );
	return R.exec( "source( '../AC-patch.R' )");
})
.then( function( rsp ){
	console.info( "exec response (2)", rsp );
	return R.internal( "2+2" );
})
.then( function( rsp ){
	console.info( "internal response", rsp );
	return R.internal( ".Autocomplete('ls(', 3)" );
})
.then( function( rsp ){
	console.info( "internal response", rsp );
	return R.exec( "x <- list(A=100, B=200, 300, D='asd')" );
})
.then( function( rsp ){
	console.info( "internal response", rsp );
	return R.internal( "utils:::.CompletionEnv" );
})
.then( function( rsp ){
	console.info( "internal response (2)", rsp );
	return R.shutdown();
})
.then( function(){
	console.info( "shutdown ok?");
})
.catch(function(err){
	console.info("caught error", err);
	R.shutdown();
});

*/

